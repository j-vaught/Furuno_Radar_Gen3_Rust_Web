use std::collections::BTreeMap;
use std::sync::{Arc, Condvar, Mutex, RwLock};
use std::sync::mpsc::{self, Receiver, SyncSender, TrySendError};
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::net::Ipv4Addr;
use std::path::{Path, PathBuf};
use std::time::{Duration, Instant};
use std::thread;
use std::{env, io};

use axum::{
    Router,
    extract::{Query, State},
    response::IntoResponse,
    routing::get,
    http::{header, StatusCode},
};
use tower_http::services::ServeDir;
use serde::{Deserialize, Serialize};

use crate::connection::{CurrentRadarReadback, RadarConnection, RegisterReadback, try_create_echo_socket};
use crate::protocol::{self, Spoke, decode_echo_packet};
use crate::render::{self, PPI_SIZE};

// --- Shared state ---

/// Tracks the current radar settings for CSV metadata.
#[derive(Clone, Serialize, PartialEq)]
pub struct CurrentSettings {
    pub tx_state: u16,
    pub range_index: u16,
    pub range_label: String,
    pub gain_auto: bool,
    pub gain_value: u16,
    pub gain_manual_value: u16,
    pub gain_auto_value: u16,
    pub pulse_width: u16,
    pub pulse_range_index: u16,
    pub sea_clutter: u16,
    pub sea_clutter_auto: bool,
    pub sea_clutter_auto_value: u16,
    pub sea_clutter_mode: u16,
    pub rain_clutter: u16,
    pub rain_clutter_auto: bool,
    pub rain_clutter_auto_value: u16,
    pub rain_clutter_ftc_mode: u16,
    pub rain_clutter_mode: u16,
    pub rezboost: u16,
    pub interference_rejection: bool,
    pub interference_rejection_level: u16,
    pub noise_reduction: bool,
    pub noise_reduction_level: u16,
    /// Raw N84 field 1 value. The protocol source calls this antenna height;
    /// the application does not infer physical installation units.
    pub antenna_height_raw: u16,
    pub bird_mode: u16,
    pub doppler: u16,
    pub antenna_speed: u16,
    pub antenna_hi_rpm: bool,
    pub main_bang: u16,
    pub near_stc: u16,
    pub middle_stc: u16,
    pub far_stc: u16,
    /// These values are the latest parsed radar readback when status is verified.
    pub status: String,
}

#[derive(Clone, Serialize)]
struct RawRegisterResponse {
    requested: String,
    register: String,
    raw: Option<String>,
    values: Vec<String>,
    error: Option<String>,
    requested_at: String,
    read_at: String,
}

#[derive(Clone, Serialize)]
struct VerifiedField {
    verified: bool,
    value: Option<serde_json::Value>,
    register: String,
    raw: Option<String>,
    requested_at: String,
    read_at: String,
    error: Option<String>,
}

#[derive(Clone, Serialize)]
struct SettingsReadback {
    verified: bool,
    transition_id: u64,
    captured_at: String,
    valid_until: Option<String>,
    source: String,
    firmware: String,
    radar_ip: String,
    host_ip: String,
    fields: BTreeMap<String, VerifiedField>,
    raw_register_responses: Vec<RawRegisterResponse>,
}

impl SettingsReadback {
    fn empty(radar_ip: Ipv4Addr, host_ip: Ipv4Addr) -> Self {
        Self {
            verified: false,
            transition_id: 0,
            captured_at: String::new(),
            valid_until: None,
            source: "none".into(),
            firmware: String::new(),
            radar_ip: radar_ip.to_string(),
            host_ip: host_ip.to_string(),
            fields: BTreeMap::new(),
            raw_register_responses: Vec::new(),
        }
    }
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

#[derive(Clone, Serialize, Default)]
struct RecordingBoundary {
    started_at: String,
    start_transition_id: u64,
    stopped_at: Option<String>,
    stop_transition_id: Option<u64>,
}

#[derive(Clone, Serialize)]
struct ManualRecordingSettingsSnapshot {
    schema_version: &'static str,
    mode: &'static str,
    recording_dir: String,
    current_settings: CurrentSettings,
    settings_readback: SettingsReadback,
    boundary: RecordingBoundary,
    history: Vec<SettingChange>,
}

#[derive(Clone, Serialize)]
struct PatternRecordingSettingsSnapshot {
    schema_version: &'static str,
    mode: &'static str,
    recording_dir: String,
    current_settings: CurrentSettings,
    settings_readback: SettingsReadback,
    boundary: RecordingBoundary,
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
            tx_state: 1,
            range_index: 0, range_label: "1/8 NM".into(),
            gain_auto: false, gain_value: 50, gain_manual_value: 50, gain_auto_value: 0,
            pulse_width: 0, pulse_range_index: 0,
            sea_clutter: 0, sea_clutter_auto: false, sea_clutter_auto_value: 0, sea_clutter_mode: 0,
            rain_clutter: 0, rain_clutter_auto: false, rain_clutter_auto_value: 0,
            rain_clutter_ftc_mode: 0, rain_clutter_mode: 0,
            rezboost: 0, interference_rejection: false,
            interference_rejection_level: 0, noise_reduction: false, noise_reduction_level: 0,
            antenna_height_raw: 0,
            bird_mode: 0, doppler: 0, antenna_speed: 0, antenna_hi_rpm: false,
            main_bang: 0, near_stc: 0, middle_stc: 0, far_stc: 0,
            status: "commanded_cache_unverified".into(),
        }
    }
}

#[derive(Clone)]
struct RecordingRoot {
    path: PathBuf,
    configured: bool,
    empty_configured: bool,
}

struct RotationWriteJob {
    spokes: Vec<Spoke>,
    settings: CurrentSettings,
    readback: SettingsReadback,
    boundary: RecordingBoundary,
    recording_dir: String,
    generation: u64,
    persist: bool,
}

struct RotationWriter {
    sender: SyncSender<RotationWriteJob>,
    pending: Mutex<usize>,
    drained: Condvar,
}

impl RotationWriter {
    fn new(sender: SyncSender<RotationWriteJob>) -> Self {
        Self {
            sender,
            pending: Mutex::new(0),
            drained: Condvar::new(),
        }
    }

    fn try_enqueue(&self, job: RotationWriteJob) -> Result<(), String> {
        let mut pending = self
            .pending
            .lock()
            .map_err(|_| "rotation writer pending lock poisoned".to_string())?;
        *pending += 1;
        drop(pending);

        match self.sender.try_send(job) {
            Ok(()) => Ok(()),
            Err(TrySendError::Full(_)) => {
                self.complete();
                Err("rotation output queue is full; recording stopped before this rotation could be persisted".into())
            }
            Err(TrySendError::Disconnected(_)) => {
                self.complete();
                Err("rotation output writer is unavailable; recording stopped before this rotation could be persisted".into())
            }
        }
    }

    fn complete(&self) {
        if let Ok(mut pending) = self.pending.lock() {
            *pending = pending.saturating_sub(1);
            if *pending == 0 {
                self.drained.notify_all();
            }
        }
    }

    fn wait_for_drain(&self, timeout: Duration) -> bool {
        let pending = match self.pending.lock() {
            Ok(pending) => pending,
            Err(_) => return false,
        };
        let result = self
            .drained
            .wait_timeout_while(pending, timeout, |pending| *pending != 0);
        match result {
            Ok((pending, _)) => *pending == 0,
            Err(_) => false,
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
    recording_root: RecordingRoot,
    host_ip: Ipv4Addr,
    settings: RwLock<CurrentSettings>,
    settings_readback: RwLock<SettingsReadback>,
    recording_snapshot_lock: Mutex<()>,
    readback_generation: AtomicU64,
    readback_last_refresh: Mutex<Instant>,
    recording_boundary: RwLock<RecordingBoundary>,
    rotation_writer: Arc<RotationWriter>,
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

    let recording_root = resolve_recording_root();
    println!(
        "Recording root{}: {}",
        if recording_root.configured { " (FURUNO_RECORDING_ROOT)" } else { "" },
        recording_root.path.display()
    );

    // A decoded rotation is several megabytes. Sixteen queued rotations cover
    // the observed ~25 s native output stall at 24 rpm while keeping memory
    // bounded to the expected few tens of megabytes. Saturation remains a
    // recorded failure that stops recording rather than silently dropping data.
    let (rotation_sender, rotation_receiver) = mpsc::sync_channel(ROTATION_WRITER_CAPACITY);
    let rotation_writer = Arc::new(RotationWriter::new(rotation_sender));

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
        recording_root,
        host_ip,
        settings: RwLock::new(CurrentSettings::default()),
        settings_readback: RwLock::new(SettingsReadback::empty(radar_ip, host_ip)),
        recording_snapshot_lock: Mutex::new(()),
        readback_generation: AtomicU64::new(0),
        readback_last_refresh: Mutex::new(Instant::now()),
        recording_boundary: RwLock::new(RecordingBoundary::default()),
        rotation_writer: rotation_writer.clone(),
        start_time: Instant::now(),
    });

    if connected {
        if let Err(error) = refresh_settings_from_radar(&state, "startup") {
            eprintln!("Startup settings readback incomplete: {error}");
        }
    }

    let readback_state = state.clone();
    thread::spawn(move || settings_readback_worker(readback_state));

