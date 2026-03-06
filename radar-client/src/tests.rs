//! Integration test suite — requires a live radar connected at RADAR_IP.
//! Run with: cargo test -- --test-threads=1
//! Tests run sequentially to avoid conflicting radar commands.

#[cfg(test)]
mod connection_tests {
    use crate::connection::{RadarConnection, create_echo_socket};
    use crate::{radar_ip, host_ip};

    #[test]
    fn test_auth_succeeds() {
        let mut conn = RadarConnection::new(radar_ip(), host_ip());
        let result = conn.auth();
        assert!(result.is_ok(), "Auth failed: {:?}", result.err());
    }

    #[test]
    fn test_connect_control() {
        let mut conn = RadarConnection::new(radar_ip(), host_ip());
        conn.auth().expect("auth failed");
        let result = conn.connect_control();
        assert!(result.is_ok(), "Control connect failed: {:?}", result.err());
    }

    #[test]
    fn test_connect_full() {
        let mut conn = RadarConnection::new(radar_ip(), host_ip());
        let result = conn.connect_full();
        assert!(result.is_ok(), "Full connect failed: {:?}", result.err());
    }

    #[test]
    fn test_firmware_populated() {
        let mut conn = RadarConnection::new(radar_ip(), host_ip());
        conn.connect_full().expect("connect failed");
        assert!(conn.firmware.is_some(), "Firmware should be populated after init");
        let fw = conn.firmware.as_ref().unwrap();
        assert!(!fw.is_empty(), "Firmware string should not be empty");
        println!("Firmware: {fw}");
    }

    #[test]
    fn test_keepalive_succeeds() {
        let mut conn = RadarConnection::new(radar_ip(), host_ip());
        conn.connect_full().expect("connect failed");
        let result = conn.send_keepalive();
        assert!(result.is_ok(), "Keepalive failed: {:?}", result.err());
    }

    #[test]
    fn test_echo_socket_creation() {
        let sock = create_echo_socket(host_ip());
        // Socket should have a timeout set
        let timeout = sock.read_timeout().unwrap();
        assert!(timeout.is_some(), "Echo socket should have a read timeout");
    }
}

#[cfg(test)]
mod tx_tests {
    use std::time::Duration;
    use crate::connection::{RadarConnection, create_echo_socket};
    use crate::protocol;
    use crate::{radar_ip, host_ip};

    #[test]
    fn test_tx_on_off_cycle() {
        let mut conn = RadarConnection::new(radar_ip(), host_ip());
        conn.connect_full().expect("connect failed");

        // TX ON
        let result = conn.tx_on();
        assert!(result.is_ok(), "TX ON failed: {:?}", result.err());

        // Verify TX state via query
        let (found, line) = conn.send_and_read_ack(
            &protocol::query_tx_state(), "$N69,", Duration::from_secs(5)
        ).unwrap_or((false, String::new()));
        assert!(found, "Should get TX state ack");
        assert!(line.contains("$N69,"), "TX state response: {line}");

        // TX OFF
        let result = conn.tx_off();
        assert!(result.is_ok(), "TX OFF failed: {:?}", result.err());
    }

    #[test]
    fn test_echo_packets_received() {
        let mut conn = RadarConnection::new(radar_ip(), host_ip());
        conn.connect_full().expect("connect failed");
        conn.tx_on().expect("TX ON failed");
        conn.set_nonblocking(true).unwrap();

        let echo_socket = create_echo_socket(host_ip());
        let mut buf = [0u8; 2048];
        let start = std::time::Instant::now();
        let mut pkt_count = 0u32;

        while start.elapsed() < Duration::from_secs(5) {
            match echo_socket.recv_from(&mut buf) {
                Ok((len, _)) => {
                    assert!(len >= 18, "Echo packet too short: {len} bytes");
                    pkt_count += 1;
                    if pkt_count >= 10 { break; }
                }
                Err(_) => {}
            }
            conn.drain_control();
        }

        conn.set_nonblocking(false).unwrap();
        let _ = conn.tx_off();

        assert!(pkt_count >= 10, "Should receive echo packets, got {pkt_count}");
        println!("Received {pkt_count} echo packets");
    }

    #[test]
    fn test_full_rotation_collected() {
        let mut conn = RadarConnection::new(radar_ip(), host_ip());
        conn.connect_full().expect("connect failed");
        conn.tx_on().expect("TX ON failed");
        conn.set_nonblocking(true).unwrap();

        let echo_socket = create_echo_socket(host_ip());
        let mut prev_spoke_data = Vec::new();
        let (spokes, stats) = crate::echo::collect_rotation_stats(
            &echo_socket, &mut conn, &mut prev_spoke_data
        );

        conn.set_nonblocking(false).unwrap();
        let _ = conn.tx_off();

        assert!(!spokes.is_empty(), "Should collect spokes in a rotation");
        assert!(stats.num_spokes > 1000, "Should have 1000+ spokes, got {}", stats.num_spokes);
        assert!(stats.sweep_len > 0, "Sweep length should be > 0");
        println!("Rotation: {stats}");
    }
}

