#[allow(dead_code)]
mod protocol;
mod connection;
mod render;
mod web;
mod echo;
mod modes;
mod testing;
mod calibrate;
#[cfg(test)]
mod tests;

use std::net::Ipv4Addr;
use std::sync::OnceLock;

const DEFAULT_RADAR_IP: Ipv4Addr = Ipv4Addr::new(172, 31, 3, 226);
const DEFAULT_HOST_IP: Ipv4Addr = Ipv4Addr::new(172, 31, 1, 100);

static RESOLVED_RADAR_IP: OnceLock<Ipv4Addr> = OnceLock::new();
static RESOLVED_HOST_IP: OnceLock<Ipv4Addr> = OnceLock::new();

pub fn radar_ip() -> Ipv4Addr {
    *RESOLVED_RADAR_IP.get().unwrap_or(&DEFAULT_RADAR_IP)
}

pub fn host_ip() -> Ipv4Addr {
    *RESOLVED_HOST_IP.get().unwrap_or(&DEFAULT_HOST_IP)
}

fn resolve_ips(args: &[String]) {
    let mut radar_ip: Option<Ipv4Addr> = None;
    let mut host_ip: Option<Ipv4Addr> = None;
    let mut i = 0;
    while i < args.len() {
        match args[i].as_str() {
            "--radar-ip" if i + 1 < args.len() => {
                radar_ip = args[i + 1].parse().ok();
                i += 2;
            }
            "--host-ip" if i + 1 < args.len() => {
                host_ip = args[i + 1].parse().ok();
                i += 2;
            }
            _ => { i += 1; }
        }
    }

    if let Some(rip) = radar_ip {
        let hip = host_ip.unwrap_or_else(|| {
            protocol::detect_host_ip(rip).unwrap_or(DEFAULT_HOST_IP)
        });
        let _ = RESOLVED_RADAR_IP.set(rip);
        let _ = RESOLVED_HOST_IP.set(hip);
        println!("Using radar={rip}, host={hip} (from args)");
        return;
    }

    // Try auto-discovery (3 second timeout)
    println!("Auto-discovering radar...");
    if let Some((rip, hip)) = protocol::discover_radar(3) {
        let hip = host_ip.unwrap_or(hip);
        let _ = RESOLVED_RADAR_IP.set(rip);
        let _ = RESOLVED_HOST_IP.set(hip);
        println!("Using radar={rip}, host={hip} (auto-discovered)");
        return;
    }

    // Fall back to defaults
    let hip = host_ip.unwrap_or(DEFAULT_HOST_IP);
    let _ = RESOLVED_RADAR_IP.set(DEFAULT_RADAR_IP);
    let _ = RESOLVED_HOST_IP.set(hip);
    println!("Discovery failed. Falling back to radar={}, host={hip}", DEFAULT_RADAR_IP);
}

fn main() {
    let args: Vec<String> = std::env::args().collect();

    // Filter out --radar-ip/--host-ip flags to get positional args
    let mut positional: Vec<String> = Vec::new();
    let mut i = 1;
    while i < args.len() {
        match args[i].as_str() {
            "--radar-ip" | "--host-ip" => { i += 2; } // skip flag + value
            _ => { positional.push(args[i].clone()); i += 1; }
        }
    }

    let mode = positional.first().map(|s| s.as_str()).unwrap_or("handshake");

    // Modes that don't need radar connection
    if mode == "render-csv" {
        let csv_path = positional.get(1).expect("Usage: radar-client render-csv <csv_file> [output.png]");
        let out_path = positional.get(2).map(|s| s.as_str()).unwrap_or("radar_ppi.png");
        modes::run_render_csv(csv_path, out_path);
        return;
    }

    // Resolve IPs for modes that need the radar
    resolve_ips(&args);

    match mode {
        "handshake" => modes::run_handshake(),
        "tx-on" => modes::run_tx_on(),
        "tx-off" => modes::run_tx_off(),
        "echo" => modes::run_echo_listen(),
        "full" => modes::run_full(),
        "command" => modes::run_command_test(),
        "render-live" => {
            let out_path = positional.get(1).map(|s| s.as_str()).unwrap_or("radar_live.png");
            modes::run_render_live(out_path);
        }
        "dump-raw" => {
            let count: usize = positional.get(1).and_then(|s| s.parse().ok()).unwrap_or(5);
            modes::run_dump_raw(count);
        }
        "test-commands" => testing::run_test_commands(),
        "calibrate" => calibrate::run_calibrate(&positional[1..]),
        "discover" => modes::run_discover(),
        "validate" => testing::run_validate(),
        "web" => {
            let port: u16 = positional.get(1).and_then(|s| s.parse().ok()).unwrap_or(8080);
            let web_dir = positional.get(2).map(|s| s.as_str()).unwrap_or("../web");
            web::start_server(radar_ip(), host_ip(), web_dir, port);
        }
        _ => {
            eprintln!("Usage: radar-client [--radar-ip IP] [--host-ip IP] <mode>");
            eprintln!("Modes: handshake|tx-on|tx-off|echo|full|command|render-csv|render-live|test-commands|calibrate|discover|validate|web");
            std::process::exit(1);
        }
    }
}