    let writer_state = state.clone();
    let writer_handle = rotation_writer.clone();
    thread::spawn(move || rotation_writer_loop(writer_state, rotation_receiver, writer_handle));

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
        .route("/set_sweep_total", get(handle_unavailable))
        .route("/set_interpolation", get(handle_unavailable))
        // Settings (new)
        .route("/set_main_bang", get(handle_set_main_bang))
        .route("/set_interference_rejection", get(handle_set_ir))
        .route("/set_noise_reduction", get(handle_set_noise_reduction))
        .route("/set_antenna_height", get(handle_set_antenna_height))
        .route("/set_bird_mode", get(handle_set_bird_mode))
        .route("/set_doppler", get(handle_set_doppler))
        // Recording
        .route("/toggle_recording", get(handle_toggle_recording))
        // Echo image
        .route("/newest_png", get(handle_newest_png))
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

const SETTINGS_READBACK_TTL: Duration = Duration::from_secs(5);
const SETTINGS_READBACK_REFRESH_INTERVAL: Duration = Duration::from_secs(2);
const ROTATION_WRITER_CAPACITY: usize = 16;

fn readback_query_age_exceeded(readback: &SettingsReadback) -> bool {
    let now = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis();
    readback.raw_register_responses.iter().any(|response| {
        response
            .read_at
            .strip_prefix("unix_ms:")
            .and_then(|value| value.parse::<u128>().ok())
            .map(|captured| now.saturating_sub(captured) >= SETTINGS_READBACK_REFRESH_INTERVAL.as_millis())
            .unwrap_or(true)
    })
}

fn settings_readback_worker(state: Arc<AppState>) {
    loop {
        thread::sleep(Duration::from_secs(1));
        if !state.connected.load(Ordering::Relaxed) {
            continue;
        }
        let stale = state
            .readback_last_refresh
            .lock()
            .map(|last| last.elapsed() >= SETTINGS_READBACK_REFRESH_INTERVAL)
            .unwrap_or(true);
        let query_stale = state
            .settings_readback
            .read()
            .map(|readback| readback_query_age_exceeded(&readback))
            .unwrap_or(true);
        if !stale && !query_stale {
            continue;
        }
        if let Err(error) = refresh_settings_from_radar(&state, "periodic") {
            if state.recording.load(Ordering::Relaxed) {
                disable_recording_on_error(&state, "periodic settings readback", &error);
            }
        }
    }
}

fn rotation_writer_loop(
    state: Arc<AppState>,
    receiver: Receiver<RotationWriteJob>,
    writer: Arc<RotationWriter>,
) {
    while let Ok(job) = receiver.recv() {
        let generation = job.generation;
        if let Err(error) = write_recording_rotation_job(&state, job) {
            record_setting_change(
                &state,
                "rotation_output_failed",
                serde_json::json!({
                    "generation": generation,
                    "error": error.to_string(),
                }),
            );
            disable_recording_on_error(&state, "rotation output", &error);
        }
        writer.complete();
    }
}

fn readback_valid_now(readback: &SettingsReadback) -> bool {
    let now = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis();
    readback.verified
        && readback
            .valid_until
            .as_deref()
            .and_then(|value| value.strip_prefix("unix_ms:"))
            .and_then(|value| value.parse::<u128>().ok())
            .is_some_and(|valid_until| now <= valid_until)
}

fn rotation_capture_error(readback: &SettingsReadback, generation: u64, reason: &str) -> String {
    let failures = readback
        .fields
        .iter()
        .filter(|(_, field)| !field.verified)
        .map(|(name, field)| format!("{name}: {}", field.error.as_deref().unwrap_or("unverified")))
        .collect::<Vec<_>>();
    format!(
        "rotation capture rejected: {reason}; expected_generation={generation}; readback_generation={}; verified={}; captured_at={}; valid_until={}; source={}; failures=[{}]",
        readback.transition_id,
        readback.verified,
        readback.captured_at,
        readback.valid_until.as_deref().unwrap_or("none"),
        readback.source,
        failures.join(" | "),
    )
}

fn capture_rotation_write_job(
    state: &AppState,
    spokes: Vec<Spoke>,
    generation: u64,
    persist: bool,
) -> Result<RotationWriteJob, String> {
    let _snapshot_guard = state
        .recording_snapshot_lock
        .lock()
        .map_err(|_| "recording snapshot lock poisoned".to_string())?;
    let settings = state
        .settings
        .read()
        .map_err(|_| "settings lock poisoned".to_string())?
        .clone();
    let readback = state
        .settings_readback
        .read()
        .map_err(|_| "settings readback lock poisoned".to_string())?
        .clone();
    if persist && !readback.verified {
        return Err(rotation_capture_error(&readback, generation, "settings readback is unverified"));
    }
    if persist && readback.transition_id != generation {
        return Err(rotation_capture_error(&readback, generation, "settings generation changed"));
    }
    if persist && !readback_valid_now(&readback) {
        return Err(rotation_capture_error(&readback, generation, "settings readback validity interval expired"));
    }
    let recording_dir = state
        .record_dir
        .read()
        .map_err(|_| "recording directory lock poisoned".to_string())?
        .clone();
    if persist && recording_dir.is_empty() {
        return Err("rotation capture rejected: recording directory is empty".into());
    }
    let boundary = state
        .recording_boundary
        .read()
        .map_err(|_| "recording boundary lock poisoned".to_string())?
        .clone();
    Ok(RotationWriteJob {
        spokes,
        settings,
        readback,
        boundary,
        recording_dir,
        generation,
        persist,
    })
}

// --- Echo listener thread ---

fn echo_listener(state: Arc<AppState>) {
    let echo_socket = match try_create_echo_socket(state.host_ip) {
        Ok(socket) => socket,
        Err(error) => {
            eprintln!("Echo socket setup failed: {error}");
            state.connected.store(false, Ordering::Relaxed);
            state.tx_active.store(false, Ordering::Relaxed);
            state.recording.store(false, Ordering::Relaxed);
            return;
        }
    };
    let mut buf = [0u8; 2048];
    let mut spokes: Vec<Spoke> = Vec::new();
    let mut prev_spoke_data: Vec<u8> = Vec::new();
    let mut prev_az: Option<u16> = None;
    let mut rotation_generation: Option<u64> = None;
    let mut last_keepalive = Instant::now();
    let mut recording_was_active = false;
    let mut discard_recording_boundary_rotation = false;

    loop {
        let recording_active = state.recording.load(Ordering::Relaxed);
        if recording_active && !recording_was_active {
            // A recording can begin while a rotation is already buffered.
            // That buffer contains pre-recording spokes and must not become
            // the first persisted sample for the new session.
            discard_recording_boundary_rotation = !spokes.is_empty();
        } else if !recording_active {
            discard_recording_boundary_rotation = false;
        }
        recording_was_active = recording_active;

        // Send keepalive every 5s regardless of TX state
        if last_keepalive.elapsed() > Duration::from_secs(5) {
            if let Ok(mut conn) = state.conn.try_lock() {
                let keepalive_result = conn.send_keepalive();
                conn.drain_control();
                if keepalive_result.is_ok() {
                    // Do not postpone a retry when the connection was busy or
                    // the write failed. The next loop can try again without
                    // blocking UDP reception.
                    last_keepalive = Instant::now();
                }
            }
        }

        if !state.tx_active.load(Ordering::Relaxed) {
            // Drain stale multicast packets while in standby. Leaving them
            // queued lets the first post-TX packet reseed a partial rotation.
            if echo_socket.recv_from(&mut buf).is_ok() {
                spokes.clear();
                prev_spoke_data.clear();
                prev_az = None;
                rotation_generation = None;
            }
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
                            let current_generation = state.readback_generation.load(Ordering::Relaxed);
                            let complete_generation = rotation_generation.unwrap_or(current_generation);
                            let crosses_recording_boundary = discard_recording_boundary_rotation;
                            if crosses_recording_boundary {
                                eprintln!("Discarding rotation spanning recording start boundary");
                                discard_recording_boundary_rotation = false;
                            }
                            if complete_generation == current_generation {
                                let persist = !crosses_recording_boundary
                                    && state.recording.load(Ordering::Relaxed);
                                let spokes_for_writer = std::mem::take(&mut spokes);
                                match capture_rotation_write_job(
                                    &state,
                                    spokes_for_writer,
                                    complete_generation,
                                    persist,
                                ) {
                                    Ok(job) => {
                                        let spoke_count = job.spokes.len();
                                        if let Err(error) = state.rotation_writer.try_enqueue(job) {
                                            record_setting_change(
                                                &state,
                                                "rotation_output_queue_failed",
                                                serde_json::json!({
                                                    "generation": complete_generation,
                                                    "spokes": spoke_count,
                                                    "error": error,
                                                }),
                                            );
                                            if persist {
                                                disable_recording_on_error(&state, "rotation output queue", &error);
                                            } else {
                                                eprintln!("Rotation render queue failed: {error}");
                                            }
                                        }
                                    }
                                    Err(error) => {
                                        record_setting_change(
                                            &state,
                                            "rotation_capture_rejected",
                                            serde_json::json!({
                                                "generation": complete_generation,
                                                "error": error,
                                            }),
                                        );
                                        if persist {
                                            disable_recording_on_error(&state, "rotation capture", &error);
                                        } else {
                                            eprintln!("Rotation capture rejected: {error}");
                                        }
                                    }
                                }
                            } else if state.recording.load(Ordering::Relaxed) {
                                eprintln!("Discarding rotation spanning settings transition {complete_generation}->{current_generation}");
                            }
                        }
                        state.rotation_count.fetch_add(1, Ordering::Relaxed);
                        spokes.clear();
                        rotation_generation = None;
                    }
                }
                prev_az = Some(az);

                if spokes.is_empty() {
                    rotation_generation = Some(state.readback_generation.load(Ordering::Relaxed));
                }

                decode_echo_packet(&buf[..len], &mut prev_spoke_data, &mut spokes);
            }
            Err(_) => {}
        }
    }
}

fn read_for<'a>(reads: &'a [RegisterReadback], register: &str) -> Option<&'a RegisterReadback> {
    reads.iter().find(|read| read.register == register)
}

fn read_for_selector<'a>(
    reads: &'a [RegisterReadback],
    register: &str,
    selector: u16,
) -> Option<&'a RegisterReadback> {
    reads.iter().find(|read| {
        read.register == register
            && read.values.get(1).and_then(|value| value.parse::<u16>().ok()) == Some(selector)
    })
}

fn value_u16(read: Option<&RegisterReadback>, index: usize) -> Option<u16> {
    read.and_then(|item| item.values.get(index))
        .and_then(|value| value.trim().parse::<u16>().ok())
}

fn verified_field(
    read: Option<&RegisterReadback>,
    read_at: &str,
    value: Option<serde_json::Value>,
) -> VerifiedField {
    let (register, raw, error, actual_requested_at, actual_read_at) = match read {
        Some(item) => (
            item.register.clone(),
            item.raw.clone(),
            item.error.clone(),
            item.requested_at.clone(),
            item.read_at.clone(),
        ),
        None => (
            String::new(),
            None,
            Some("register was not requested".into()),
            read_at.to_string(),
            read_at.to_string(),
        ),
    };
    let verified = raw.is_some() && error.is_none() && value.is_some();
    VerifiedField {
        verified,
        value: if verified { value } else { None },
        register,
        raw,
        requested_at: actual_requested_at,
        read_at: actual_read_at,
        error: if verified { None } else { error.or_else(|| Some("response did not contain a valid value".into())) },
    }
}