#[cfg(test)]
mod settings_tests {
    use std::time::Duration;
    use crate::connection::RadarConnection;
    use crate::protocol;
    use crate::{radar_ip, host_ip};

    fn setup_conn() -> RadarConnection {
        let mut conn = RadarConnection::new(radar_ip(), host_ip());
        conn.connect_full().expect("connect failed");
        conn
    }

    fn set_and_verify(conn: &mut RadarConnection, set_cmd: &str, read_cmd: &str, ack_prefix: &str, expect: &str) -> bool {
        let (_, _) = conn.send_and_read_ack(set_cmd, ack_prefix, Duration::from_secs(5))
            .unwrap_or((false, String::new()));
        std::thread::sleep(Duration::from_secs(3));
        let (found, line) = conn.send_and_read_ack(read_cmd, ack_prefix, Duration::from_secs(5))
            .unwrap_or((false, String::new()));
        if found && line.contains(expect) {
            true
        } else {
            eprintln!("Expected '{}' in '{}' (found={})", expect, line, found);
            false
        }
    }

    #[test]
    fn test_range_set_and_read() {
        let mut conn = setup_conn();

        // Set range to 8 NM (index 10)
        assert!(set_and_verify(&mut conn, &protocol::cmd_range(10), &protocol::query_range(), "$N62,", "$N62,10,"));

        // Restore to 0.125 NM (index 0)
        assert!(set_and_verify(&mut conn, &protocol::cmd_range(0), &protocol::query_range(), "$N62,", "$N62,0,"));
    }

    #[test]
    fn test_gain_manual_set_and_read() {
        let mut conn = setup_conn();

        assert!(set_and_verify(&mut conn, &protocol::cmd_gain(false, 75), &protocol::query_gain(), "$N63,", ",75,"));

        // Restore to auto
        let _ = conn.send_and_read_ack(&protocol::cmd_gain(true, 0), "$N63,", Duration::from_secs(5));
    }

    #[test]
    fn test_gain_auto_set_and_read() {
        let mut conn = setup_conn();

        let (_, line) = conn.send_and_read_ack(&protocol::cmd_gain(true, 0), "$N63,", Duration::from_secs(5))
            .unwrap_or((false, String::new()));
        assert!(line.contains("$N63,1,"), "Auto gain should have status=1, got: {line}");
    }

    #[test]
    fn test_pulse_width_set_and_read() {
        let mut conn = setup_conn();

        // Set to L (5)
        assert!(set_and_verify(&mut conn, &protocol::cmd_pulse_width(5, 0), &protocol::query_pulse_width(), "$N68,", "$N68,5,"));

        // Restore to S1 (0)
        let _ = conn.send_and_read_ack(&protocol::cmd_pulse_width(0, 0), "$N68,", Duration::from_secs(5));
    }

    #[test]
    fn test_rezboost_set_and_read() {
        let mut conn = setup_conn();

        // Set to High (3)
        assert!(set_and_verify(&mut conn, &protocol::cmd_rezboost(3), &protocol::query_rezboost(), "$NEE,", "$NEE,3"));

        // Restore to Off (0)
        let _ = conn.send_and_read_ack(&protocol::cmd_rezboost(0), "$NEE,", Duration::from_secs(5));
    }

    #[test]
    fn test_sea_clutter_set_and_read() {
        let mut conn = setup_conn();

        assert!(set_and_verify(&mut conn, &protocol::cmd_sea_clutter(false, 60), &protocol::query_sea_clutter(), "$N64,", ",60,"));

        // Restore
        let _ = conn.send_and_read_ack(&protocol::cmd_sea_clutter(false, 0), "$N64,", Duration::from_secs(5));
    }

    #[test]
    fn test_rain_clutter_set_and_read() {
        let mut conn = setup_conn();

        assert!(set_and_verify(&mut conn, &protocol::cmd_rain_clutter(false, 40), &protocol::query_rain_clutter(), "$N65,", ",40,"));

        // Restore
        let _ = conn.send_and_read_ack(&protocol::cmd_rain_clutter(false, 0), "$N65,", Duration::from_secs(5));
    }

    #[test]
    fn test_main_bang_set_and_read() {
        let mut conn = setup_conn();

        assert!(set_and_verify(&mut conn, &protocol::cmd_main_bang(150), &protocol::query_main_bang(), "$N83,", ",150"));

        // Restore
        let _ = conn.send_and_read_ack(&protocol::cmd_main_bang(0), "$N83,", Duration::from_secs(5));
    }

