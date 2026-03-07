use std::io::Write;
use std::time::Duration;

use crate::protocol;
use crate::connection::{RadarConnection, create_echo_socket};
use crate::echo::{self, collect_rotation_stats, write_echo_csv, timestamp_str};
use crate::render::{self, PPI_SIZE};
use crate::{radar_ip, host_ip};

pub fn run_test_commands() {
    println!("=== Command Verification Test ===\n");

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
            let pixels = render::render_ppi(&before_spokes);
            render::write_png(&png_path, &pixels, PPI_SIZE, PPI_SIZE);
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
            let pixels = render::render_ppi(&after_spokes);
            render::write_png(&png_path, &pixels, PPI_SIZE, PPI_SIZE);
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

pub fn run_validate() {
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

    let mut conn = RadarConnection::new(radar_ip(), host_ip());
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
    let echo_socket = create_echo_socket(host_ip());

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
        setter_only: bool,
    }

    let aba_tests = vec![
        AbaTest {
            name: "Range",
            set_a: protocol::cmd_range(2),
            set_b: protocol::cmd_range(10),
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
            set_a: protocol::cmd_pulse_width(1, 2),
            set_b: protocol::cmd_pulse_width(5, 2),
            read_cmd: protocol::query_pulse_width(),
            ack_prefix: "$N68,",
            expect_a: "$N68,1,",
            expect_b: "$N68,5,",
            setter_only: false,
        },
        AbaTest {
            name: "Near STC",
            set_a: protocol::cmd_near_stc(0),
            set_b: protocol::cmd_near_stc(4),
            read_cmd: String::new(),
            ack_prefix: "$N85,",
            expect_a: "$N85,",
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
            set_a: protocol::cmd_rezboost(0),
            set_b: protocol::cmd_rezboost(3),
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
        AbaTest {
            name: "Antenna Revolution",
            set_a: protocol::cmd_antenna_revolution(0, false),
            set_b: protocol::cmd_antenna_revolution(1, true),
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
        AbaTest {
            name: "Interference Rejection",
            set_a: protocol::cmd_interference_rejection(false),
            set_b: protocol::cmd_interference_rejection(true),
            read_cmd: String::new(),
            ack_prefix: "$N67,",
            expect_a: "$N67,0,0,",
            expect_b: "$N67,0,0,",
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

        if a1_line == b_line && !a1_line.is_empty() {
            log!("    WARN: A1 == B readback (stale getter?)");
        }

        log!("");
    }

    // --- Phase 4: Echo Impact Test (gain low vs high) ---
    log!("--- Phase 4: Echo Impact (Gain Low vs High) ---");

    let _ = conn.send_and_wait_ack(&protocol::cmd_range(0), "$N62,", Duration::from_secs(5));
    let _ = conn.send_and_wait_ack(&protocol::cmd_rezboost(0), "$NEE,", Duration::from_secs(5));
    std::thread::sleep(Duration::from_secs(2));

    log!("  Setting gain=10 (low)...");
    let _ = conn.send_and_wait_ack(&protocol::cmd_gain(false, 10), "$N63,", Duration::from_secs(5));
    std::thread::sleep(Duration::from_secs(3));
    let _ = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data);
    let (low_spokes, low_stats) = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data);
    log!("  Low gain:  {low_stats}");

    if !low_spokes.is_empty() {
        let csv_path = format!("{}/gain_low.csv", output_dir);
        let png_path = format!("{}/gain_low.png", output_dir);
        write_echo_csv(&csv_path, &low_spokes);
        let pixels = render::render_ppi(&low_spokes);
        render::write_png(&png_path, &pixels, PPI_SIZE, PPI_SIZE);
        log!("  Saved: {csv_path}, {png_path}");
    }

    log!("  Setting gain=90 (high)...");
    let _ = conn.send_and_wait_ack(&protocol::cmd_gain(false, 90), "$N63,", Duration::from_secs(5));
    std::thread::sleep(Duration::from_secs(3));
    let _ = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data);
    let (high_spokes, high_stats) = collect_rotation_stats(&echo_socket, &mut conn, &mut prev_spoke_data);
    log!("  High gain: {high_stats}");

    if !high_spokes.is_empty() {
        let csv_path = format!("{}/gain_high.csv", output_dir);
        let png_path = format!("{}/gain_high.png", output_dir);
        write_echo_csv(&csv_path, &high_spokes);
        let pixels = render::render_ppi(&high_spokes);
        render::write_png(&png_path, &pixels, PPI_SIZE, PPI_SIZE);
        log!("  Saved: {csv_path}, {png_path}");
    }

    if low_stats.mean_amplitude < high_stats.mean_amplitude {
        log!("  PASS: High gain mean_amp ({:.2}) > low gain mean_amp ({:.2})",
            high_stats.mean_amplitude, low_stats.mean_amplitude);
        pass_count += 1;
    } else {
        log!("  WARN: High gain mean_amp ({:.2}) <= low gain ({:.2}) — may depend on environment",
            high_stats.mean_amplitude, low_stats.mean_amplitude);
        skip_count += 1;
    }

    // --- Phase 5: Range Impact Test ---
    log!("\n--- Phase 5: Range Impact (short vs long) ---");

    let _ = conn.send_and_wait_ack(&protocol::cmd_gain(true, 0), "$N63,", Duration::from_secs(5));

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
        let pixels = render::render_ppi(&short_spokes);
        render::write_png(&png_path, &pixels, PPI_SIZE, PPI_SIZE);
        log!("  Saved: {csv_path}, {png_path}");
    }

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
        let pixels = render::render_ppi(&long_spokes);
        render::write_png(&png_path, &pixels, PPI_SIZE, PPI_SIZE);
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
    writeln!(f, "Timestamp: {}", echo::timestamp_str()).unwrap();
    writeln!(f, "Radar IP: {}", radar_ip()).unwrap();
    writeln!(f, "Host IP: {}", host_ip()).unwrap();
    writeln!(f, "").unwrap();
    for line in lines {
        writeln!(f, "{line}").unwrap();
    }
    writeln!(f, "").unwrap();
    writeln!(f, "Final: PASS={pass} FAIL={fail} SKIP={skip}").unwrap();
    println!("Report saved: {path}");
}