fn build_settings_readback(
    snapshot: &CurrentRadarReadback,
    firmware: &str,
    radar_ip: Ipv4Addr,
    host_ip: Ipv4Addr,
    source: &str,
) -> (CurrentSettings, SettingsReadback) {
    let captured_at = timestamp_precise();
    let reads = &snapshot.reads;
    let r62 = read_for(reads, "$N62");
    let r63 = read_for(reads, "$N63");
    let r64 = read_for(reads, "$N64");
    let r65 = read_for(reads, "$N65");
    let r68 = read_for(reads, "$N68");
    let r69 = read_for(reads, "$N69");
    let r83 = read_for(reads, "$N83");
    let r85 = read_for(reads, "$N85");
    let r86 = read_for(reads, "$N86");
    let r87 = read_for(reads, "$N87");
    let r89 = read_for(reads, "$N89");
    let red = read_for(reads, "$NED");
    let ree = read_for(reads, "$NEE");
    let ref_ = read_for(reads, "$NEF");
    let r67_ir = read_for_selector(reads, "$N67", 0);
    let r67_noise = read_for_selector(reads, "$N67", 2);
    let r84 = read_for(reads, "$N84");

    let range = value_u16(r62, 0);
    let gain_auto = value_u16(r63, 0).map(|value| value != 0);
    let gain_manual_value = value_u16(r63, 1);
    let gain_auto_value = value_u16(r63, 3);
    let gain_value = match gain_auto {
        Some(true) => gain_auto_value,
        Some(false) => gain_manual_value,
        None => None,
    };
    let sea = value_u16(r64, 1);
    let sea_auto = value_u16(r64, 0).map(|value| value != 0);
    let sea_auto_value = value_u16(r64, 2);
    let sea_mode = value_u16(r64, 3);
    let rain = value_u16(r65, 1);
    let rain_auto = value_u16(r65, 0).map(|value| value != 0);
    let rain_auto_value = value_u16(r65, 2);
    let rain_ftc_mode = value_u16(r65, 3);
    let rain_mode = value_u16(r65, 4);
    let pulse_range_index = value_u16(r68, 2);
    let pulse = match (value_u16(r68, 0), pulse_range_index, range) {
        (Some(pulse), Some(observed_range), Some(expected_range)) if observed_range == expected_range => Some(pulse),
        _ => None,
    };
    let tx_state = value_u16(r69, 0);
    let main_bang = value_u16(r83, 0);
    let near_stc = value_u16(r85, 0);
    let middle_stc = value_u16(r86, 0);
    let far_stc = value_u16(r87, 0);
    let antenna_speed = value_u16(r89, 0);
    let antenna_hi_rpm = value_u16(r89, 1).map(|value| value != 0);
    let bird_mode = value_u16(red, 0);
    let rezboost = value_u16(ree, 0);
    let doppler = match (value_u16(ref_, 0), value_u16(ref_, 1)) {
        (Some(0), _) => Some(0),
        (Some(1), Some(0)) => Some(1),
        (Some(1), Some(1)) => Some(2),
        _ => None,
    };
    let ir_level = value_u16(r67_ir, 2);
    let interference_rejection = ir_level.map(|value| value != 0);
    let noise_reduction_level = value_u16(r67_noise, 2);
    let noise_reduction = noise_reduction_level.map(|value| value != 0);
    let antenna_height_raw = value_u16(r84, 1);

    let mut fields = BTreeMap::new();
    fields.insert("tx_state".into(), verified_field(r69, &captured_at, tx_state.map(|v| serde_json::json!(v))));
    fields.insert("range_index".into(), verified_field(r62, &captured_at, range.map(|v| serde_json::json!(v))));
    fields.insert("gain_auto".into(), verified_field(r63, &captured_at, gain_auto.map(|v| serde_json::json!(v))));
    fields.insert("gain_value".into(), verified_field(r63, &captured_at, gain_value.map(|v| serde_json::json!(v))));
    fields.insert("gain_manual_value".into(), verified_field(r63, &captured_at, gain_manual_value.map(|v| serde_json::json!(v))));
    fields.insert("gain_auto_value".into(), verified_field(r63, &captured_at, gain_auto_value.map(|v| serde_json::json!(v))));
    fields.insert("sea_clutter".into(), verified_field(r64, &captured_at, sea.map(|v| serde_json::json!(v))));
    fields.insert("sea_clutter_auto".into(), verified_field(r64, &captured_at, sea_auto.map(|v| serde_json::json!(v))));
    fields.insert("sea_clutter_auto_value".into(), verified_field(r64, &captured_at, sea_auto_value.map(|v| serde_json::json!(v))));
    fields.insert("sea_clutter_mode".into(), verified_field(r64, &captured_at, sea_mode.map(|v| serde_json::json!(v))));
    fields.insert("rain_clutter".into(), verified_field(r65, &captured_at, rain.map(|v| serde_json::json!(v))));
    fields.insert("rain_clutter_auto".into(), verified_field(r65, &captured_at, rain_auto.map(|v| serde_json::json!(v))));
    fields.insert("rain_clutter_auto_value".into(), verified_field(r65, &captured_at, rain_auto_value.map(|v| serde_json::json!(v))));
    fields.insert("rain_clutter_ftc_mode".into(), verified_field(r65, &captured_at, rain_ftc_mode.map(|v| serde_json::json!(v))));
    fields.insert("rain_clutter_mode".into(), verified_field(r65, &captured_at, rain_mode.map(|v| serde_json::json!(v))));
    fields.insert("pulse_width".into(), verified_field(r68, &captured_at, pulse.map(|v| serde_json::json!(v))));
    fields.insert("pulse_range_index".into(), verified_field(r68, &captured_at, pulse_range_index.map(|v| serde_json::json!(v))));
    fields.insert("main_bang".into(), verified_field(r83, &captured_at, main_bang.map(|v| serde_json::json!(v))));
    fields.insert("near_stc".into(), verified_field(r85, &captured_at, near_stc.map(|v| serde_json::json!(v))));
    fields.insert("middle_stc".into(), verified_field(r86, &captured_at, middle_stc.map(|v| serde_json::json!(v))));
    fields.insert("far_stc".into(), verified_field(r87, &captured_at, far_stc.map(|v| serde_json::json!(v))));
    fields.insert("antenna_speed".into(), verified_field(r89, &captured_at, antenna_speed.map(|v| serde_json::json!(v))));
    fields.insert("antenna_hi_rpm".into(), verified_field(r89, &captured_at, antenna_hi_rpm.map(|v| serde_json::json!(v))));
    fields.insert("bird_mode".into(), verified_field(red, &captured_at, bird_mode.map(|v| serde_json::json!(v))));
    fields.insert("rezboost".into(), verified_field(ree, &captured_at, rezboost.map(|v| serde_json::json!(v))));
    fields.insert("doppler".into(), verified_field(ref_, &captured_at, doppler.map(|v| serde_json::json!(v))));
    fields.insert("interference_rejection_level".into(), verified_field(r67_ir, &captured_at, ir_level.map(|v| serde_json::json!(v))));
    fields.insert("interference_rejection".into(), verified_field(r67_ir, &captured_at, interference_rejection.map(|v| serde_json::json!(v))));
    fields.insert("noise_reduction_level".into(), verified_field(r67_noise, &captured_at, noise_reduction_level.map(|v| serde_json::json!(v))));
    fields.insert("noise_reduction".into(), verified_field(r67_noise, &captured_at, noise_reduction.map(|v| serde_json::json!(v))));
    fields.insert("antenna_height_raw".into(), verified_field(r84, &captured_at, antenna_height_raw.map(|v| serde_json::json!(v))));

    let raw_register_responses = reads.iter().map(|read| RawRegisterResponse {
        requested: read.requested.clone(),
        register: read.register.clone(),
        raw: read.raw.clone(),
        values: read.values.clone(),
        error: read.error.clone(),
        requested_at: read.requested_at.clone(),
        read_at: read.read_at.clone(),
    }).collect::<Vec<_>>();
    let verified = fields.values().all(|field| field.verified);
    let settings = CurrentSettings {
        tx_state: tx_state.unwrap_or_default(),
        range_index: range.unwrap_or_default(),
        range_label: range.map(|value| protocol::wire_index_to_label(value).to_string()).unwrap_or_else(|| "unknown".into()),
        gain_auto: gain_auto.unwrap_or_default(),
        gain_value: gain_value.unwrap_or_default(),
        gain_manual_value: gain_manual_value.unwrap_or_default(),
        gain_auto_value: gain_auto_value.unwrap_or_default(),
        pulse_width: pulse.unwrap_or_default(),
        pulse_range_index: pulse_range_index.unwrap_or_default(),
        sea_clutter: sea.unwrap_or_default(),
        sea_clutter_auto: sea_auto.unwrap_or_default(),
        sea_clutter_auto_value: sea_auto_value.unwrap_or_default(),
        sea_clutter_mode: sea_mode.unwrap_or_default(),
        rain_clutter: rain.unwrap_or_default(),
        rain_clutter_auto: rain_auto.unwrap_or_default(),
        rain_clutter_auto_value: rain_auto_value.unwrap_or_default(),
        rain_clutter_ftc_mode: rain_ftc_mode.unwrap_or_default(),
        rain_clutter_mode: rain_mode.unwrap_or_default(),
        rezboost: rezboost.unwrap_or_default(),
        interference_rejection: interference_rejection.unwrap_or_default(),
        interference_rejection_level: ir_level.unwrap_or_default(),
        noise_reduction: noise_reduction.unwrap_or_default(),
        noise_reduction_level: noise_reduction_level.unwrap_or_default(),
        antenna_height_raw: antenna_height_raw.unwrap_or_default(),
        bird_mode: bird_mode.unwrap_or_default(),
        doppler: doppler.unwrap_or_default(),
        antenna_speed: antenna_speed.unwrap_or_default(),
        antenna_hi_rpm: antenna_hi_rpm.unwrap_or_default(),
        main_bang: main_bang.unwrap_or_default(),
        near_stc: near_stc.unwrap_or_default(),
        middle_stc: middle_stc.unwrap_or_default(),
        far_stc: far_stc.unwrap_or_default(),
        status: if verified { "verified_readback" } else { "readback_incomplete" }.into(),
    };
    let readback = SettingsReadback {
        verified,
        transition_id: 0,
        captured_at,
        valid_until: Some(timestamp_precise_after(SETTINGS_READBACK_TTL)),
        source: source.into(),
        firmware: firmware.into(),
        radar_ip: radar_ip.to_string(),
        host_ip: host_ip.to_string(),
        fields,
        raw_register_responses,
    };
    (settings, readback)
}

fn raw_readback_signature(readback: &SettingsReadback) -> Vec<(String, Vec<String>, Option<String>, Option<String>)> {
    readback
        .raw_register_responses
        .iter()
        .map(|response| (
            response.register.clone(),
            response.values.clone(),
            response.raw.clone(),
            response.error.clone(),
        ))
        .collect()
}

fn readback_capture_millis(readback: &SettingsReadback) -> Option<u128> {
    readback
        .captured_at
        .strip_prefix("unix_ms:")
        .and_then(|value| value.parse::<u128>().ok())
}

fn readback_is_newer_or_equal(candidate: &SettingsReadback, current: &SettingsReadback) -> bool {
    match (readback_capture_millis(candidate), readback_capture_millis(current)) {
        (Some(candidate), Some(current)) => candidate >= current,
        (Some(_), None) => true,
        (None, Some(_)) => false,
        (None, None) => true,
    }
}

fn invalidate_settings_readback(state: &AppState, source: &str, error: &str) {
    let transition_id = state.readback_generation.fetch_add(1, Ordering::Relaxed) + 1;
    if let Ok(mut readback) = state.settings_readback.write() {
        readback.verified = false;
        readback.transition_id = transition_id;
        let now = timestamp_precise();
        readback.valid_until = Some(now.clone());
        readback.captured_at = now;
        readback.source = source.into();
        readback.fields.values_mut().for_each(|field| {
            field.verified = false;
            field.error = Some(error.into());
        });
    }
    if let Ok(mut settings) = state.settings.write() {
        settings.status = "readback_incomplete".into();
    }
    if let Ok(mut refreshed) = state.readback_last_refresh.lock() {
        *refreshed = Instant::now();
    }
}

fn refresh_settings_from_radar(state: &AppState, source: &str) -> Result<(), String> {
    // Keep the connection guard through publication. Without this, two
    // refreshes can read in order A/B but publish in order B/A, restoring an
    // older settings snapshot after a newer setter readback completed.
    let mut conn = state.conn.lock().map_err(|_| "connection lock poisoned".to_string())?;
    let snapshot = conn.read_current_settings();
    let firmware = conn.firmware.clone().unwrap_or_default();
    let radar_ip = conn.radar_ip;
    let host_ip = conn.host_ip;
    let (settings, readback) = build_settings_readback(&snapshot, &firmware, radar_ip, host_ip, source);
    let mut readback = readback;
    let current_readback = state.settings_readback
        .read()
        .map(|current| current.clone())
        .unwrap_or_else(|_| SettingsReadback::empty(radar_ip, host_ip));
    if !readback_is_newer_or_equal(&readback, &current_readback) {
        let failures = current_readback.fields.iter()
            .filter(|(_, field)| !field.verified)
            .map(|(name, field)| format!("{name}: {}", field.error.as_deref().unwrap_or("unverified")))
            .collect::<Vec<_>>();
        return if current_readback.verified {
            Ok(())
        } else {
            Err(failures.join("; "))
        };
    }
    let raw_signature = raw_readback_signature(&readback);
    let settings_changed = state.settings.read()
        .map(|previous| *previous != settings)
        .unwrap_or(true);
    let raw_readback_changed = state.settings_readback
        .read()
        .map(|previous| raw_readback_signature(&previous) != raw_signature)
        .unwrap_or(true);
    let transition_id = if settings_changed || raw_readback_changed || !readback.verified {
        state.readback_generation.fetch_add(1, Ordering::Relaxed) + 1
    } else {
        state.readback_generation.load(Ordering::Relaxed)
    };
    readback.transition_id = transition_id;
    let verified = readback.verified;
    let failures = readback.fields.iter()
        .filter(|(_, field)| !field.verified)
        .map(|(name, field)| format!("{name}: {}", field.error.as_deref().unwrap_or("unverified")))
        .collect::<Vec<_>>();
    let _snapshot_guard = state
        .recording_snapshot_lock
        .lock()
        .map_err(|_| "recording snapshot lock poisoned".to_string())?;
    *state.settings.write().map_err(|_| "settings lock poisoned".to_string())? = settings;
    *state.settings_readback.write().map_err(|_| "readback lock poisoned".to_string())? = readback;
    if let Ok(mut refreshed) = state.readback_last_refresh.lock() {
        *refreshed = Instant::now();
    }
    record_setting_change(
        state,
        "radar_readback",
        serde_json::json!({
            "transition_id": state.readback_generation.load(Ordering::Relaxed),
            "verified": verified,
            "source": source,
        }),
    );
    if verified { Ok(()) } else { Err(failures.join("; ")) }
}

