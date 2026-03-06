use std::time::Duration;

use crate::protocol;
use crate::connection::{self, RadarConnection, create_echo_socket};
use crate::echo::collect_rotation_stats;
use crate::render::{self, PPI_SIZE};
use crate::{RADAR_IP, HOST_IP};

pub fn run_calibrate(args: &[String]) {
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
                let pixels = render::render_ppi(&spokes);
                render::write_png(&png_path, &pixels, PPI_SIZE, PPI_SIZE);
            }
        }

        prev_step = Some(step);
    }

    // TX OFF
    conn.set_nonblocking(false).unwrap();
    let _ = conn.tx_off();
    println!("\nCalibration complete. {} steps executed.", total_steps);
}
