use std::sync::{Arc, Mutex, RwLock};
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::net::Ipv4Addr;
use std::time::{Duration, Instant};
use std::thread;

use axum::{
    Router,
    extract::{Query, State},
    response::IntoResponse,
    routing::get,
    http::{header, StatusCode},
};
use tower_http::services::ServeDir;
use serde::{Deserialize, Serialize};

use crate::connection::{RadarConnection, create_echo_socket};
use crate::protocol::{self, Spoke, decode_echo_packet};
use crate::render::{self, PPI_SIZE};

// --- Shared state ---

/// Tracks the current radar settings for CSV metadata.
#[derive(Clone, Serialize)]
pub struct CurrentSettings {
    pub range_index: u16,
    pub range_label: String,
    pub gain_auto: bool,
    pub gain_value: u16,
    pub pulse_width: u16,
    pub sea_clutter: u16,
    pub rain_clutter: u16,
    pub rezboost: u16,
    pub interference_rejection: bool,
    pub bird_mode: u16,
    pub doppler: u16,
    pub antenna_speed: u16,
    pub main_bang: u16,
    pub near_stc: u16,
    pub middle_stc: u16,
    pub far_stc: u16,
}

#[derive(Clone, Serialize)]
struct PatternStepInfo {
    #[serde(skip_serializing_if = "Option::is_none")]
    range: Option<u16>,
    #[serde(skip_serializing_if = "Option::is_none")]
    gain: Option<u16>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pulse_length: Option<u16>,
    #[serde(skip_serializing_if = "Option::is_none")]
    rez_boost: Option<u16>,
    #[serde(skip_serializing_if = "Option::is_none")]
    rotation_speed: Option<u16>,
    #[serde(skip_serializing_if = "Option::is_none")]
    interpolation: Option<u16>,
    #[serde(skip_serializing_if = "Option::is_none")]
    sea_clutter: Option<u16>,
    #[serde(skip_serializing_if = "Option::is_none")]
    rain_clutter: Option<u16>,
    #[serde(skip_serializing_if = "Option::is_none")]
    main_bang: Option<u16>,
    #[serde(skip_serializing_if = "Option::is_none")]
    interference_rejection: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    bird_mode: Option<u16>,
    #[serde(skip_serializing_if = "Option::is_none")]
    doppler: Option<u16>,
    repeat: u16,
}

#[derive(Clone, Serialize)]
struct PatternInfo {
    mode: String,
    label: String,
    repeat: Option<i32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    queue_index: Option<usize>,
    #[serde(skip_serializing_if = "Option::is_none")]
    queue_total: Option<usize>,
    steps: Vec<PatternStepInfo>,
}

#[derive(Clone, Serialize)]
struct SettingChange {
    timestamp: String,
    setting: String,
    value: serde_json::Value,
}

#[derive(Clone, Serialize)]
struct ManualRecordingSettingsSnapshot {
    mode: &'static str,
    recording_dir: String,
    current_settings: CurrentSettings,
    history: Vec<SettingChange>,
}

#[derive(Clone, Serialize)]
struct PatternRecordingSettingsSnapshot {
    mode: &'static str,
    recording_dir: String,
    current_settings: CurrentSettings,
    pattern: PatternInfo,
    #[serde(skip_serializing_if = "Vec::is_empty")]
    history: Vec<SettingChange>,
}

#[derive(Clone)]
enum RecordingSessionKind {
    Manual,
    Pattern(PatternInfo),
}

impl Default for CurrentSettings {
    fn default() -> Self {
        Self {
            range_index: 0, range_label: "1/8 NM".into(),
            gain_auto: false, gain_value: 50,
            pulse_width: 0, sea_clutter: 0, rain_clutter: 0,
            rezboost: 0, interference_rejection: false,
            bird_mode: 0, doppler: 0, antenna_speed: 0,
            main_bang: 0, near_stc: 0, middle_stc: 0, far_stc: 0,
        }
    }
}

pub struct AppState {
    conn: Mutex<RadarConnection>,
    latest_png: RwLock<Vec<u8>>,
    tx_active: AtomicBool,
    connected: AtomicBool,
    recording: AtomicBool,
    record_dir: RwLock<String>,
    rotation_count: AtomicU64,
    pattern_stop: AtomicBool,
    pattern_running: AtomicBool,
    pattern_status: RwLock<String>,
    active_pattern: RwLock<Option<PatternInfo>>,
    active_pattern_step: RwLock<Option<protocol::ControlStep>>,
    setting_history: RwLock<Vec<SettingChange>>,
    recording_session_kind: RwLock<RecordingSessionKind>,
    host_ip: Ipv4Addr,
    settings: RwLock<CurrentSettings>,
    start_time: Instant,
}

// --- Server entry point ---

#[tokio::main]
pub async fn start_server(radar_ip: Ipv4Addr, host_ip: Ipv4Addr, web_dir: &str, port: u16) {
    let mut conn = RadarConnection::new(radar_ip, host_ip);

    println!("Connecting to radar at {radar_ip}...");
    let connected = match conn.connect_full() {
        Ok(()) => {
            println!("Radar connected. Firmware: {}", conn.firmware.as_deref().unwrap_or("unknown"));
            true
        }
        Err(e) => {
            eprintln!("Warning: Initial connection failed: {e}");
            eprintln!("Web server will start. Use /reconnect to retry.");
            false
        }
    };

    let state = Arc::new(AppState {
        conn: Mutex::new(conn),
        latest_png: RwLock::new(Vec::new()),
        tx_active: AtomicBool::new(false),
        connected: AtomicBool::new(connected),
        recording: AtomicBool::new(false),
        record_dir: RwLock::new(String::new()),
        rotation_count: AtomicU64::new(0),
        pattern_stop: AtomicBool::new(false),
        pattern_running: AtomicBool::new(false),
        pattern_status: RwLock::new(String::new()),
        active_pattern: RwLock::new(None),
        active_pattern_step: RwLock::new(None),
        setting_history: RwLock::new(Vec::new()),
        recording_session_kind: RwLock::new(RecordingSessionKind::Manual),
        host_ip,
        settings: RwLock::new(CurrentSettings::default()),
        start_time: Instant::now(),
    });

    // Start echo listener thread
    let echo_state = state.clone();
    thread::spawn(move || echo_listener(echo_state));

    let app = Router::new()
        // Power
        .route("/toggle_echo", get(handle_toggle_echo))
        .route("/reconnect", get(handle_reconnect))
        // Read state
        .route("/get_current", get(handle_get_current))
        .route("/get_system_info", get(handle_get_system_info))
        .route("/system_metrics", get(handle_system_metrics))
        // Settings (existing)
        .route("/set_range", get(handle_set_range))
        .route("/set_pulse_width", get(handle_set_pulse))
        .route("/set_gain", get(handle_set_gain))
        .route("/set_acsea", get(handle_set_acsea))
        .route("/set_acrain", get(handle_set_acrain))
        .route("/set_bearing_resolution", get(handle_set_rezboost))
        .route("/set_antenna_revolution", get(handle_set_antenna))
        .route("/set_near_stc", get(handle_set_near_stc))
        .route("/set_middle_stc", get(handle_set_middle_stc))
        .route("/set_far_stc", get(handle_set_far_stc))
        .route("/set_sweep_total", get(handle_stub))
        .route("/set_interpolation", get(handle_stub))
        // Settings (new)
        .route("/set_main_bang", get(handle_set_main_bang))
        .route("/set_interference_rejection", get(handle_set_ir))
        .route("/set_bird_mode", get(handle_set_bird_mode))
        .route("/set_doppler", get(handle_set_doppler))
        // Recording
        .route("/toggle_recording", get(handle_toggle_recording))
        // Echo image
        .route("/newest_png", get(handle_newest_png))
        .route("/newest_trail", get(handle_newest_png))
        .route("/newest_detection", get(handle_newest_png))
        // Pattern execution
        .route("/run_pattern", get(handle_run_pattern))
        .route("/stop_pattern", get(handle_stop_pattern))
        .route("/run_pattern_queue", axum::routing::post(handle_run_pattern_queue))
        .route("/pattern_status", get(handle_pattern_status))
        .route("/begin_calibration", get(handle_run_pattern))
        .route("/run_ramp_pattern", get(handle_run_pattern))
        // Static files
        .fallback_service(ServeDir::new(web_dir))
        .with_state(state);

    let addr = format!("0.0.0.0:{port}");
    println!("Web server listening on http://{addr}");
    println!("Serving static files from {web_dir}");
    let listener = tokio::net::TcpListener::bind(&addr).await.unwrap();
    axum::serve(listener, app).await.unwrap();
}

// --- Echo listener thread ---

