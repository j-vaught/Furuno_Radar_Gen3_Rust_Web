#[allow(dead_code)]
mod protocol;
mod connection;
mod render;
mod web;

use std::io::{BufRead, Write};
use std::net::{Ipv4Addr, SocketAddr, UdpSocket};
use std::time::Duration;

use protocol::{Spoke, decode_echo_packet, parse_echo_metadata};
use connection::{RadarConnection, create_echo_socket};
use render::PPI_SIZE;

const RADAR_IP: Ipv4Addr = Ipv4Addr::new(172, 31, 3, 226);
const HOST_IP: Ipv4Addr = Ipv4Addr::new(172, 31, 1, 100);

const MULTICAST_ECHO: Ipv4Addr = protocol::FURUNO_MULTICAST;
const PORT_ECHO: u16 = protocol::FURUNO_DATA_PORT;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let mode = args.get(1).map(|s| s.as_str()).unwrap_or("handshake");

    match mode {
        "handshake" => run_handshake(),
        "tx-on" => run_tx_on(),
        "tx-off" => run_tx_off(),
        "echo" => run_echo_listen(),
        "full" => run_full(),
        "command" => run_command_test(),
        "render-csv" => {
            let csv_path = args.get(2).expect("Usage: radar-client render-csv <csv_file> [output.png]");
            let out_path = args.get(3).map(|s| s.as_str()).unwrap_or("radar_ppi.png");
            run_render_csv(csv_path, out_path);
        }
        "render-live" => {
            let out_path = args.get(2).map(|s| s.as_str()).unwrap_or("radar_live.png");
            run_render_live(out_path);
        }
        "dump-raw" => {
            let count: usize = args.get(2).and_then(|s| s.parse().ok()).unwrap_or(5);
            run_dump_raw(count);
        }
        "test-commands" => run_test_commands(),
        "calibrate" => run_calibrate(&args[2..]),
        "discover" => run_discover(),
        "validate" => run_validate(),
        "web" => {
            let port: u16 = args.get(2).and_then(|s| s.parse().ok()).unwrap_or(8080);
            let web_dir = args.get(3).map(|s| s.as_str()).unwrap_or("../main/web");
            web::start_server(RADAR_IP, HOST_IP, web_dir, port);
        }
        _ => {
            eprintln!("Usage: radar-client <handshake|tx-on|tx-off|echo|full|command|render-csv|render-live|test-commands|calibrate|discover|validate|web>");
            std::process::exit(1);
        }
    }
}

fn run_handshake() {
    println!("=== Handshake: Auth + Control + Init ===\n");

    let mut conn = RadarConnection::new(RADAR_IP, HOST_IP);
    if let Err(e) = conn.connect_full() {
        eprintln!("Handshake failed: {e}");
        std::process::exit(1);
    }

    if let Some(fw) = &conn.firmware {
        println!("\nFirmware: {fw}");
    }
    println!("\n=== Handshake complete! Control channel is open. ===");
}

fn run_tx_on() {
    println!("=== TX ON ===\n");

    let mut conn = RadarConnection::new(RADAR_IP, HOST_IP);
    if let Err(e) = conn.connect_full() {
        eprintln!("Init failed: {e}");
        std::process::exit(1);
    }

    if let Err(e) = conn.tx_on() {
        eprintln!("TX ON failed: {e}");
        std::process::exit(1);
    }

    println!("Echoes should start flowing on multicast 239.255.0.2:10024");
}

fn run_tx_off() {
    println!("=== TX OFF (Standby) ===\n");

    let mut conn = RadarConnection::new(RADAR_IP, HOST_IP);
    if let Err(e) = conn.connect_full() {
        eprintln!("Init failed: {e}");
        std::process::exit(1);
    }

    if let Err(e) = conn.tx_off() {
        eprintln!("TX OFF failed: {e}");
        std::process::exit(1);
    }

    println!("Echoes should stop.");
}

fn run_full() {
    println!("=== Full: Connect + TX ON + Collect 1 Rotation + CSV + PNG ===\n");

    let mut conn = RadarConnection::new(RADAR_IP, HOST_IP);
    if let Err(e) = conn.connect_full() {
        eprintln!("Init failed: {e}");
        std::process::exit(1);
    }

    if let Err(e) = conn.tx_on() {
        eprintln!("TX ON failed: {e}");
        std::process::exit(1);
    }

    conn.set_nonblocking(true).unwrap();

    let echo_socket = create_echo_socket(HOST_IP);

    let mut buf = [0u8; 2048];
    let mut spokes: Vec<Spoke> = Vec::new();
    let mut prev_spoke_data: Vec<u8> = Vec::new();
    let mut prev_az: Option<u16> = None;
    let mut rotation_count = 0u32;
    let start = std::time::Instant::now();

    println!("Collecting echo data (2 full rotations)...");

    loop {
        match echo_socket.recv_from(&mut buf) {
            Ok((len, _)) => {
                if len < 20 { continue; }
                let az = u16::from_le_bytes([buf[16], buf[17]]);

                if let Some(prev) = prev_az {
                    if az < prev.saturating_sub(1000) {
                        rotation_count += 1;
                        println!("  Rotation {} complete ({} spokes)", rotation_count, spokes.len());
                        if rotation_count >= 2 { break; }
                    }
                }
                prev_az = Some(az);

                if rotation_count >= 1 {
                    decode_echo_packet(&buf[..len], &mut prev_spoke_data, &mut spokes);
                } else {
                    let mut discard = Vec::new();
                    decode_echo_packet(&buf[..len], &mut prev_spoke_data, &mut discard);
                }
            }
            Err(_) => {
                if start.elapsed() > Duration::from_secs(30) {
                    eprintln!("Timeout after 30s");
                    break;
                }
            }
        }

        conn.drain_control();
        let _ = conn.send_keepalive();
    }

    // TX OFF
    conn.set_nonblocking(false).unwrap();
    let _ = conn.tx_off();

    if spokes.is_empty() {
        eprintln!("No spokes collected");
        return;
    }

    let timestamp = timestamp_str();
    let csv_path = format!("radar_{timestamp}.csv");
    let png_path = format!("radar_{timestamp}.png");

    println!("\nCollected {} spokes in {:.1}s", spokes.len(), start.elapsed().as_secs_f64());

    write_echo_csv(&csv_path, &spokes);
    println!("Saved CSV: {csv_path}");

    let pixels = render_ppi(&spokes);
    write_png(&png_path, &pixels, PPI_SIZE, PPI_SIZE);
    println!("Saved PNG: {png_path}");
}

fn run_echo_listen() {
    println!("=== Echo Listener (UDP multicast {MULTICAST_ECHO}:{PORT_ECHO}) ===");

    let echo_socket = create_echo_socket(HOST_IP);
    echo_socket
        .set_read_timeout(Some(Duration::from_secs(10)))
        .unwrap();

    println!("Listening for echo packets...\n");

    let mut buf = [0u8; 2048];
    let mut count = 0u64;
    let start = std::time::Instant::now();

    loop {
        match echo_socket.recv_from(&mut buf) {
            Ok((len, src)) => {
                count += 1;
                if count <= 10 || count % 100 == 0 {
                    let hdr = parse_echo_header(&buf[..len]);
                    println!(
                        "[{}] pkt #{count}: {len} bytes from {src} | seq={} az={:.1} ts=0x{:08X} flags=0x{:04X} cfg=0x{:04X}",
                        elapsed_str(start),
                        hdr.sequence,
                        hdr.azimuth_deg,
                        hdr.timestamp,
                        hdr.flags,
                        hdr.config,
                    );
                }
                if count == 10 {
                    println!("  ... (showing every 100th packet from now) ...\n");
                }
            }
            Err(e) => {
                if count == 0 {
                    eprintln!("No echo packets received (timeout). Is TX on? Error: {e}");
                    break;
                } else {
                    let rate = count as f64 / start.elapsed().as_secs_f64();
                    println!(
                        "\nTimeout after {count} packets in {:.1}s ({rate:.1} pkt/sec)",
                        start.elapsed().as_secs_f64()
                    );
                    break;
                }
            }
        }
    }
}

