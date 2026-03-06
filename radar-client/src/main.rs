#[allow(dead_code)]
mod protocol;

use std::io::{BufRead, Read, Write};
use std::net::{Ipv4Addr, SocketAddr, TcpStream, UdpSocket};
use std::os::unix::io::AsRawFd;
use std::time::Duration;

use protocol::{Spoke, decode_echo_packet, parse_echo_metadata};

const RADAR_IP: Ipv4Addr = Ipv4Addr::new(172, 31, 3, 226);
const HOST_IP: Ipv4Addr = Ipv4Addr::new(172, 31, 1, 100);

const PORT_DISCOVERY: u16 = protocol::FURUNO_BEACON_PORT;
const PORT_CONTROL: u16 = protocol::FURUNO_CONTROL_PORT;
const PORT_ECHO: u16 = protocol::FURUNO_DATA_PORT;
const MULTICAST_ECHO: Ipv4Addr = protocol::FURUNO_MULTICAST;

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
        _ => {
            eprintln!("Usage: radar-client <handshake|tx-on|tx-off|echo|full|command|render-csv|render-live|test-commands>");
            std::process::exit(1);
        }
    }
}

fn run_handshake() {
    println!("=== Phase 1: Authentication (TCP {RADAR_IP}:{PORT_DISCOVERY}) ===");
    let auth_ok = do_auth();
    if !auth_ok {
        eprintln!("Auth failed, aborting");
        std::process::exit(1);
    }
    println!();

    println!("=== Phase 2: Control Channel (TCP {RADAR_IP}:{PORT_CONTROL}) ===");
    let mut ctrl = do_connect_control();

    // Read initial messages from radar
    println!("Reading initial radar messages...");
    read_and_print_messages(&mut ctrl, Duration::from_secs(2));

    // Send firmware version query
    println!("\nSending firmware query: $R96,,,,,,,");
    send_command(&mut ctrl, "$R96,,,,,,,\n");
    read_and_print_messages(&mut ctrl, Duration::from_secs(2));

    // Send a subset of init register reads (the important ones)
    println!("\nSending init register reads...");
    let init_cmds = [
        "$R61,0,0,0\r\n",
        "$R62,0,0,0,0\r\n",
        "$R66\r\n",
        "$R69,0,0,0,0,0,0\r\n",
        "$R6D,0,0,0,0,0,0\r\n",
        "$R6E,0,0,0,0,0,0,0\r\n",
        "$R83,0,0\r\n",
        "$R85,0\r\n",
        "$R86,0\r\n",
        "$R87,0\r\n",
        "$REE,0,0\r\n",
    ];
    for cmd in &init_cmds {
        ctrl.write_all(cmd.as_bytes()).expect("write failed");
    }
    read_and_print_messages(&mut ctrl, Duration::from_secs(3));

    println!("\n=== Handshake complete! Control channel is open. ===");
}

fn run_tx_on() {
    println!("=== TX ON ===");
    let auth_ok = do_auth();
    if !auth_ok {
        eprintln!("Auth failed");
        std::process::exit(1);
    }

    let mut ctrl = do_connect_control();
    read_and_print_messages(&mut ctrl, Duration::from_secs(2));

    // Send firmware query (required part of init)
    send_command(&mut ctrl, "$R96,,,,,,,\n");
    read_and_print_messages(&mut ctrl, Duration::from_secs(1));

    // TX ON = $S69,2,0,0,0,0,0 (state 2 = transmit)
    println!("Sending TX ON: $S69,2,0,0,0,0,0");
    send_command(&mut ctrl, "$S69,2,0,0,0,0,0\r\n");
    send_command(&mut ctrl, "$S85,0\r\n");
    send_command(&mut ctrl, "$S86,0\r\n");
    send_command(&mut ctrl, "$S87,0\r\n");
    read_and_print_messages(&mut ctrl, Duration::from_secs(3));

    println!("TX ON sent. Echoes should start flowing on multicast 239.255.0.2:10024");
}

fn run_tx_off() {
    println!("=== TX OFF (Standby) ===");
    let auth_ok = do_auth();
    if !auth_ok {
        eprintln!("Auth failed");
        std::process::exit(1);
    }

    let mut ctrl = do_connect_control();
    read_and_print_messages(&mut ctrl, Duration::from_secs(2));

    send_command(&mut ctrl, "$R96,,,,,,,\n");
    read_and_print_messages(&mut ctrl, Duration::from_secs(1));

    // TX OFF = $S69,1,0,0,0,0,0 (state 1 = standby)
    println!("Sending TX OFF: $S69,1,0,0,0,0,0");
    send_command(&mut ctrl, "$S69,1,0,0,0,0,0\r\n");
    send_command(&mut ctrl, "$S85,0\r\n");
    send_command(&mut ctrl, "$S86,0\r\n");
    send_command(&mut ctrl, "$S87,0\r\n");
    read_and_print_messages(&mut ctrl, Duration::from_secs(3));

    println!("TX OFF sent. Echoes should stop.");
}