fn echo_listener(state: Arc<AppState>) {
    let echo_socket = create_echo_socket(state.host_ip);
    let mut buf = [0u8; 2048];
    let mut spokes: Vec<Spoke> = Vec::new();
    let mut prev_spoke_data: Vec<u8> = Vec::new();
    let mut prev_az: Option<u16> = None;
    let mut last_keepalive = Instant::now();

    loop {
        // Send keepalive every 5s regardless of TX state
        if last_keepalive.elapsed() > Duration::from_secs(5) {
            if let Ok(mut conn) = state.conn.try_lock() {
                let _ = conn.send_keepalive();
                conn.drain_control();
            }
            last_keepalive = Instant::now();
        }

        if !state.tx_active.load(Ordering::Relaxed) {
            thread::sleep(Duration::from_millis(100));
            continue;
        }

        match echo_socket.recv_from(&mut buf) {
            Ok((len, _)) => {
                if len < 20 { continue; }
                let az = u16::from_le_bytes([buf[16], buf[17]]);

                // Detect rotation wrap
                if let Some(prev) = prev_az {
                    if az < prev.saturating_sub(1000) {
                        // Rotation complete — render PPI + optionally save CSV
                        if !spokes.is_empty() {
                            let pixels = render::render_ppi(&spokes);
                            let png_bytes = render::encode_png_bytes(&pixels, PPI_SIZE, PPI_SIZE);
                            if let Ok(mut png) = state.latest_png.write() {
                                *png = png_bytes;
                            }

                            // Save CSV + PNG if recording
                            if state.recording.load(Ordering::Relaxed) {
                                if let Ok(dir) = state.record_dir.read() {
                                    if !dir.is_empty() {
                                        let ts = timestamp_compact();
                                        let csv_dir = format!("{dir}/csv");
                                        let png_dir = format!("{dir}/png");
                                        let _ = std::fs::create_dir_all(&csv_dir);
                                        let _ = std::fs::create_dir_all(&png_dir);
                                        let settings = state.settings.read().unwrap().clone();
                                        write_echo_csv(&format!("{csv_dir}/rotation_{ts}.csv"), &spokes, &settings);
                                        render::write_png(&format!("{png_dir}/rotation_{ts}.png"), &pixels, PPI_SIZE, PPI_SIZE);
                                    }
                                }
                            }

                        }
                        state.rotation_count.fetch_add(1, Ordering::Relaxed);
                        spokes.clear();
                    }
                }
                prev_az = Some(az);

                decode_echo_packet(&buf[..len], &mut prev_spoke_data, &mut spokes);
            }
            Err(_) => {}
        }
    }
}

// --- Helper: send a command and return ack text ---

async fn send_cmd(
    state: Arc<AppState>,
    cmd: String,
    ack_prefix: &'static str,
    success_msg: String,
) -> String {
    tokio::task::spawn_blocking(move || {
        let mut conn = state.conn.lock().unwrap();
        match conn.send_and_read_ack(&cmd, ack_prefix, Duration::from_secs(3)) {
            Ok((true, line)) => format!("{success_msg} | {line}"),
            Ok((false, _)) => format!("{success_msg} (no ack)"),
            Err(e) => format!("Error: {e}"),
        }
    }).await.unwrap_or_else(|e| format!("Error: {e}"))
}

// --- Endpoint parameter structs ---

#[derive(Deserialize)]
struct ToggleEchoParams {
    mode: String,
    #[serde(rename = "radarNo")]
    #[allow(dead_code)]
    radar_no: Option<u16>,
}

#[derive(Deserialize)]
struct GetCurrentParams {
    command: String,
    #[serde(rename = "radarNo")]
    #[allow(dead_code)]
    radar_no: Option<u16>,
    format: Option<String>,
}

#[derive(Deserialize)]
struct RangeParams {
    #[serde(rename = "rangeIndex")]
    range_index: u16,
    #[serde(rename = "radarNo")]
    #[allow(dead_code)]
    radar_no: Option<u16>,
    #[serde(rename = "rangeUnit")]
    #[allow(dead_code)]
    range_unit: Option<u16>,
}

#[derive(Deserialize)]
struct PulseParams {
    pulse: u16,
    #[serde(rename = "rangeIndex")]
    range_index: Option<u16>,
    #[serde(rename = "radarNo")]
    #[allow(dead_code)]
    radar_no: Option<u16>,
    #[serde(rename = "rangeUnit")]
    #[allow(dead_code)]
    range_unit: Option<u16>,
}

#[derive(Deserialize)]
struct GainParams {
    status: u16,
    #[serde(rename = "manualValue")]
    manual_value: Option<u16>,
    #[serde(rename = "autoValue")]
    auto_value: Option<u16>,
    #[serde(rename = "radarNo")]
    #[allow(dead_code)]
    radar_no: Option<u16>,
}

#[derive(Deserialize)]
struct SeaParams {
    manual: Option<u16>,
    #[serde(rename = "auto")]
    auto_val: Option<u16>,
    mode: Option<u16>,
    status: Option<u16>,
    #[serde(rename = "radarNo")]
    #[allow(dead_code)]
    radar_no: Option<u16>,
}

#[derive(Deserialize)]
struct RainParams {
    manual: Option<u16>,
    #[serde(rename = "auto")]
    auto_val: Option<u16>,
    #[serde(rename = "ftcMode")]
    ftc_mode: Option<u16>,
    #[serde(rename = "rainMode")]
    rain_mode: Option<u16>,
    status: Option<u16>,
    #[serde(rename = "radarNo")]
    #[allow(dead_code)]
    radar_no: Option<u16>,
}

#[derive(Deserialize)]
struct RezboostParams {
    mode: Option<u16>,
    #[serde(rename = "radarNo")]
    #[allow(dead_code)]
    radar_no: Option<u16>,
}

#[derive(Deserialize)]
struct AntennaParams {
    #[serde(rename = "rollSpeed")]
    roll_speed: Option<u16>,
    #[serde(rename = "hiRpm")]
    hi_rpm: Option<u16>,
    #[serde(rename = "radarNo")]
    #[allow(dead_code)]
    radar_no: Option<u16>,
}

#[derive(Deserialize)]
struct ValueParam {
    value: u16,
    #[serde(rename = "radarNo")]
    #[allow(dead_code)]
    radar_no: Option<u16>,
}

#[derive(Deserialize)]
struct ModeParam {
    mode: Option<u16>,
    enabled: Option<u16>,
    level: Option<u16>,
    value: Option<u16>,
}

// --- Handlers ---

async fn handle_toggle_echo(
    State(state): State<Arc<AppState>>,
    Query(params): Query<ToggleEchoParams>,
) -> String {
    if !state.connected.load(Ordering::Relaxed) {
        return "Not connected to radar. Use /reconnect first.".into();
    }

    let st = state.clone();
    tokio::task::spawn_blocking(move || {
        let mut conn = st.conn.lock().unwrap();
        match params.mode.as_str() {
            "on" => {
                // Ensure we're in the right state for tx_on
                match conn.tx_on() {
                    Ok(()) => {
                        st.tx_active.store(true, Ordering::Relaxed);
                        "TX ON: Radar is transmitting".to_string()
                    }
                    Err(e) => format!("TX ON failed: {e}"),
                }
            }
            "off" => {
                match conn.tx_off() {
                    Ok(()) => {
                        st.tx_active.store(false, Ordering::Relaxed);
                        "TX OFF: Radar stopped".to_string()
                    }
                    Err(e) => format!("TX OFF failed: {e}"),
                }
            }
            _ => "Unknown mode (use on/off)".to_string(),
        }
    }).await.unwrap_or_else(|e| format!("Error: {e}"))
}

async fn handle_reconnect(State(state): State<Arc<AppState>>) -> String {
    let st = state.clone();
    tokio::task::spawn_blocking(move || {
        let mut conn = st.conn.lock().unwrap();
        let radar_ip = conn.radar_ip;
        let host_ip = conn.host_ip;
        *conn = RadarConnection::new(radar_ip, host_ip);
        match conn.connect_full() {
            Ok(()) => {
                st.connected.store(true, Ordering::Relaxed);
                st.tx_active.store(false, Ordering::Relaxed);
                let fw = conn.firmware.as_deref().unwrap_or("unknown");
                format!("Reconnected. Firmware: {fw}")
            }
            Err(e) => {
                st.connected.store(false, Ordering::Relaxed);
                format!("Reconnect failed: {e}")
            }
        }
    }).await.unwrap_or_else(|e| format!("Error: {e}"))
}