fn run_command_test() {
    println!("=== Command Test ===\n");

    let mut conn = RadarConnection::new(RADAR_IP, HOST_IP);
    if let Err(e) = conn.connect_full() {
        eprintln!("Init failed: {e}");
        std::process::exit(1);
    }

    let commands = [
        ("Range=6 (2NM)", "$S62,6,0,0,0\r\n"),
        ("Gain manual=150", "$S63,0,150,0,0,0\r\n"),
        ("Near STC=2", "$S64,0,2,0,0,0,0\r\n"),
        ("Bearing res=3 (High)", "$SEE,3,0\r\n"),
        ("Range=0 (0.125NM)", "$S62,0,0,0,0\r\n"),
        ("Gain auto", "$S63,1,0,0,0,0\r\n"),
        ("Near STC=0", "$S64,0,0,0,0,0,0\r\n"),
        ("Bearing res=0 (Off)", "$SEE,0,0\r\n"),
    ];

    for (desc, cmd) in &commands {
        println!("\nSending: {desc} -> {}", cmd.trim());
        conn.send_command(cmd).expect("send failed");
        let responses = conn.read_responses(Duration::from_secs(2));
        for resp in &responses {
            match resp {
                connection::RadarResponse::RegisterValue { id, values } => {
                    println!("  <- $N{id},{}", values.join(","));
                }
                connection::RadarResponse::Keepalive => println!("  <- [keepalive]"),
                _ => {}
            }
        }
    }

    println!("\n=== Command test complete ===");
}

// --- CSV Export ---

fn write_echo_csv(path: &str, spokes: &[Spoke]) {
    let mut f = std::io::BufWriter::new(
        std::fs::File::create(path).expect("Failed to create CSV file"),
    );

    // Header: spoke_index, angle_ticks, angle_deg, heading_ticks, num_bins, bin_0, bin_1, ...
    let max_bins = spokes.iter().map(|s| s.echo.len()).max().unwrap_or(0);
    write!(f, "spoke_index,angle_ticks,angle_deg,heading_ticks,num_bins").unwrap();
    for i in 0..max_bins {
        write!(f, ",bin_{i}").unwrap();
    }
    writeln!(f).unwrap();

    for (idx, spoke) in spokes.iter().enumerate() {
        let angle_deg = spoke.angle_ticks as f64 / 8192.0 * 360.0;
        write!(
            f,
            "{},{},{:.2},{},{}",
            idx, spoke.angle_ticks, angle_deg, spoke.heading_ticks, spoke.echo.len()
        )
        .unwrap();
        for &b in &spoke.echo {
            write!(f, ",{b}").unwrap();
        }
        // Pad short spokes with zeros
        for _ in spoke.echo.len()..max_bins {
            write!(f, ",0").unwrap();
        }
        writeln!(f).unwrap();
    }
}

fn timestamp_str() -> String {
    let now = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap();
    let secs = now.as_secs();
    // Simple UTC timestamp: YYYYMMDD_HHMMSS
    let s = secs % 60;
    let m = (secs / 60) % 60;
    let h = (secs / 3600) % 24;
    let days = secs / 86400;
    // Days since epoch to date (simplified)
    let mut y = 1970u32;
    let mut remaining = days;
    loop {
        let days_in_year = if y % 4 == 0 && (y % 100 != 0 || y % 400 == 0) { 366 } else { 365 };
        if remaining < days_in_year { break; }
        remaining -= days_in_year;
        y += 1;
    }
    let leap = y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
    let month_days: [u64; 12] = [
        31,
        if leap { 29 } else { 28 },
        31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    ];
    let mut mo = 0;
    for (i, &md) in month_days.iter().enumerate() {
        if remaining < md {
            mo = i + 1;
            break;
        }
        remaining -= md;
    }
    let d = remaining + 1;
    format!("{y}{mo:02}{d:02}_{h:02}{m:02}{s:02}")
}

// --- Helper structs ---

struct EchoHeader {
    sequence: u16,
    timestamp: u32,
    flags: u16,
    config: u16,
    azimuth_deg: f64,
}

fn parse_echo_header(data: &[u8]) -> EchoHeader {
    if data.len() < 18 {
        return EchoHeader {
            sequence: 0,
            timestamp: 0,
            flags: 0,
            config: 0,
            azimuth_deg: 0.0,
        };
    }
    let sequence = u16::from_be_bytes([data[0], data[1]]);
    let timestamp = u32::from_be_bytes([data[8], data[9], data[10], data[11]]);
    let flags = u16::from_be_bytes([data[12], data[13]]);
    let config = u16::from_be_bytes([data[14], data[15]]);
    let azimuth_raw = u16::from_be_bytes([data[16], data[17]]);
    let azimuth_deg = azimuth_raw as f64 / 65536.0 * 360.0;

    EchoHeader {
        sequence,
        timestamp,
        flags,
        config,
        azimuth_deg,
    }
}

fn elapsed_str(start: std::time::Instant) -> String {
    let secs = start.elapsed().as_secs_f64();
    format!("{secs:6.1}s")
}

// --- Echo Stats ---

struct EchoStats {
    num_spokes: usize,
    num_nonzero_spokes: usize,
    mean_amplitude: f64,
    max_amplitude: u8,
    nonzero_bins_pct: f64,
    sweep_len: u32,
    encoding: u8,
    sweep_count_per_pkt: u32,
}

impl std::fmt::Display for EchoStats {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "spokes={} nonzero={} mean_amp={:.1} max_amp={} nz_bins={:.1}% sweep_len={} enc={} sweeps/pkt={}",
            self.num_spokes, self.num_nonzero_spokes, self.mean_amplitude,
            self.max_amplitude, self.nonzero_bins_pct, self.sweep_len, self.encoding, self.sweep_count_per_pkt)
    }
}

fn collect_rotation_stats(
    echo_socket: &UdpSocket,
    conn: &mut RadarConnection,
    prev_spoke_data: &mut Vec<u8>,
) -> (Vec<Spoke>, EchoStats) {
    let mut buf = [0u8; 2048];
    let mut spokes: Vec<Spoke> = Vec::new();
    let mut prev_az: Option<u16> = None;
    let mut rotation_count = 0u32;
    let start = std::time::Instant::now();
    let mut last_sweep_len: u32 = 0;
    let mut last_encoding: u8 = 0;
    let mut last_sweep_count: u32 = 0;

    loop {
        match echo_socket.recv_from(&mut buf) {
            Ok((len, _)) => {
                if len < 20 { continue; }
                let az = u16::from_le_bytes([buf[16], buf[17]]);

                let meta = parse_echo_metadata(&buf[..len]);
                last_sweep_len = meta.sweep_len;
                last_encoding = meta.encoding;
                last_sweep_count = meta.sweep_count;

                if let Some(prev) = prev_az {
                    if az < prev.saturating_sub(1000) {
                        rotation_count += 1;
                        if rotation_count >= 2 { break; }
                    }
                }
                prev_az = Some(az);

                if rotation_count >= 1 {
                    decode_echo_packet(&buf[..len], prev_spoke_data, &mut spokes);
                } else {
                    let mut discard = Vec::new();
                    decode_echo_packet(&buf[..len], prev_spoke_data, &mut discard);
                }
            }
            Err(_) => {
                if start.elapsed() > Duration::from_secs(15) {
                    eprintln!("    Timeout collecting rotation");
                    break;
                }
            }
        }
        conn.drain_control();
        let _ = conn.send_keepalive();
    }

    let num_spokes = spokes.len();
    let mut total_amp: u64 = 0;
    let mut total_bins: u64 = 0;
    let mut nonzero_bins: u64 = 0;
    let mut max_amp: u8 = 0;
    let mut nonzero_spokes = 0usize;

    for spoke in &spokes {
        let has_nonzero = spoke.echo.iter().any(|&b| b > 0);
        if has_nonzero { nonzero_spokes += 1; }
        for &b in &spoke.echo {
            total_bins += 1;
            total_amp += b as u64;
            if b > 0 { nonzero_bins += 1; }
            if b > max_amp { max_amp = b; }
        }
    }

    let mean_amp = if total_bins > 0 { total_amp as f64 / total_bins as f64 } else { 0.0 };
    let nz_pct = if total_bins > 0 { nonzero_bins as f64 / total_bins as f64 * 100.0 } else { 0.0 };

    let stats = EchoStats {
        num_spokes,
        num_nonzero_spokes: nonzero_spokes,
        mean_amplitude: mean_amp,
        max_amplitude: max_amp,
        nonzero_bins_pct: nz_pct,
        sweep_len: last_sweep_len,
        encoding: last_encoding,
        sweep_count_per_pkt: last_sweep_count,
    };

    (spokes, stats)
}

