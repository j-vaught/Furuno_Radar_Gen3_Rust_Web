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

pub const RADAR_IP: Ipv4Addr = Ipv4Addr::new(172, 31, 3, 226);
pub const HOST_IP: Ipv4Addr = Ipv4Addr::new(172, 31, 1, 100);

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let mode = args.get(1).map(|s| s.as_str()).unwrap_or("handshake");

    match mode {
        "handshake" => modes::run_handshake(),
        "tx-on" => modes::run_tx_on(),
        "tx-off" => modes::run_tx_off(),
        "echo" => modes::run_echo_listen(),
        "full" => modes::run_full(),
        "command" => modes::run_command_test(),
        "render-csv" => {
            let csv_path = args.get(2).expect("Usage: radar-client render-csv <csv_file> [output.png]");
            let out_path = args.get(3).map(|s| s.as_str()).unwrap_or("radar_ppi.png");
            modes::run_render_csv(csv_path, out_path);
        }
        "render-live" => {
            let out_path = args.get(2).map(|s| s.as_str()).unwrap_or("radar_live.png");
            modes::run_render_live(out_path);
        }
        "dump-raw" => {
            let count: usize = args.get(2).and_then(|s| s.parse().ok()).unwrap_or(5);
            modes::run_dump_raw(count);
        }
        "test-commands" => testing::run_test_commands(),
        "calibrate" => calibrate::run_calibrate(&args[2..]),
        "discover" => modes::run_discover(),
        "validate" => testing::run_validate(),
        "web" => {
            let port: u16 = args.get(2).and_then(|s| s.parse().ok()).unwrap_or(8080);
            let web_dir = args.get(3).map(|s| s.as_str()).unwrap_or("../web");
            web::start_server(RADAR_IP, HOST_IP, web_dir, port);
        }
        _ => {
            eprintln!("Usage: radar-client <handshake|tx-on|tx-off|echo|full|command|render-csv|render-live|test-commands|calibrate|discover|validate|web>");
            std::process::exit(1);
        }
    }
}