fn run_full() {
    println!("=== Full Test: Handshake + TX ON + Echo Listen ===\n");

    // Phase 1: Auth
    let auth_ok = do_auth();
    if !auth_ok {
        eprintln!("Auth failed");
        std::process::exit(1);
    }
    println!();

    // Phase 2: Control channel
    let mut ctrl = do_connect_control();
    read_and_print_messages(&mut ctrl, Duration::from_secs(2));

    // Firmware query
    send_command(&mut ctrl, "$R96,,,,,,,\n");
    read_and_print_messages(&mut ctrl, Duration::from_secs(1));

    // Send all init register reads (the full set the C++ app sends)
    println!("Sending full init register reads...");
    let init_cmds = include_str!("init_commands.txt");
    for line in init_cmds.lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') {
            continue;
        }
        ctrl.write_all(line.as_bytes()).expect("write failed");
        ctrl.write_all(b"\r\n").expect("write failed");
    }
    read_and_print_messages(&mut ctrl, Duration::from_secs(3));
    println!("Init complete.\n");

    // TX ON
    println!("Sending TX ON...");
    send_command(&mut ctrl, "$S69,2,0,0,0,0,0\r\n");
    send_command(&mut ctrl, "$S85,0\r\n");
    send_command(&mut ctrl, "$S86,0\r\n");
    send_command(&mut ctrl, "$S87,0\r\n");
    read_and_print_messages(&mut ctrl, Duration::from_secs(3));
    println!("TX ON sent.\n");

    // Set the control socket to non-blocking for the keepalive loop
    ctrl.set_nonblocking(true).unwrap();

    // Phase 3: Listen for echo data while keeping control alive
    println!("=== Listening for echo packets ===");
    let sock = socket2::Socket::new(
        socket2::Domain::IPV4,
        socket2::Type::DGRAM,
        Some(socket2::Protocol::UDP),
    )
    .expect("socket");
    sock.set_reuse_address(true).unwrap();
    unsafe {
        let optval: i32 = 1;
        libc::setsockopt(
            sock.as_raw_fd(),
            libc::SOL_SOCKET,
            libc::SO_REUSEPORT,
            &optval as *const _ as *const libc::c_void,
            std::mem::size_of::<i32>() as u32,
        );
    }
    sock.bind(&SocketAddr::from((Ipv4Addr::UNSPECIFIED, PORT_ECHO)).into())
        .expect("bind");
    let echo_socket: UdpSocket = sock.into();
    echo_socket
        .join_multicast_v4(&MULTICAST_ECHO, &HOST_IP)
        .expect("join multicast");
    echo_socket
        .set_read_timeout(Some(Duration::from_millis(500)))
        .unwrap();

    let mut buf = [0u8; 2048];
    let mut ctrl_buf = [0u8; 4096];
    let mut echo_count = 0u64;
    let mut keepalive_timer = std::time::Instant::now();
    let start = std::time::Instant::now();

    loop {
        // Try to receive echo
        match echo_socket.recv_from(&mut buf) {
            Ok((len, src)) => {
                echo_count += 1;
                if echo_count <= 10 || echo_count % 500 == 0 {
                    let hdr = parse_echo_header(&buf[..len]);
                    println!(
                        "[{}] echo #{echo_count}: {len}B from {src} | seq={} az={:.1} cfg=0x{:04X}",
                        elapsed_str(start),
                        hdr.sequence,
                        hdr.azimuth_deg,
                        hdr.config,
                    );
                }
                if echo_count == 10 {
                    println!("  ... (showing every 500th packet) ...\n");
                }
            }
            Err(_) => {
                // Timeout - check if we should send keepalive or exit
                if echo_count == 0 && start.elapsed() > Duration::from_secs(30) {
                    eprintln!("No echo packets after 30s. Radar may need more init.");
                    break;
                }
                if echo_count > 0 && start.elapsed() > Duration::from_secs(30) {
                    let rate = echo_count as f64 / start.elapsed().as_secs_f64();
                    println!(
                        "\nDone: {echo_count} echo packets in {:.1}s ({rate:.1}/sec)",
                        start.elapsed().as_secs_f64()
                    );
                    break;
                }
            }
        }

        // Drain control channel messages
        match ctrl.read(&mut ctrl_buf) {
            Ok(n) if n > 0 => {
                // Silently consume control messages
            }
            _ => {}
        }

        // Send keepalive every 5 seconds
        if keepalive_timer.elapsed() > Duration::from_secs(5) {
            let _ = ctrl.write_all(protocol::KEEPALIVE);
            keepalive_timer = std::time::Instant::now();
        }
    }

    // TX OFF before exit
    ctrl.set_nonblocking(false).unwrap();
    ctrl.set_write_timeout(Some(Duration::from_secs(2))).unwrap();
    let _ = ctrl.write_all(b"$S69,1,0,0,0,0,0\r\n");
    println!("TX OFF sent. Done.");
}