async fn handle_get_current(
    State(state): State<Arc<AppState>>,
    Query(params): Query<GetCurrentParams>,
) -> axum::response::Response {
    let is_json = params.format.as_deref() == Some("json");
    let st = state.clone();
    let command = params.command.to_uppercase();

    let result = tokio::task::spawn_blocking(move || {
        let mut conn = st.conn.lock().unwrap();
        match command.as_str() {
            "TX" => {
                let cmd = "$R69,,,,,,,\r\n";
                match conn.send_and_read_ack(cmd, "$N69,", Duration::from_secs(3)) {
                    Ok((true, line)) => {
                        if line.contains(",2,") { "TX State: ON (transmitting)".into() }
                        else if line.contains(",1,") { "TX State: Standby".into() }
                        else { format!("TX State: {line}") }
                    }
                    Ok((false, _)) => "TX State: Unknown".into(),
                    Err(e) => format!("Error: {e}"),
                }
            }
            "RANGE" => {
                let cmd = "$R62,,,,,,,\r\n";
                match conn.send_and_read_ack(cmd, "$N62,", Duration::from_secs(3)) {
                    Ok((true, line)) => format!("Range: {line}"),
                    _ => "Range: Unknown".into(),
                }
            }
            "PULSE" => {
                let cmd = "$R68,,,,,,,\r\n";
                match conn.send_and_read_ack(cmd, "$N68,", Duration::from_secs(3)) {
                    Ok((true, line)) => format!("Pulse: {line}"),
                    _ => "Pulse: Unknown".into(),
                }
            }
            "GAIN" => {
                let cmd = "$R63,,,,,,,\r\n";
                match conn.send_and_read_ack(cmd, "$N63,", Duration::from_secs(3)) {
                    Ok((true, line)) => format!("Gain: {line}"),
                    _ => "Gain: Unknown".into(),
                }
            }
            "CLUTTER" => {
                let cmd_sea = "$R64,,,,,,,\r\n";
                let sea = match conn.send_and_read_ack(cmd_sea, "$N64,", Duration::from_secs(3)) {
                    Ok((true, line)) => line,
                    _ => "unknown".into(),
                };
                let cmd_rain = "$R65,,,,,,,\r\n";
                let rain = match conn.send_and_read_ack(cmd_rain, "$N65,", Duration::from_secs(3)) {
                    Ok((true, line)) => line,
                    _ => "unknown".into(),
                };
                format!("SEA: {sea} | RAIN: {rain}")
            }
            "ANTENNA" => {
                let cmd = "$R89,,,,,,,\r\n";
                match conn.send_and_read_ack(cmd, "$N89,", Duration::from_secs(3)) {
                    Ok((true, line)) => format!("Antenna: {line}"),
                    _ => "Antenna: Unknown".into(),
                }
            }
            "STC" => {
                let near = conn.send_and_read_ack("$R85,0\r\n", "$N85,", Duration::from_secs(2));
                let mid = conn.send_and_read_ack("$R86,0\r\n", "$N86,", Duration::from_secs(2));
                let far = conn.send_and_read_ack("$R87,0\r\n", "$N87,", Duration::from_secs(2));

                let parse_val = |res: Result<(bool, String), _>| -> i32 {
                    match res {
                        Ok((true, line)) => {
                            line.split(',').nth(1)
                                .and_then(|v| v.trim().parse::<i32>().ok())
                                .unwrap_or(0)
                        }
                        _ => 0,
                    }
                };
                let n = parse_val(near);
                let m = parse_val(mid);
                let f = parse_val(far);

                if is_json {
                    return format!("JSON:{{\"near\":{n},\"middle\":{m},\"far\":{f}}}");
                }
                format!("STC: near={n}, middle={m}, far={f} (note: may show power-on defaults)")
            }
            "MAINBANG" => {
                let cmd = "$R83,,,,,,,\r\n";
                match conn.send_and_read_ack(cmd, "$N83,", Duration::from_secs(3)) {
                    Ok((true, line)) => format!("Main Bang: {line}"),
                    _ => "Main Bang: Unknown".into(),
                }
            }
            "IR" => {
                let cmd = "$R67,0,0,,,,,\r\n";
                match conn.send_and_read_ack(cmd, "$N67,", Duration::from_secs(3)) {
                    Ok((true, line)) => format!("IR: {line}"),
                    _ => "IR: Unknown".into(),
                }
            }
            "BIRDMODE" => {
                let cmd = "$RED,,,,,,,\r\n";
                match conn.send_and_read_ack(cmd, "$NED,", Duration::from_secs(3)) {
                    Ok((true, line)) => format!("Bird Mode: {line}"),
                    _ => "Bird Mode: Unknown".into(),
                }
            }
            "DOPPLER" => {
                let cmd = "$REF,,,,,,,\r\n";
                match conn.send_and_read_ack(cmd, "$NEF,", Duration::from_secs(3)) {
                    Ok((true, line)) => format!("Doppler: {line}"),
                    _ => "Doppler: Unknown".into(),
                }
            }
            "SWEEP" => "Sweep: controlled locally (800x800 PPI)".into(),
            _ => format!("Unknown command: {command}"),
        }
    }).await.unwrap_or_else(|e| format!("Error: {e}"));

    if result.starts_with("JSON:") {
        let json = &result[5..];
        (
            StatusCode::OK,
            [(header::CONTENT_TYPE, "application/json")],
            json.to_string(),
        ).into_response()
    } else {
        result.into_response()
    }
}

async fn handle_set_range(
    State(state): State<Arc<AppState>>,
    Query(params): Query<RangeParams>,
) -> String {
    if let Some(msg) = reject_manual_setting_change(&state, "range") {
        return msg;
    }
    let cmd = protocol::cmd_range(params.range_index);
    apply_settings_update(
        &state,
        vec![
            ("range_index", serde_json::json!(params.range_index)),
            ("range_label", serde_json::json!(protocol::wire_index_to_label(params.range_index))),
        ],
        |s| {
            s.range_index = params.range_index;
            s.range_label = protocol::wire_index_to_label(params.range_index).to_string();
        },
    );
    send_cmd(state, cmd, "$N62,", format!("Range set to index {}", params.range_index)).await
}

async fn handle_set_pulse(
    State(state): State<Arc<AppState>>,
    Query(params): Query<PulseParams>,
) -> String {
    if let Some(msg) = reject_manual_setting_change(&state, "pulse_width") {
        return msg;
    }
    let range_idx = params.range_index.unwrap_or(0);
    let cmd = protocol::cmd_pulse_width(params.pulse, range_idx);
    apply_settings_update(&state, vec![("pulse_width", serde_json::json!(params.pulse))], |s| {
        s.pulse_width = params.pulse;
    });
    let names = ["S1", "S2", "M1", "M2", "M3", "L"];
    let name = names.get(params.pulse as usize).unwrap_or(&"?");
    send_cmd(state, cmd, "$N68,", format!("Pulse set to {name}")).await
}

async fn handle_set_gain(
    State(state): State<Arc<AppState>>,
    Query(params): Query<GainParams>,
) -> String {
    if let Some(msg) = reject_manual_setting_change(&state, "gain") {
        return msg;
    }
    let auto = params.status == 1;
    let value = if auto { params.auto_value.unwrap_or(0) } else { params.manual_value.unwrap_or(50) };
    let cmd = protocol::cmd_gain(auto, value);
    apply_settings_update(
        &state,
        vec![
            ("gain_auto", serde_json::json!(auto)),
            ("gain_value", serde_json::json!(value)),
        ],
        |s| {
            s.gain_auto = auto;
            s.gain_value = value;
        },
    );
    let mode = if auto { "auto" } else { "manual" };
    send_cmd(state, cmd, "$N63,", format!("Gain {mode}={value}")).await
}

async fn handle_set_acsea(
    State(state): State<Arc<AppState>>,
    Query(params): Query<SeaParams>,
) -> String {
    if let Some(msg) = reject_manual_setting_change(&state, "sea_clutter") {
        return msg;
    }
    let status = params.status.unwrap_or(1) != 0;
    let manual = params.manual.unwrap_or(0);
    let auto_val = params.auto_val.unwrap_or(0);
    let sea_mode = params.mode.unwrap_or(0);
    let cmd = protocol::cmd_sea_clutter_full(status, manual, auto_val, sea_mode);
    apply_settings_update(&state, vec![("sea_clutter", serde_json::json!(manual))], |s| {
        s.sea_clutter = manual;
    });
    send_cmd(state, cmd, "$N64,", format!("A/C SEA: manual={manual}, auto={auto_val}")).await
}

async fn handle_set_acrain(
    State(state): State<Arc<AppState>>,
    Query(params): Query<RainParams>,
) -> String {
    if let Some(msg) = reject_manual_setting_change(&state, "rain_clutter") {
        return msg;
    }
    let status = params.status.unwrap_or(1) != 0;
    let manual = params.manual.unwrap_or(0);
    let auto_val = params.auto_val.unwrap_or(0);
    let ftc = params.ftc_mode.unwrap_or(0);
    let rain = params.rain_mode.unwrap_or(0);
    let cmd = protocol::cmd_rain_clutter_full(status, manual, auto_val, ftc, rain);
    apply_settings_update(&state, vec![("rain_clutter", serde_json::json!(manual))], |s| {
        s.rain_clutter = manual;
    });
    send_cmd(state, cmd, "$N65,", format!("A/C RAIN: manual={manual}, auto={auto_val}")).await
}

async fn handle_set_rezboost(
    State(state): State<Arc<AppState>>,
    Query(params): Query<RezboostParams>,
) -> String {
    if let Some(msg) = reject_manual_setting_change(&state, "rezboost") {
        return msg;
    }
    let mode = params.mode.unwrap_or(0);
    let cmd = protocol::cmd_rezboost(mode);
    apply_settings_update(&state, vec![("rezboost", serde_json::json!(mode))], |s| {
        s.rezboost = mode;
    });
    let names = ["Off", "Low", "Medium", "High"];
    let name = names.get(mode as usize).unwrap_or(&"?");
    send_cmd(state, cmd, "$NEE,", format!("RezBoost set to {name}")).await
}

