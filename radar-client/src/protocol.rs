// Furuno NXT radar wire protocol implementation.
// Covers: authentication, control commands, echo decoding, discovery.

use std::fmt;
use std::net::Ipv4Addr;

// --- Constants ---

pub const FURUNO_SPOKES: usize = 8192;
pub const FURUNO_SPOKE_LEN: usize = 883;
pub const FURUNO_BASE_PORT: u16 = 10000;
pub const FURUNO_BEACON_PORT: u16 = 10010;
pub const FURUNO_CONTROL_PORT: u16 = 10100;
pub const FURUNO_DATA_PORT: u16 = 10024;
pub const FURUNO_MULTICAST: Ipv4Addr = Ipv4Addr::new(239, 255, 0, 2);
pub const FURUNO_BEACON_BROADCAST: Ipv4Addr = Ipv4Addr::new(172, 31, 255, 255);

pub const AUTH_PAYLOAD: &[u8] = b"\x08\x01\x00\x38\x01\x00\x00\x00\x00\x01\x00\x00\
COPYRIGHT (C) 2001 FURUNO ELECTRIC CO.,LTD. ";

pub const KEEPALIVE: &[u8] = b"\x00\x00\x00\x08\x00\x00\x00\x00";

// Beacon packets for auto-discovery
pub const BEACON_ANNOUNCE: [u8; 32] = [
    0x1, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x1, 0x0, 0x18,
    0x1, 0x0, 0x0, 0x0,
    b'R', b'C', b'L', b'I', b'E', b'N', b'T', 0x0, // "RCLIENT" name
    0x1, 0x1, 0x0, 0x2, 0x0, 0x1, 0x0, 0x12,
];

pub const BEACON_REQUEST: [u8; 16] = [
    0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x01, 0x00, 0x08,
    0x01, 0x00, 0x00, 0x00,
];

pub const BEACON_REQUEST_MODEL: [u8; 16] = [
    0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x14, 0x01, 0x00, 0x08,
    0x01, 0x00, 0x00, 0x00,
];

// --- Range Table ---

/// (wire_index, meters, label)
pub const RANGE_TABLE: &[(u16, u32, &str)] = &[
    (21, 116,    "1/16 NM"),
    (0,  231,    "1/8 NM"),
    (1,  463,    "1/4 NM"),
    (2,  926,    "1/2 NM"),
    (3,  1389,   "3/4 NM"),
    (4,  1852,   "1 NM"),
    (5,  2778,   "1.5 NM"),
    (6,  3704,   "2 NM"),
    (7,  5556,   "3 NM"),
    (8,  7408,   "4 NM"),
    (9,  11112,  "6 NM"),
    (10, 14816,  "8 NM"),
    (11, 22224,  "12 NM"),
    (12, 29632,  "16 NM"),
    (13, 44448,  "24 NM"),
    (14, 59264,  "32 NM"),
    (19, 66672,  "36 NM"),
    (15, 88896,  "48 NM"),
];

pub fn meters_to_wire_index(meters: u32) -> u16 {
    for &(wire_idx, m, _) in RANGE_TABLE {
        if m == meters { return wire_idx; }
    }
    for &(wire_idx, m, _) in RANGE_TABLE {
        if m >= meters { return wire_idx; }
    }
    15 // fallback: max range
}

pub fn wire_index_to_meters(wire_index: u16) -> Option<u32> {
    RANGE_TABLE.iter()
        .find(|&&(idx, _, _)| idx == wire_index)
        .map(|&(_, meters, _)| meters)
}

pub fn wire_index_to_label(wire_index: u16) -> &'static str {
    RANGE_TABLE.iter()
        .find(|&&(idx, _, _)| idx == wire_index)
        .map(|&(_, _, label)| label)
        .unwrap_or("unknown")
}

// --- Echo Packet Metadata ---

pub struct EchoMetadata {
    pub sweep_count: u32,
    pub sweep_len: u32,
    pub encoding: u8,
}

pub fn parse_echo_metadata(data: &[u8]) -> EchoMetadata {
    EchoMetadata {
        sweep_count: (data[9] >> 1) as u32,
        sweep_len: (((data[11] & 0x07) as u32) << 8) | data[10] as u32,
        encoding: (data[11] & 0x18) >> 3,
    }
}

// --- Spoke ---