    #[test]
    fn test_near_stc_ack() {
        let mut conn = setup_conn();

        let (found, line) = conn.send_and_read_ack(&protocol::cmd_near_stc(3), "$N85,", Duration::from_secs(5))
            .unwrap_or((false, String::new()));
        assert!(found, "Near STC should ack: {line}");

        // Restore
        let _ = conn.send_and_read_ack(&protocol::cmd_near_stc(0), "$N85,", Duration::from_secs(5));
    }

    #[test]
    fn test_middle_stc_ack() {
        let mut conn = setup_conn();

        let (found, line) = conn.send_and_read_ack(&protocol::cmd_middle_stc(2), "$N86,", Duration::from_secs(5))
            .unwrap_or((false, String::new()));
        assert!(found, "Middle STC should ack: {line}");

        let _ = conn.send_and_read_ack(&protocol::cmd_middle_stc(0), "$N86,", Duration::from_secs(5));
    }

    #[test]
    fn test_far_stc_ack() {
        let mut conn = setup_conn();

        let (found, line) = conn.send_and_read_ack(&protocol::cmd_far_stc(2), "$N87,", Duration::from_secs(5))
            .unwrap_or((false, String::new()));
        assert!(found, "Far STC should ack: {line}");

        let _ = conn.send_and_read_ack(&protocol::cmd_far_stc(0), "$N87,", Duration::from_secs(5));
    }

    #[test]
    fn test_interference_rejection_ack() {
        let mut conn = setup_conn();

        let (found, line) = conn.send_and_read_ack(&protocol::cmd_interference_rejection(true), "$N67,", Duration::from_secs(5))
            .unwrap_or((false, String::new()));
        assert!(found, "IR should ack: {line}");

        let _ = conn.send_and_read_ack(&protocol::cmd_interference_rejection(false), "$N67,", Duration::from_secs(5));
    }

    #[test]
    fn test_bird_mode_set_and_read() {
        let mut conn = setup_conn();

        let (found, line) = conn.send_and_read_ack(&protocol::cmd_bird_mode(2), "$NED,", Duration::from_secs(5))
            .unwrap_or((false, String::new()));
        assert!(found, "Bird mode should ack: {line}");
        assert!(line.contains("$NED,2"), "Should reflect level 2, got: {line}");

        let _ = conn.send_and_read_ack(&protocol::cmd_bird_mode(0), "$NED,", Duration::from_secs(5));
    }

    #[test]
    fn test_doppler_set_and_read() {
        let mut conn = setup_conn();

        let (found, line) = conn.send_and_read_ack(&protocol::cmd_doppler(1), "$NEF,", Duration::from_secs(5))
            .unwrap_or((false, String::new()));
        assert!(found, "Doppler should ack: {line}");

        let _ = conn.send_and_read_ack(&protocol::cmd_doppler(0), "$NEF,", Duration::from_secs(5));
    }

    #[test]
    fn test_antenna_revolution_ack() {
        let mut conn = setup_conn();

        let (found, line) = conn.send_and_read_ack(&protocol::cmd_antenna_revolution(1, true), "$N89,", Duration::from_secs(5))
            .unwrap_or((false, String::new()));
        assert!(found, "Antenna rev should ack: {line}");

        // Restore to low speed
        let _ = conn.send_and_read_ack(&protocol::cmd_antenna_revolution(0, false), "$N89,", Duration::from_secs(5));
    }
}

#[cfg(test)]
mod echo_impact_tests {
    use std::time::Duration;
    use crate::connection::{RadarConnection, create_echo_socket};
    use crate::echo::collect_rotation_stats;
    use crate::protocol;
    use crate::{radar_ip, host_ip};