async fn handle_set_antenna(
    State(state): State<Arc<AppState>>,
    Query(params): Query<AntennaParams>,
) -> String {
    if let Some(msg) = reject_manual_setting_change(&state, "antenna_speed") {
        return msg;
    }
    let speed = params.roll_speed.unwrap_or(0);
    let hi_rpm = params.hi_rpm.unwrap_or(0) != 0;
    let cmd = protocol::cmd_antenna_revolution(speed, hi_rpm);
    apply_settings_update(&state, vec![("antenna_speed", serde_json::json!(speed))], |s| {
        s.antenna_speed = speed;
    });
    let speed_names = ["Low", "High", "Auto"];
    let speed_name = speed_names.get(speed as usize).unwrap_or(&"?");
    send_cmd(state, cmd, "$N89,", format!("Antenna: speed={speed_name}, hiRPM={hi_rpm}")).await
}

async fn handle_set_near_stc(
    State(state): State<Arc<AppState>>,
    Query(params): Query<ValueParam>,
) -> String {
    if let Some(msg) = reject_manual_setting_change(&state, "near_stc") {
        return msg;
    }
    let cmd = protocol::cmd_near_stc(params.value);
    apply_settings_update(&state, vec![("near_stc", serde_json::json!(params.value))], |s| {
        s.near_stc = params.value;
    });
    send_cmd(state, cmd, "$N85,", format!("Near STC set to {}", params.value)).await
}

async fn handle_set_middle_stc(
    State(state): State<Arc<AppState>>,
    Query(params): Query<ValueParam>,
) -> String {
    if let Some(msg) = reject_manual_setting_change(&state, "middle_stc") {
        return msg;
    }
    let cmd = protocol::cmd_middle_stc(params.value);
    apply_settings_update(&state, vec![("middle_stc", serde_json::json!(params.value))], |s| {
        s.middle_stc = params.value;
    });
    send_cmd(state, cmd, "$N86,", format!("Middle STC set to {}", params.value)).await
}

async fn handle_set_far_stc(
    State(state): State<Arc<AppState>>,
    Query(params): Query<ValueParam>,
) -> String {
    if let Some(msg) = reject_manual_setting_change(&state, "far_stc") {
        return msg;
    }
    let cmd = protocol::cmd_far_stc(params.value);
    apply_settings_update(&state, vec![("far_stc", serde_json::json!(params.value))], |s| {
        s.far_stc = params.value;
    });
    send_cmd(state, cmd, "$N87,", format!("Far STC set to {}", params.value)).await
}

async fn handle_set_main_bang(
    State(state): State<Arc<AppState>>,
    Query(params): Query<ModeParam>,
) -> String {
    if let Some(msg) = reject_manual_setting_change(&state, "main_bang") {
        return msg;
    }
    let val = params.value.unwrap_or(0);
    let cmd = protocol::cmd_main_bang(val);
    apply_settings_update(&state, vec![("main_bang", serde_json::json!(val))], |s| {
        s.main_bang = val;
    });
    send_cmd(state, cmd, "$N83,", format!("Main Bang Suppression set to {val}")).await
}

async fn handle_set_ir(
    State(state): State<Arc<AppState>>,
    Query(params): Query<ModeParam>,
) -> String {
    if let Some(msg) = reject_manual_setting_change(&state, "interference_rejection") {
        return msg;
    }
    let enabled = params.enabled.unwrap_or(0) != 0;
    let cmd = protocol::cmd_interference_rejection(enabled);
    apply_settings_update(
        &state,
        vec![("interference_rejection", serde_json::json!(enabled))],
        |s| {
            s.interference_rejection = enabled;
        },
    );
    let label = if enabled { "ON" } else { "OFF" };
    send_cmd(state, cmd, "$N67,", format!("Interference Rejection {label}")).await
}

async fn handle_set_bird_mode(
    State(state): State<Arc<AppState>>,
    Query(params): Query<ModeParam>,
) -> String {
    if let Some(msg) = reject_manual_setting_change(&state, "bird_mode") {
        return msg;
    }
    let level = params.level.unwrap_or(0);
    let cmd = protocol::cmd_bird_mode(level);
    apply_settings_update(&state, vec![("bird_mode", serde_json::json!(level))], |s| {
        s.bird_mode = level;
    });
    let names = ["Off", "Low", "Medium", "High"];
    let name = names.get(level as usize).unwrap_or(&"?");
    send_cmd(state, cmd, "$NED,", format!("Bird Mode set to {name}")).await
}

async fn handle_set_doppler(
    State(state): State<Arc<AppState>>,
    Query(params): Query<ModeParam>,
) -> String {
    if let Some(msg) = reject_manual_setting_change(&state, "doppler") {
        return msg;
    }
    let mode = params.mode.unwrap_or(0);
    let cmd = protocol::cmd_doppler(mode);
    apply_settings_update(&state, vec![("doppler", serde_json::json!(mode))], |s| {
        s.doppler = mode;
    });
    let names = ["Off", "On (internal heading)", "On (external heading)"];
    let name = names.get(mode as usize).unwrap_or(&"?");
    send_cmd(state, cmd, "$NEF,", format!("Doppler set to {name}")).await
}

async fn handle_newest_png(
    State(state): State<Arc<AppState>>,
) -> axum::response::Response {
    let png = state.latest_png.read().unwrap();
    if png.is_empty() {
        // Return a 1x1 black pixel PNG as placeholder
        let pixels = vec![0u8, 0, 0, 255];
        let placeholder = render::encode_png_bytes(&pixels, 1, 1);
        return (
            StatusCode::OK,
            [(header::CONTENT_TYPE, "image/png"),
             (header::CACHE_CONTROL, "no-cache")],
            placeholder,
        ).into_response();
    }
    (
        StatusCode::OK,
        [(header::CONTENT_TYPE, "image/png"),
         (header::CACHE_CONTROL, "no-cache")],
        png.clone(),
    ).into_response()
}

#[derive(Serialize)]
struct SystemMetrics {
    cpu_cores: Vec<f64>,
    cpu_load: f64,
    memory_used: f64,
    storage_used: f64,
    storage_path: String,
    network_rx_mbps: f64,
    network_tx_mbps: f64,
    temperature_c: f64,
}

async fn handle_system_metrics() -> axum::response::Response {
    let metrics = tokio::task::spawn_blocking(read_system_metrics)
        .await
        .unwrap_or_else(|_| SystemMetrics {
            cpu_cores: vec![],
            cpu_load: 0.0,
            memory_used: 0.0,
            storage_used: 0.0,
            storage_path: "/".into(),
            network_rx_mbps: 0.0,
            network_tx_mbps: 0.0,
            temperature_c: 0.0,
        });

    let json = serde_json::to_string(&metrics).unwrap_or_default();
    (
        StatusCode::OK,
        [(header::CONTENT_TYPE, "application/json")],
        json,
    ).into_response()
}

#[derive(Serialize)]
struct SystemInfo {
    newest_png: String,
    connected: bool,
    tx_active: bool,
    firmware: String,
    radar_ip: String,
    host_ip: String,
    recording: bool,
    record_dir: String,
    rotation_count: u64,
    uptime_seconds: u64,
    pattern_running: bool,
    settings: CurrentSettings,
    memory_total_mb: u64,
    memory_used_mb: u64,
    storage_total_gb: f64,
    storage_used_gb: f64,
}

async fn handle_get_system_info(State(state): State<Arc<AppState>>) -> axum::response::Response {
    let connected = state.connected.load(Ordering::Relaxed);
    let tx = state.tx_active.load(Ordering::Relaxed);
    let recording = state.recording.load(Ordering::Relaxed);
    let pattern_running = state.pattern_running.load(Ordering::Relaxed);
    let rotation_count = state.rotation_count.load(Ordering::Relaxed);
    let uptime_seconds = state.start_time.elapsed().as_secs();

    let (firmware, radar_ip) = state.conn.lock()
        .map(|c| (
            c.firmware.clone().unwrap_or_default(),
            c.radar_ip.to_string(),
        ))
        .unwrap_or_default();

    let record_dir = state.record_dir.read()
        .map(|d| d.clone())
        .unwrap_or_default();

    let settings = state.settings.read()
        .map(|s| s.clone())
        .unwrap_or_default();

    let (memory_total_mb, memory_used_mb) = read_memory_abs();
    let (storage_total_gb, storage_used_gb) = read_storage_abs();

    let info = SystemInfo {
        newest_png: if tx { "Live radar feed" } else { "TX off" }.into(),
        connected,
        tx_active: tx,
        firmware,
        radar_ip,
        host_ip: state.host_ip.to_string(),
        recording,
        record_dir,
        rotation_count,
        uptime_seconds,
        pattern_running,
        settings,
        memory_total_mb,
        memory_used_mb,
        storage_total_gb,
        storage_used_gb,
    };

    let json = serde_json::to_string(&info).unwrap_or_default();
    (
        StatusCode::OK,
        [(header::CONTENT_TYPE, "application/json")],
        json,
    ).into_response()
}

