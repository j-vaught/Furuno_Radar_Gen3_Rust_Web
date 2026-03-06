use std::sync::{Arc, Mutex, RwLock};
use std::sync::atomic::{AtomicBool, Ordering};
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

pub struct AppState {
    conn: Mutex<RadarConnection>,
    latest_png: RwLock<Vec<u8>>,
    tx_active: AtomicBool,
    connected: AtomicBool,
    recording: AtomicBool,
    record_dir: RwLock<String>,
    host_ip: Ipv4Addr,
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
        host_ip,
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
        // Pattern stubs
        .route("/run_pattern", get(handle_pattern_stub))
        .route("/stop_pattern", get(handle_pattern_stub))
        .route("/run_pattern_queue", get(handle_pattern_stub))
        .route("/begin_calibration", get(handle_pattern_stub))
        .route("/run_ramp_pattern", get(handle_pattern_stub))
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

                            // Save CSV if recording
                            if state.recording.load(Ordering::Relaxed) {
                                if let Ok(dir) = state.record_dir.read() {
                                    if !dir.is_empty() {
                                        let ts = timestamp_compact();
                                        let csv_path = format!("{dir}/rotation_{ts}.csv");
                                        let png_path = format!("{dir}/rotation_{ts}.png");
                                        write_echo_csv(&csv_path, &spokes);
                                        render::write_png(&png_path, &pixels, PPI_SIZE, PPI_SIZE);
                                    }
                                }
                            }

                        }
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
    let cmd = protocol::cmd_range(params.range_index);
    send_cmd(state, cmd, "$N62,", format!("Range set to index {}", params.range_index)).await
}

async fn handle_set_pulse(
    State(state): State<Arc<AppState>>,
    Query(params): Query<PulseParams>,
) -> String {
    let range_idx = params.range_index.unwrap_or(0);
    let cmd = protocol::cmd_pulse_width(params.pulse, range_idx);
    let names = ["S1", "S2", "M1", "M2", "M3", "L"];
    let name = names.get(params.pulse as usize).unwrap_or(&"?");
    send_cmd(state, cmd, "$N68,", format!("Pulse set to {name}")).await
}

async fn handle_set_gain(
    State(state): State<Arc<AppState>>,
    Query(params): Query<GainParams>,
) -> String {
    let auto = params.status == 1;
    let value = if auto { params.auto_value.unwrap_or(0) } else { params.manual_value.unwrap_or(50) };
    let cmd = protocol::cmd_gain(auto, value);
    let mode = if auto { "auto" } else { "manual" };
    send_cmd(state, cmd, "$N63,", format!("Gain {mode}={value}")).await
}

async fn handle_set_acsea(
    State(state): State<Arc<AppState>>,
    Query(params): Query<SeaParams>,
) -> String {
    let status = params.status.unwrap_or(1) != 0;
    let manual = params.manual.unwrap_or(0);
    let auto_val = params.auto_val.unwrap_or(0);
    let sea_mode = params.mode.unwrap_or(0);
    let cmd = protocol::cmd_sea_clutter_full(status, manual, auto_val, sea_mode);
    send_cmd(state, cmd, "$N64,", format!("A/C SEA: manual={manual}, auto={auto_val}")).await
}

async fn handle_set_acrain(
    State(state): State<Arc<AppState>>,
    Query(params): Query<RainParams>,
) -> String {
    let status = params.status.unwrap_or(1) != 0;
    let manual = params.manual.unwrap_or(0);
    let auto_val = params.auto_val.unwrap_or(0);
    let ftc = params.ftc_mode.unwrap_or(0);
    let rain = params.rain_mode.unwrap_or(0);
    let cmd = protocol::cmd_rain_clutter_full(status, manual, auto_val, ftc, rain);
    send_cmd(state, cmd, "$N65,", format!("A/C RAIN: manual={manual}, auto={auto_val}")).await
}

async fn handle_set_rezboost(
    State(state): State<Arc<AppState>>,
    Query(params): Query<RezboostParams>,
) -> String {
    let mode = params.mode.unwrap_or(0);
    let cmd = protocol::cmd_rezboost(mode);
    let names = ["Off", "Low", "Medium", "High"];
    let name = names.get(mode as usize).unwrap_or(&"?");
    send_cmd(state, cmd, "$NEE,", format!("RezBoost set to {name}")).await
}

async fn handle_set_antenna(
    State(state): State<Arc<AppState>>,
    Query(params): Query<AntennaParams>,
) -> String {
    let speed = params.roll_speed.unwrap_or(0);
    let hi_rpm = params.hi_rpm.unwrap_or(0) != 0;
    let cmd = protocol::cmd_antenna_revolution(speed, hi_rpm);
    let speed_names = ["Low", "High", "Auto"];
    let speed_name = speed_names.get(speed as usize).unwrap_or(&"?");
    send_cmd(state, cmd, "$N89,", format!("Antenna: speed={speed_name}, hiRPM={hi_rpm}")).await
}

