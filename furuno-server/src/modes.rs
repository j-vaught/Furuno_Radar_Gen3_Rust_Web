use std::net::{Ipv4Addr, SocketAddr, UdpSocket};
use std::time::Duration;

use crate::protocol::{self, Spoke, decode_echo_packet};
use crate::connection::{self, RadarConnection, create_echo_socket};
use crate::echo::{self, parse_echo_header, write_echo_csv};
use crate::render::{self, PPI_SIZE};
use crate::{radar_ip, host_ip};

pub fn run_handshake() {
    println!("=== Handshake: Auth + Control + Init ===\n");

    let mut conn = RadarConnection::new(radar_ip(), host_ip());
    if let Err(e) = conn.connect_full() {
        eprintln!("Handshake failed: {e}");
        std::process::exit(1);
    }

    if let Some(fw) = &conn.firmware {
        println!("\nFirmware: {fw}");
    }
    println!("\n=== Handshake complete! Control channel is open. ===");
}

pub fn run_tx_on() {
    println!("=== TX ON ===\n");

    let mut conn = RadarConnection::new(radar_ip(), host_ip());
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

pub fn run_tx_off() {
    println!("=== TX OFF (Standby) ===\n");

    let mut conn = RadarConnection::new(radar_ip(), host_ip());
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

pub fn run_full() {
    println!("=== Full: Connect + TX ON + Collect 1 Rotation + CSV + PNG ===\n");

    let mut conn = RadarConnection::new(radar_ip(), host_ip());
    if let Err(e) = conn.connect_full() {
        eprintln!("Init failed: {e}");
        std::process::exit(1);
    }

    if let Err(e) = conn.tx_on() {
        eprintln!("TX ON failed: {e}");
        std::process::exit(1);
    }

    conn.set_nonblocking(true).unwrap();

    let echo_socket = create_echo_socket(host_ip());

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

    let timestamp = echo::timestamp_str();
    let csv_path = format!("radar_{timestamp}.csv");
    let png_path = format!("radar_{timestamp}.png");

    println!("\nCollected {} spokes in {:.1}s", spokes.len(), start.elapsed().as_secs_f64());

    write_echo_csv(&csv_path, &spokes);
    println!("Saved CSV: {csv_path}");

    let pixels = render::render_ppi(&spokes);
    render::write_png(&png_path, &pixels, PPI_SIZE, PPI_SIZE);
    println!("Saved PNG: {png_path}");
}

pub fn run_echo_listen() {
    let multicast_echo = protocol::FURUNO_MULTICAST;
    let port_echo = protocol::FURUNO_DATA_PORT;
    println!("=== Echo Listener (UDP multicast {multicast_echo}:{port_echo}) ===");

    let echo_socket = create_echo_socket(host_ip());
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
                        echo::elapsed_str(start),
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

pub fn run_command_test() {
    println!("=== Command Test ===\n");

    let mut conn = RadarConnection::new(radar_ip(), host_ip());
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

pub fn run_dump_raw(count: usize) {
    println!("=== Raw Packet Dump ({count} packets) ===\n");

    let mut conn = RadarConnection::new(radar_ip(), host_ip());
    if let Err(e) = conn.connect_full() {
        eprintln!("Init failed: {e}");
        std::process::exit(1);
    }

    if let Err(e) = conn.tx_on() {
        eprintln!("TX ON failed: {e}");
        std::process::exit(1);
    }
    conn.set_nonblocking(true).unwrap();

    let echo_socket = create_echo_socket(host_ip());
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

pub fn run_render_live(out_path: &str) {
    println!("=== Live PPI Capture ===\n");

    let mut conn = RadarConnection::new(radar_ip(), host_ip());
    if let Err(e) = conn.connect_full() {
        eprintln!("Init failed: {e}");
        std::process::exit(1);
    }

    if let Err(e) = conn.tx_on() {
        eprintln!("TX ON failed: {e}");
        std::process::exit(1);
    }
    conn.set_nonblocking(true).unwrap();

    let echo_socket = create_echo_socket(host_ip());

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

    let csv_path = out_path.replace(".png", ".csv");
    write_echo_csv(&csv_path, &spokes);
    println!("Saved CSV: {csv_path}");

    let pixels = render::render_ppi(&spokes);
    render::write_png(out_path, &pixels, PPI_SIZE, PPI_SIZE);
    println!("Saved PNG: {out_path}");
}

pub fn run_discover() {
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