#[derive(Deserialize)]
struct RecordingParams {
    mode: String,
}

async fn handle_toggle_recording(
    State(state): State<Arc<AppState>>,
    Query(params): Query<RecordingParams>,
) -> String {
    match params.mode.as_str() {
        "on" | "start" => {
            state.recording.store(true, Ordering::Relaxed);
            let active_pattern = state.active_pattern.read().ok().and_then(|p| p.clone());
            let dir = if let Some(pattern) = active_pattern {
                match start_pattern_recording_session(&state, pattern, Some("pattern")) {
                    Ok(dir) => dir,
                    Err(e) => return format!("Failed to create directory: {e}"),
                }
            } else {
                match start_manual_recording_session(&state) {
                    Ok(dir) => dir,
                    Err(e) => return format!("Failed to create directory: {e}"),
                }
            };
            format!("Recording started. Saving to {dir}/")
        }
        "off" | "stop" => {
            state.recording.store(false, Ordering::Relaxed);
            let dir = state.record_dir.read().map(|d| d.clone()).unwrap_or_default();
            format!("Recording stopped. Files in {dir}/")
        }
        _ => "Unknown mode (use start/stop)".into(),
    }
}

async fn handle_stub() -> String {
    "OK".into()
}

// --- Pattern execution ---

#[derive(Deserialize)]
struct PatternParams {
    steps: String,
    repeat: Option<i32>,
    label: Option<String>,
}

#[derive(Deserialize)]
struct PatternQueuePayload {
    payload: String,
}

fn parse_pattern_steps(steps_str: &str) -> Result<Vec<protocol::ControlStep>, String> {
    let mut steps = Vec::new();
    for (i, part) in steps_str.split(';').enumerate() {
        let part = part.trim();
        if part.is_empty() { continue; }
        let fields: Vec<&str> = part.split(',').collect();
        if fields.len() != 7 && fields.len() != 13 {
            return Err(format!(
                "Step {} has {} fields, need 7 (legacy) or 13 (advanced)",
                i + 1,
                fields.len()
            ));
        }
        let parse = |idx: usize, name: &str| -> Result<u16, String> {
            fields[idx].trim().parse::<u16>().map_err(|_| format!("Step {}: invalid {}: '{}'", i + 1, name, fields[idx].trim()))
        };
        let parse_bool = |idx: usize, name: &str| -> Result<bool, String> {
            match fields[idx].trim() {
                "0" => Ok(false),
                "1" => Ok(true),
                other => Err(format!("Step {}: invalid {}: '{}'", i + 1, name, other)),
            }
        };
        let parse_optional = |idx: usize, name: &str| -> Result<Option<u16>, String> {
            let raw = fields[idx].trim();
            if raw.eq_ignore_ascii_case("nan") {
                Ok(None)
            } else {
                raw.parse::<u16>()
                    .map(Some)
                    .map_err(|_| format!("Step {}: invalid {}: '{}'", i + 1, name, raw))
            }
        };
        let parse_optional_bool = |idx: usize, name: &str| -> Result<Option<bool>, String> {
            let raw = fields[idx].trim();
            if raw.eq_ignore_ascii_case("nan") {
                Ok(None)
            } else {
                parse_bool(idx, name).map(Some)
            }
        };
        let mut step = protocol::ControlStep {
            range: None,
            gain: None,
            pulse_length: None,
            rez_boost: None,
            rotation_speed: None,
            interpolation: None,
            sea_clutter: None,
            rain_clutter: None,
            main_bang: None,
            interference_rejection: None,
            bird_mode: None,
            doppler: None,
            repeat: 0,
        };
        if fields.len() == 7 {
            step.range = Some(parse(0, "range")?);
            step.gain = Some(parse(1, "gain")?);
            step.pulse_length = Some(parse(2, "pulse")?);
            step.rez_boost = Some(parse(3, "rezboost")?);
            step.rotation_speed = Some(parse(4, "speed")?);
            step.interpolation = Some(parse(5, "interp")?);
            step.repeat = parse(6, "repeat")?;
        } else {
            step.range = parse_optional(0, "range")?;
            step.gain = parse_optional(1, "gain")?;
            step.pulse_length = parse_optional(2, "pulse")?;
            step.rez_boost = parse_optional(3, "rezboost")?;
            step.rotation_speed = parse_optional(4, "speed")?;
            step.interpolation = parse_optional(5, "interp")?;
            step.sea_clutter = parse_optional(6, "sea_clutter")?;
            step.rain_clutter = parse_optional(7, "rain_clutter")?;
            step.main_bang = parse_optional(8, "main_bang")?;
            step.interference_rejection = parse_optional_bool(9, "interference_rejection")?;
            step.bird_mode = parse_optional(10, "bird_mode")?;
            step.doppler = parse_optional(11, "doppler")?;
            step.repeat = parse(12, "repeat")?;
        }
        steps.push(step);
    }
    if steps.is_empty() {
        return Err("No valid steps found".into());
    }
    Ok(steps)
}

fn format_pattern_value<T: std::fmt::Display>(value: Option<T>) -> String {
    value.map(|v| v.to_string()).unwrap_or_else(|| "NaN".to_string())
}

fn pattern_step_allows_override(step: &protocol::ControlStep, setting: &str) -> bool {
    match setting {
        "range" => step.range.is_none(),
        "gain" => step.gain.is_none(),
        "pulse_width" => step.pulse_length.is_none(),
        "rezboost" => step.rez_boost.is_none(),
        "antenna_speed" => step.rotation_speed.is_none(),
        "sea_clutter" => step.sea_clutter.is_none(),
        "rain_clutter" => step.rain_clutter.is_none(),
        "main_bang" => step.main_bang.is_none(),
        "interference_rejection" => step.interference_rejection.is_none(),
        "bird_mode" => step.bird_mode.is_none(),
        "doppler" => step.doppler.is_none(),
        _ => false,
    }
}

fn reject_manual_setting_change(state: &AppState, setting: &str) -> Option<String> {
    if !state.pattern_running.load(Ordering::Relaxed) {
        return None;
    }
    let allowed = state
        .active_pattern_step
        .read()
        .ok()
        .and_then(|step| step.clone())
        .is_some_and(|step| pattern_step_allows_override(&step, setting));
    if allowed {
        None
    } else {
        Some(format!(
            "Pattern is running. '{setting}' is locked unless the current pattern step uses NaN for that field."
        ))
    }
}

fn pattern_info_from_steps(
    mode: &str,
    label: &str,
    repeat: Option<i32>,
    queue_index: Option<usize>,
    queue_total: Option<usize>,
    steps: &[protocol::ControlStep],
) -> PatternInfo {
    PatternInfo {
        mode: mode.to_string(),
        label: label.to_string(),
        repeat,
        queue_index,
        queue_total,
        steps: steps
            .iter()
            .map(|step| PatternStepInfo {
                range: step.range,
                gain: step.gain,
                pulse_length: step.pulse_length,
                rez_boost: step.rez_boost,
                rotation_speed: step.rotation_speed,
                interpolation: step.interpolation,
                sea_clutter: step.sea_clutter,
                rain_clutter: step.rain_clutter,
                main_bang: step.main_bang,
                interference_rejection: step.interference_rejection,
                bird_mode: step.bird_mode,
                doppler: step.doppler,
                repeat: step.repeat,
            })
            .collect(),
    }
}

fn record_setting_change(state: &AppState, setting: &str, value: serde_json::Value) {
    if let Ok(mut history) = state.setting_history.write() {
        history.push(SettingChange {
            timestamp: timestamp_compact(),
            setting: setting.to_string(),
            value,
        });
    }
}

fn apply_settings_update<F>(state: &AppState, changes: Vec<(&str, serde_json::Value)>, update: F)
where
    F: FnOnce(&mut CurrentSettings),
{
    if let Ok(mut settings) = state.settings.write() {
        update(&mut settings);
    }
    for (setting, value) in changes {
        record_setting_change(state, setting, value);
    }
    if state.recording.load(Ordering::Relaxed) {
        persist_recording_settings(state);
    }
}

fn write_manual_settings_snapshot(state: &AppState, dir: &str) {
    let current_settings = match state.settings.read() {
        Ok(settings) => settings.clone(),
        Err(_) => return,
    };
    let history = match state.setting_history.read() {
        Ok(history) => history.clone(),
        Err(_) => return,
    };

    let snapshot = ManualRecordingSettingsSnapshot {
        mode: "manual",
        recording_dir: dir.to_string(),
        current_settings,
        history,
    };

    if let Ok(json) = serde_json::to_string_pretty(&snapshot) {
        let _ = std::fs::write(format!("{dir}/settings.json"), json);
    }
}