fn parse_setting_command(command: &str) -> Result<(String, Vec<u16>), String> {
    let line = command.trim();
    let body = line
        .strip_prefix('$')
        .ok_or_else(|| format!("command is missing '$' prefix: {line}"))?;
    let mut parts = body.split(',');
    let register = parts
        .next()
        .ok_or_else(|| format!("command has no register: {line}"))?;
    let register = register
        .strip_prefix('S')
        .ok_or_else(|| format!("command is not a setting write: {line}"))?
        .to_ascii_uppercase();
    let values = parts
        .map(|value| {
            value
                .parse::<u16>()
                .map_err(|_| format!("setting {register} contains non-numeric value '{value}'"))
        })
        .collect::<Result<Vec<_>, _>>()?;
    Ok((register, values))
}

fn readback_u16(readback: &SettingsReadback, field_name: &str) -> Result<u16, String> {
    let field = readback
        .fields
        .get(field_name)
        .ok_or_else(|| format!("fresh readback is missing field {field_name}"))?;
    if !field.verified {
        return Err(format!(
            "fresh readback field {field_name} is unverified: {}",
            field.error.as_deref().unwrap_or("no value")
        ));
    }
    field
        .value
        .as_ref()
        .and_then(serde_json::Value::as_u64)
        .and_then(|value| u16::try_from(value).ok())
        .ok_or_else(|| format!("fresh readback field {field_name} is not a numeric value"))
}

fn readback_bool(readback: &SettingsReadback, field_name: &str) -> Result<bool, String> {
    let field = readback
        .fields
        .get(field_name)
        .ok_or_else(|| format!("fresh readback is missing field {field_name}"))?;
    if !field.verified {
        return Err(format!(
            "fresh readback field {field_name} is unverified: {}",
            field.error.as_deref().unwrap_or("no value")
        ));
    }
    field
        .value
        .as_ref()
        .and_then(serde_json::Value::as_bool)
        .ok_or_else(|| format!("fresh readback field {field_name} is not a boolean value"))
}

fn expect_readback_u16(
    readback: &SettingsReadback,
    field_name: &str,
    requested: u16,
) -> Result<(), String> {
    let observed = readback_u16(readback, field_name)?;
    if observed == requested {
        Ok(())
    } else {
        Err(format!(
            "{field_name}: requested {requested}, observed {observed}"
        ))
    }
}

fn expect_readback_bool(
    readback: &SettingsReadback,
    field_name: &str,
    requested: bool,
) -> Result<(), String> {
    let observed = readback_bool(readback, field_name)?;
    if observed == requested {
        Ok(())
    } else {
        Err(format!(
            "{field_name}: requested {requested}, observed {observed}"
        ))
    }
}

/// Verify that a command's requested mutable values were actually applied.
///
/// The immediate N-register response is only an acknowledgement of the wire
/// transaction. This second check uses the fresh, selector-aware settings
/// snapshot so a radar that echoes an unapplied command cannot be recorded as
/// having changed state.
fn validate_command_against_readback(
    readback: &SettingsReadback,
    command: &str,
) -> Result<(), String> {
    if !readback.verified {
        return Err("fresh settings readback is not verified".into());
    }
    let (register, values) = parse_setting_command(command)?;
    let value = |index: usize| {
        values.get(index).copied().ok_or_else(|| {
            format!("setting {register} is missing mutable field {index}")
        })
    };
    match register.as_str() {
        "62" => expect_readback_u16(readback, "range_index", value(0)?)?,
        "63" => {
            let auto = value(0)? != 0;
            expect_readback_bool(readback, "gain_auto", auto)?;
            if auto {
                expect_readback_u16(readback, "gain_auto_value", value(3)?)?;
            } else {
                expect_readback_u16(readback, "gain_manual_value", value(1)?)?;
            }
        }
        "64" => {
            let auto = value(0)? != 0;
            expect_readback_bool(readback, "sea_clutter_auto", auto)?;
            expect_readback_u16(
                readback,
                if auto { "sea_clutter_auto_value" } else { "sea_clutter" },
                value(if auto { 2 } else { 1 })?,
            )?;
            expect_readback_u16(readback, "sea_clutter_mode", value(3)?)?;
        }
        "65" => {
            let auto = value(0)? != 0;
            expect_readback_bool(readback, "rain_clutter_auto", auto)?;
            expect_readback_u16(
                readback,
                if auto { "rain_clutter_auto_value" } else { "rain_clutter" },
                value(if auto { 2 } else { 1 })?,
            )?;
            expect_readback_u16(readback, "rain_clutter_ftc_mode", value(3)?)?;
            expect_readback_u16(readback, "rain_clutter_mode", value(4)?)?;
        }
        "67" => {
            let selector = value(1)?;
            match selector {
                0 => expect_readback_u16(readback, "interference_rejection_level", value(2)?)?,
                2 => expect_readback_u16(readback, "noise_reduction_level", value(2)?)?,
                _ => return Err(format!("unsupported S67 selector {selector}")),
            }
        }
        "68" => {
            expect_readback_u16(readback, "pulse_width", value(0)?)?;
            expect_readback_u16(readback, "pulse_range_index", value(2)?)?;
        }
        "69" => expect_readback_u16(readback, "tx_state", value(0)?)?,
        "83" => expect_readback_u16(readback, "main_bang", value(0)?)?,
        "84" => expect_readback_u16(readback, "antenna_height_raw", value(1)?)?,
        "85" => expect_readback_u16(readback, "near_stc", value(0)?)?,
        "86" => expect_readback_u16(readback, "middle_stc", value(0)?)?,
        "87" => expect_readback_u16(readback, "far_stc", value(0)?)?,
        "89" => {
            expect_readback_u16(readback, "antenna_speed", value(0)?)?;
            expect_readback_bool(readback, "antenna_hi_rpm", value(1)? != 0)?;
        }
        "ED" => expect_readback_u16(readback, "bird_mode", value(0)?)?,
        "EE" => expect_readback_u16(readback, "rezboost", value(0)?)?,
        "EF" => {
            let enabled = value(0)?;
            let wire_mode = value(1)?;
            let desired = match (enabled, wire_mode) {
                (0, _) => 0,
                (1, 0) => 1,
                (1, 1) => 2,
                _ => return Err(format!("unsupported SEF mode {enabled},{wire_mode}")),
            };
            expect_readback_u16(readback, "doppler", desired)?;
        }
        _ => return Err(format!("no post-readback validator for setting S{register}")),
    }
    Ok(())
}

fn validate_command_applied(state: &AppState, command: &str) -> Result<(), String> {
    let readback = state
        .settings_readback
        .read()
        .map_err(|_| "settings readback lock poisoned".to_string())?;
    validate_command_against_readback(&readback, command)
}

fn validate_commands_applied(state: &AppState, commands: &[String]) -> Result<(), String> {
    for command in commands {
        validate_command_applied(state, command)
            .map_err(|error| format!("{command}: {error}"))?;
    }
    Ok(())
}

fn observed_tx_active(state: &AppState) -> Option<bool> {
    let readback = state.settings_readback.read().ok()?;
    if !readback.verified {
        return None;
    }
    readback
        .fields
        .get("tx_state")
        .filter(|field| field.verified)
        .and_then(|field| field.value.as_ref())
        .and_then(serde_json::Value::as_u64)
        .map(|value| value == 2)
}

// --- Helper: send a command and return ack text ---