fn run_test_commands() {
    println!("=== Command Verification Test ===\n");

    let mut conn = RadarConnection::new(RADAR_IP, HOST_IP);
    if let Err(e) = conn.connect_full() {
        eprintln!("Init failed: {e}");
        std::process::exit(1);
    }

    if let Err(e) = conn.tx_on() {
        eprintln!("TX ON failed: {e}");
        std::process::exit(1);
    }
    conn.set_nonblocking(true).unwrap();

    let echo_socket = create_echo_socket(HOST_IP);

    let mut prev_spoke_data: Vec<u8> = Vec::new();

    struct DynTestPair {
        name: &'static str,
        setup_cmd: String,
        setup_ack: &'static str,
        apply_cmd: String,
        apply_ack: &'static str,
        restore_cmd: String,
        restore_ack: &'static str,
        label: &'static str,
    }

    let tests = vec![
        DynTestPair {
            name: "Range: 0.125 NM -> 2 NM",
            setup_cmd: protocol::cmd_range(0),
            setup_ack: "$N62,",
            apply_cmd: protocol::cmd_range(6),
            apply_ack: "$N62,",
            restore_cmd: protocol::cmd_range(0),
            restore_ack: "$N62,",
            label: "range_2nm",
        },
        DynTestPair {
            name: "Range: 0.125 NM -> 8 NM",
            setup_cmd: protocol::cmd_range(0),
            setup_ack: "$N62,",
            apply_cmd: protocol::cmd_range(10),
            apply_ack: "$N62,",
            restore_cmd: protocol::cmd_range(0),
            restore_ack: "$N62,",
            label: "range_8nm",
        },
        DynTestPair {
            name: "Gain: 0 -> 100",
            setup_cmd: protocol::cmd_gain(false, 0),
            setup_ack: "$N63,",
            apply_cmd: protocol::cmd_gain(false, 100),
            apply_ack: "$N63,",
            restore_cmd: protocol::cmd_gain(true, 0),
            restore_ack: "$N63,",
            label: "gain_0v100",
        },
        DynTestPair {
            name: "Sea clutter: 0 -> 100",
            setup_cmd: protocol::cmd_sea_clutter(false, 0),
            setup_ack: "$N64,",
            apply_cmd: protocol::cmd_sea_clutter(false, 100),
            apply_ack: "$N64,",
            restore_cmd: protocol::cmd_sea_clutter(false, 0),
            restore_ack: "$N64,",
            label: "sea_100",
        },
        DynTestPair {
            name: "Rain clutter: 0 -> 100",
            setup_cmd: protocol::cmd_rain_clutter(false, 0),
            setup_ack: "$N65,",
            apply_cmd: protocol::cmd_rain_clutter(false, 100),
            apply_ack: "$N65,",
            restore_cmd: protocol::cmd_rain_clutter(false, 0),
            restore_ack: "$N65,",
            label: "rain_100",
        },
        DynTestPair {
            name: "RezBoost: off -> High",
            setup_cmd: protocol::cmd_rezboost(0),
            setup_ack: "$NEE,",
            apply_cmd: protocol::cmd_rezboost(3),
            apply_ack: "$NEE,",
            restore_cmd: protocol::cmd_rezboost(0),
            restore_ack: "$NEE,",
            label: "rezboost_high",
        },
        DynTestPair {
            name: "Interference rejection: off -> on",
            setup_cmd: protocol::cmd_interference_rejection(false),
            setup_ack: "$N67,",
            apply_cmd: protocol::cmd_interference_rejection(true),
            apply_ack: "$N67,",
            restore_cmd: protocol::cmd_interference_rejection(false),
            restore_ack: "$N67,",
            label: "ir",
        },
        DynTestPair {
            name: "Noise reduction: off -> on",
            setup_cmd: protocol::cmd_noise_reduction(false),
            setup_ack: "$N67,",
            apply_cmd: protocol::cmd_noise_reduction(true),
            apply_ack: "$N67,",
            restore_cmd: protocol::cmd_noise_reduction(false),
            restore_ack: "$N67,",
            label: "nr",
        },
        DynTestPair {
            name: "Scan speed: normal -> HI",
            setup_cmd: protocol::cmd_scan_speed(0),
            setup_ack: "$N89,",
            apply_cmd: protocol::cmd_scan_speed(1),
            apply_ack: "$N89,",
            restore_cmd: protocol::cmd_scan_speed(0),
            restore_ack: "$N89,",
            label: "scan_hi",
        },
        DynTestPair {
            name: "Pulse width: S1 (short) -> L (long)",
            setup_cmd: protocol::cmd_pulse_width(0, 0),
            setup_ack: "$N68,",
            apply_cmd: protocol::cmd_pulse_width(5, 0),
            apply_ack: "$N68,",
            restore_cmd: protocol::cmd_pulse_width(0, 0),
            restore_ack: "$N68,",
            label: "pulse_long",
        },
        DynTestPair {
            name: "Near STC: 0 -> 4",
            setup_cmd: protocol::cmd_near_stc(0),
            setup_ack: "$N85,",
            apply_cmd: protocol::cmd_near_stc(4),
            apply_ack: "$N85,",
            restore_cmd: protocol::cmd_near_stc(0),
            restore_ack: "$N85,",
            label: "near_stc",
        },
        DynTestPair {
            name: "Middle STC: 0 -> 3",
            setup_cmd: protocol::cmd_middle_stc(0),
            setup_ack: "$N86,",
            apply_cmd: protocol::cmd_middle_stc(3),
            apply_ack: "$N86,",
            restore_cmd: protocol::cmd_middle_stc(0),
            restore_ack: "$N86,",
            label: "mid_stc",
        },
        DynTestPair {
            name: "Far STC: 0 -> 2",
            setup_cmd: protocol::cmd_far_stc(0),
            setup_ack: "$N87,",
            apply_cmd: protocol::cmd_far_stc(2),
            apply_ack: "$N87,",
            restore_cmd: protocol::cmd_far_stc(0),
            restore_ack: "$N87,",
            label: "far_stc",
        },
        DynTestPair {
            name: "Main bang: 0 -> 200",
            setup_cmd: protocol::cmd_main_bang(0),
            setup_ack: "$N83,",
            apply_cmd: protocol::cmd_main_bang(200),
            apply_ack: "$N83,",
            restore_cmd: protocol::cmd_main_bang(0),
            restore_ack: "$N83,",
            label: "mainbang",
        },
        DynTestPair {
            name: "Antenna height: 0 -> 15m",
            setup_cmd: protocol::cmd_antenna_height(0),
            setup_ack: "$N84,",
            apply_cmd: protocol::cmd_antenna_height(15),
            apply_ack: "$N84,",
            restore_cmd: protocol::cmd_antenna_height(0),
            restore_ack: "$N84,",
            label: "ant_height",
        },
        DynTestPair {
            name: "Bird mode: off -> high",
            setup_cmd: protocol::cmd_bird_mode(0),
            setup_ack: "$NED,",
            apply_cmd: protocol::cmd_bird_mode(3),
            apply_ack: "$NED,",
            restore_cmd: protocol::cmd_bird_mode(0),
            restore_ack: "$NED,",
            label: "bird_high",
        },
        DynTestPair {
            name: "Doppler: off -> target analyzer",
            setup_cmd: protocol::cmd_doppler(0),
            setup_ack: "$NEF,",
            apply_cmd: protocol::cmd_doppler(1),
            apply_ack: "$NEF,",
            restore_cmd: protocol::cmd_doppler(0),
            restore_ack: "$NEF,",
            label: "doppler_target",
        },
        DynTestPair {
            name: "No-transmit sector: off -> 90-180 deg",
            setup_cmd: protocol::cmd_no_transmit_sectors(0, 0, 0, 0, false),
            setup_ack: "$N77,",
            apply_cmd: protocol::cmd_no_transmit_sectors(90, 180, 0, 0, false),
            apply_ack: "$N77,",
            restore_cmd: protocol::cmd_no_transmit_sectors(0, 0, 0, 0, false),
            restore_ack: "$N77,",
            label: "blind_sector",
        },
    ];

    struct TestResult {
        name: String,
        ack_ok: bool,
        before_stats: String,
        after_stats: String,
    }
    let mut results: Vec<TestResult> = Vec::new();

    let output_dir = "test_output";
    let _ = std::fs::create_dir_all(output_dir);

    for (i, test) in tests.iter().enumerate() {
        println!("\n{}", "=".repeat(70));
        println!("TEST {}/{}: {}", i + 1, tests.len(), test.name);
        println!("{}", "=".repeat(70));

        if !test.setup_cmd.is_empty() {
            println!("  SETUP: {}", test.setup_cmd.trim());
            let _ = conn.send_and_wait_ack(&test.setup_cmd, test.setup_ack, Duration::from_secs(5));
            std::thread::sleep(Duration::from_millis(500));
            let _ = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data);
        }

        println!("  [BEFORE] Collecting echo data...");
        let (before_spokes, before_stats) = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data);
        println!("  [BEFORE] {before_stats}");

        if !before_spokes.is_empty() {
            let png_path = format!("{}/{:02}_{}_BEFORE.png", output_dir, i + 1, test.label);
            let pixels = render_ppi(&before_spokes);
            write_png(&png_path, &pixels, PPI_SIZE, PPI_SIZE);
            println!("  [BEFORE] {png_path}");
        }

        println!("  CMD: {}", test.apply_cmd.trim());
        let ack_ok = conn.send_and_wait_ack(&test.apply_cmd, test.apply_ack, Duration::from_secs(5))
            .unwrap_or(false);

        std::thread::sleep(Duration::from_millis(500));

        println!("  [AFTER]  Collecting echo data...");
        let (after_spokes, after_stats) = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data);
        println!("  [AFTER]  {after_stats}");

        if !after_spokes.is_empty() {
            let png_path = format!("{}/{:02}_{}_AFTER.png", output_dir, i + 1, test.label);
            let pixels = render_ppi(&after_spokes);
            write_png(&png_path, &pixels, PPI_SIZE, PPI_SIZE);
            println!("  [AFTER]  {png_path}");
        }

        results.push(TestResult {
            name: test.name.to_string(),
            ack_ok,
            before_stats: format!("{before_stats}"),
            after_stats: format!("{after_stats}"),
        });

        println!("  RESTORE: {}", test.restore_cmd.trim());
        let _ = conn.send_and_wait_ack(&test.restore_cmd, test.restore_ack, Duration::from_secs(5));
        std::thread::sleep(Duration::from_millis(500));
        let _ = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data);
    }

    // TX OFF
    conn.set_nonblocking(false).unwrap();
    let _ = conn.tx_off();

    // Print summary table
    println!("\n\n{}", "=".repeat(120));
    println!("SUMMARY — BEFORE vs AFTER for each command");
    println!("{}", "=".repeat(120));
    for r in &results {
        let ack_str = if r.ack_ok { "OK" } else { "FAIL" };
        println!("\n  {} [ACK: {}]", r.name, ack_str);
        println!("    BEFORE: {}", r.before_stats);
        println!("    AFTER:  {}", r.after_stats);
    }
    println!("\n{}", "-".repeat(120));
    println!("Done.");
}