pub struct Spoke {
    pub angle_ticks: u16,
    pub heading_ticks: u16,
    pub echo: Vec<u8>,
}

// --- Encoding Decoders ---

/// Encoding 0: raw uncompressed data.
pub fn decode_sweep_encoding_0(sweep: &[u8], sweep_len: usize) -> (Vec<u8>, usize) {
    let n = sweep.len().min(sweep_len);
    let mut spoke = sweep[..n].to_vec();
    spoke.resize(sweep_len, 0);
    let used = (n + 3) & !3;
    (spoke, used)
}

/// Encoding 1: RLE with repeat marker.
/// LSB=0 → literal value. LSB=1 → repeat previous value (count = byte >> 1, 0 means 128).
pub fn decode_sweep_encoding_1(sweep: &[u8], sweep_len: usize) -> (Vec<u8>, usize) {
    let mut spoke = Vec::with_capacity(sweep_len);
    let mut used = 0;
    let mut strength: u8 = 0;

    while spoke.len() < sweep_len && used < sweep.len() {
        let b = sweep[used];
        if b & 0x01 == 0 {
            strength = b;
            spoke.push(strength);
        } else {
            let mut repeat = (b >> 1) as usize;
            if repeat == 0 { repeat = 128; }
            for _ in 0..repeat {
                spoke.push(strength);
                if spoke.len() >= sweep_len { break; }
            }
        }
        used += 1;
    }

    used = (used + 3) & !3;
    (spoke, used)
}

/// Encoding 2: delta RLE referencing previous spoke.
/// LSB=0 → literal value. LSB=1 → copy from prev spoke (count = byte >> 1, 0 means 128).
pub fn decode_sweep_encoding_2(sweep: &[u8], prev_spoke: &[u8], sweep_len: usize) -> (Vec<u8>, usize) {
    let mut spoke = Vec::with_capacity(sweep_len);
    let mut used = 0;

    while spoke.len() < sweep_len && used < sweep.len() {
        let b = sweep[used];
        if b & 0x01 == 0 {
            spoke.push(b);
        } else {
            let mut repeat = (b >> 1) as usize;
            if repeat == 0 { repeat = 128; }
            for _ in 0..repeat {
                let i = spoke.len();
                let strength = if prev_spoke.len() > i { prev_spoke[i] } else { 0 };
                spoke.push(strength);
                if spoke.len() >= sweep_len { break; }
            }
        }
        used += 1;
    }

    used = (used + 3) & !3;
    (spoke, used)
}

/// Encoding 3: 2-bit RLE with repeat-previous and copy-from-prev-spoke.
/// bits[1:0]=00 → literal value.
/// bits[1:0]!=00, bit0=1 → repeat previous strength (count = byte >> 2, 0 means 64).
/// bits[1:0]!=00, bit0=0 → copy from prev spoke (count = byte >> 2, 0 means 64).
pub fn decode_sweep_encoding_3(sweep: &[u8], prev_spoke: &[u8], sweep_len: usize) -> (Vec<u8>, usize) {
    let mut spoke = Vec::with_capacity(sweep_len);
    let mut used = 0;
    let mut strength: u8 = 0;

    while spoke.len() < sweep_len && used < sweep.len() {
        let b = sweep[used];
        if b & 0x03 == 0 {
            strength = b;
            spoke.push(strength);
        } else {
            let mut repeat = (b >> 2) as usize;
            if repeat == 0 { repeat = 64; }
            if b & 0x01 == 0 {
                for _ in 0..repeat {
                    let i = spoke.len();
                    strength = if prev_spoke.len() > i { prev_spoke[i] } else { 0 };
                    spoke.push(strength);
                    if spoke.len() >= sweep_len { break; }
                }
            } else {
                for _ in 0..repeat {
                    spoke.push(strength);
                    if spoke.len() >= sweep_len { break; }
                }
            }
        }
        used += 1;
    }

    used = (used + 3) & !3;
    (spoke, used)
}