fn write_pattern_settings_snapshot(state: &AppState, dir: &str, pattern: &PatternInfo) {
    let current_settings = match state.settings.read() {
        Ok(settings) => settings.clone(),
        Err(_) => return,
    };
    let history = match state.setting_history.read() {
        Ok(history) => history.clone(),
        Err(_) => return,
    };
    let snapshot = PatternRecordingSettingsSnapshot {
        mode: "pattern",
        recording_dir: dir.to_string(),
        current_settings,
        pattern: pattern.clone(),
        history,
    };
    if let Ok(json) = serde_json::to_string_pretty(&snapshot) {
        let _ = std::fs::write(format!("{dir}/settings.json"), json);
    }
}

fn persist_recording_settings(state: &AppState) {
    let dir = match state.record_dir.read() {
        Ok(dir) if !dir.is_empty() => dir.clone(),
        _ => return,
    };
    let kind = match state.recording_session_kind.read() {
        Ok(kind) => kind.clone(),
        Err(_) => return,
    };
    match kind {
        RecordingSessionKind::Manual => write_manual_settings_snapshot(state, &dir),
        RecordingSessionKind::Pattern(pattern) => write_pattern_settings_snapshot(state, &dir, &pattern),
    }
}

fn create_recording_dir(tag: Option<&str>) -> Result<String, std::io::Error> {
    let ts = timestamp_compact();
    let base = match tag {
        Some(tag) => format!("recordings/{ts}_{tag}"),
        None => format!("recordings/{ts}"),
    };
    let mut dir = base.clone();
    let mut suffix = 1usize;
    while std::path::Path::new(&dir).exists() {
        dir = format!("{base}_{suffix:02}");
        suffix += 1;
    }
    std::fs::create_dir_all(&dir)?;
    Ok(dir)
}

fn sanitize_pattern_label(label: &str) -> String {
    let cleaned = label
        .chars()
        .map(|ch| {
            if ch.is_ascii_alphanumeric() {
                ch.to_ascii_lowercase()
            } else if ch == '-' || ch == '_' {
                ch
            } else {
                '_'
            }
        })
        .collect::<String>();
    let trimmed = cleaned.trim_matches('_');
    if trimmed.is_empty() {
        "pattern".to_string()
    } else {
        trimmed.to_string()
    }
}

fn start_manual_recording_session(state: &AppState) -> Result<String, std::io::Error> {
    let dir = create_recording_dir(None)?;
    if let Ok(mut record_dir) = state.record_dir.write() {
        *record_dir = dir.clone();
    }
    if let Ok(mut history) = state.setting_history.write() {
        history.clear();
    }
    if let Ok(mut kind) = state.recording_session_kind.write() {
        *kind = RecordingSessionKind::Manual;
    }
    persist_recording_settings(state);
    Ok(dir)
}

fn start_pattern_recording_session(
    state: &AppState,
    pattern: PatternInfo,
    tag: Option<&str>,
) -> Result<String, std::io::Error> {
    let dir = create_recording_dir(tag)?;
    if let Ok(mut record_dir) = state.record_dir.write() {
        *record_dir = dir.clone();
    }
    if let Ok(mut history) = state.setting_history.write() {
        history.clear();
    }
    if let Ok(mut kind) = state.recording_session_kind.write() {
        *kind = RecordingSessionKind::Pattern(pattern);
    }
    persist_recording_settings(state);
    Ok(dir)
}

fn apply_pattern_step_to_settings(state: &AppState, step: &protocol::ControlStep) {
    if let Ok(mut settings) = state.settings.write() {
        if let Some(value) = step.range {
            settings.range_index = value;
            settings.range_label = protocol::wire_index_to_label(value).to_string();
        }
        if let Some(value) = step.gain {
            settings.gain_auto = false;
            settings.gain_value = value;
        }
        if let Some(value) = step.pulse_length {
            settings.pulse_width = value;
        }
        if let Some(value) = step.rez_boost {
            settings.rezboost = value;
        }
        if let Some(value) = step.rotation_speed {
            settings.antenna_speed = value;
        }
        if let Some(value) = step.sea_clutter {
            settings.sea_clutter = value;
        }
        if let Some(value) = step.rain_clutter {
            settings.rain_clutter = value;
        }
        if let Some(value) = step.main_bang {
            settings.main_bang = value;
        }
        if let Some(value) = step.interference_rejection {
            settings.interference_rejection = value;
        }
        if let Some(value) = step.bird_mode {
            settings.bird_mode = value;
        }
        if let Some(value) = step.doppler {
            settings.doppler = value;
        }
    }
}

fn execute_pattern(
    state: Arc<AppState>,
    steps: Vec<protocol::ControlStep>,
    loop_count: i32,
) {
    state.pattern_running.store(true, Ordering::Relaxed);
    state.pattern_stop.store(false, Ordering::Relaxed);

    let total_steps = steps.len();
    let loop_label = if loop_count < 0 { "infinite".to_string() } else { loop_count.to_string() };

    let mut iteration = 0u32;
    loop {
        iteration += 1;
        if loop_count >= 0 && iteration > loop_count as u32 {
            break;
        }

        let iter_label = if loop_count < 0 {
            format!("loop {iteration} (infinite)")
        } else {
            format!("loop {iteration}/{loop_label}")
        };

        let mut prev_step: Option<&protocol::ControlStep> = None;

        for (step_idx, step) in steps.iter().enumerate() {
            if state.pattern_stop.load(Ordering::Relaxed) {
                if let Ok(mut status) = state.pattern_status.write() {
                    *status = "Stopped by user".into();
                }
                if let Ok(mut pattern) = state.active_pattern.write() {
                    *pattern = None;
                }
                if let Ok(mut step_state) = state.active_pattern_step.write() {
                    *step_state = None;
                }
                if state.recording.load(Ordering::Relaxed) {
                    let _ = start_manual_recording_session(&state);
                }
                state.pattern_running.store(false, Ordering::Relaxed);
                return;
            }

            if let Ok(mut status) = state.pattern_status.write() {
                *status = format!("{iter_label} | step {}/{total_steps} | range={} gain={} pulse={} rez={} speed={} interp={} | waiting {} rotations",
                    step_idx + 1, format_pattern_value(step.range), format_pattern_value(step.gain), format_pattern_value(step.pulse_length),
                    format_pattern_value(step.rez_boost), format_pattern_value(step.rotation_speed), format_pattern_value(step.interpolation), step.repeat);
            }
            if let Ok(mut step_state) = state.active_pattern_step.write() {
                *step_state = Some(step.clone());
            }

            // Send commands for changed parameters
            let cmds = step.commands_vs(prev_step);
            if !cmds.is_empty() {
                if let Ok(mut conn) = state.conn.lock() {
                    for cmd in &cmds {
                        let _ = conn.send_and_read_ack(cmd, "$N", Duration::from_secs(3));
                    }
                }
                // Brief settle time after config change
                thread::sleep(Duration::from_millis(300));
            }
            apply_pattern_step_to_settings(&state, step);
            if state.recording.load(Ordering::Relaxed) {
                persist_recording_settings(&state);
            }

            // Wait for the specified number of rotations
            let start_rot = state.rotation_count.load(Ordering::Relaxed);
            let target_rot = start_rot + step.repeat as u64;

            loop {
                if state.pattern_stop.load(Ordering::Relaxed) {
                    break;
                }
                let current = state.rotation_count.load(Ordering::Relaxed);
                if current >= target_rot {
                    break;
                }
                thread::sleep(Duration::from_millis(50));
            }

            prev_step = Some(step);
        }
    }

    if let Ok(mut status) = state.pattern_status.write() {
        *status = format!("Complete ({total_steps} steps x {loop_label} loops)");
    }
    if let Ok(mut pattern) = state.active_pattern.write() {
        *pattern = None;
    }
    if let Ok(mut step_state) = state.active_pattern_step.write() {
        *step_state = None;
    }
    if state.recording.load(Ordering::Relaxed) {
        let _ = start_manual_recording_session(&state);
    }
    state.pattern_running.store(false, Ordering::Relaxed);
}

async fn handle_run_pattern(
    State(state): State<Arc<AppState>>,
    Query(params): Query<PatternParams>,
) -> String {
    if !state.connected.load(Ordering::Relaxed) {
        return "Not connected to radar".into();
    }
    if state.pattern_running.load(Ordering::Relaxed) {
        return "Pattern already running. Stop it first.".into();
    }

    let steps = match parse_pattern_steps(&params.steps) {
        Ok(s) => s,
        Err(e) => return format!("Invalid pattern: {e}"),
    };

    let repeat = params.repeat.unwrap_or(1);
    let num_steps = steps.len();
    let loop_label = if repeat < 0 { "infinite".to_string() } else { repeat.to_string() };
    let pattern_label = params.label.unwrap_or_else(|| "pattern".to_string());
    let pattern_info = pattern_info_from_steps("single", &pattern_label, Some(repeat), None, None, &steps);
    if let Ok(mut pattern) = state.active_pattern.write() {
        *pattern = Some(pattern_info.clone());
    }
    if state.recording.load(Ordering::Relaxed) {
        let folder_tag = sanitize_pattern_label(&pattern_info.label);
        match start_pattern_recording_session(&state, pattern_info.clone(), Some(&folder_tag)) {
            Ok(_) => {}
            Err(e) => return format!("Failed to create recording directory: {e}"),
        }
    }

    let st = state.clone();
    thread::spawn(move || execute_pattern(st, steps, repeat));

    format!("Pattern started: {num_steps} steps, repeat={loop_label}")
}