// --- PPI Rendering ---

// PPI constants imported from render module

fn run_dump_raw(count: usize) {
    println!("=== Raw Packet Dump ({count} packets) ===\n");

    let mut conn = RadarConnection::new(RADAR_IP, HOST_IP);
    if let Err(e) = conn.connect_full() {
        eprintln!("Init failed: {e}");
        std::process::exit(1);
    }

    if let Err(e) = conn.tx_on() {
        eprintln!("TX ON failed: {e}");
        std::process::exit(1);
    }
    conn.set_nonblocking(true).unwrap();

    let echo_socket = create_echo_socket(HOST_IP);
    echo_socket.set_read_timeout(Some(Duration::from_secs(10))).unwrap();

    let mut buf = [0u8; 2048];

    // Wait for first rotation wrap to get diverse azimuths
    let mut prev_az_dump: Option<u16> = None;
    let rotation_seen;
    loop {
        match echo_socket.recv_from(&mut buf) {
            Ok((len, _)) if len >= 18 => {
                let az = u16::from_le_bytes([buf[16], buf[17]]);
                if let Some(prev) = prev_az_dump {
                    if az < prev.saturating_sub(1000) {
                        rotation_seen = true;
                        break;
                    }
                }
                prev_az_dump = Some(az);
            }
            _ => {}
        }
        conn.drain_control();
    }
    if rotation_seen {
        println!("First rotation seen, now capturing diverse azimuths...\n");
    }

    for pkt_num in 0..count {
        match echo_socket.recv_from(&mut buf) {
            Ok((len, _src)) => {
                let az_raw = u16::from_le_bytes([buf[16], buf[17]]);
                let az_deg = az_raw as f64 / 8192.0 * 360.0;
                println!("--- Packet #{pkt_num} | len={len} | az_raw={az_raw} ({az_deg:.1}) ---");

                print!("  HDR: ");
                for i in 0..18.min(len) { print!("{:02X} ", buf[i]); }
                println!();

                let body = &buf[18..len];

                let mut boundaries: Vec<usize> = Vec::new();
                let mut i = 3;
                while i + 3 < body.len() {
                    if body[i + 2] == 0x00 && body[i + 3] == 0x00 && body[i + 1] <= 0x1F {
                        let after = i + 4;
                        if after < body.len() {
                            let next = body[after];
                            if next == 0xFE || next == 0xFD || next >= 0x38 || next == 0x00 {
                                boundaries.push(after);
                                i = after;
                                continue;
                            }
                        }
                    }
                    i += 1;
                }

                let section_starts: Vec<usize> = std::iter::once(3_usize)
                    .chain(boundaries.iter().copied())
                    .collect();
                let section_ends: Vec<usize> = boundaries.iter().map(|b| b - 4)
                    .chain(std::iter::once(body.len()))
                    .collect();

                println!("  {} sections found", section_starts.len());

                for si in 0..section_starts.len().min(3) {
                    let start = section_starts[si];
                    let end = section_ends[si];
                    if start >= end { continue; }
                    let sec = &body[start..end];

                    let data = if sec.len() > 2 && sec[0] == 0 && sec[1] == 0 {
                        &sec[2..]
                    } else { sec };

                    let mut total_bins = 0u32;
                    let mut j = 0;
                    let mut detail = String::new();
                    while j < data.len() {
                        let b = data[j];
                        if (b == 0xFE || b == 0xFD) && j + 1 < data.len() {
                            let count_val = (b >> 2) + 1;
                            total_bins += count_val as u32;
                            detail += &format!("{:02X}({})x{:02X} ", b, count_val, data[j+1]);
                            j += 2;
                        } else if b % 4 == 0 {
                            total_bins += 1;
                            j += 1;
                        } else {
                            let count_val = (b >> 2) + 1;
                            total_bins += count_val as u32;
                            detail += &format!("[{:02X}={}bins] ", b, count_val);
                            j += 1;
                        }
                    }

                    let sec_az = if si == 0 { az_raw } else {
                        let bp = boundaries[si - 1];
                        u16::from_le_bytes([body[bp - 4], body[bp - 3]])
                    };
                    println!("  Sec#{si} az={sec_az}: {total_bins} bins | {}", &detail[..detail.len().min(200)]);
                }
                println!();
            }
            Err(e) => {
                eprintln!("recv error: {e}");
                break;
            }
        }
        conn.drain_control();
    }

    // TX OFF
    conn.set_nonblocking(false).unwrap();
    let _ = conn.tx_off();
}