    #[test]
    fn test_gain_affects_echo_amplitude() {
        let mut conn = RadarConnection::new(radar_ip(), host_ip());
        conn.connect_full().expect("connect failed");
        conn.tx_on().expect("TX ON failed");
        conn.set_nonblocking(true).unwrap();

        let echo_socket = create_echo_socket(host_ip());
        let mut prev_spoke_data = Vec::new();

        // Reset to known state
        let _ = conn.send_and_wait_ack(&protocol::cmd_range(0), "$N62,", Duration::from_secs(5));
        let _ = conn.send_and_wait_ack(&protocol::cmd_rezboost(0), "$NEE,", Duration::from_secs(5));
        std::thread::sleep(Duration::from_secs(2));

        // Low gain
        let _ = conn.send_and_wait_ack(&protocol::cmd_gain(false, 5), "$N63,", Duration::from_secs(5));
        std::thread::sleep(Duration::from_secs(2));
        let _ = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data); // settle
        let (_, low_stats) = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data);

        // High gain
        let _ = conn.send_and_wait_ack(&protocol::cmd_gain(false, 95), "$N63,", Duration::from_secs(5));
        std::thread::sleep(Duration::from_secs(2));
        let _ = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data); // settle
        let (_, high_stats) = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data);

        // Restore
        let _ = conn.send_and_wait_ack(&protocol::cmd_gain(true, 0), "$N63,", Duration::from_secs(5));
        conn.set_nonblocking(false).unwrap();
        let _ = conn.tx_off();

        println!("Low gain:  mean_amp={:.2}, nz_bins={:.1}%", low_stats.mean_amplitude, low_stats.nonzero_bins_pct);
        println!("High gain: mean_amp={:.2}, nz_bins={:.1}%", high_stats.mean_amplitude, high_stats.nonzero_bins_pct);

        assert!(high_stats.mean_amplitude > low_stats.mean_amplitude,
            "High gain ({:.2}) should produce higher amplitude than low gain ({:.2})",
            high_stats.mean_amplitude, low_stats.mean_amplitude);
    }

    #[test]
    fn test_range_change_produces_data() {
        let mut conn = RadarConnection::new(radar_ip(), host_ip());
        conn.connect_full().expect("connect failed");
        conn.tx_on().expect("TX ON failed");
        conn.set_nonblocking(true).unwrap();

        let echo_socket = create_echo_socket(host_ip());
        let mut prev_spoke_data = Vec::new();

        // Short range
        let _ = conn.send_and_wait_ack(&protocol::cmd_range(0), "$N62,", Duration::from_secs(5));
        std::thread::sleep(Duration::from_secs(2));
        let _ = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data);
        let (_, short_stats) = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data);

        // Long range
        let _ = conn.send_and_wait_ack(&protocol::cmd_range(10), "$N62,", Duration::from_secs(5));
        std::thread::sleep(Duration::from_secs(2));
        let _ = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data);
        let (_, long_stats) = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data);

        // Restore
        let _ = conn.send_and_wait_ack(&protocol::cmd_range(0), "$N62,", Duration::from_secs(5));
        conn.set_nonblocking(false).unwrap();
        let _ = conn.tx_off();

        println!("Short range: {short_stats}");
        println!("Long range:  {long_stats}");

        // Both should produce valid data
        assert!(short_stats.num_spokes > 1000, "Short range should produce spokes");
        assert!(long_stats.num_spokes > 1000, "Long range should produce spokes");

        // Spoke count or amplitude should differ between ranges
        let spokes_differ = short_stats.num_spokes != long_stats.num_spokes;
        let amp_differs = (short_stats.mean_amplitude - long_stats.mean_amplitude).abs() > 0.1;
        assert!(spokes_differ || amp_differs,
            "Range change should produce different echo characteristics");
    }
}

#[cfg(test)]
mod render_integration_tests {
    use crate::connection::{RadarConnection, create_echo_socket};
    use crate::echo::collect_rotation_stats;
    use crate::render::{self, PPI_SIZE};
    use crate::{radar_ip, host_ip};

    #[test]
    fn test_render_live_rotation() {
        let mut conn = RadarConnection::new(radar_ip(), host_ip());
        conn.connect_full().expect("connect failed");
        conn.tx_on().expect("TX ON failed");
        conn.set_nonblocking(true).unwrap();

        let echo_socket = create_echo_socket(host_ip());
        let mut prev_spoke_data = Vec::new();
        let (spokes, _stats) = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data);

        conn.set_nonblocking(false).unwrap();
        let _ = conn.tx_off();

        assert!(!spokes.is_empty());

        let pixels = render::render_ppi(&spokes);
        assert_eq!(pixels.len(), PPI_SIZE * PPI_SIZE * 4);

        let png = render::encode_png_bytes(&pixels, PPI_SIZE, PPI_SIZE);
        assert!(png.len() > 100, "PNG should be substantial, got {} bytes", png.len());
        assert_eq!(&png[1..4], b"PNG", "Should be valid PNG");

        // Verify some pixels are non-black (we have real echo data)
        let mut nonzero_count = 0u64;
        for i in (0..pixels.len()).step_by(4) {
            if pixels[i] > 0 || pixels[i + 1] > 0 || pixels[i + 2] > 0 {
                nonzero_count += 1;
            }
        }
        assert!(nonzero_count > 100, "Live PPI should have visible echo pixels, got {nonzero_count}");
        println!("PPI rendered: {nonzero_count} non-black pixels, PNG={} bytes", png.len());
    }
}