async fn handle_set_near_stc(
    State(state): State<Arc<AppState>>,
    Query(params): Query<ValueParam>,
) -> String {
    let cmd = protocol::cmd_near_stc(params.value);
    send_cmd(state, cmd, "$N85,", format!("Near STC set to {}", params.value)).await
}

async fn handle_set_middle_stc(
    State(state): State<Arc<AppState>>,
    Query(params): Query<ValueParam>,
) -> String {
    let cmd = protocol::cmd_middle_stc(params.value);
    send_cmd(state, cmd, "$N86,", format!("Middle STC set to {}", params.value)).await
}

async fn handle_set_far_stc(
    State(state): State<Arc<AppState>>,
    Query(params): Query<ValueParam>,
) -> String {
    let cmd = protocol::cmd_far_stc(params.value);
    send_cmd(state, cmd, "$N87,", format!("Far STC set to {}", params.value)).await
}

async fn handle_set_main_bang(
    State(state): State<Arc<AppState>>,
    Query(params): Query<ModeParam>,
) -> String {
    let val = params.value.unwrap_or(0);
    let cmd = protocol::cmd_main_bang(val);
    send_cmd(state, cmd, "$N83,", format!("Main Bang Suppression set to {val}")).await
}

async fn handle_set_ir(
    State(state): State<Arc<AppState>>,
    Query(params): Query<ModeParam>,
) -> String {
    let enabled = params.enabled.unwrap_or(0) != 0;
    let cmd = protocol::cmd_interference_rejection(enabled);
    let label = if enabled { "ON" } else { "OFF" };
    send_cmd(state, cmd, "$N67,", format!("Interference Rejection {label}")).await
}

async fn handle_set_bird_mode(
    State(state): State<Arc<AppState>>,
    Query(params): Query<ModeParam>,
) -> String {
    let level = params.level.unwrap_or(0);
    let cmd = protocol::cmd_bird_mode(level);
    let names = ["Off", "Low", "Medium", "High"];
    let name = names.get(level as usize).unwrap_or(&"?");
    send_cmd(state, cmd, "$NED,", format!("Bird Mode set to {name}")).await
}

async fn handle_set_doppler(
    State(state): State<Arc<AppState>>,
    Query(params): Query<ModeParam>,
) -> String {
    let mode = params.mode.unwrap_or(0);
    let cmd = protocol::cmd_doppler(mode);
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
}

async fn handle_get_system_info(State(state): State<Arc<AppState>>) -> axum::response::Response {
    let connected = state.connected.load(Ordering::Relaxed);
    let tx = state.tx_active.load(Ordering::Relaxed);
    let firmware = state.conn.lock()
        .map(|c| c.firmware.clone().unwrap_or_default())
        .unwrap_or_default();

    let info = SystemInfo {
        newest_png: if tx { "Live radar feed" } else { "TX off" }.into(),
        connected,
        tx_active: tx,
        firmware,
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
            let ts = timestamp_compact();
            let dir = format!("recordings/{ts}");
            if let Err(e) = std::fs::create_dir_all(&dir) {
                return format!("Failed to create directory: {e}");
            }
            if let Ok(mut d) = state.record_dir.write() {
                *d = dir.clone();
            }
            state.recording.store(true, Ordering::Relaxed);
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

async fn handle_pattern_stub() -> String {
    "Pattern queue not yet implemented in Rust backend. Use CLI calibrate mode.".into()
}

// --- System metrics reader ---

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

    // CPU load from /proc/loadavg
    if let Ok(content) = std::fs::read_to_string("/proc/loadavg") {
        if let Some(load) = content.split_whitespace().next() {
            if let Ok(val) = load.parse::<f64>() {
                let num_cpus = num_cpus_count();
                metrics.cpu_load = (val / num_cpus as f64).min(1.0);
                metrics.cpu_cores = vec![metrics.cpu_load; num_cpus];
            }
        }
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

    // Network (simplified — just report 0 for now since proper tracking needs two samples)
    metrics.network_rx_mbps = 0.0;
    metrics.network_tx_mbps = 0.0;

    metrics
}

fn num_cpus_count() -> usize {
    std::fs::read_to_string("/proc/cpuinfo")
        .map(|c| c.matches("processor").count())
        .unwrap_or(4)
        .max(1)
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

fn write_echo_csv(path: &str, spokes: &[Spoke]) {
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

    let max_bins = spokes.iter().map(|s| s.echo.len()).max().unwrap_or(0);
    let mut header = String::from("spoke_index,angle_ticks,angle_deg,heading_ticks,num_bins");
    for i in 0..max_bins {
        header.push_str(&format!(",bin_{i}"));
    }
    let _ = writeln!(f, "{header}");

    for (i, spoke) in spokes.iter().enumerate() {
        let angle_deg = spoke.angle_ticks as f64 / 8192.0 * 360.0;
        let _ = write!(
            f, "{i},{},{angle_deg:.2},{},{}",
            spoke.angle_ticks, spoke.heading_ticks, spoke.echo.len()
        );
        for &val in &spoke.echo {
            let _ = write!(f, ",{val}");
        }
        for _ in spoke.echo.len()..max_bins {
            let _ = write!(f, ",0");
        }
        let _ = writeln!(f);
    }
}