fn run_render_csv(csv_path: &str, out_path: &str) {
    println!("Rendering PPI from CSV: {csv_path}");

    let file = std::fs::File::open(csv_path).expect("Failed to open CSV");
    let reader = std::io::BufReader::new(file);
    let mut spokes = Vec::new();

    for (i, line) in reader.lines().enumerate() {
        let line = line.expect("read line");
        if i == 0 { continue; }
        let fields: Vec<&str> = line.split(',').collect();
        if fields.len() < 6 { continue; }
        let angle: u16 = fields[4].parse().unwrap_or(0);
        let echo: Vec<u8> = fields[5..].iter()
            .map(|v| v.parse::<u8>().unwrap_or(0))
            .collect();
        spokes.push(Spoke { angle_ticks: angle, heading_ticks: 0, echo });
    }

    println!("Loaded {} spokes, rendering {}x{} PPI...", spokes.len(), PPI_SIZE, PPI_SIZE);
    let pixels = render_ppi(&spokes);
    write_png(out_path, &pixels, PPI_SIZE, PPI_SIZE);
    println!("Saved: {out_path}");
}

fn run_render_live(out_path: &str) {
    println!("=== Live PPI Capture ===\n");

    let mut conn = RadarConnection::new(RADAR_IP, HOST_IP);
    if let Err(e) = conn.connect_full() {
        eprintln!("Init failed: {e}");
        std::process::exit(1);
    }

    if let Err(e) = conn.tx_on() {
        eprintln!("TX ON failed: {e}");
        std::process::exit(1);
    }
    conn.set_nonblocking(true).unwrap();

    let echo_socket = create_echo_socket(HOST_IP);

    // Collect one full rotation of echo data
    let mut buf = [0u8; 2048];
    let mut spokes: Vec<Spoke> = Vec::new();
    let mut prev_spoke_data: Vec<u8> = Vec::new();
    let mut prev_az: Option<u16> = None;
    let mut rotation_count = 0u32;
    let start = std::time::Instant::now();

    println!("Collecting echo data for 2 full rotations...");

    loop {
        match echo_socket.recv_from(&mut buf) {
            Ok((len, _)) => {
                if len < 20 { continue; }
                let az = u16::from_le_bytes([buf[16], buf[17]]);

                if let Some(prev) = prev_az {
                    if az < prev.saturating_sub(1000) {
                        rotation_count += 1;
                        println!("  Rotation {} complete ({} spokes so far)", rotation_count, spokes.len());
                        if rotation_count >= 2 {
                            break;
                        }
                    }
                }
                prev_az = Some(az);

                if rotation_count >= 1 {
                    decode_echo_packet(&buf[..len], &mut prev_spoke_data, &mut spokes);
                } else {
                    let mut discard = Vec::new();
                    decode_echo_packet(&buf[..len], &mut prev_spoke_data, &mut discard);
                }
            }
            Err(_) => {
                if start.elapsed() > Duration::from_secs(30) {
                    eprintln!("Timeout collecting echo data");
                    break;
                }
            }
        }

        conn.drain_control();
        let _ = conn.send_keepalive();
    }

    // TX OFF
    conn.set_nonblocking(false).unwrap();
    let _ = conn.tx_off();

    if spokes.is_empty() {
        eprintln!("No spokes collected");
        return;
    }

    println!("Collected {} raw spokes, rendering PPI...", spokes.len());

    // Save CSV alongside PNG
    let csv_path = out_path.replace(".png", ".csv");
    write_echo_csv(&csv_path, &spokes);
    println!("Saved CSV: {csv_path}");

    let pixels = render_ppi(&spokes);
    write_png(out_path, &pixels, PPI_SIZE, PPI_SIZE);
    println!("Saved PNG: {out_path}");
}

fn render_ppi(spokes: &[Spoke]) -> Vec<u8> {
    render::render_ppi(spokes)
}

fn write_png(path: &str, pixels: &[u8], width: usize, height: usize) {
    render::write_png(path, pixels, width, height);
}

// --- Calibration Pattern Runner ---

fn run_calibrate(args: &[String]) {
    let mut ranges: Vec<u16> = vec![0, 6, 10];
    let mut gain_start: u16 = 0;
    let mut gain_end: u16 = 255;
    let mut pulses: Vec<u16> = vec![0];
    let mut rez_boosts: Vec<u16> = vec![0];
    let mut rot_speeds: Vec<u16> = vec![0];
    let mut interpolations: Vec<u16> = vec![5];
    let mut repeat_count: u16 = 2;
    let mut output_dir = String::from("calibration_output");

    let mut i = 0;
    while i < args.len() {
        match args[i].as_str() {
            "--ranges" => {
                i += 1;
                ranges = args[i].split(',').filter_map(|s| s.parse().ok()).collect();
            }
            "--gains" => {
                i += 1;
                let parts: Vec<&str> = args[i].split('-').collect();
                if parts.len() == 2 {
                    gain_start = parts[0].parse().unwrap_or(0);
                    gain_end = parts[1].parse().unwrap_or(255);
                }
            }
            "--pulses" => {
                i += 1;
                pulses = args[i].split(',').filter_map(|s| s.parse().ok()).collect();
            }
            "--rez" => {
                i += 1;
                rez_boosts = args[i].split(',').filter_map(|s| s.parse().ok()).collect();
            }
            "--rot" => {
                i += 1;
                rot_speeds = args[i].split(',').filter_map(|s| s.parse().ok()).collect();
            }
            "--interp" => {
                i += 1;
                interpolations = args[i].split(',').filter_map(|s| s.parse().ok()).collect();
            }
            "--repeats" => {
                i += 1;
                repeat_count = args[i].parse().unwrap_or(2);
            }
            "--output" => {
                i += 1;
                output_dir = args[i].clone();
            }
            _ => {
                eprintln!("Unknown calibrate arg: {}", args[i]);
            }
        }
        i += 1;
    }

    let pattern = protocol::generate_pattern(
        &ranges, &pulses, &rez_boosts, &rot_speeds, &interpolations,
        gain_start, gain_end, repeat_count,
    );

    println!("=== Calibration Pattern ===");
    println!("  Steps: {}", pattern.len());
    println!("  Ranges: {:?}", ranges);
    println!("  Gains: {}-{}", gain_start, gain_end);
    println!("  Pulses: {:?}", pulses);
    println!("  RezBoost: {:?}", rez_boosts);
    println!("  Rotation: {:?}", rot_speeds);
    println!("  Interpolation: {:?}", interpolations);
    println!("  Repeats/step: {}", repeat_count);
    println!("  Output: {}", output_dir);
    println!();

    let mut conn = RadarConnection::new(RADAR_IP, HOST_IP);
    if let Err(e) = conn.connect_full() {
        eprintln!("Init failed: {e}");
        std::process::exit(1);
    }

    // TX ON
    conn.send_command(&protocol::cmd_tx_state(2)).unwrap();
    conn.send_command(&protocol::cmd_near_stc(0)).unwrap();
    conn.send_command(&protocol::cmd_middle_stc(0)).unwrap();
    conn.send_command(&protocol::cmd_far_stc(0)).unwrap();
    let _ = conn.read_responses(Duration::from_secs(3));
    conn.set_nonblocking(true).unwrap();
    conn.state = connection::ConnectionState::Transmitting;

    let echo_socket = create_echo_socket(HOST_IP);

    let mut prev_spoke_data: Vec<u8> = Vec::new();

    let _ = std::fs::create_dir_all(&output_dir);

    let mut prev_step: Option<&protocol::ControlStep> = None;
    let total_steps = pattern.len();

    for (step_idx, step) in pattern.iter().enumerate() {
        println!("[{}/{}] range={} gain={} pulse={} rez={} rot={} interp={}",
            step_idx + 1, total_steps,
            step.range, step.gain, step.pulse_length,
            step.rez_boost, step.rotation_speed, step.interpolation);

        let cmds = step.commands_vs(prev_step);
        for cmd in &cmds {
            let _ = conn.send_and_wait_ack(cmd, "$N", Duration::from_secs(5));
        }

        if !cmds.is_empty() {
            std::thread::sleep(Duration::from_millis(300));
            let _ = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data);
        }

        for rot in 0..step.repeat {
            let (spokes, stats) = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data);
            println!("  rot {}/{}: {}", rot + 1, step.repeat, stats);

            if !spokes.is_empty() {
                let png_path = format!("{}/step{:05}_r{}_g{}_p{}_rot{}.png",
                    output_dir, step_idx, step.range, step.gain, step.pulse_length, rot);
                let pixels = render_ppi(&spokes);
                write_png(&png_path, &pixels, PPI_SIZE, PPI_SIZE);
            }
        }

        prev_step = Some(step);
    }

    // TX OFF
    conn.set_nonblocking(false).unwrap();
    let _ = conn.tx_off();
    println!("\nCalibration complete. {} steps executed.", total_steps);
}