/// Decode a raw UDP echo packet into multiple spokes.
/// Supports all 4 encoding types.
pub fn decode_echo_packet(pkt: &[u8], prev_spoke: &mut Vec<u8>, out: &mut Vec<Spoke>) {
    if pkt.len() < 20 { return; }

    let meta = parse_echo_metadata(pkt);
    let sweep_len = meta.sweep_len as usize;
    let mut sweep = &pkt[16..];

    for sweep_idx in 0..meta.sweep_count {
        if sweep.len() < 4 { break; }

        let angle = u16::from_le_bytes([sweep[0], sweep[1]]);
        let heading = u16::from_le_bytes([sweep[2], sweep[3]]);
        sweep = &sweep[4..];

        let (spoke_data, used) = match meta.encoding {
            0 => decode_sweep_encoding_0(sweep, sweep_len),
            1 => {
                if sweep_idx == 0 {
                    decode_sweep_encoding_1(sweep, sweep_len)
                } else {
                    decode_sweep_encoding_1(sweep, sweep_len)
                }
            }
            2 => {
                if sweep_idx == 0 {
                    decode_sweep_encoding_1(sweep, sweep_len)
                } else {
                    decode_sweep_encoding_2(sweep, prev_spoke, sweep_len)
                }
            }
            3 => decode_sweep_encoding_3(sweep, prev_spoke, sweep_len),
            _ => break,
        };

        if used > sweep.len() { break; }
        sweep = &sweep[used..];

        // Display conversion: wire value >> 2
        let echo: Vec<u8> = spoke_data.iter().map(|&b| b >> 2).collect();

        *prev_spoke = spoke_data;

        out.push(Spoke { angle_ticks: angle, heading_ticks: heading, echo });
    }
}

// --- Command Builder ---

/// Build a Furuno $S command string.
pub fn build_command(id: &str, params: &[i32]) -> String {
    let mut cmd = format!("$S{}", id);
    for p in params {
        cmd.push(',');
        cmd.push_str(&p.to_string());
    }
    cmd.push_str("\r\n");
    cmd
}

/// Build a Furuno $R (read/request) command string.
pub fn build_request(id: &str, params: &[i32]) -> String {
    let mut cmd = format!("$R{}", id);
    for p in params {
        cmd.push(',');
        cmd.push_str(&p.to_string());
    }
    cmd.push_str("\r\n");
    cmd
}

// --- High-level command helpers ---

/// TX state: 1=standby, 2=transmit
pub fn cmd_tx_state(state: u16) -> String {
    build_command("69", &[state as i32, 0, 0, 0, 0, 0])
}

/// Range by wire index (see RANGE_TABLE)
pub fn cmd_range(wire_index: u16) -> String {
    build_command("62", &[wire_index as i32, 0, 0])
}

/// Range by meters (auto-lookup wire index)
pub fn cmd_range_meters(meters: u32) -> String {
    cmd_range(meters_to_wire_index(meters))
}

/// Gain: mode 0=manual, 1=auto. value=0-255 for manual.
pub fn cmd_gain(auto: bool, value: u16) -> String {
    build_command("63", &[auto as i32, value as i32, 0, 80, 0])
}

/// Sea clutter (A/C SEA): status 0=manual, 1=auto. value=0-255.
/// auto_value: auto threshold (used when status=1).
/// sea_mode: 0=Advanced, 1=Coastal.
pub fn cmd_sea_clutter(status: bool, value: u16) -> String {
    cmd_sea_clutter_full(status, value, 50, 0)
}

pub fn cmd_sea_clutter_full(status: bool, manual: u16, auto_val: u16, sea_mode: u16) -> String {
    build_command("64", &[status as i32, manual as i32, auto_val as i32, sea_mode as i32, 0, 0])
}

/// Rain clutter (A/C RAIN): status 0=manual, 1=auto. value=0-255.
/// ftc_mode: 0=OFF, 1=FTC-1, 2=FTC-2.
/// rain_mode: 0=Calm, 1=Moderate, 2=Rough.
pub fn cmd_rain_clutter(status: bool, value: u16) -> String {
    cmd_rain_clutter_full(status, value, 0, 0, 0)
}

pub fn cmd_rain_clutter_full(status: bool, manual: u16, auto_val: u16, ftc_mode: u16, rain_mode: u16) -> String {
    build_command("65", &[status as i32, manual as i32, auto_val as i32, ftc_mode as i32, rain_mode as i32, 0])
}

/// Pulse width: 0=S1, 1=S2, 2=M1, 3=M2, 4=M3, 5=L. range_index=wire index.
pub fn cmd_pulse_width(pulse: u16, range_index: u16) -> String {
    build_command("68", &[pulse as i32, range_index as i32, 0, 0, 0])
}