async fn handle_stop_pattern(
    State(state): State<Arc<AppState>>,
) -> String {
    if !state.pattern_running.load(Ordering::Relaxed) {
        return "No pattern running".into();
    }
    state.pattern_stop.store(true, Ordering::Relaxed);
    "Stop signal sent".into()
}

async fn handle_run_pattern_queue(
    State(state): State<Arc<AppState>>,
    axum::extract::Form(form): axum::extract::Form<PatternQueuePayload>,
) -> String {
    if !state.connected.load(Ordering::Relaxed) {
        return "Not connected to radar".into();
    }
    if state.pattern_running.load(Ordering::Relaxed) {
        return "Pattern already running. Stop it first.".into();
    }

    // Parse payload: "repeat1:base64(steps1),repeat2:base64(steps2),..."
    let mut queue: Vec<(String, i32, Vec<protocol::ControlStep>)> = Vec::new();

    for (i, item) in form.payload.split(',').enumerate() {
        let parts: Vec<&str> = item.splitn(3, ':').collect();
        if parts.len() != 3 {
            return format!("Queue item {} invalid format (expected repeat:base64(label):base64(steps))", i + 1);
        }
        let repeat: i32 = match parts[0].parse() {
            Ok(r) => r,
            Err(_) => return format!("Queue item {}: invalid repeat count '{}'", i + 1, parts[0]),
        };
        let label = match base64_decode(parts[1]) {
            Some(s) => s,
            None => return format!("Queue item {}: invalid label base64", i + 1),
        };
        let decoded = match base64_decode(parts[2]) {
            Some(s) => s,
            None => return format!("Queue item {}: invalid base64", i + 1),
        };
        let steps = match parse_pattern_steps(&decoded) {
            Ok(s) => s,
            Err(e) => return format!("Queue item {}: {e}", i + 1),
        };
        queue.push((label, repeat, steps));
    }

    if queue.is_empty() {
        return "Empty queue".into();
    }

    let num_items = queue.len();
    let st = state.clone();
    thread::spawn(move || {
        st.pattern_running.store(true, Ordering::Relaxed);
        st.pattern_stop.store(false, Ordering::Relaxed);

        for (qi, (label, repeat, steps)) in queue.iter().enumerate() {
            if st.pattern_stop.load(Ordering::Relaxed) {
                break;
            }
            let queue_pattern = pattern_info_from_steps("queue", label, Some(*repeat), Some(qi + 1), Some(num_items), steps);
            if let Ok(mut pattern) = st.active_pattern.write() {
                *pattern = Some(queue_pattern.clone());
            }
            if st.recording.load(Ordering::Relaxed) {
                let tag = sanitize_pattern_label(&queue_pattern.label);
                if let Err(e) = start_pattern_recording_session(&st, queue_pattern.clone(), Some(&tag)) {
                    if let Ok(mut status) = st.pattern_status.write() {
                        *status = format!("Queue {}/{} failed to create recording directory: {e}", qi + 1, num_items);
                    }
                    break;
                }
            }
            if let Ok(mut status) = st.pattern_status.write() {
                *status = format!("Queue {}/{num_items}: starting ({} steps, repeat={})", qi + 1, steps.len(), repeat);
            }
            // Reuse execute_pattern logic inline since we need to check stop between queue items
            let total_steps = steps.len();
            let mut iteration = 0u32;
            'outer: loop {
                iteration += 1;
                if *repeat >= 0 && iteration > *repeat as u32 {
                    break;
                }
                let mut prev_step: Option<&protocol::ControlStep> = None;
                for (step_idx, step) in steps.iter().enumerate() {
                    if st.pattern_stop.load(Ordering::Relaxed) {
                        break 'outer;
                    }
                    if let Ok(mut status) = st.pattern_status.write() {
                        *status = format!("Queue {}/{num_items} | loop {iteration} | step {}/{total_steps} | r={} g={} p={} rez={} spd={} interp={} | {} rots",
                            qi + 1, step_idx + 1, format_pattern_value(step.range), format_pattern_value(step.gain), format_pattern_value(step.pulse_length),
                            format_pattern_value(step.rez_boost), format_pattern_value(step.rotation_speed), format_pattern_value(step.interpolation), step.repeat);
                    }
                    if let Ok(mut step_state) = st.active_pattern_step.write() {
                        *step_state = Some(step.clone());
                    }
                    let cmds = step.commands_vs(prev_step);
                    if !cmds.is_empty() {
                        if let Ok(mut conn) = st.conn.lock() {
                            for cmd in &cmds {
                                let _ = conn.send_and_read_ack(cmd, "$N", Duration::from_secs(3));
                            }
                        }
                        thread::sleep(Duration::from_millis(300));
                    }
                    apply_pattern_step_to_settings(&st, step);
                    if st.recording.load(Ordering::Relaxed) {
                        persist_recording_settings(&st);
                    }
                    let start_rot = st.rotation_count.load(Ordering::Relaxed);
                    let target_rot = start_rot + step.repeat as u64;
                    loop {
                        if st.pattern_stop.load(Ordering::Relaxed) { break; }
                        if st.rotation_count.load(Ordering::Relaxed) >= target_rot { break; }
                        thread::sleep(Duration::from_millis(50));
                    }
                    prev_step = Some(step);
                }
            }
        }

        if let Ok(mut status) = st.pattern_status.write() {
            if st.pattern_stop.load(Ordering::Relaxed) {
                *status = "Queue stopped by user".into();
            } else {
                *status = format!("Queue complete ({num_items} patterns)");
            }
        }
        if let Ok(mut pattern) = st.active_pattern.write() {
            *pattern = None;
        }
        if let Ok(mut step_state) = st.active_pattern_step.write() {
            *step_state = None;
        }
        if st.recording.load(Ordering::Relaxed) {
            let _ = start_manual_recording_session(&st);
        }
        st.pattern_running.store(false, Ordering::Relaxed);
    });

    format!("Queue started: {num_items} patterns")
}

async fn handle_pattern_status(
    State(state): State<Arc<AppState>>,
) -> String {
    let running = state.pattern_running.load(Ordering::Relaxed);
    let status = state.pattern_status.read().map(|s| s.clone()).unwrap_or_default();
    if running {
        format!("running|{status}")
    } else if status.is_empty() {
        "idle|No pattern has been run".into()
    } else {
        format!("idle|{status}")
    }
}

fn base64_decode(input: &str) -> Option<String> {
    // Simple base64 decode (standard alphabet + padding)
    const TABLE: &[u8; 64] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    let input = input.trim();
    let mut buf: Vec<u8> = Vec::with_capacity(input.len() * 3 / 4);
    let mut accum: u32 = 0;
    let mut bits: u32 = 0;
    for &b in input.as_bytes() {
        if b == b'=' { break; }
        let val = TABLE.iter().position(|&c| c == b)? as u32;
        accum = (accum << 6) | val;
        bits += 6;
        if bits >= 8 {
            bits -= 8;
            buf.push((accum >> bits) as u8);
            accum &= (1 << bits) - 1;
        }
    }
    String::from_utf8(buf).ok()
}

// --- System metrics reader ---

use std::sync::OnceLock;

struct PrevSample {
    cpu_times: Vec<(u64, u64)>,  // (idle, total) per core
    net_rx: u64,
    net_tx: u64,
    timestamp: Instant,
}

static PREV_SAMPLE: OnceLock<Mutex<PrevSample>> = OnceLock::new();

fn get_prev_sample() -> &'static Mutex<PrevSample> {
    PREV_SAMPLE.get_or_init(|| Mutex::new(PrevSample {
        cpu_times: vec![],
        net_rx: 0,
        net_tx: 0,
        timestamp: Instant::now(),
    }))
}

fn parse_cpu_times() -> Vec<(u64, u64)> {
    let content = match std::fs::read_to_string("/proc/stat") {
        Ok(c) => c,
        Err(_) => return vec![],
    };
    let mut cores = vec![];
    for line in content.lines() {
        if !line.starts_with("cpu") { continue; }
        // Skip the aggregate "cpu " line — only want "cpu0", "cpu1", etc.
        let label = line.split_whitespace().next().unwrap_or("");
        if label == "cpu" { continue; }
        if !label.starts_with("cpu") { continue; }
        let vals: Vec<u64> = line.split_whitespace().skip(1)
            .filter_map(|s| s.parse::<u64>().ok())
            .collect();
        if vals.len() >= 4 {
            // user, nice, system, idle, iowait, irq, softirq, ...
            let idle = vals[3] + vals.get(4).copied().unwrap_or(0);
            let total: u64 = vals.iter().sum();
            cores.push((idle, total));
        }
    }
    cores
}