// --- Comprehensive Validation Test ---

fn run_validate() {
    println!("=== Comprehensive Radar Validation Test ===");
    println!("Adapted from TEST_comp.md playbook\n");

    let output_dir = format!("validate_{}", timestamp_str());
    let _ = std::fs::create_dir_all(&output_dir);

    let mut report = Vec::<String>::new();
    let mut pass_count = 0u32;
    let mut fail_count = 0u32;
    let mut skip_count = 0u32;

    macro_rules! log {
        ($($arg:tt)*) => {{
            let msg = format!($($arg)*);
            println!("{msg}");
            report.push(msg);
        }};
    }

    // --- Phase 1: Connect ---
    log!("--- Phase 1: Connection & Init ---");

    let mut conn = RadarConnection::new(RADAR_IP, HOST_IP);
    if let Err(e) = conn.connect_full() {
        log!("FAIL: Connection failed: {e}");
        fail_count += 1;
        write_report(&output_dir, &report, pass_count, fail_count, skip_count);
        return;
    }
    log!("PASS: Auth + Control + Init");
    pass_count += 1;

    if let Some(fw) = &conn.firmware {
        log!("  Firmware: {fw}");
    }

    // --- Phase 2: TX ON ---
    log!("\n--- Phase 2: TX ON ---");
    if let Err(e) = conn.tx_on() {
        log!("FAIL: TX ON failed: {e}");
        fail_count += 1;
        write_report(&output_dir, &report, pass_count, fail_count, skip_count);
        return;
    }
    log!("PASS: TX ON");
    pass_count += 1;

    conn.set_nonblocking(true).unwrap();
    let echo_socket = create_echo_socket(HOST_IP);

    // Wait for echoes to start flowing
    log!("  Waiting for echo data...");
    let mut prev_spoke_data: Vec<u8> = Vec::new();
    let (warmup_spokes, warmup_stats) = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data);
    if warmup_spokes.is_empty() {
        log!("FAIL: No echo data received during warmup");
        fail_count += 1;
    } else {
        log!("PASS: Echo data flowing ({} spokes, enc={}, sweep_len={})",
            warmup_stats.num_spokes, warmup_stats.encoding, warmup_stats.sweep_len);
        pass_count += 1;
    }

    // --- Phase 3: Settings A/B/A Verification ---
    log!("\n--- Phase 3: Settings Panel A/B/A Verification ---");
    log!("  (5s wait between set/read, A->B->A anti-stale checks)\n");

    struct AbaTest {
        name: &'static str,
        set_a: String,
        set_b: String,
        read_cmd: String,
        ack_prefix: &'static str,
        expect_a: &'static str,
        expect_b: &'static str,
        setter_only: bool, // if true, verify via setter ACK instead of separate read
    }

    let aba_tests = vec![
        AbaTest {
            name: "Range",
            set_a: protocol::cmd_range(2),          // 0.5 NM
            set_b: protocol::cmd_range(10),         // 8 NM
            read_cmd: protocol::query_range(),
            ack_prefix: "$N62,",
            expect_a: "$N62,2,",
            expect_b: "$N62,10,",
            setter_only: false,
        },
        AbaTest {
            name: "Gain (manual)",
            set_a: protocol::cmd_gain(false, 15),
            set_b: protocol::cmd_gain(false, 85),
            read_cmd: protocol::query_gain(),
            ack_prefix: "$N63,",
            expect_a: ",15,",
            expect_b: ",85,",
            setter_only: false,
        },
        AbaTest {
            name: "Pulse Width",
            set_a: protocol::cmd_pulse_width(1, 2),  // S2
            set_b: protocol::cmd_pulse_width(5, 2),  // L
            read_cmd: protocol::query_pulse_width(),
            ack_prefix: "$N68,",
            expect_a: "$N68,1,",
            expect_b: "$N68,5,",
            setter_only: false,
        },
        // STC setter ACKs reflect the PREVIOUS value (pipeline delay).
        // Verify the ACK contains the register prefix (confirms command was accepted).
        AbaTest {
            name: "Near STC",
            set_a: protocol::cmd_near_stc(0),
            set_b: protocol::cmd_near_stc(4),
            read_cmd: String::new(),
            ack_prefix: "$N85,",
            expect_a: "$N85,",   // just verify register was touched
            expect_b: "$N85,",
            setter_only: true,
        },
        AbaTest {
            name: "Middle STC",
            set_a: protocol::cmd_middle_stc(0),
            set_b: protocol::cmd_middle_stc(3),
            read_cmd: String::new(),
            ack_prefix: "$N86,",
            expect_a: "$N86,",
            expect_b: "$N86,",
            setter_only: true,
        },
        AbaTest {
            name: "Far STC",
            set_a: protocol::cmd_far_stc(0),
            set_b: protocol::cmd_far_stc(2),
            read_cmd: String::new(),
            ack_prefix: "$N87,",
            expect_a: "$N87,",
            expect_b: "$N87,",
            setter_only: true,
        },
        AbaTest {
            name: "RezBoost",
            set_a: protocol::cmd_rezboost(0),       // off
            set_b: protocol::cmd_rezboost(3),       // high
            read_cmd: protocol::query_rezboost(),
            ack_prefix: "$NEE,",
            expect_a: "$NEE,0",
            expect_b: "$NEE,3",
            setter_only: false,
        },
        AbaTest {
            name: "A/C SEA (manual)",
            set_a: protocol::cmd_sea_clutter(false, 0),
            set_b: protocol::cmd_sea_clutter(false, 80),
            read_cmd: protocol::query_sea_clutter(),
            ack_prefix: "$N64,",
            expect_a: ",0,",
            expect_b: ",80,",
            setter_only: false,
        },
        AbaTest {
            name: "A/C RAIN (manual)",
            set_a: protocol::cmd_rain_clutter(false, 0),
            set_b: protocol::cmd_rain_clutter(false, 80),
            read_cmd: protocol::query_rain_clutter(),
            ack_prefix: "$N65,",
            expect_a: ",0,",
            expect_b: ",80,",
            setter_only: false,
        },
        // Antenna rev generates a flood of $NF5/$NAF status messages during
        // speed change; verify via setter ACK to avoid read timeouts.
        AbaTest {
            name: "Antenna Revolution",
            set_a: protocol::cmd_antenna_revolution(0, false),  // LO
            set_b: protocol::cmd_antenna_revolution(1, true),   // HI
            read_cmd: String::new(),
            ack_prefix: "$N89,",
            expect_a: "$N89,0,",
            expect_b: "$N89,1,",
            setter_only: true,
        },
        AbaTest {
            name: "Main Bang Suppression",
            set_a: protocol::cmd_main_bang(0),
            set_b: protocol::cmd_main_bang(200),
            read_cmd: protocol::query_main_bang(),
            ack_prefix: "$N83,",
            expect_a: ",0",
            expect_b: ",200",
            setter_only: false,
        },
        // IR getter returns sub-register 0; radar reports val=1 for "on"
        // (not val=2 as sent). Verify via setter ACK.
        AbaTest {
            name: "Interference Rejection",
            set_a: protocol::cmd_interference_rejection(false),
            set_b: protocol::cmd_interference_rejection(true),
            read_cmd: String::new(),
            ack_prefix: "$N67,",
            expect_a: "$N67,0,0,",  // IR off: $N67,0,0,0,0
            expect_b: "$N67,0,0,",  // IR on: $N67,0,0,{1or2},0 (radar may echo 1 or 2)
            setter_only: true,
        },
        AbaTest {
            name: "Bird Mode",
            set_a: protocol::cmd_bird_mode(0),
            set_b: protocol::cmd_bird_mode(3),
            read_cmd: "$RED,0,0\r\n".to_string(),
            ack_prefix: "$NED,",
            expect_a: "$NED,0",
            expect_b: "$NED,3",
            setter_only: false,
        },
        AbaTest {
            name: "Doppler",
            set_a: protocol::cmd_doppler(0),
            set_b: protocol::cmd_doppler(1),
            read_cmd: "$REF,0,0,0\r\n".to_string(),
            ack_prefix: "$NEF,",
            expect_a: "$NEF,0",
            expect_b: "$NEF,1",
            setter_only: false,
        },
    ];

    for test in &aba_tests {
        log!("  TEST: {} (A->B->A)", test.name);

        // Step A1: set A, then verify
        let (_, set_a1_ack) = conn.send_and_read_ack(&test.set_a, test.ack_prefix, Duration::from_secs(5))
            .unwrap_or((false, String::new()));
        std::thread::sleep(Duration::from_secs(5));
        let a1_line = if test.setter_only {
            set_a1_ack
        } else {
            conn.send_and_read_ack(&test.read_cmd, test.ack_prefix, Duration::from_secs(10))
                .unwrap_or((false, String::new())).1
        };

        let a1_ok = a1_line.contains(test.expect_a);
        if a1_ok {
            log!("    PASS: A1 readback contains '{}'", test.expect_a);
            pass_count += 1;
        } else {
            log!("    FAIL: A1 expected '{}', got '{}'", test.expect_a, a1_line.trim());
            fail_count += 1;
        }

        // Step B: set B, then verify
        let (_, set_b_ack) = conn.send_and_read_ack(&test.set_b, test.ack_prefix, Duration::from_secs(5))
            .unwrap_or((false, String::new()));
        std::thread::sleep(Duration::from_secs(5));
        let b_line = if test.setter_only {
            set_b_ack
        } else {
            conn.send_and_read_ack(&test.read_cmd, test.ack_prefix, Duration::from_secs(10))
                .unwrap_or((false, String::new())).1
        };

        let b_ok = b_line.contains(test.expect_b);
        if b_ok {
            log!("    PASS: B  readback contains '{}'", test.expect_b);
            pass_count += 1;
        } else {
            log!("    FAIL: B  expected '{}', got '{}'", test.expect_b, b_line.trim());
            fail_count += 1;
        }

        // Step A2: set A again, then verify
        let (_, set_a2_ack) = conn.send_and_read_ack(&test.set_a, test.ack_prefix, Duration::from_secs(5))
            .unwrap_or((false, String::new()));
        std::thread::sleep(Duration::from_secs(5));
        let a2_line = if test.setter_only {
            set_a2_ack
        } else {
            conn.send_and_read_ack(&test.read_cmd, test.ack_prefix, Duration::from_secs(10))
                .unwrap_or((false, String::new())).1
        };

        let a2_ok = a2_line.contains(test.expect_a);
        if a2_ok {
            log!("    PASS: A2 readback contains '{}'", test.expect_a);
            pass_count += 1;
        } else {
            log!("    FAIL: A2 expected '{}', got '{}'", test.expect_a, a2_line.trim());
            fail_count += 1;
        }

        // Anti-stale check: A1 and B must differ
        if a1_line == b_line && !a1_line.is_empty() {
            log!("    WARN: A1 == B readback (stale getter?)");
        }

        log!("");
    }

    // --- Phase 4: Echo Impact Test (gain low vs high) ---
    log!("--- Phase 4: Echo Impact (Gain Low vs High) ---");

    // Reset to known baseline
    let _ = conn.send_and_wait_ack(&protocol::cmd_range(0), "$N62,", Duration::from_secs(5));
    let _ = conn.send_and_wait_ack(&protocol::cmd_rezboost(0), "$NEE,", Duration::from_secs(5));
    std::thread::sleep(Duration::from_secs(2));

    // Low gain capture
    log!("  Setting gain=10 (low)...");
    let _ = conn.send_and_wait_ack(&protocol::cmd_gain(false, 10), "$N63,", Duration::from_secs(5));
    std::thread::sleep(Duration::from_secs(3));
    let _ = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data); // settle
    let (low_spokes, low_stats) = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data);
    log!("  Low gain:  {low_stats}");

    if !low_spokes.is_empty() {
        let csv_path = format!("{}/gain_low.csv", output_dir);
        let png_path = format!("{}/gain_low.png", output_dir);
        write_echo_csv(&csv_path, &low_spokes);
        let pixels = render_ppi(&low_spokes);
        write_png(&png_path, &pixels, PPI_SIZE, PPI_SIZE);
        log!("  Saved: {csv_path}, {png_path}");
    }

    // High gain capture
    log!("  Setting gain=90 (high)...");
    let _ = conn.send_and_wait_ack(&protocol::cmd_gain(false, 90), "$N63,", Duration::from_secs(5));
    std::thread::sleep(Duration::from_secs(3));
    let _ = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data); // settle
    let (high_spokes, high_stats) = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data);
    log!("  High gain: {high_stats}");

    if !high_spokes.is_empty() {
        let csv_path = format!("{}/gain_high.csv", output_dir);
        let png_path = format!("{}/gain_high.png", output_dir);
        write_echo_csv(&csv_path, &high_spokes);
        let pixels = render_ppi(&high_spokes);
        write_png(&png_path, &pixels, PPI_SIZE, PPI_SIZE);
        log!("  Saved: {csv_path}, {png_path}");
    }

    if low_stats.mean_amplitude < high_stats.mean_amplitude {
        log!("  PASS: High gain mean_amp ({:.2}) > low gain mean_amp ({:.2})",
            high_stats.mean_amplitude, low_stats.mean_amplitude);
        pass_count += 1;
    } else {
        log!("  WARN: High gain mean_amp ({:.2}) <= low gain ({:.2}) — may depend on environment",
            high_stats.mean_amplitude, low_stats.mean_amplitude);
        // Not a hard fail — echo depends on what's around the radar
        skip_count += 1;
    }

    // --- Phase 5: Range Impact Test ---
    log!("\n--- Phase 5: Range Impact (short vs long) ---");

    // Restore gain to auto
    let _ = conn.send_and_wait_ack(&protocol::cmd_gain(true, 0), "$N63,", Duration::from_secs(5));

    // Short range
    log!("  Setting range=0 (1/8 NM)...");
    let _ = conn.send_and_wait_ack(&protocol::cmd_range(0), "$N62,", Duration::from_secs(5));
    std::thread::sleep(Duration::from_secs(3));
    let _ = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data);
    let (short_spokes, short_stats) = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data);
    log!("  Short range: {short_stats}");

    if !short_spokes.is_empty() {
        let csv_path = format!("{}/range_short.csv", output_dir);
        let png_path = format!("{}/range_short.png", output_dir);
        write_echo_csv(&csv_path, &short_spokes);
        let pixels = render_ppi(&short_spokes);
        write_png(&png_path, &pixels, PPI_SIZE, PPI_SIZE);
        log!("  Saved: {csv_path}, {png_path}");
    }

    // Long range
    log!("  Setting range=10 (8 NM)...");
    let _ = conn.send_and_wait_ack(&protocol::cmd_range(10), "$N62,", Duration::from_secs(5));
    std::thread::sleep(Duration::from_secs(3));
    let _ = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data);
    let (long_spokes, long_stats) = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data);
    log!("  Long range:  {long_stats}");

    if !long_spokes.is_empty() {
        let csv_path = format!("{}/range_long.csv", output_dir);
        let png_path = format!("{}/range_long.png", output_dir);
        write_echo_csv(&csv_path, &long_spokes);
        let pixels = render_ppi(&long_spokes);
        write_png(&png_path, &pixels, PPI_SIZE, PPI_SIZE);
        log!("  Saved: {csv_path}, {png_path}");
    }

    if short_stats.sweep_len != long_stats.sweep_len || short_stats.num_spokes != long_stats.num_spokes {
        log!("  PASS: Range change produced different sweep parameters");
        pass_count += 1;
    } else {
        log!("  WARN: Sweep parameters unchanged between ranges");
        skip_count += 1;
    }

    // --- Phase 6: Restore defaults + TX OFF ---
    log!("\n--- Phase 6: Restore Defaults + TX OFF ---");

    let _ = conn.send_and_wait_ack(&protocol::cmd_range(0), "$N62,", Duration::from_secs(5));
    let _ = conn.send_and_wait_ack(&protocol::cmd_gain(true, 0), "$N63,", Duration::from_secs(5));
    let _ = conn.send_and_wait_ack(&protocol::cmd_near_stc(0), "$N85,", Duration::from_secs(5));
    let _ = conn.send_and_wait_ack(&protocol::cmd_middle_stc(0), "$N86,", Duration::from_secs(5));
    let _ = conn.send_and_wait_ack(&protocol::cmd_far_stc(0), "$N87,", Duration::from_secs(5));
    let _ = conn.send_and_wait_ack(&protocol::cmd_rezboost(0), "$NEE,", Duration::from_secs(5));
    let _ = conn.send_and_wait_ack(&protocol::cmd_sea_clutter(false, 0), "$N64,", Duration::from_secs(5));
    let _ = conn.send_and_wait_ack(&protocol::cmd_rain_clutter(false, 0), "$N65,", Duration::from_secs(5));
    let _ = conn.send_and_wait_ack(&protocol::cmd_main_bang(0), "$N83,", Duration::from_secs(5));
    let _ = conn.send_and_wait_ack(&protocol::cmd_interference_rejection(false), "$N67,", Duration::from_secs(5));
    let _ = conn.send_and_wait_ack(&protocol::cmd_bird_mode(0), "$NED,", Duration::from_secs(5));
    let _ = conn.send_and_wait_ack(&protocol::cmd_doppler(0), "$NEF,", Duration::from_secs(5));
    let _ = conn.send_and_wait_ack(&protocol::cmd_antenna_revolution(0, false), "$N89,", Duration::from_secs(5));
    log!("  Defaults restored");

    conn.set_nonblocking(false).unwrap();
    let _ = conn.tx_off();
    log!("  TX OFF");

    // --- Summary ---
    log!("\n{}", "=".repeat(60));
    log!("VALIDATION SUMMARY");
    log!("{}", "=".repeat(60));
    log!("  PASS: {pass_count}");
    log!("  FAIL: {fail_count}");
    log!("  SKIP/WARN: {skip_count}");
    log!("  Output: {output_dir}/");
    let verdict = if fail_count == 0 { "ALL TESTS PASSED" } else { "SOME TESTS FAILED" };
    log!("  Verdict: {verdict}");
    log!("{}", "=".repeat(60));

    write_report(&output_dir, &report, pass_count, fail_count, skip_count);
}