/// Near STC: 0-4
pub fn cmd_near_stc(value: u16) -> String {
    build_command("85", &[value as i32])
}

/// Middle STC: 0-3
pub fn cmd_middle_stc(value: u16) -> String {
    build_command("86", &[value as i32])
}

/// Far STC: 0-2
pub fn cmd_far_stc(value: u16) -> String {
    build_command("87", &[value as i32])
}

/// Main bang suppression: 0-255
pub fn cmd_main_bang(value: u16) -> String {
    build_command("83", &[value as i32, 0])
}

/// Antenna height in meters
pub fn cmd_antenna_height(meters: u16) -> String {
    build_command("84", &[0, meters as i32, 0])
}

/// Scan speed: 0=normal (24RPM), 1=HI, 2=auto
pub fn cmd_scan_speed(mode: u16) -> String {
    build_command("89", &[mode as i32, 0])
}

/// Bearing resolution (RezBoost): 0=off, 1=low, 2=medium, 3=high
pub fn cmd_rezboost(level: u16) -> String {
    build_command("EE", &[level as i32, 0])
}

/// Interference rejection: 0=off, 1-2=on (2 is standard "on" per protocol)
pub fn cmd_interference_rejection(enabled: bool) -> String {
    let val = if enabled { 2 } else { 0 };
    build_command("67", &[0, 0, val, 0])
}

/// Noise reduction: 0=off, 1=on
pub fn cmd_noise_reduction(enabled: bool) -> String {
    let val = if enabled { 1 } else { 0 };
    build_command("67", &[0, 3, val, 0])
}

/// No-transmit sectors (blind zones).
/// Angles in degrees. Width is computed from start/end.
pub fn cmd_no_transmit_sectors(
    sector1_start: u16, sector1_end: u16,
    sector2_start: u16, sector2_end: u16, sector2_enabled: bool,
) -> String {
    let s1_width = if sector1_end >= sector1_start {
        sector1_end - sector1_start
    } else {
        360 + sector1_end - sector1_start
    };
    let s2_width = if sector2_end >= sector2_start {
        sector2_end - sector2_start
    } else {
        360 + sector2_end - sector2_start
    };
    let s2_enable = if sector2_enabled && s2_width > 0 { 1 } else { 0 };
    build_command("77", &[
        s2_enable as i32,
        sector1_start as i32, s1_width as i32,
        sector2_start as i32, s2_width as i32,
    ])
}

/// Bird mode: 0=off, 1=low, 2=medium, 3=high
pub fn cmd_bird_mode(level: u16) -> String {
    build_command("ED", &[level as i32, 0])
}

/// Doppler / Target analyzer: 0=off, 1=target, 2=rain
pub fn cmd_doppler(mode: u16) -> String {
    let (enabled, wire_mode) = match mode {
        0 => (0, 0),
        1 => (1, 0),
        2 => (1, 1),
        _ => (0, 0),
    };
    build_command("EF", &[enabled, wire_mode, 0])
}

/// Antenna revolution: speed 0=LO, 1=HI, 2=AUTO; hi_rpm 0=off, 1=on
pub fn cmd_antenna_revolution(speed: u16, hi_rpm: bool) -> String {
    build_command("89", &[speed as i32, hi_rpm as i32])
}

// --- Query (Read) Commands ---
// These send $R requests to read current radar state.
// The radar responds with $N messages containing the current values.

/// Query current range setting
pub fn query_range() -> String {
    build_request("62", &[0, 0, 0, 0])
}

/// Query current gain setting
pub fn query_gain() -> String {
    build_request("63", &[0, 0, 0, 0, 0])
}

/// Query current sea clutter setting
pub fn query_sea_clutter() -> String {
    build_request("64", &[0, 0, 0, 0, 0, 0])
}

/// Query current rain clutter setting
pub fn query_rain_clutter() -> String {
    build_request("65", &[0, 0, 0, 0, 0, 0])
}

/// Query current pulse width
pub fn query_pulse_width() -> String {
    build_request("68", &[0, 0, 0, 0, 0])
}

/// Query TX state
pub fn query_tx_state() -> String {
    build_request("69", &[0, 0, 0, 0, 0, 0])
}