async fn send_cmd(
    state: Arc<AppState>,
    cmd: String,
    ack_prefix: &'static str,
    success_msg: String,
) -> String {
    tokio::task::spawn_blocking(move || {
        let result = {
            let mut conn = state.conn.lock().unwrap();
            conn.send_and_read_ack(&cmd, ack_prefix, Duration::from_secs(3))
        };
        match result {
            Ok((true, line)) => match crate::connection::validate_setting_ack(&cmd, &line) {
                Err(error) => {
                    invalidate_settings_readback(&state, "command_ack_mismatch", &error.to_string());
                    disable_recording_on_error(&state, "command acknowledgement mismatch", &error);
                    format!("{success_msg} | invalid acknowledgement: {error}")
                }
                Ok(()) => match refresh_settings_from_radar(&state, "post_command") {
                Ok(()) => {
                    if let Err(error) = validate_command_applied(&state, &cmd) {
                        record_setting_change(
                            &state,
                            "command_not_applied",
                            serde_json::json!({
                                "command": cmd,
                                "ack": line,
                                "error": error,
                            }),
                        );
                        disable_recording_on_error(&state, "command application", &error);
                        return format!(
                            "{success_msg} | {line} | command acknowledged but not applied: {error}"
                        );
                    }
                    if state.recording.load(Ordering::Relaxed) {
                        if let Err(error) = persist_recording_settings(&state) {
                            disable_recording_on_error(&state, "post-command settings output", &error);
                            return format!("{success_msg} | {line} | recording disabled: {error}");
                        }
                    }
                    format!("{success_msg} | {line} | readback verified")
                }
                Err(error) => {
                    invalidate_settings_readback(&state, "post_command_failed", &error);
                    disable_recording_on_error(&state, "post-command readback", &error);
                    format!("{success_msg} | {line} | readback failed: {error}")
                }
                },
            },
            Ok((false, _)) => {
                invalidate_settings_readback(&state, "command_unacknowledged", "command was not acknowledged");
                disable_recording_on_error(&state, "unacknowledged command", "command was not acknowledged");
                format!("{success_msg} (no ack; state not persisted)")
            }
            Err(e) => {
                invalidate_settings_readback(&state, "command_error", &e.to_string());
                disable_recording_on_error(&state, "command error", &e);
                format!("Error: {e}")
            }
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
    /// `fresh=1` forces the aggregate register read before returning ALL.
    fresh: Option<u8>,
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
        match params.mode.as_str() {
            "on" => {
                // Ensure we're in the right state for tx_on
                let result = st.conn.lock().map_err(|_| "connection lock poisoned".to_string())
                    .and_then(|mut conn| conn.tx_on().map_err(|e| e.to_string()));
                match result {
                    Ok(()) => {
                        match refresh_settings_from_radar(&st, "tx_on") {
                            Ok(()) => {
                                match validate_command_applied(&st, &protocol::cmd_tx_state(2)) {
                                    Ok(()) => {
                                        st.tx_active.store(true, Ordering::Relaxed);
                                        "TX ON: Radar is transmitting; settings readback verified".to_string()
                                    }
                                    Err(error) => {
                                        st.tx_active.store(
                                            observed_tx_active(&st).unwrap_or(false),
                                            Ordering::Relaxed,
                                        );
                                        record_setting_change(
                                            &st,
                                            "command_not_applied",
                                            serde_json::json!({
                                                "command": protocol::cmd_tx_state(2),
                                                "error": error,
                                            }),
                                        );
                                        disable_recording_on_error(&st, "TX ON application", &error);
                                        format!("TX ON acknowledged, but requested state was not applied: {error}")
                                    }
                                }
                            }
                            Err(error) => {
                                format!("TX ON acknowledged, but settings readback failed: {error}")
                            }
                        }
                    }
                    Err(e) => format!("TX ON failed: {e}"),
                }
            }
            "off" => {
                let result = st.conn.lock().map_err(|_| "connection lock poisoned".to_string())
                    .and_then(|mut conn| conn.tx_off().map_err(|e| e.to_string()));
                match result {
                    Ok(()) => {
                        match refresh_settings_from_radar(&st, "tx_off") {
                            Ok(()) => match validate_command_applied(&st, &protocol::cmd_tx_state(1)) {
                                Ok(()) => "TX OFF: Radar stopped; settings readback verified".to_string(),
                                Err(error) => {
                                    st.tx_active.store(
                                        observed_tx_active(&st).unwrap_or(false),
                                        Ordering::Relaxed,
                                    );
                                    record_setting_change(
                                        &st,
                                        "command_not_applied",
                                        serde_json::json!({
                                            "command": protocol::cmd_tx_state(1),
                                            "error": error,
                                        }),
                                    );
                                    disable_recording_on_error(&st, "TX OFF application", &error);
                                    format!("TX OFF acknowledged, but requested state was not applied: {error}")
                                }
                            },
                            Err(error) => format!("TX OFF acknowledged, but settings readback failed: {error}"),
                        }
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
        let connect_result = {
            let mut conn = st.conn.lock().unwrap();
            let radar_ip = conn.radar_ip;
            let host_ip = conn.host_ip;
            *conn = RadarConnection::new(radar_ip, host_ip);
            conn.connect_full()
        };
        match connect_result {
            Ok(()) => {
                match refresh_settings_from_radar(&st, "reconnect") {
                    Ok(()) => {
                        st.connected.store(true, Ordering::Relaxed);
                        st.tx_active.store(false, Ordering::Relaxed);
                        let fw = st.conn.lock().ok().and_then(|conn| conn.firmware.clone()).unwrap_or_else(|| "unknown".into());
                        format!("Reconnected. Firmware: {fw}. Settings readback verified.")
                    }
                    Err(error) => {
                        st.connected.store(true, Ordering::Relaxed);
                        st.tx_active.store(false, Ordering::Relaxed);
                        format!("Reconnected, but settings readback failed: {error}")
                    }
                }
            }
            Err(e) => {
                st.connected.store(false, Ordering::Relaxed);
                invalidate_settings_readback(&st, "reconnect_failed", &e.to_string());
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

    if command == "ALL" {
        let refresh_error = if params.fresh.unwrap_or(0) != 0 {
            let refresh_state = state.clone();
            tokio::task::spawn_blocking(move || refresh_settings_from_radar(&refresh_state, "api_fresh"))
                .await
                .ok()
                .and_then(Result::err)
        } else {
            None
        };
        let readback = state.settings_readback
            .read()
            .map(|value| value.clone())
            .unwrap_or_else(|_| SettingsReadback::empty(Ipv4Addr::UNSPECIFIED, state.host_ip));
        let settings = state.settings
            .read()
            .map(|value| value.clone())
            .unwrap_or_else(|_| CurrentSettings::default());
        let settings_verified = readback.verified;
        let payload = serde_json::json!({
            "settings_readback": readback,
            "settings": settings,
            "settings_verified": settings_verified,
            "fresh_requested": params.fresh.unwrap_or(0) != 0,
            "refresh_error": refresh_error,
        });
        return (
            StatusCode::OK,
            [(header::CONTENT_TYPE, "application/json")],
            payload.to_string(),
        ).into_response();
    }

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
                    Ok((true, line)) => format!("EF processing mode: {line}"),
                    _ => "EF processing mode: Unknown".into(),
                }
            }
            "SWEEP" => "Sweep: controlled locally (640x640 PPI)".into(),
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
    let (manual_value, auto_value) = match (state.settings.read(), state.settings_readback.read()) {
        (Ok(settings), Ok(readback)) if readback.verified => {
            let manual = params.manual_value.unwrap_or(settings.gain_manual_value);
            let automatic = params.auto_value.unwrap_or(settings.gain_auto_value);
            (manual, automatic)
        }
        (Ok(_), Ok(_)) => {
            return "Gain change unavailable: a verified current gain readback is required to preserve the inactive value".into();
        }
        _ => return "Gain change unavailable: settings state lock is unavailable".into(),
    };
    let value = if auto { auto_value } else { manual_value };
    let cmd = protocol::cmd_gain_full(auto, manual_value, auto_value);
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

async fn handle_set_noise_reduction(
    State(state): State<Arc<AppState>>,
    Query(params): Query<ModeParam>,
) -> String {
    if let Some(msg) = reject_manual_setting_change(&state, "noise_reduction") {
        return msg;
    }
    let enabled = params.enabled.unwrap_or(0) != 0;
    let cmd = protocol::cmd_noise_reduction(enabled);
    let label = if enabled { "ON" } else { "OFF" };
    send_cmd(state, cmd, "$N67,", format!("Noise Reduction {label}")).await
}

async fn handle_set_antenna_height(
    State(state): State<Arc<AppState>>,
    Query(params): Query<ValueParam>,
) -> String {
    if let Some(msg) = reject_manual_setting_change(&state, "antenna_height_raw") {
        return msg;
    }
    let cmd = protocol::cmd_antenna_height(params.value);
    send_cmd(state, cmd, "$N84,", format!("Antenna height raw value set to {}", params.value)).await
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
    send_cmd(state, cmd, "$NEF,", format!("EF processing mode set to {mode}")).await
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
    cpu_cores: Option<Vec<f64>>,
    cpu_load: Option<f64>,
    memory_used: Option<f64>,
    storage_used: Option<f64>,
    storage_path: String,
    network_rx_mbps: Option<f64>,
    network_tx_mbps: Option<f64>,
    temperature_c: Option<f64>,
}

async fn handle_system_metrics(
    State(state): State<Arc<AppState>>,
) -> axum::response::Response {
    let storage_path = state.recording_root.path.clone();
    let storage_path_display = storage_path.display().to_string();
    let metrics = tokio::task::spawn_blocking(move || read_system_metrics(&storage_path))
        .await
        .unwrap_or_else(|_| SystemMetrics {
            cpu_cores: None,
            cpu_load: None,
            memory_used: None,
            storage_used: None,
            storage_path: storage_path_display,
            network_rx_mbps: None,
            network_tx_mbps: None,
            temperature_c: None,
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
    memory_total_mb: Option<u64>,
    memory_used_mb: Option<u64>,
    storage_total_gb: Option<f64>,
    storage_used_gb: Option<f64>,
    recording_root: String,
    recording_root_configured: bool,
    settings_verified: bool,
    settings_readback: SettingsReadback,
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
    let settings_readback = state.settings_readback.read()
        .map(|s| s.clone())
        .unwrap_or_else(|_| SettingsReadback::empty(state.conn.lock().map(|c| c.radar_ip).unwrap_or(Ipv4Addr::UNSPECIFIED), state.host_ip));

    let (memory_total_mb, memory_used_mb) = read_memory_abs();
    let (storage_total_gb, storage_used_gb) = read_storage_abs(&state.recording_root.path);

    let info = SystemInfo {
        newest_png: if tx { "Live radar feed" } else { "TX off" }.into(),
        connected,
        tx_active: tx,
        firmware,
        radar_ip,
        host_ip: state.host_ip.to_string(),
        recording,
        record_dir,
        recording_root: state.recording_root.path.display().to_string(),
        recording_root_configured: state.recording_root.configured,
        rotation_count,
        uptime_seconds,
        pattern_running,
        settings,
        memory_total_mb,
        memory_used_mb,
        storage_total_gb,
        storage_used_gb,
        settings_verified: settings_readback.verified,
        settings_readback,
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
            if let Err(e) = validate_recording_root(&state.recording_root) {
                return format!("Recording unavailable: {e}");
            }
            if !state.connected.load(Ordering::Relaxed) {
                return "Recording unavailable: radar is not connected".into();
            }
            if let Err(error) = refresh_settings_from_radar(&state, "recording_start") {
                disable_recording_on_error(&state, "recording-start readback", &error);
                return format!("Recording unavailable: settings readback failed: {error}");
            }
            let active_pattern = state.active_pattern.read().ok().and_then(|p| p.clone());
            let dir = if let Some(pattern) = active_pattern {
                match start_pattern_recording_session(&state, pattern, Some("pattern")) {
                    Ok(dir) => dir,
                    Err(e) => return format!("Failed to start recording: {e}"),
                }
            } else {
                match start_manual_recording_session(&state) {
                    Ok(dir) => dir,
                    Err(e) => return format!("Failed to start recording: {e}"),
                }
            };
            state.recording.store(true, Ordering::Relaxed);
            if let Err(e) = persist_recording_settings(&state) {
                disable_recording_on_error(&state, "initial settings output", &e);
                return format!("Recording disabled: {e}");
            }
            format!("Recording started. Saving to {dir}/")
        }
        "off" | "stop" => {
            let was_recording = state.recording.swap(false, Ordering::Relaxed);
            let drained = state.rotation_writer.wait_for_drain(Duration::from_secs(30));
            if was_recording && state.connected.load(Ordering::Relaxed) {
                let _ = refresh_settings_from_radar(&state, "recording_stop");
                finish_recording_boundary(&state);
                let _ = persist_recording_settings(&state);
            }
            let dir = state.record_dir.read().map(|d| d.clone()).unwrap_or_default();
            if drained {
                format!("Recording stopped. Files in {dir}/")
            } else {
                format!("Recording stopped, but rotation output drain is incomplete. Files in {dir}/ may still be receiving queued output.")
            }
        }
        _ => "Unknown mode (use start/stop)".into(),
    }
}

async fn handle_unavailable(uri: axum::http::Uri) -> impl IntoResponse {
    (
        StatusCode::NOT_IMPLEMENTED,
        format!("Unavailable: {} is not implemented", uri.path()),
    )
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

fn pattern_commands_with_preserved_gain(
    state: &AppState,
    commands: Vec<String>,
) -> Result<Vec<String>, String> {
    if !commands.iter().any(|command| command.starts_with("$S63,")) {
        return Ok(commands);
    }
    let auto_value = {
        let settings = state.settings.read().map_err(|_| "settings lock poisoned".to_string())?;
        let readback = state.settings_readback.read().map_err(|_| "settings readback lock poisoned".to_string())?;
        if !readback.verified {
            return Err("pattern gain change requires verified readback to preserve automatic gain".into());
        }
        settings.gain_auto_value
    };
    commands
        .into_iter()
        .map(|command| {
            if !command.starts_with("$S63,") {
                return Ok(command);
            }
            let manual_value = command
                .split(',')
                .nth(2)
                .and_then(|value| value.trim().parse::<u16>().ok())
                .ok_or_else(|| format!("invalid pattern gain command {command:?}"))?;
            Ok(protocol::cmd_gain_full(false, manual_value, auto_value))
        })
        .collect()
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
    // The state cache is updated only by a successful radar readback. Keep
    // this helper as a source-compatible no-op for the existing handlers;
    // applying a command before its acknowledgement would make metadata
    // claim a value that the radar may have rejected.
    let _ = (state, changes, update);
}

fn write_manual_settings_snapshot(state: &AppState, dir: &str) -> Result<(), io::Error> {
    let current_settings = state
        .settings
        .read()
        .map_err(|_| io::Error::other("settings lock poisoned"))?
        .clone();
    let settings_readback = state
        .settings_readback
        .read()
        .map_err(|_| io::Error::other("settings readback lock poisoned"))?
        .clone();
    let boundary = state
        .recording_boundary
        .read()
        .map_err(|_| io::Error::other("recording boundary lock poisoned"))?
        .clone();
    let history = state
        .setting_history
        .read()
        .map_err(|_| io::Error::other("setting history lock poisoned"))?
        .clone();

    let snapshot = ManualRecordingSettingsSnapshot {
        schema_version: "furuno-web-2",
        mode: "manual",
        recording_dir: dir.to_string(),
        current_settings,
        settings_readback,
        boundary,
        history,
    };

    let json = serde_json::to_string_pretty(&snapshot)
        .map_err(|e| io::Error::other(format!("serialize settings: {e}")))?;
    std::fs::write(format!("{dir}/settings.json"), json)
}

fn write_pattern_settings_snapshot(
    state: &AppState,
    dir: &str,
    pattern: &PatternInfo,
) -> Result<(), io::Error> {
    let current_settings = state
        .settings
        .read()
        .map_err(|_| io::Error::other("settings lock poisoned"))?
        .clone();
    let settings_readback = state
        .settings_readback
        .read()
        .map_err(|_| io::Error::other("settings readback lock poisoned"))?
        .clone();
    let boundary = state
        .recording_boundary
        .read()
        .map_err(|_| io::Error::other("recording boundary lock poisoned"))?
        .clone();
    let history = state
        .setting_history
        .read()
        .map_err(|_| io::Error::other("setting history lock poisoned"))?
        .clone();
    let snapshot = PatternRecordingSettingsSnapshot {
        schema_version: "furuno-web-2",
        mode: "pattern",
        recording_dir: dir.to_string(),
        current_settings,
        settings_readback,
        boundary,
        pattern: pattern.clone(),
        history,
    };
    let json = serde_json::to_string_pretty(&snapshot)
        .map_err(|e| io::Error::other(format!("serialize settings: {e}")))?;
    std::fs::write(format!("{dir}/settings.json"), json)
}

fn persist_recording_settings(state: &AppState) -> Result<(), io::Error> {
    let dir = match state.record_dir.read() {
        Ok(dir) if !dir.is_empty() => dir.clone(),
        Ok(_) => return Ok(()),
        Err(_) => return Err(io::Error::other("recording directory lock poisoned")),
    };
    let kind = match state.recording_session_kind.read() {
        Ok(kind) => kind.clone(),
        Err(_) => return Err(io::Error::other("recording session lock poisoned")),
    };
    let result = match kind {
        RecordingSessionKind::Manual => write_manual_settings_snapshot(state, &dir),
        RecordingSessionKind::Pattern(pattern) => write_pattern_settings_snapshot(state, &dir, &pattern),
    };
    result
}

fn disable_recording_on_error<E: std::fmt::Display>(state: &AppState, context: &str, error: E) {
    eprintln!("Recording disabled after {context} error: {error}");
    state.recording.store(false, Ordering::Relaxed);
}

fn write_recording_rotation_job(
    state: &AppState,
    job: RotationWriteJob,
) -> Result<(), io::Error> {
    let pixels = render::render_ppi(&job.spokes);
    let png_bytes = render::encode_png_bytes(&pixels, PPI_SIZE, PPI_SIZE);
    if let Ok(mut png) = state.latest_png.write() {
        *png = png_bytes.clone();
    }
    if !job.persist {
        return Ok(());
    }

    validate_recording_root(&state.recording_root)?;
    let dir = job.recording_dir;
    if dir.is_empty() {
        return Err(io::Error::new(
            io::ErrorKind::NotFound,
            "recording is enabled but no recording directory is active",
        ));
    }
    let ts = timestamp_compact();
    let csv_dir = Path::new(&dir).join("csv");
    let png_dir = Path::new(&dir).join("png");
    std::fs::create_dir_all(&csv_dir)?;
    std::fs::create_dir_all(&png_dir)?;
    write_echo_csv(
        &csv_dir.join(format!("rotation_{ts}.csv")).display().to_string(),
        &job.spokes,
        &job.settings,
        &job.readback,
        &job.boundary,
    )?;
    std::fs::write(png_dir.join(format!("rotation_{ts}.png")), png_bytes)
}

fn resolve_recording_root() -> RecordingRoot {
    match env::var_os("FURUNO_RECORDING_ROOT") {
        Some(raw) => {
            let raw_path = PathBuf::from(raw);
            let empty_configured = raw_path.as_os_str().is_empty();
            let path = if raw_path.is_absolute() {
                raw_path
            } else {
                env::current_dir()
                    .unwrap_or_else(|_| PathBuf::from("."))
                    .join(raw_path)
            };
            RecordingRoot {
                path,
                configured: true,
                empty_configured,
            }
        }
        None => RecordingRoot {
            path: env::current_dir()
                .unwrap_or_else(|_| PathBuf::from("."))
                .join("recordings"),
            configured: false,
            empty_configured: false,
        },
    }
}

fn validate_recording_root(root: &RecordingRoot) -> Result<(), io::Error> {
    if root.empty_configured {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "FURUNO_RECORDING_ROOT is empty",
        ));
    }
    if !root.path.exists() {
        if root.configured {
            return Err(io::Error::new(
                io::ErrorKind::NotFound,
                format!("configured recording root does not exist: {}", root.path.display()),
            ));
        }
        std::fs::create_dir_all(&root.path)?;
    }
    let metadata = std::fs::metadata(&root.path)?;
    if !metadata.is_dir() {
        return Err(io::Error::new(
            io::ErrorKind::NotADirectory,
            format!("recording root is not a directory: {}", root.path.display()),
        ));
    }

    let probe = root.path.join(format!(
        ".furuno_write_probe_{}_{}",
        std::process::id(),
        timestamp_compact()
    ));
    let file = std::fs::OpenOptions::new()
        .write(true)
        .create_new(true)
        .open(&probe)?;
    drop(file);
    std::fs::remove_file(&probe)?;
    Ok(())
}

fn create_recording_dir(root: &RecordingRoot, tag: Option<&str>) -> Result<String, io::Error> {
    validate_recording_root(root)?;
    let ts = timestamp_compact();
    let base = match tag {
        Some(tag) => root.path.join(format!("{ts}_{tag}")),
        None => root.path.join(ts),
    };
    let mut dir = base.clone();
    let mut suffix = 1usize;
    while dir.exists() {
        dir = PathBuf::from(format!("{}_{suffix:02}", base.display()));
        suffix += 1;
    }
    std::fs::create_dir_all(&dir)?;
    Ok(dir.display().to_string())
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

fn begin_recording_boundary(state: &AppState) -> Result<(), io::Error> {
    let readback = state
        .settings_readback
        .read()
        .map_err(|_| io::Error::other("settings readback lock poisoned"))?;
    if !readback.verified {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "recording requires a verified radar settings readback",
        ));
    }
    let mut boundary = state
        .recording_boundary
        .write()
        .map_err(|_| io::Error::other("recording boundary lock poisoned"))?;
    *boundary = RecordingBoundary {
        started_at: timestamp_precise(),
        start_transition_id: readback.transition_id,
        stopped_at: None,
        stop_transition_id: None,
    };
    Ok(())
}

fn finish_recording_boundary(state: &AppState) {
    if let Ok(mut boundary) = state.recording_boundary.write() {
        boundary.stopped_at = Some(timestamp_precise());
        boundary.stop_transition_id = state
            .settings_readback
            .read()
            .ok()
            .map(|readback| readback.transition_id);
    }
}

fn start_manual_recording_session(state: &AppState) -> Result<String, std::io::Error> {
    let dir = match create_recording_dir(&state.recording_root, None) {
        Ok(dir) => dir,
        Err(e) => {
            state.recording.store(false, Ordering::Relaxed);
            return Err(e);
        }
    };
    if let Ok(mut record_dir) = state.record_dir.write() {
        *record_dir = dir.clone();
    }
    if let Ok(mut history) = state.setting_history.write() {
        history.clear();
    }
    if let Ok(mut kind) = state.recording_session_kind.write() {
        *kind = RecordingSessionKind::Manual;
    }
    begin_recording_boundary(state)?;
    if let Err(error) = persist_recording_settings(state) {
        disable_recording_on_error(state, "manual settings output", &error);
        return Err(error);
    }
    Ok(dir)
}

fn start_pattern_recording_session(
    state: &AppState,
    pattern: PatternInfo,
    tag: Option<&str>,
) -> Result<String, std::io::Error> {
    let dir = match create_recording_dir(&state.recording_root, tag) {
        Ok(dir) => dir,
        Err(e) => {
            state.recording.store(false, Ordering::Relaxed);
            return Err(e);
        }
    };
    if let Ok(mut record_dir) = state.record_dir.write() {
        *record_dir = dir.clone();
    }
    if let Ok(mut history) = state.setting_history.write() {
        history.clear();
    }
    if let Ok(mut kind) = state.recording_session_kind.write() {
        *kind = RecordingSessionKind::Pattern(pattern);
    }
    begin_recording_boundary(state)?;
    if let Err(error) = persist_recording_settings(state) {
        disable_recording_on_error(state, "pattern settings output", &error);
        return Err(error);
    }
    Ok(dir)
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
            let cmds = match pattern_commands_with_preserved_gain(&state, step.commands_vs(prev_step)) {
                Ok(commands) => commands,
                Err(error) => {
                    invalidate_settings_readback(&state, "pattern_gain_readback_missing", &error);
                    disable_recording_on_error(&state, "pattern gain command", &error);
                    if let Ok(mut status) = state.pattern_status.write() { *status = format!("Pattern stopped: {error}"); }
                    state.pattern_running.store(false, Ordering::Relaxed);
                    return;
                }
            };
            if !cmds.is_empty() {
                let mut command_error = None;
                if let Ok(mut conn) = state.conn.lock() {
                    for cmd in &cmds {
                        match conn.send_and_read_ack(cmd, "$N", Duration::from_secs(3)) {
                            Ok((true, line)) => {
                                if let Err(error) = crate::connection::validate_setting_ack(cmd, &line) {
                                    command_error = Some(error.to_string());
                                    break;
                                }
                            }
                            Ok((false, _)) => {
                                command_error = Some("pattern command was not acknowledged".to_string());
                                break;
                            }
                            Err(error) => {
                                command_error = Some(error.to_string());
                                break;
                            }
                        }
                    }
                } else {
                    command_error = Some("connection lock poisoned".into());
                }
                if let Some(error) = command_error {
                    invalidate_settings_readback(&state, "pattern_command_failed", &error);
                    disable_recording_on_error(&state, "pattern command", &error);
                    if let Ok(mut status) = state.pattern_status.write() { *status = format!("Pattern stopped: {error}"); }
                    state.pattern_running.store(false, Ordering::Relaxed);
                    return;
                }
                // Brief settle time after config change
                thread::sleep(Duration::from_millis(300));
            }
            if let Err(error) = refresh_settings_from_radar(&state, "pattern_step") {
                invalidate_settings_readback(&state, "pattern_readback_failed", &error);
                disable_recording_on_error(&state, "pattern readback", &error);
                if let Ok(mut status) = state.pattern_status.write() { *status = format!("Pattern stopped: {error}"); }
                state.pattern_running.store(false, Ordering::Relaxed);
                return;
            }
            if let Err(error) = validate_commands_applied(&state, &cmds) {
                record_setting_change(
                    &state,
                    "command_not_applied",
                    serde_json::json!({ "commands": cmds, "error": error }),
                );
                disable_recording_on_error(&state, "pattern command application", &error);
                if let Ok(mut status) = state.pattern_status.write() { *status = format!("Pattern stopped: {error}"); }
                state.pattern_running.store(false, Ordering::Relaxed);
                return;
            }
            if state.recording.load(Ordering::Relaxed) {
                if let Err(error) = persist_recording_settings(&state) {
                    disable_recording_on_error(&state, "pattern settings output", &error);
                    if let Ok(mut status) = state.pattern_status.write() {
                        *status = format!("Recording disabled: {error}");
                    }
                }
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
                    let cmds = match pattern_commands_with_preserved_gain(&st, step.commands_vs(prev_step)) {
                        Ok(commands) => commands,
                        Err(error) => {
                            invalidate_settings_readback(&st, "queued_pattern_gain_readback_missing", &error);
                            disable_recording_on_error(&st, "queued pattern gain command", &error);
                            if let Ok(mut status) = st.pattern_status.write() { *status = format!("Queue stopped: {error}"); }
                            break 'outer;
                        }
                    };
                    if !cmds.is_empty() {
                        let mut command_error = None;
                        if let Ok(mut conn) = st.conn.lock() {
                            for cmd in &cmds {
                                match conn.send_and_read_ack(cmd, "$N", Duration::from_secs(3)) {
                                    Ok((true, line)) => {
                                        if let Err(error) = crate::connection::validate_setting_ack(cmd, &line) {
                                            command_error = Some(error.to_string());
                                            break;
                                        }
                                    }
                                    Ok((false, _)) => {
                                        command_error = Some("queued pattern command was not acknowledged".to_string());
                                        break;
                                    }
                                    Err(error) => {
                                        command_error = Some(error.to_string());
                                        break;
                                    }
                                }
                            }
                        } else {
                            command_error = Some("connection lock poisoned".into());
                        }
                        if let Some(error) = command_error {
                            invalidate_settings_readback(&st, "queued_pattern_command_failed", &error);
                            disable_recording_on_error(&st, "queued pattern command", &error);
                            if let Ok(mut status) = st.pattern_status.write() { *status = format!("Queue stopped: {error}"); }
                            break 'outer;
                        }
                        thread::sleep(Duration::from_millis(300));
                    }
                    if let Err(error) = refresh_settings_from_radar(&st, "queued_pattern_step") {
                        invalidate_settings_readback(&st, "queued_pattern_readback_failed", &error);
                        disable_recording_on_error(&st, "queued pattern readback", &error);
                        if let Ok(mut status) = st.pattern_status.write() { *status = format!("Queue stopped: {error}"); }
                        break 'outer;
                    }
                    if let Err(error) = validate_commands_applied(&st, &cmds) {
                        record_setting_change(
                            &st,
                            "command_not_applied",
                            serde_json::json!({ "commands": cmds, "error": error }),
                        );
                        disable_recording_on_error(&st, "queued pattern command application", &error);
                        if let Ok(mut status) = st.pattern_status.write() { *status = format!("Queue stopped: {error}"); }
                        break 'outer;
                    }
                    if st.recording.load(Ordering::Relaxed) {
                        if let Err(error) = persist_recording_settings(&st) {
                            disable_recording_on_error(&st, "queued pattern settings output", &error);
                            if let Ok(mut status) = st.pattern_status.write() {
                                *status = format!("Recording disabled: {error}");
                            }
                        }
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

fn parse_net_bytes() -> Option<(u64, u64)> {
    let content = match std::fs::read_to_string("/proc/net/dev") {
        Ok(c) => c,
        Err(_) => return None,
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
    Some((rx_total, tx_total))
}

fn read_system_metrics(storage_path: &Path) -> SystemMetrics {
    let mut metrics = SystemMetrics {
        cpu_cores: None,
        cpu_load: None,
        memory_used: None,
        storage_used: None,
        storage_path: storage_path.display().to_string(),
        network_rx_mbps: None,
        network_tx_mbps: None,
        temperature_c: None,
    };

    // Per-core CPU usage from /proc/stat delta
    let cur_cpu = parse_cpu_times();
    let net_sample = parse_net_bytes();
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
            metrics.cpu_load = Some(if core_usage.is_empty() { 0.0 } else {
                core_usage.iter().sum::<f64>() / core_usage.len() as f64
            });
            metrics.cpu_cores = Some(core_usage);
        }

        // Network throughput
        if let Some((cur_rx, cur_tx)) = net_sample {
            if elapsed > 0.05 && prev.net_rx > 0 {
                let d_rx = cur_rx.saturating_sub(prev.net_rx) as f64;
                let d_tx = cur_tx.saturating_sub(prev.net_tx) as f64;
                metrics.network_rx_mbps = Some((d_rx / elapsed) / (1024.0 * 1024.0));
                metrics.network_tx_mbps = Some((d_tx / elapsed) / (1024.0 * 1024.0));
            }
            prev.net_rx = cur_rx;
            prev.net_tx = cur_tx;
        }

        prev.cpu_times = cur_cpu;
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
            metrics.memory_used = Some(1.0 - (available as f64 / total as f64));
        }
    }

    // Storage
    if let Ok(stat) = nix_statvfs(storage_path) {
        if stat.total > 0 {
            metrics.storage_used = Some(1.0 - (stat.available as f64 / stat.total as f64));
        }
    }

    // Temperature
    if let Ok(content) = std::fs::read_to_string("/sys/class/thermal/thermal_zone0/temp") {
        if let Ok(millic) = content.trim().parse::<f64>() {
            metrics.temperature_c = Some(millic / 1000.0);
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

fn nix_statvfs(path: &Path) -> Result<StatVfs, ()> {
    use std::ffi::CString;
    let c_path = CString::new(path.to_string_lossy().as_bytes()).map_err(|_| ())?;
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

fn read_memory_abs() -> (Option<u64>, Option<u64>) {
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
        if total_mb > 0 {
            (Some(total_mb), Some(used_mb))
        } else {
            (None, None)
        }
    } else {
        (None, None)
    }
}

fn read_storage_abs(path: &Path) -> (Option<f64>, Option<f64>) {
    if let Ok(stat) = nix_statvfs(path) {
        let total_gb = stat.total as f64 / (1024.0 * 1024.0 * 1024.0);
        let used_gb = total_gb - (stat.available as f64 / (1024.0 * 1024.0 * 1024.0));
        (Some(total_gb), Some(used_gb))
    } else {
        (None, None)
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

fn timestamp_precise() -> String {
    let now = std::time::SystemTime::now();
    let millis = now
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis();
    format!("unix_ms:{millis}")
}

fn timestamp_precise_after(offset: Duration) -> String {
    let millis = std::time::SystemTime::now()
        .checked_add(offset)
        .unwrap_or_else(std::time::SystemTime::now)
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis();
    format!("unix_ms:{millis}")
}

fn write_echo_csv(
    path: &str,
    spokes: &[Spoke],
    settings: &CurrentSettings,
    readback: &SettingsReadback,
    boundary: &RecordingBoundary,
) -> Result<(), io::Error> {
    use std::io::Write;
    let mut f = std::io::BufWriter::new(
        std::fs::File::create(path)?,
    );

    // Write metadata header as comment lines. The gain row is the raw value
    // for the active radar mode: automatic mode uses N63 field 3 and manual
    // mode uses N63 field 1. It is not a percentage conversion.
    writeln!(f, "# schema_version,furuno-web-2")?;
    writeln!(f, "# row_gain_semantics,mode_specific_raw_control_value")?;
    writeln!(f, "# settings_verified,{}", readback.verified)?;
    writeln!(f, "# settings_readback_captured_at,{}", readback.captured_at)?;
    writeln!(f, "# settings_readback_transition_id,{}", readback.transition_id)?;
    writeln!(f, "# settings_readback_valid_until,{}", readback.valid_until.as_deref().unwrap_or(""))?;
    writeln!(f, "# settings_readback_source,{}", readback.source)?;
    writeln!(f, "# radar_ip,{}", readback.radar_ip)?;
    writeln!(f, "# host_ip,{}", readback.host_ip)?;
    writeln!(f, "# firmware,{}", readback.firmware)?;
    writeln!(f, "# recording_started_at,{}", boundary.started_at)?;
    writeln!(f, "# recording_start_transition_id,{}", boundary.start_transition_id)?;
    writeln!(f, "# recording_stopped_at,{}", boundary.stopped_at.as_deref().unwrap_or(""))?;
    let readback_json = serde_json::to_string(readback)
        .map_err(|error| io::Error::other(format!("serialize readback metadata: {error}")))?;
    writeln!(f, "# settings_readback_json,{}", readback_json)?;
    writeln!(f, "# tx_state,{}", settings.tx_state)?;
    writeln!(f, "# range_index,{}", settings.range_index)?;
    writeln!(f, "# gain_auto,{}", settings.gain_auto)?;
    writeln!(f, "# gain_value,{}", settings.gain_value)?;
    writeln!(f, "# gain_manual_value,{}", settings.gain_manual_value)?;
    writeln!(f, "# gain_auto_value,{}", settings.gain_auto_value)?;
    writeln!(f, "# pulse_width,{}", settings.pulse_width)?;
    writeln!(f, "# pulse_range_index,{}", settings.pulse_range_index)?;
    writeln!(f, "# sea_clutter,{}", settings.sea_clutter)?;
    writeln!(f, "# sea_clutter_auto,{}", settings.sea_clutter_auto)?;
    writeln!(f, "# sea_clutter_auto_value,{}", settings.sea_clutter_auto_value)?;
    writeln!(f, "# sea_clutter_mode,{}", settings.sea_clutter_mode)?;
    writeln!(f, "# rain_clutter,{}", settings.rain_clutter)?;
    writeln!(f, "# rain_clutter_auto,{}", settings.rain_clutter_auto)?;
    writeln!(f, "# rain_clutter_auto_value,{}", settings.rain_clutter_auto_value)?;
    writeln!(f, "# rain_clutter_ftc_mode,{}", settings.rain_clutter_ftc_mode)?;
    writeln!(f, "# rain_clutter_mode,{}", settings.rain_clutter_mode)?;
    writeln!(f, "# rezboost,{}", settings.rezboost)?;
    writeln!(f, "# interference_rejection_level,{}", settings.interference_rejection_level)?;
    writeln!(f, "# interference_rejection,{}", settings.interference_rejection)?;
    writeln!(f, "# noise_reduction_level,{}", settings.noise_reduction_level)?;
    writeln!(f, "# noise_reduction,{}", settings.noise_reduction)?;
    writeln!(f, "# antenna_height_raw,{}", settings.antenna_height_raw)?;
    writeln!(f, "# bird_mode,{}", settings.bird_mode)?;
    writeln!(f, "# doppler,{}", settings.doppler)?;
    writeln!(f, "# antenna_speed,{}", settings.antenna_speed)?;
    writeln!(f, "# antenna_hi_rpm,{}", settings.antenna_hi_rpm)?;
    writeln!(f, "# main_bang,{}", settings.main_bang)?;
    writeln!(f, "# near_stc,{}", settings.near_stc)?;
    writeln!(f, "# middle_stc,{}", settings.middle_stc)?;
    writeln!(f, "# far_stc,{}", settings.far_stc)?;
    writeln!(f, "Status\tScale\tRange\tGain\tAngle\tEchoValues")?;

    let gain = settings.gain_value;
    let scale = settings.range_index;
    let range = settings.range_index;

    for spoke in spokes {
        let echo_values = spoke
            .echo
            .iter()
            .map(|value| value.to_string())
            .collect::<Vec<_>>()
            .join(",");
        writeln!(
            f,
            "0\t{scale}\t{range}\t{gain}\t{}\t{}",
            spoke.angle_ticks.min(8191),
            echo_values
        )?;
    }
    f.flush()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn configured_recording_root_refuses_missing_path() {
        let path = env::temp_dir().join(format!(
            "furuno-missing-recording-root-{}-{}",
            std::process::id(),
            timestamp_compact()
        ));
        let root = RecordingRoot {
            path: path.clone(),
            configured: true,
            empty_configured: false,
        };

        let error = create_recording_dir(&root, None)
            .expect_err("missing configured root must fail");
        assert_eq!(error.kind(), io::ErrorKind::NotFound);
        assert!(!path.exists(), "configured validation must not recreate the root");
    }

    #[test]
    fn configured_empty_recording_root_is_rejected() {
        let root = RecordingRoot {
            path: env::current_dir().expect("current directory"),
            configured: true,
            empty_configured: true,
        };

        let error = validate_recording_root(&root).expect_err("empty configured root must fail");
        assert_eq!(error.kind(), io::ErrorKind::InvalidInput);
    }

    #[test]
    fn configured_recording_root_refuses_file_path() {
        let path = env::temp_dir().join(format!(
            "furuno-recording-root-file-{}-{}",
            std::process::id(),
            timestamp_compact()
        ));
        std::fs::write(&path, b"not a directory").expect("create test path");
        let root = RecordingRoot {
            path: path.clone(),
            configured: true,
            empty_configured: false,
        };

        let error = create_recording_dir(&root, None).expect_err("file root must fail");
        assert_eq!(error.kind(), io::ErrorKind::NotADirectory);
        std::fs::remove_file(path).expect("remove test path");
    }

    fn test_rotation_job(persist: bool) -> RotationWriteJob {
        RotationWriteJob {
            spokes: Vec::new(),
            settings: CurrentSettings::default(),
            readback: SettingsReadback::empty(Ipv4Addr::LOCALHOST, Ipv4Addr::LOCALHOST),
            boundary: RecordingBoundary::default(),
            recording_dir: String::new(),
            generation: 1,
            persist,
        }
    }

    #[test]
    fn rotation_writer_is_bounded_and_reports_saturation() {
        let (sender, receiver) = mpsc::sync_channel(1);
        let writer = RotationWriter::new(sender);
        writer.try_enqueue(test_rotation_job(false)).expect("first job fits");
        let error = writer
            .try_enqueue(test_rotation_job(false))
            .expect_err("second job must report a full bounded queue");
        assert!(error.contains("queue is full"));
        assert_eq!(writer.pending.lock().unwrap().to_owned(), 1);

        let _queued = receiver.try_recv().expect("first job remains queued");
        writer.complete();
        assert!(writer.wait_for_drain(Duration::from_millis(10)));
    }

    #[test]
    fn readback_validity_interval_is_checked_at_capture() {
        let mut readback = SettingsReadback::empty(Ipv4Addr::LOCALHOST, Ipv4Addr::LOCALHOST);
        readback.verified = true;
        readback.valid_until = Some(timestamp_precise_after(Duration::from_secs(1)));
        assert!(readback_valid_now(&readback));
        readback.valid_until = Some("unix_ms:0".into());
        assert!(!readback_valid_now(&readback));
    }

    fn synthetic_read(register: &str, values: &[&str]) -> RegisterReadback {
        let id = register.trim_start_matches("$N");
        let raw = format!("$N{},{}", id, values.join(","));
        RegisterReadback {
            requested: format!("$R{}\r\n", id),
            register: register.into(),
            raw: Some(raw),
            values: values.iter().map(|value| (*value).into()).collect(),
            error: None,
            requested_at: "unix_ms:test".into(),
            read_at: "unix_ms:test".into(),
        }
    }

    #[test]
    fn older_refresh_snapshot_cannot_replace_newer_capture() {
        let mut current = SettingsReadback::empty(Ipv4Addr::LOCALHOST, Ipv4Addr::LOCALHOST);
        current.captured_at = "unix_ms:200".into();
        let mut older = current.clone();
        older.captured_at = "unix_ms:199".into();
        let mut equal = current.clone();
        equal.captured_at = "unix_ms:200".into();
        assert!(!readback_is_newer_or_equal(&older, &current));
        assert!(readback_is_newer_or_equal(&equal, &current));
    }

    #[test]
    fn readback_maps_device_fields_and_retains_raw_registers() {
        let snapshot = CurrentRadarReadback {
            reads: vec![
                synthetic_read("$N62", &["4", "0", "0"]),
                synthetic_read("$N63", &["1", "12", "0", "80", "0"]),
                synthetic_read("$N64", &["1", "23", "45", "2", "0", "0"]),
                synthetic_read("$N65", &["0", "34", "56", "1", "2", "0", "999"]),
                synthetic_read("$N68", &["5", "0", "4", "0", "0"]),
                synthetic_read("$N69", &["1", "0", "0", "0", "0", "0"]),
                synthetic_read("$N83", &["17", "0"]),
                synthetic_read("$N85", &["3"]),
                synthetic_read("$N86", &["2"]),
                synthetic_read("$N87", &["1"]),
                synthetic_read("$N89", &["1", "1"]),
                synthetic_read("$NED", &["2", "0"]),
                synthetic_read("$NEE", &["3", "0"]),
                synthetic_read("$NEF", &["1", "1", "0", "1"]),
                synthetic_read("$N67", &["0", "0", "2", "0"]),
                synthetic_read("$N67", &["0", "2", "1", "0"]),
                synthetic_read("$N84", &["0", "0", "0"]),
            ],
        };
        let (settings, readback) = build_settings_readback(
            &snapshot,
            "firmware",
            Ipv4Addr::new(172, 31, 3, 226),
            Ipv4Addr::new(172, 31, 1, 100),
            "test",
        );
        assert!(readback.verified);
        assert_eq!(settings.range_index, 4);
        assert_eq!(settings.pulse_width, 5);
        assert!(settings.gain_auto);
        assert_eq!(settings.gain_manual_value, 12);
        assert_eq!(settings.gain_auto_value, 80);
        assert_eq!(settings.sea_clutter, 23);
        assert!(settings.sea_clutter_auto);
        assert_eq!(settings.sea_clutter_auto_value, 45);
        assert_eq!(settings.sea_clutter_mode, 2);
        assert_eq!(settings.rain_clutter, 34);
        assert!(!settings.rain_clutter_auto);
        assert_eq!(settings.rain_clutter_auto_value, 56);
        assert_eq!(settings.rain_clutter_ftc_mode, 1);
        assert_eq!(settings.rain_clutter_mode, 2);
        assert!(settings.antenna_hi_rpm);
        assert_eq!(settings.doppler, 2);
        assert!(settings.interference_rejection);
        assert_eq!(settings.interference_rejection_level, 2);
        assert!(readback.valid_until.as_deref().is_some_and(|value| value.starts_with("unix_ms:")));
        assert!(settings.noise_reduction);
        assert_eq!(settings.noise_reduction_level, 1);
        assert_eq!(settings.antenna_height_raw, 0);
        assert_eq!(readback.raw_register_responses.len(), 17);
        assert_eq!(readback.raw_register_responses[1].raw.as_deref(), Some("$N63,1,12,0,80,0"));
        assert_eq!(readback.raw_register_responses[3].values.last().map(String::as_str), Some("999"));

        assert!(validate_command_against_readback(
            &readback,
            "$S63,1,0,0,80,0\r\n",
        ).is_ok());
        assert!(validate_command_against_readback(
            &readback,
            "$S63,0,128,0,80,0\r\n",
        ).is_err(), "an echoed but unapplied gain command must be rejected");
        assert!(validate_command_against_readback(
            &readback,
            "$S67,0,0,2,0\r\n",
        ).is_ok());
        assert!(validate_command_against_readback(
            &readback,
            "$S67,0,2,1,0\r\n",
        ).is_ok());
        assert!(validate_command_against_readback(
            &readback,
            "$S68,5,0,4,0,0\r\n",
        ).is_ok());
        assert!(validate_command_against_readback(
            &readback,
            "$S69,1,0,0,0,0,0\r\n",
        ).is_ok());

        let mut wrong_selector = snapshot.clone();
        let ir = wrong_selector.reads.iter_mut().find(|read| read.register == "$N67").expect("IR readback");
        ir.values[1] = "2".into();
        ir.raw = Some("$N67,0,2,2,0".into());
        let (_, wrong_readback) = build_settings_readback(
            &wrong_selector,
            "firmware",
            Ipv4Addr::new(172, 31, 3, 226),
            Ipv4Addr::new(172, 31, 1, 100),
            "wrong_selector",
        );
        assert!(!wrong_readback.verified);
        assert!(!wrong_readback.fields["interference_rejection"].verified);
        assert!(!wrong_readback.fields["interference_rejection_level"].verified);

        let mut short_n84 = snapshot.clone();
        let antenna = short_n84.reads.iter_mut().find(|read| read.register == "$N84").expect("antenna height readback");
        antenna.values = vec!["0".into(), "15".into()];
        antenna.raw = Some("$N84,0,15".into());
        let (short_settings, short_readback) = build_settings_readback(
            &short_n84,
            "firmware",
            Ipv4Addr::new(172, 31, 3, 226),
            Ipv4Addr::new(172, 31, 1, 100),
            "short_n84",
        );
        assert!(short_readback.verified);
        assert_eq!(short_settings.antenna_height_raw, 15);
    }

    #[test]
    fn incomplete_readback_cannot_be_verified() {
        let snapshot = CurrentRadarReadback {
            reads: vec![synthetic_read("$N62", &["0", "0", "0"])],
        };
        let (settings, readback) = build_settings_readback(
            &snapshot,
            "firmware",
            Ipv4Addr::LOCALHOST,
            Ipv4Addr::LOCALHOST,
            "test_failure",
        );
        assert!(!readback.verified);
        assert_eq!(settings.status, "readback_incomplete");
        assert!(readback.fields.values().any(|field| !field.verified));
    }

    #[test]
    fn pulse_readback_must_echo_the_requested_range() {
        let mut reads = vec![
            synthetic_read("$N62", &["4", "0", "0"]),
            synthetic_read("$N63", &["0", "12", "0", "80", "0"]),
            synthetic_read("$N64", &["0", "23", "45", "2", "0", "0"]),
            synthetic_read("$N65", &["0", "34", "56", "1", "2", "0", "999"]),
            synthetic_read("$N68", &["5", "0", "3", "0", "0"]),
            synthetic_read("$N69", &["1", "0", "0", "0", "0", "0"]),
            synthetic_read("$N83", &["17", "0"]),
            synthetic_read("$N85", &["3"]),
            synthetic_read("$N86", &["2"]),
            synthetic_read("$N87", &["1"]),
            synthetic_read("$N89", &["1", "1"]),
            synthetic_read("$NED", &["2", "0"]),
            synthetic_read("$NEE", &["3", "0"]),
            synthetic_read("$NEF", &["1", "1", "0", "1"]),
            synthetic_read("$N67", &["0", "0", "2", "0"]),
            synthetic_read("$N67", &["0", "2", "1", "0"]),
            synthetic_read("$N84", &["0", "0", "0"]),
        ];
        let snapshot = CurrentRadarReadback { reads: std::mem::take(&mut reads) };
        let (settings, readback) = build_settings_readback(
            &snapshot,
            "firmware",
            Ipv4Addr::LOCALHOST,
            Ipv4Addr::LOCALHOST,
            "pulse_mismatch",
        );
        assert_eq!(settings.pulse_range_index, 3);
        assert!(!readback.verified);
        assert!(!readback.fields["pulse_width"].verified);
    }
}
