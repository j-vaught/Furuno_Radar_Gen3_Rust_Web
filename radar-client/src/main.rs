use std::io::{Read, Write};
use std::net::{Ipv4Addr, SocketAddr, TcpStream, UdpSocket};
use std::os::unix::io::AsRawFd;
use std::time::Duration;

const RADAR_IP: Ipv4Addr = Ipv4Addr::new(172, 31, 3, 226);
const HOST_IP: Ipv4Addr = Ipv4Addr::new(172, 31, 1, 100);

const PORT_DISCOVERY: u16 = 10010;
const PORT_CONTROL: u16 = 10100;
const PORT_ECHO: u16 = 10024;
const MULTICAST_ECHO: Ipv4Addr = Ipv4Addr::new(239, 255, 0, 2);

// Auth payload: type=0x08, subtype=0x01, length=0x0038, then copyright string
const AUTH_PAYLOAD: &[u8] = b"\x08\x01\x00\x38\x01\x00\x00\x00\x00\x01\x00\x00\
COPYRIGHT (C) 2001 FURUNO ELECTRIC CO.,LTD. ";

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
        _ => {
            eprintln!("Usage: radar-client <handshake|tx-on|tx-off|echo|full|command>");
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
            let _ = ctrl.write_all(b"\x00\x00\x00\x08\x00\x00\x00\x00");
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

    print!("Sending auth payload ({} bytes)... ", AUTH_PAYLOAD.len());
    stream.write_all(AUTH_PAYLOAD).expect("write failed");
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
                if n == 8 && data == b"\x00\x00\x00\x08\x00\x00\x00\x00" {
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