fn run_echo_listen() {
    println!("=== Echo Listener (UDP multicast {MULTICAST_ECHO}:{PORT_ECHO}) ===");

    // Use socket2 for SO_REUSEADDR/SO_REUSEPORT before bind
    let sock = socket2::Socket::new(
        socket2::Domain::IPV4,
        socket2::Type::DGRAM,
        Some(socket2::Protocol::UDP),
    )
    .expect("Failed to create socket");
    sock.set_reuse_address(true).unwrap();
    // SO_REUSEPORT
    unsafe {
        let optval: i32 = 1;
        libc::setsockopt(
            sock.as_raw_fd(),
            libc::SOL_SOCKET,
            libc::SO_REUSEPORT,
            &optval as *const _ as *const libc::c_void,
            std::mem::size_of::<i32>() as u32,
        );
    }
    sock.bind(&SocketAddr::from((Ipv4Addr::UNSPECIFIED, PORT_ECHO)).into())
        .expect("Failed to bind echo socket");

    let socket: UdpSocket = sock.into();

    // Join multicast group
    socket
        .join_multicast_v4(&MULTICAST_ECHO, &HOST_IP)
        .expect("Failed to join multicast group");

    socket
        .set_read_timeout(Some(Duration::from_secs(10)))
        .unwrap();

    println!("Listening for echo packets...\n");

    let mut buf = [0u8; 2048];
    let mut count = 0u64;
    let start = std::time::Instant::now();

    loop {
        match socket.recv_from(&mut buf) {
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
    println!("=== Command Test ===");
    let auth_ok = do_auth();
    if !auth_ok {
        eprintln!("Auth failed");
        std::process::exit(1);
    }

    let mut ctrl = do_connect_control();
    read_and_print_messages(&mut ctrl, Duration::from_secs(2));

    send_command(&mut ctrl, "$R96,,,,,,,\n");
    read_and_print_messages(&mut ctrl, Duration::from_secs(1));

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
        send_command(&mut ctrl, cmd);
        read_and_print_messages(&mut ctrl, Duration::from_secs(2));
    }

    println!("\n=== Command test complete ===");
}

// --- Low-level functions ---

fn do_auth() -> bool {
    let addr = SocketAddr::from((RADAR_IP, PORT_DISCOVERY));
    print!("Connecting to {addr}... ");

    let mut stream = match TcpStream::connect_timeout(&addr, Duration::from_secs(5)) {
        Ok(s) => {
            println!("connected");
            s
        }
        Err(e) => {
            eprintln!("FAILED: {e}");
            return false;
        }
    };

    stream
        .set_read_timeout(Some(Duration::from_secs(3)))
        .unwrap();

    print!("Sending auth payload ({} bytes)... ", protocol::AUTH_PAYLOAD.len());
    stream.write_all(protocol::AUTH_PAYLOAD).expect("write failed");
    println!("sent");

    // Read response (expect 12 bytes: 09 01 00 0C 01 00 00 00 00 64 00 00)
    let mut resp = [0u8; 64];
    match stream.read(&mut resp) {
        Ok(n) => {
            print!("Response ({n} bytes): ");
            for b in &resp[..n] {
                print!("{b:02X} ");
            }
            println!();

            if n >= 10 && resp[0] == 0x09 && resp[1] == 0x01 && resp[9] == 0x64 {
                println!("Auth SUCCESS (status=0x64=100)");
                true
            } else {
                eprintln!("Auth FAILED: unexpected response");
                false
            }
        }
        Err(e) => {
            eprintln!("No response: {e}");
            false
        }
    }
}

fn do_connect_control() -> TcpStream {
    let addr = SocketAddr::from((RADAR_IP, PORT_CONTROL));
    print!("Connecting to {addr}... ");

    let stream = TcpStream::connect_timeout(&addr, Duration::from_secs(5)).expect("connect failed");
    println!("connected");

    stream
        .set_read_timeout(Some(Duration::from_millis(500)))
        .unwrap();
    stream.set_nodelay(true).unwrap();

    stream
}

fn send_command(stream: &mut TcpStream, cmd: &str) {
    stream.write_all(cmd.as_bytes()).expect("write failed");
}

fn read_and_print_messages(stream: &mut TcpStream, timeout: Duration) {
    stream.set_read_timeout(Some(timeout)).unwrap();
    let mut buf = [0u8; 4096];

    loop {
        match stream.read(&mut buf) {
            Ok(0) => break,
            Ok(n) => {
                let data = &buf[..n];
                // Check if it's the binary keepalive (00 00 00 08 00 00 00 00)
                if n == 8 && data == protocol::KEEPALIVE {
                    println!("  <- [keepalive]");
                    continue;
                }

                // Try to parse as ASCII messages
                let text = String::from_utf8_lossy(data);
                let mut handled = false;
                for line in text.split('\n') {
                    let line = line.trim_end_matches('\r');
                    if line.is_empty() {
                        continue;
                    }
                    if line.starts_with('$') {
                        println!("  <- {line}");
                        handled = true;
                    }
                }

                if !handled {
                    // Binary or unknown data
                    print!("  <- [bin:{n}] ");
                    for b in data.iter().take(32.min(n)) {
                        print!("{b:02X} ");
                    }
                    if n > 32 {
                        print!("...");
                    }
                    println!();
                }

                // Shorter timeout for subsequent reads
                stream
                    .set_read_timeout(Some(Duration::from_millis(200)))
                    .unwrap();
            }
            Err(_) => break,
        }
    }
}

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

// Echo decoding is now in protocol.rs (supports all 4 encoding types)

// --- Command Test Harness ---

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

/// Collect echo data for one full rotation and compute statistics.
fn collect_rotation_stats(
    echo_socket: &UdpSocket,
    ctrl: &mut TcpStream,
    prev_spoke_data: &mut Vec<u8>,
) -> (Vec<Spoke>, EchoStats) {
    let mut buf = [0u8; 2048];
    let mut ctrl_buf = [0u8; 4096];
    let mut spokes: Vec<Spoke> = Vec::new();
    let mut prev_az: Option<u16> = None;
    let mut rotation_count = 0u32;
    let start = std::time::Instant::now();
    let mut keepalive_timer = std::time::Instant::now();
    let mut last_sweep_len: u32 = 0;
    let mut last_encoding: u8 = 0;
    let mut last_sweep_count: u32 = 0;

    loop {
        match echo_socket.recv_from(&mut buf) {
            Ok((len, _)) => {
                if len < 20 { continue; }
                let az = u16::from_le_bytes([buf[16], buf[17]]);

                // Parse metadata for stats
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
        let _ = ctrl.read(&mut ctrl_buf);
        if keepalive_timer.elapsed() > Duration::from_secs(5) {
            let _ = ctrl.write_all(protocol::KEEPALIVE);
            keepalive_timer = std::time::Instant::now();
        }
    }

    // Compute stats
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

/// Send a command and wait for the $N acknowledgment on the control channel.
fn send_and_wait_ack(ctrl: &mut TcpStream, cmd: &str, expected_prefix: &str) -> bool {
    ctrl.set_nonblocking(false).unwrap();
    ctrl.set_read_timeout(Some(Duration::from_secs(3))).unwrap();
    ctrl.write_all(cmd.as_bytes()).expect("write failed");

    let mut buf = [0u8; 4096];
    let deadline = std::time::Instant::now() + Duration::from_secs(5);
    let mut all_responses = String::new();

    while std::time::Instant::now() < deadline {
        match ctrl.read(&mut buf) {
            Ok(n) if n > 0 => {
                let text = String::from_utf8_lossy(&buf[..n]);
                all_responses.push_str(&text);
                for line in all_responses.lines() {
                    if line.starts_with(expected_prefix) {
                        println!("    ACK: {line}");
                        ctrl.set_nonblocking(true).unwrap();
                        return true;
                    }
                }
            }
            _ => {}
        }
    }
    eprintln!("    No ACK for {expected_prefix} (got: {})", all_responses.trim().replace('\n', " | "));
    ctrl.set_nonblocking(true).unwrap();
    false
}

fn run_test_commands() {
    println!("=== Command Verification Test ===\n");

    // Boot up
    let auth_ok = do_auth();
    if !auth_ok { eprintln!("Auth failed"); std::process::exit(1); }
    println!();

    let mut ctrl = do_connect_control();
    read_and_print_messages(&mut ctrl, Duration::from_secs(2));
    send_command(&mut ctrl, "$R96,,,,,,,\n");
    read_and_print_messages(&mut ctrl, Duration::from_secs(1));

    let init_cmds = include_str!("init_commands.txt");
    for line in init_cmds.lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') { continue; }
        ctrl.write_all(line.as_bytes()).expect("write failed");
        ctrl.write_all(b"\r\n").expect("write failed");
    }
    read_and_print_messages(&mut ctrl, Duration::from_secs(3));

    // TX ON
    println!("Sending TX ON...");
    send_command(&mut ctrl, "$S69,2,0,0,0,0,0\r\n");
    send_command(&mut ctrl, "$S85,0\r\n");
    send_command(&mut ctrl, "$S86,0\r\n");
    send_command(&mut ctrl, "$S87,0\r\n");
    read_and_print_messages(&mut ctrl, Duration::from_secs(3));
    ctrl.set_nonblocking(true).unwrap();

    // Setup echo socket
    let sock = socket2::Socket::new(socket2::Domain::IPV4, socket2::Type::DGRAM, Some(socket2::Protocol::UDP)).expect("socket");
    sock.set_reuse_address(true).unwrap();
    unsafe {
        let optval: i32 = 1;
        libc::setsockopt(sock.as_raw_fd(), libc::SOL_SOCKET, libc::SO_REUSEPORT,
            &optval as *const _ as *const libc::c_void, std::mem::size_of::<i32>() as u32);
    }
    let bind_result = sock.bind(&SocketAddr::from((Ipv4Addr::UNSPECIFIED, PORT_ECHO)).into());
    if bind_result.is_err() {
        sock.bind(&SocketAddr::from((MULTICAST_ECHO, PORT_ECHO)).into()).expect("bind");
    }
    let echo_socket: UdpSocket = sock.into();
    echo_socket.join_multicast_v4(&MULTICAST_ECHO, &HOST_IP).expect("join multicast");
    echo_socket.set_read_timeout(Some(Duration::from_millis(500))).unwrap();

    let mut prev_spoke_data: Vec<u8> = Vec::new();

    // ============================================================
    // Test sequence: each test = (description, command, ack_prefix, expected_behavior)
    // ============================================================

    // Each test pair: setup (optional) -> capture BEFORE -> apply command -> capture AFTER -> restore
    struct TestPair {
        name: &'static str,
        setup_cmd: &'static str,       // optional: set the "before" state explicitly
        setup_ack: &'static str,
        apply_cmd: &'static str,
        apply_ack: &'static str,
        restore_cmd: &'static str,
        restore_ack: &'static str,
        label: &'static str,
    }

    let tests = [
        TestPair {
            name: "Range: 0.125 NM -> 2 NM",
            setup_cmd: "$S62,0,0,0\r\n",
            setup_ack: "$N62,",
            apply_cmd: "$S62,6,0,0\r\n",
            apply_ack: "$N62,",
            restore_cmd: "$S62,0,0,0\r\n",
            restore_ack: "$N62,",
            label: "range_2nm",
        },
        TestPair {
            name: "Range: 0.125 NM -> 8 NM",
            setup_cmd: "$S62,0,0,0\r\n",
            setup_ack: "$N62,",
            apply_cmd: "$S62,10,0,0\r\n",
            apply_ack: "$N62,",
            restore_cmd: "$S62,0,0,0\r\n",
            restore_ack: "$N62,",
            label: "range_8nm",
        },
        TestPair {
            name: "Gain: 0 -> 100",
            setup_cmd: "$S63,0,0,0,80,0\r\n",
            setup_ack: "$N63,",
            apply_cmd: "$S63,0,100,0,80,0\r\n",
            apply_ack: "$N63,",
            restore_cmd: "$S63,1,0,0,80,0\r\n",
            restore_ack: "$N63,",
            label: "gain_0v100",
        },
        TestPair {
            name: "Sea clutter: 0 -> 100",
            setup_cmd: "$S64,0,0,50,0,0,0\r\n",
            setup_ack: "$N64,",
            apply_cmd: "$S64,0,100,50,0,0,0\r\n",
            apply_ack: "$N64,",
            restore_cmd: "$S64,0,0,50,0,0,0\r\n",
            restore_ack: "$N64,",
            label: "sea_100",
        },
        TestPair {
            name: "Rain clutter: 0 -> 100",
            setup_cmd: "$S65,0,0,0,0,0,0\r\n",
            setup_ack: "$N65,",
            apply_cmd: "$S65,0,100,0,0,0,0\r\n",
            apply_ack: "$N65,",
            restore_cmd: "$S65,0,0,0,0,0,0\r\n",
            restore_ack: "$N65,",
            label: "rain_100",
        },
        TestPair {
            name: "RezBoost: off -> High",
            setup_cmd: "$SEE,0,0\r\n",
            setup_ack: "$NEE,",
            apply_cmd: "$SEE,3,0\r\n",
            apply_ack: "$NEE,",
            restore_cmd: "$SEE,0,0\r\n",
            restore_ack: "$NEE,",
            label: "rezboost_high",
        },
        TestPair {
            name: "Interference rejection: off -> on",
            setup_cmd: "$S67,0,0,0,0\r\n",
            setup_ack: "$N67,",
            apply_cmd: "$S67,0,0,2,0\r\n",
            apply_ack: "$N67,",
            restore_cmd: "$S67,0,0,0,0\r\n",
            restore_ack: "$N67,",
            label: "ir",
        },
        TestPair {
            name: "Noise reduction: off -> on",
            setup_cmd: "$S67,0,3,0,0\r\n",
            setup_ack: "$N67,",
            apply_cmd: "$S67,0,3,1,0\r\n",
            apply_ack: "$N67,",
            restore_cmd: "$S67,0,3,0,0\r\n",
            restore_ack: "$N67,",
            label: "nr",
        },
        TestPair {
            name: "Scan speed: normal -> HI",
            setup_cmd: "$S89,0,0\r\n",
            setup_ack: "$N89,",
            apply_cmd: "$S89,1,0\r\n",
            apply_ack: "$N89,",
            restore_cmd: "$S89,0,0\r\n",
            restore_ack: "$N89,",
            label: "scan_hi",
        },
        // --- Pulse width ---
        TestPair {
            name: "Pulse width: S1 (short) -> L (long)",
            setup_cmd: "$S68,0,0,0,0,0\r\n",   // S1 at range 0
            setup_ack: "$N68,",
            apply_cmd: "$S68,5,0,0,0,0\r\n",    // L (long) at range 0
            apply_ack: "$N68,",
            restore_cmd: "$S68,0,0,0,0,0\r\n",
            restore_ack: "$N68,",
            label: "pulse_long",
        },
        // --- Near STC ---
        TestPair {
            name: "Near STC: 0 -> 4",
            setup_cmd: "$S85,0\r\n",
            setup_ack: "$N85,",
            apply_cmd: "$S85,4\r\n",
            apply_ack: "$N85,",
            restore_cmd: "$S85,0\r\n",
            restore_ack: "$N85,",
            label: "near_stc",
        },
        // --- Middle STC ---
        TestPair {
            name: "Middle STC: 0 -> 3",
            setup_cmd: "$S86,0\r\n",
            setup_ack: "$N86,",
            apply_cmd: "$S86,3\r\n",
            apply_ack: "$N86,",
            restore_cmd: "$S86,0\r\n",
            restore_ack: "$N86,",
            label: "mid_stc",
        },
        // --- Far STC ---
        TestPair {
            name: "Far STC: 0 -> 2",
            setup_cmd: "$S87,0\r\n",
            setup_ack: "$N87,",
            apply_cmd: "$S87,2\r\n",
            apply_ack: "$N87,",
            restore_cmd: "$S87,0\r\n",
            restore_ack: "$N87,",
            label: "far_stc",
        },
        // --- Main bang suppression ---
        TestPair {
            name: "Main bang: 0 -> 200",
            setup_cmd: "$S83,0,0\r\n",
            setup_ack: "$N83,",
            apply_cmd: "$S83,200,0\r\n",
            apply_ack: "$N83,",
            restore_cmd: "$S83,0,0\r\n",
            restore_ack: "$N83,",
            label: "mainbang",
        },
        // --- Antenna height ---
        TestPair {
            name: "Antenna height: 0 -> 15m",
            setup_cmd: "$S84,0,0,0\r\n",
            setup_ack: "$N84,",
            apply_cmd: "$S84,0,15,0\r\n",
            apply_ack: "$N84,",
            restore_cmd: "$S84,0,0,0\r\n",
            restore_ack: "$N84,",
            label: "ant_height",
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

        // --- SETUP: explicitly set the "before" state ---
        if !test.setup_cmd.is_empty() {
            println!("  SETUP: {}", test.setup_cmd.trim());
            send_and_wait_ack(&mut ctrl, test.setup_cmd, test.setup_ack);
            std::thread::sleep(Duration::from_millis(500));
            // Drain one rotation for setup to settle
            let _ = collect_rotation_stats(&echo_socket, &mut ctrl, &mut prev_spoke_data);
        }

        // --- BEFORE: capture with current settings ---
        println!("  [BEFORE] Collecting echo data...");
        let (before_spokes, before_stats) = collect_rotation_stats(&echo_socket, &mut ctrl, &mut prev_spoke_data);
        println!("  [BEFORE] {before_stats}");

        if !before_spokes.is_empty() {
            let png_path = format!("{}/{:02}_{}_BEFORE.png", output_dir, i + 1, test.label);
            let pixels = render_ppi(&before_spokes);
            write_png(&png_path, &pixels, PPI_SIZE, PPI_SIZE);
            println!("  [BEFORE] {png_path}");
        }

        // --- APPLY command ---
        println!("  CMD: {}", test.apply_cmd.trim());
        let ack_ok = send_and_wait_ack(&mut ctrl, test.apply_cmd, test.apply_ack);

        // Wait for settings to take effect
        std::thread::sleep(Duration::from_millis(500));

        // --- AFTER: capture with new settings ---
        println!("  [AFTER]  Collecting echo data...");
        let (after_spokes, after_stats) = collect_rotation_stats(&echo_socket, &mut ctrl, &mut prev_spoke_data);
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

        // --- RESTORE to defaults ---
        println!("  RESTORE: {}", test.restore_cmd.trim());
        send_and_wait_ack(&mut ctrl, test.restore_cmd, test.restore_ack);
        std::thread::sleep(Duration::from_millis(500));
        // Drain one rotation to let restored settings settle
        {
            let _ = collect_rotation_stats(&echo_socket, &mut ctrl, &mut prev_spoke_data);
        }
    }

    // TX OFF
    ctrl.set_nonblocking(false).unwrap();
    ctrl.set_write_timeout(Some(Duration::from_secs(2))).unwrap();
    let _ = ctrl.write_all(b"$S69,1,0,0,0,0,0\r\n");

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
    println!("TX OFF sent. Done.");
}

// --- PPI Rendering ---

const PPI_SIZE: usize = 800;
const PPI_NUM_RANGE_BINS: usize = 868;
const PPI_ANGLE_TICKS: f64 = 8192.0;

fn run_dump_raw(count: usize) {
    println!("=== Raw Packet Dump ({count} packets) ===\n");

    let auth_ok = do_auth();
    if !auth_ok { eprintln!("Auth failed"); std::process::exit(1); }

    let mut ctrl = do_connect_control();
    read_and_print_messages(&mut ctrl, Duration::from_secs(2));
    send_command(&mut ctrl, "$R96,,,,,,,\n");
    read_and_print_messages(&mut ctrl, Duration::from_secs(1));

    let init_cmds = include_str!("init_commands.txt");
    for line in init_cmds.lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') { continue; }
        ctrl.write_all(line.as_bytes()).expect("write failed");
        ctrl.write_all(b"\r\n").expect("write failed");
    }
    read_and_print_messages(&mut ctrl, Duration::from_secs(3));

    send_command(&mut ctrl, "$S69,2,0,0,0,0,0\r\n");
    send_command(&mut ctrl, "$S85,0\r\n");
    send_command(&mut ctrl, "$S86,0\r\n");
    send_command(&mut ctrl, "$S87,0\r\n");
    read_and_print_messages(&mut ctrl, Duration::from_secs(3));
    ctrl.set_nonblocking(true).unwrap();

    let sock = socket2::Socket::new(socket2::Domain::IPV4, socket2::Type::DGRAM, Some(socket2::Protocol::UDP)).expect("socket");
    sock.set_reuse_address(true).unwrap();
    unsafe {
        let optval: i32 = 1;
        libc::setsockopt(sock.as_raw_fd(), libc::SOL_SOCKET, libc::SO_REUSEPORT, &optval as *const _ as *const libc::c_void, std::mem::size_of::<i32>() as u32);
    }
    sock.bind(&SocketAddr::from((Ipv4Addr::UNSPECIFIED, PORT_ECHO)).into()).expect("bind");
    let echo_socket: UdpSocket = sock.into();
    echo_socket.join_multicast_v4(&MULTICAST_ECHO, &HOST_IP).expect("join multicast");
    echo_socket.set_read_timeout(Some(Duration::from_secs(10))).unwrap();

    let mut buf = [0u8; 2048];
    let mut ctrl_buf = [0u8; 4096];

    // Wait for first rotation wrap to get diverse azimuths
    let mut prev_az_dump: Option<u16> = None;
    let mut rotation_seen = false;
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
        let _ = ctrl.read(&mut ctrl_buf);
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

                // Print header
                print!("  HDR: ");
                for i in 0..18.min(len) { print!("{:02X} ", buf[i]); }
                println!();

                // Split body into sections and analyze each
                let body = &buf[18..len];
                let az_lo = buf[16];
                let az_hi = buf[17];

                // Find section boundaries using xx yy 00 00 pattern
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

                // Analyze first 3 sections
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

                    // Skip 00 00 prefix
                    let data = if sec.len() > 2 && sec[0] == 0 && sec[1] == 0 {
                        &sec[2..]
                    } else { sec };

                    // Test run-length hypothesis: byte%4==0 is amplitude,
                    // byte%4!=0 is run-length count = (byte >> 2) + 1
                    let mut total_bins = 0u32;
                    let mut j = 0;
                    let mut detail = String::new();
                    while j < data.len() {
                        let b = data[j];
                        if (b == 0xFE || b == 0xFD) && j + 1 < data.len() {
                            let count = (b >> 2) + 1;
                            total_bins += count as u32;
                            detail += &format!("{:02X}({})×{:02X} ", b, count, data[j+1]);
                            j += 2;
                        } else if b % 4 == 0 {
                            total_bins += 1;
                            j += 1;
                        } else {
                            let count = (b >> 2) + 1;
                            total_bins += count as u32;
                            detail += &format!("[{:02X}={}bins] ", b, count);
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
        let _ = ctrl.read(&mut ctrl_buf);
    }

    // TX OFF
    ctrl.set_nonblocking(false).unwrap();
    ctrl.set_write_timeout(Some(Duration::from_secs(2))).unwrap();
    let _ = ctrl.write_all(b"$S69,1,0,0,0,0,0\r\n");
    println!("TX OFF sent. Done.");
}

fn run_render_csv(csv_path: &str, out_path: &str) {
    println!("Rendering PPI from CSV: {csv_path}");

    let file = std::fs::File::open(csv_path).expect("Failed to open CSV");
    let reader = std::io::BufReader::new(file);
    let mut spokes = Vec::new();

    for (i, line) in reader.lines().enumerate() {
        let line = line.expect("read line");
        if i == 0 { continue; } // skip header
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

    let auth_ok = do_auth();
    if !auth_ok {
        eprintln!("Auth failed");
        std::process::exit(1);
    }

    let mut ctrl = do_connect_control();
    read_and_print_messages(&mut ctrl, Duration::from_secs(2));

    send_command(&mut ctrl, "$R96,,,,,,,\n");
    read_and_print_messages(&mut ctrl, Duration::from_secs(1));

    let init_cmds = include_str!("init_commands.txt");
    for line in init_cmds.lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') { continue; }
        ctrl.write_all(line.as_bytes()).expect("write failed");
        ctrl.write_all(b"\r\n").expect("write failed");
    }
    read_and_print_messages(&mut ctrl, Duration::from_secs(3));

    println!("Sending TX ON...");
    send_command(&mut ctrl, "$S69,2,0,0,0,0,0\r\n");
    send_command(&mut ctrl, "$S85,0\r\n");
    send_command(&mut ctrl, "$S86,0\r\n");
    send_command(&mut ctrl, "$S87,0\r\n");
    read_and_print_messages(&mut ctrl, Duration::from_secs(3));

    ctrl.set_nonblocking(true).unwrap();

    // Join multicast for echo data
    let sock = socket2::Socket::new(
        socket2::Domain::IPV4,
        socket2::Type::DGRAM,
        Some(socket2::Protocol::UDP),
    ).expect("socket");
    sock.set_reuse_address(true).unwrap();
    unsafe {
        let optval: i32 = 1;
        libc::setsockopt(
            sock.as_raw_fd(),
            libc::SOL_SOCKET,
            libc::SO_REUSEPORT,
            &optval as *const _ as *const libc::c_void,
            std::mem::size_of::<i32>() as u32,
        );
    }
    // Try binding to INADDR_ANY first; fall back to multicast address if C++ app holds the port
    let bind_result = sock.bind(&SocketAddr::from((Ipv4Addr::UNSPECIFIED, PORT_ECHO)).into());
    if bind_result.is_err() {
        sock.bind(&SocketAddr::from((MULTICAST_ECHO, PORT_ECHO)).into()).expect("bind multicast addr");
    }
    let echo_socket: UdpSocket = sock.into();
    echo_socket.join_multicast_v4(&MULTICAST_ECHO, &HOST_IP).expect("join multicast");
    echo_socket.set_read_timeout(Some(Duration::from_millis(500))).unwrap();

    // Collect one full rotation of echo data
    let mut buf = [0u8; 2048];
    let mut ctrl_buf = [0u8; 4096];
    let mut spokes: Vec<Spoke> = Vec::new();
    let mut prev_spoke_data: Vec<u8> = Vec::new();
    let mut prev_az: Option<u16> = None;
    let mut rotation_count = 0u32;
    let mut keepalive_timer = std::time::Instant::now();
    let start = std::time::Instant::now();

    println!("Collecting echo data for 2 full rotations...");

    loop {
        match echo_socket.recv_from(&mut buf) {
            Ok((len, _)) => {
                if len < 20 { continue; }
                let az = u16::from_le_bytes([buf[16], buf[17]]);

                // Detect rotation wrap
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

                // Always decode to keep prev_spoke_data updated
                if rotation_count >= 1 {
                    decode_echo_packet(&buf[..len], &mut prev_spoke_data, &mut spokes);
                } else {
                    // Pre-rotation: decode but discard spokes (keep prev_spoke state)
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

        // Drain control channel
        let _ = ctrl.read(&mut ctrl_buf);

        // Keepalive
        if keepalive_timer.elapsed() > Duration::from_secs(5) {
            let _ = ctrl.write_all(protocol::KEEPALIVE);
            keepalive_timer = std::time::Instant::now();
        }
    }

    // TX OFF
    ctrl.set_nonblocking(false).unwrap();
    ctrl.set_write_timeout(Some(Duration::from_secs(2))).unwrap();
    let _ = ctrl.write_all(b"$S69,1,0,0,0,0,0\r\n");

    if spokes.is_empty() {
        eprintln!("No spokes collected");
        return;
    }

    println!("Collected {} raw spokes, rendering PPI...", spokes.len());
    let pixels = render_ppi(&spokes);
    write_png(out_path, &pixels, PPI_SIZE, PPI_SIZE);
    println!("Saved: {out_path}");
    println!("TX OFF sent. Done.");
}

fn render_ppi(spokes: &[Spoke]) -> Vec<u8> {
    let size = PPI_SIZE;
    let center = size as f64 / 2.0;
    let max_r = center - 2.0;

    // RGBA image: black background
    let mut pixels = vec![0u8; size * size * 4];

    // Set alpha to 255 for all pixels (opaque black background)
    for y in 0..size {
        for x in 0..size {
            let idx = (y * size + x) * 4;
            pixels[idx + 3] = 255; // alpha
        }
    }

    // Sort spokes by angle
    let mut sorted: Vec<(f64, &[u8])> = spokes.iter()
        .map(|s| (s.angle_ticks as f64 / PPI_ANGLE_TICKS * std::f64::consts::TAU, s.echo.as_slice()))
        .collect();
    sorted.sort_by(|a, b| a.0.partial_cmp(&b.0).unwrap());

    // For each pixel, find the nearest spoke and range bin
    for y in 0..size {
        for x in 0..size {
            let dx = x as f64 - center;
            let dy = -(y as f64 - center); // Y-up for proper orientation (north=up)
            let r = (dx * dx + dy * dy).sqrt();
            if r < 1.0 || r > max_r { continue; }

            // Angle: 0=north, clockwise
            let mut angle = dy.atan2(dx);
            // Convert from math angle (east=0, CCW) to radar angle (north=0, CW)
            angle = std::f64::consts::FRAC_PI_2 - angle;
            if angle < 0.0 { angle += std::f64::consts::TAU; }

            // Find nearest spoke
            let spoke_idx = match sorted.binary_search_by(|s| s.0.partial_cmp(&angle).unwrap()) {
                Ok(i) => i,
                Err(i) => {
                    if i == 0 { sorted.len() - 1 }
                    else if i >= sorted.len() { 0 }
                    else {
                        let d1 = (sorted[i - 1].0 - angle).abs();
                        let d2 = (sorted[i].0 - angle).abs();
                        if d1 < d2 { i - 1 } else { i }
                    }
                }
            };

            let echo = sorted[spoke_idx].1;
            let num_bins = echo.len().min(PPI_NUM_RANGE_BINS);
            if num_bins == 0 { continue; }

            let bin = ((r / max_r) * num_bins as f64) as usize;
            if bin >= num_bins { continue; }

            let amplitude = echo[bin];
            if amplitude == 0 { continue; }

            // Display values are 0-63 (wire values >> 2)
            let intensity = (amplitude as u32 * 255 / 63).min(255) as u8;
            let idx = (y * size + x) * 4;
            // Garnet-to-white color scheme based on brand colors
            pixels[idx] = intensity;                                    // R
            pixels[idx + 1] = (intensity as u32 * 180 / 255) as u8;   // G (warm tint)
            pixels[idx + 2] = (intensity as u32 * 20 / 255) as u8;    // B
        }
    }

    // Draw crosshairs
    let c = center as usize;
    for i in 0..size {
        let idx = (c * size + i) * 4;
        pixels[idx] = 54; pixels[idx + 1] = 54; pixels[idx + 2] = 54; // 90% black
        let idx = (i * size + c) * 4;
        pixels[idx] = 54; pixels[idx + 1] = 54; pixels[idx + 2] = 54;
    }

    // Draw range rings
    for ring in 1..=4 {
        let ring_r = max_r * ring as f64 / 4.0;
        let steps = (ring_r * std::f64::consts::TAU) as usize;
        for s in 0..steps {
            let angle = s as f64 / steps as f64 * std::f64::consts::TAU;
            let px = (center + ring_r * angle.cos()) as usize;
            let py = (center - ring_r * angle.sin()) as usize;
            if px < size && py < size {
                let idx = (py * size + px) * 4;
                pixels[idx] = 54; pixels[idx + 1] = 54; pixels[idx + 2] = 54;
            }
        }
    }

    pixels
}

fn write_png(path: &str, pixels: &[u8], width: usize, height: usize) {
    let file = std::fs::File::create(path).expect("create PNG file");
    let ref mut w = std::io::BufWriter::new(file);
    let mut encoder = png::Encoder::new(w, width as u32, height as u32);
    encoder.set_color(png::ColorType::Rgba);
    encoder.set_depth(png::BitDepth::Eight);
    let mut writer = encoder.write_header().expect("write PNG header");
    writer.write_image_data(pixels).expect("write PNG data");
}