/// Query antenna revolution settings
pub fn query_antenna_revolution() -> String {
    build_request("89", &[0, 0])
}

/// Query bearing resolution (RezBoost)
pub fn query_rezboost() -> String {
    build_request("EE", &[0, 0])
}

/// Query STC values (near, middle, far)
pub fn query_near_stc() -> String { build_request("85", &[0]) }
pub fn query_middle_stc() -> String { build_request("86", &[0]) }
pub fn query_far_stc() -> String { build_request("87", &[0]) }

/// Query main bang suppression
pub fn query_main_bang() -> String { build_request("83", &[0, 0]) }

/// Query firmware version
pub fn query_firmware() -> String {
    "$R96,,,,,,,\r\n".to_string()
}

/// Query all init registers (returns the full set of $R commands)
pub fn init_register_reads() -> Vec<String> {
    include_str!("init_commands.txt")
        .lines()
        .filter(|l| {
            let l = l.trim();
            !l.is_empty() && !l.starts_with('#')
        })
        .map(|l| format!("{}\r\n", l.trim()))
        .collect()
}

// --- Software Echo Interpolation ---

/// Interpolate spokes to fill angular gaps.
/// target_count: desired number of spokes per rotation (e.g. 2048, 4096, 8192).
pub fn interpolate_spokes(spokes: &[Spoke], target_count: usize) -> Vec<Spoke> {
    if spokes.is_empty() || target_count == 0 { return Vec::new(); }

    // Sort by angle
    let mut sorted: Vec<&Spoke> = spokes.iter().collect();
    sorted.sort_by_key(|s| s.angle_ticks);

    let mut result = Vec::with_capacity(target_count);
    let step = FURUNO_SPOKES as f64 / target_count as f64;

    for i in 0..target_count {
        let target_angle = (i as f64 * step) as u16;

        // Find the two nearest source spokes
        let pos = sorted.partition_point(|s| s.angle_ticks < target_angle);
        let idx_after = if pos < sorted.len() { pos } else { 0 };
        let idx_before = if pos > 0 { pos - 1 } else { sorted.len() - 1 };

        let before = sorted[idx_before];
        let after = sorted[idx_after];

        // Angular distance for weighting
        let d_before = angle_diff(target_angle, before.angle_ticks);
        let d_after = angle_diff(after.angle_ticks, target_angle);
        let total = d_before + d_after;

        if total == 0 || d_before == 0 {
            // Exact match or very close — use the before spoke
            result.push(Spoke {
                angle_ticks: target_angle,
                heading_ticks: before.heading_ticks,
                echo: before.echo.clone(),
            });
        } else if d_after == 0 {
            result.push(Spoke {
                angle_ticks: target_angle,
                heading_ticks: after.heading_ticks,
                echo: after.echo.clone(),
            });
        } else {
            // Linear interpolation between the two spokes
            let w_after = d_before as f64 / total as f64;
            let w_before = 1.0 - w_after;
            let num_bins = before.echo.len().min(after.echo.len());
            let mut echo = Vec::with_capacity(num_bins);
            for bin in 0..num_bins {
                let v = before.echo[bin] as f64 * w_before + after.echo[bin] as f64 * w_after;
                echo.push(v.round() as u8);
            }
            result.push(Spoke {
                angle_ticks: target_angle,
                heading_ticks: before.heading_ticks,
                echo,
            });
        }
    }

    result
}

fn angle_diff(a: u16, b: u16) -> u16 {
    if a >= b {
        a - b
    } else {
        FURUNO_SPOKES as u16 - b + a
    }
}

/// Sweep total modes matching the Furuno library convention.
pub fn interpolation_target_count(sweep_total_mode: u16) -> usize {
    match sweep_total_mode {
        0 => 2048,
        1 => 4096,
        2 => 8192,
        3 => 1024,
        4 => 720,
        _ => 0, // 5 = OFF
    }
}

// --- Beacon / Discovery ---

pub struct RadarInfo {
    pub addr: Ipv4Addr,
    pub name: String,
    pub model: String,
    pub serial: String,
}

impl fmt::Display for RadarInfo {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{} ({}) @ {} [SN: {}]", self.name, self.model, self.addr, self.serial)
    }
}