fn parse_net_bytes() -> (u64, u64) {
    let content = match std::fs::read_to_string("/proc/net/dev") {
        Ok(c) => c,
        Err(_) => return (0, 0),
    };
    let mut rx_total = 0u64;
    let mut tx_total = 0u64;
    for line in content.lines().skip(2) {
        let line = line.trim();
        if line.starts_with("lo:") { continue; }
        let parts: Vec<&str> = line.split_whitespace().collect();
        if parts.len() >= 10 {
            rx_total += parts[1].parse::<u64>().unwrap_or(0);
            tx_total += parts[9].parse::<u64>().unwrap_or(0);
        }
    }
    (rx_total, tx_total)
}

fn read_system_metrics() -> SystemMetrics {
    let mut metrics = SystemMetrics {
        cpu_cores: vec![],
        cpu_load: 0.0,
        memory_used: 0.0,
        storage_used: 0.0,
        storage_path: "/".into(),
        network_rx_mbps: 0.0,
        network_tx_mbps: 0.0,
        temperature_c: 0.0,
    };

    // Per-core CPU usage from /proc/stat delta
    let cur_cpu = parse_cpu_times();
    let (cur_rx, cur_tx) = parse_net_bytes();
    let now = Instant::now();

    if let Ok(mut prev) = get_prev_sample().lock() {
        let elapsed = now.duration_since(prev.timestamp).as_secs_f64();
        if elapsed > 0.05 && !prev.cpu_times.is_empty() && prev.cpu_times.len() == cur_cpu.len() {
            let mut core_usage = Vec::with_capacity(cur_cpu.len());
            for (i, &(cur_idle, cur_total)) in cur_cpu.iter().enumerate() {
                let (prev_idle, prev_total) = prev.cpu_times[i];
                let d_total = cur_total.saturating_sub(prev_total);
                let d_idle = cur_idle.saturating_sub(prev_idle);
                if d_total > 0 {
                    core_usage.push(1.0 - (d_idle as f64 / d_total as f64));
                } else {
                    core_usage.push(0.0);
                }
            }
            metrics.cpu_load = if core_usage.is_empty() { 0.0 } else {
                core_usage.iter().sum::<f64>() / core_usage.len() as f64
            };
            metrics.cpu_cores = core_usage;
        } else if !cur_cpu.is_empty() {
            metrics.cpu_cores = vec![0.0; cur_cpu.len()];
        }

        // Network throughput
        if elapsed > 0.05 && prev.net_rx > 0 {
            let d_rx = cur_rx.saturating_sub(prev.net_rx) as f64;
            let d_tx = cur_tx.saturating_sub(prev.net_tx) as f64;
            metrics.network_rx_mbps = (d_rx / elapsed) / (1024.0 * 1024.0);
            metrics.network_tx_mbps = (d_tx / elapsed) / (1024.0 * 1024.0);
        }

        prev.cpu_times = cur_cpu;
        prev.net_rx = cur_rx;
        prev.net_tx = cur_tx;
        prev.timestamp = now;
    }

    // Memory from /proc/meminfo
    if let Ok(content) = std::fs::read_to_string("/proc/meminfo") {
        let mut total = 0u64;
        let mut available = 0u64;
        for line in content.lines() {
            if line.starts_with("MemTotal:") {
                total = parse_meminfo_kb(line);
            } else if line.starts_with("MemAvailable:") {
                available = parse_meminfo_kb(line);
            }
        }
        if total > 0 {
            metrics.memory_used = 1.0 - (available as f64 / total as f64);
        }
    }

    // Storage
    if let Ok(stat) = nix_statvfs("/") {
        if stat.total > 0 {
            metrics.storage_used = 1.0 - (stat.available as f64 / stat.total as f64);
        }
    }

    // Temperature
    if let Ok(content) = std::fs::read_to_string("/sys/class/thermal/thermal_zone0/temp") {
        if let Ok(millic) = content.trim().parse::<f64>() {
            metrics.temperature_c = millic / 1000.0;
        }
    }

    metrics
}

fn parse_meminfo_kb(line: &str) -> u64 {
    line.split_whitespace()
        .nth(1)
        .and_then(|s| s.parse::<u64>().ok())
        .unwrap_or(0)
}

struct StatVfs {
    total: u64,
    available: u64,
}

fn nix_statvfs(path: &str) -> Result<StatVfs, ()> {
    use std::ffi::CString;
    let c_path = CString::new(path).map_err(|_| ())?;
    unsafe {
        let mut stat: libc::statvfs = std::mem::zeroed();
        if libc::statvfs(c_path.as_ptr(), &mut stat) == 0 {
            Ok(StatVfs {
                total: stat.f_blocks as u64 * stat.f_frsize as u64,
                available: stat.f_bavail as u64 * stat.f_frsize as u64,
            })
        } else {
            Err(())
        }
    }
}

fn read_memory_abs() -> (u64, u64) {
    if let Ok(content) = std::fs::read_to_string("/proc/meminfo") {
        let mut total_kb = 0u64;
        let mut available_kb = 0u64;
        for line in content.lines() {
            if line.starts_with("MemTotal:") {
                total_kb = parse_meminfo_kb(line);
            } else if line.starts_with("MemAvailable:") {
                available_kb = parse_meminfo_kb(line);
            }
        }
        let total_mb = total_kb / 1024;
        let used_mb = total_mb.saturating_sub(available_kb / 1024);
        (total_mb, used_mb)
    } else {
        (0, 0)
    }
}

fn read_storage_abs() -> (f64, f64) {
    if let Ok(stat) = nix_statvfs("/") {
        let total_gb = stat.total as f64 / (1024.0 * 1024.0 * 1024.0);
        let used_gb = total_gb - (stat.available as f64 / (1024.0 * 1024.0 * 1024.0));
        (total_gb, used_gb)
    } else {
        (0.0, 0.0)
    }
}

// --- CSV + timestamp helpers ---

fn timestamp_compact() -> String {
    let secs = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs();
    // Simple YYYYMMDD_HHMMSS from epoch
    let s = secs as i64;
    let days = s / 86400;
    let time_of_day = s % 86400;
    let hours = time_of_day / 3600;
    let minutes = (time_of_day % 3600) / 60;
    let seconds = time_of_day % 60;

    // Days since epoch to Y/M/D
    let mut y = 1970i32;
    let mut remaining = days;
    loop {
        let days_in_year = if y % 4 == 0 && (y % 100 != 0 || y % 400 == 0) { 366 } else { 365 };
        if remaining < days_in_year { break; }
        remaining -= days_in_year;
        y += 1;
    }
    let leap = y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
    let month_days = [31, if leap { 29 } else { 28 }, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
    let mut m = 0usize;
    for md in &month_days {
        if remaining < *md { break; }
        remaining -= md;
        m += 1;
    }
    let d = remaining + 1;

    format!("{y:04}{:02}{d:02}_{hours:02}{minutes:02}{seconds:02}", m + 1)
}

fn write_echo_csv(path: &str, spokes: &[Spoke], settings: &CurrentSettings) {
    use std::io::Write;
    let mut f = std::io::BufWriter::new(
        match std::fs::File::create(path) {
            Ok(f) => f,
            Err(e) => {
                eprintln!("Failed to create CSV {path}: {e}");
                return;
            }
        },
    );

    // Write metadata header as comment lines
    let _ = writeln!(f, "# range_index,{}", settings.range_index);
    let _ = writeln!(f, "# gain_auto,{}", settings.gain_auto);
    let _ = writeln!(f, "# gain_value,{}", settings.gain_value);
    let _ = writeln!(f, "# pulse_width,{}", settings.pulse_width);
    let _ = writeln!(f, "# sea_clutter,{}", settings.sea_clutter);
    let _ = writeln!(f, "# rain_clutter,{}", settings.rain_clutter);
    let _ = writeln!(f, "# rezboost,{}", settings.rezboost);
    let _ = writeln!(f, "# interference_rejection,{}", settings.interference_rejection);
    let _ = writeln!(f, "# bird_mode,{}", settings.bird_mode);
    let _ = writeln!(f, "# doppler,{}", settings.doppler);
    let _ = writeln!(f, "# antenna_speed,{}", settings.antenna_speed);
    let _ = writeln!(f, "# main_bang,{}", settings.main_bang);
    let _ = writeln!(f, "# near_stc,{}", settings.near_stc);
    let _ = writeln!(f, "# middle_stc,{}", settings.middle_stc);
    let _ = writeln!(f, "# far_stc,{}", settings.far_stc);
    let _ = writeln!(f, "Status\tScale\tRange\tGain\tAngle\tEchoValues");

    let gain = ((settings.gain_value as u32 * 100) / 255) as u16;
    let scale = settings.range_index;
    let range = settings.range_index;

    for spoke in spokes {
        let echo_values = spoke
            .echo
            .iter()
            .map(|value| value.to_string())
            .collect::<Vec<_>>()
            .join(",");
        let _ = writeln!(
            f,
            "0\t{scale}\t{range}\t{gain}\t{}\t{}",
            spoke.angle_ticks.min(8191),
            echo_values
        );
    }
}