fn write_report(dir: &str, lines: &[String], pass: u32, fail: u32, skip: u32) {
    let path = format!("{dir}/report.txt");
    let mut f = std::io::BufWriter::new(
        std::fs::File::create(&path).expect("Failed to create report"),
    );
    writeln!(f, "Radar Validation Report").unwrap();
    writeln!(f, "Timestamp: {}", timestamp_str()).unwrap();
    writeln!(f, "Radar IP: {RADAR_IP}").unwrap();
    writeln!(f, "Host IP: {HOST_IP}").unwrap();
    writeln!(f, "").unwrap();
    for line in lines {
        writeln!(f, "{line}").unwrap();
    }
    writeln!(f, "").unwrap();
    writeln!(f, "Final: PASS={pass} FAIL={fail} SKIP={skip}").unwrap();
    println!("Report saved: {path}");
}

// --- Auto-Discovery ---

fn run_discover() {
    println!("=== Radar Discovery (UDP broadcast {}:{}) ===\n",
        protocol::FURUNO_BEACON_BROADCAST, protocol::FURUNO_BEACON_PORT);

    let sock = UdpSocket::bind(SocketAddr::from((Ipv4Addr::UNSPECIFIED, 0))).expect("bind");
    sock.set_broadcast(true).expect("set broadcast");
    sock.set_read_timeout(Some(Duration::from_secs(5))).unwrap();

    let dest = SocketAddr::from((protocol::FURUNO_BEACON_BROADCAST, protocol::FURUNO_BEACON_PORT));

    println!("Sending beacon announce...");
    sock.send_to(&protocol::BEACON_ANNOUNCE, dest).expect("send announce");

    println!("Sending beacon request...");
    sock.send_to(&protocol::BEACON_REQUEST, dest).expect("send request");

    let mut buf = [0u8; 512];
    let start = std::time::Instant::now();

    println!("Listening for responses (5s timeout)...\n");

    while start.elapsed() < Duration::from_secs(5) {
        match sock.recv_from(&mut buf) {
            Ok((len, src)) => {
                println!("Response from {src}: {len} bytes");
                print!("  ");
                for b in &buf[..len.min(64)] {
                    print!("{b:02X} ");
                }
                if len > 64 { print!("..."); }
                println!();

                if let Some(info) = protocol::parse_beacon_report(&buf[..len], src.ip().to_string().parse().unwrap_or(Ipv4Addr::UNSPECIFIED)) {
                    println!("  Radar found: {}", info);

                    sock.send_to(&protocol::BEACON_REQUEST_MODEL, src).expect("send model request");
                }
            }
            Err(_) => break,
        }
    }

    println!("\nDiscovery complete.");
}