/// Parse a beacon report packet (>=19 bytes) into a partial RadarInfo.
pub fn parse_beacon_report(data: &[u8], src_addr: Ipv4Addr) -> Option<RadarInfo> {
    if data.len() < 24 { return None; }
    if data[16] != b'R' { return None; } // name must start with 'R'

    let name_end = data[16..24].iter().position(|&b| b == 0).unwrap_or(8);
    let name = String::from_utf8_lossy(&data[16..16 + name_end]).to_string();

    Some(RadarInfo {
        addr: src_addr,
        name,
        model: String::new(),
        serial: String::new(),
    })
}

/// Parse a model report packet (170 bytes) to extract model + serial.
pub fn parse_model_report(data: &[u8]) -> Option<(String, String)> {
    if data.len() < 152 { return None; }

    let model_end = data[24..56].iter().position(|&b| b == 0).unwrap_or(32);
    let model = String::from_utf8_lossy(&data[24..24 + model_end]).to_string();

    let serial_end = data[120..152].iter().position(|&b| b == 0).unwrap_or(32);
    let serial = String::from_utf8_lossy(&data[120..120 + serial_end]).to_string();

    Some((model, serial))
}

// --- Control Response Parser ---

/// Parse a $N response line and return (command_id, values).
/// E.g. "$N62,0,0,0" -> ("62", vec!["0","0","0"])
pub fn parse_response(line: &str) -> Option<(&str, Vec<&str>)> {
    let line = line.trim();
    if !line.starts_with("$N") { return None; }
    let body = &line[2..];
    let parts: Vec<&str> = body.split(',').collect();
    if parts.is_empty() { return None; }
    Some((parts[0], parts[1..].to_vec()))
}

// --- Calibration Pattern ---

/// A single step in a calibration pattern sequence.
/// Each step defines a complete radar configuration and how many
/// full antenna rotations to hold at that configuration.
#[derive(Clone)]
pub struct ControlStep {
    pub range: u16,
    pub gain: u16,
    pub pulse_length: u16,     // 0=S1, 1=S2, 2=M1, 3=M2, 4=M3, 5=L
    pub rez_boost: u16,        // 0=off, 1=low, 2=med, 3=high
    pub rotation_speed: u16,   // 0=LO, 1=HI, 2=AUTO
    pub interpolation: u16,    // 0=2048, 1=4096, 2=8192, 3=1024, 4=720, 5=OFF
    pub repeat: u16,           // number of rotations at this setting
}

impl ControlStep {
    /// Generate the set of $S commands needed to apply this step.
    /// Only emits commands for parameters that differ from `prev`.
    pub fn commands_vs(&self, prev: Option<&ControlStep>) -> Vec<String> {
        let mut cmds = Vec::new();
        let changed = |getter: fn(&ControlStep) -> u16| -> bool {
            match prev {
                Some(p) => getter(self) != getter(p),
                None => true,
            }
        };

        if changed(|s| s.range) {
            cmds.push(cmd_range(self.range));
        }
        if changed(|s| s.gain) {
            cmds.push(cmd_gain(false, self.gain));
        }
        if changed(|s| s.pulse_length) {
            cmds.push(cmd_pulse_width(self.pulse_length, self.range));
        }
        if changed(|s| s.rez_boost) {
            cmds.push(cmd_rezboost(self.rez_boost));
        }
        if changed(|s| s.rotation_speed) {
            cmds.push(cmd_antenna_revolution(self.rotation_speed, self.rotation_speed == 1));
        }
        cmds
    }
}

/// Generate a calibration pattern that sweeps through all combinations.
pub fn generate_pattern(
    ranges: &[u16],
    pulse_lengths: &[u16],
    rez_boosts: &[u16],
    rotation_speeds: &[u16],
    interpolations: &[u16],
    gain_start: u16,
    gain_end: u16,
    repeat_count: u16,
) -> Vec<ControlStep> {
    let mut pattern = Vec::new();
    for &range in ranges {
        for &pulse in pulse_lengths {
            for &rez in rez_boosts {
                for &rot in rotation_speeds {
                    for &interp in interpolations {
                        for gain in gain_start..=gain_end {
                            pattern.push(ControlStep {
                                range,
                                gain,
                                pulse_length: pulse,
                                rez_boost: rez,
                                rotation_speed: rot,
                                interpolation: interp,
                                repeat: repeat_count,
                            });
                        }
                    }
                }
            }
        }
    }
    pattern
}
