#[allow(dead_code)]
mod protocol;
mod connection;
mod render;
mod web;
mod echo;
mod modes;
mod testing;
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

#[derive(Debug, Default, PartialEq, Eq)]
struct CliOptions {
    radar_ip: Option<Ipv4Addr>,
    host_ip: Option<Ipv4Addr>,
    interface: Option<String>,
    configure_network: bool,
    positional: Vec<String>,
}

fn parse_args(args: &[String]) -> Result<CliOptions, String> {
    let mut options = CliOptions::default();
    let mut i = 1;
    while i < args.len() {
        match args[i].as_str() {
            "--radar-ip" | "--host-ip" | "--interface" => {
                let flag = args[i].as_str();
                let value = args.get(i + 1).ok_or_else(|| format!("{flag} requires a value"))?;
                if value.starts_with('-') {
                    return Err(format!("{flag} requires a value"));
                }
                match flag {
                    "--radar-ip" => {
                        options.radar_ip = Some(value.parse().map_err(|_| format!("Invalid radar IPv4 address {value:?}"))?);
                    }
                    "--host-ip" => {
                        options.host_ip = Some(value.parse().map_err(|_| format!("Invalid host IPv4 address {value:?}"))?);
                    }
                    "--interface" => options.interface = Some(value.clone()),
                    _ => unreachable!(),
                }
                i += 2;
            }
            "--configure-network" => {
                options.configure_network = true;
                i += 1;
            }
            value if value.starts_with("--") => {
                return Err(format!("Unknown option {value}"));
            }
            value => {
                options.positional.push(value.to_string());
                i += 1;
            }
        }
    }
    Ok(options)
}

fn resolve_ips(options: &CliOptions) -> Result<(), String> {
    if let Some(interface_name) = options.interface.as_deref() {
        if protocol::detect_interfaces().iter().all(|iface| iface.name != interface_name) {
            return Err(format!(
                "Interface {interface_name:?} has no detected IPv4 address. Connect it and retry, or omit --interface."
            ));
        }
    }

    if let Some(rip) = options.radar_ip {
        let hip = match options.host_ip {
            Some(hip) => hip,
            None => protocol::ensure_subnet_reachable(
                rip,
                DEFAULT_HOST_IP,
                options.interface.as_deref(),
                options.configure_network,
            )?,
        };
        let _ = RESOLVED_RADAR_IP.set(rip);
        let _ = RESOLVED_HOST_IP.set(hip);
        println!("Using radar={rip}, host={hip} (from args)");
        return Ok(());
    }

    // Try auto-discovery (3 second timeout)
    println!("Auto-discovering radar...");
    if let Some((rip, discovered_host_ip)) =
        protocol::discover_radar_on_interface(3, options.interface.as_deref())
    {
        let hip = options.host_ip.unwrap_or(discovered_host_ip);
        let _ = RESOLVED_RADAR_IP.set(rip);
        let _ = RESOLVED_HOST_IP.set(hip);
        println!("Using radar={rip}, host={hip} (auto-discovered)");
        return Ok(());
    }

    // Keep the known radar fallback, but require an explicit host address or
    // an explicitly authorized interface configuration.
    let rip = DEFAULT_RADAR_IP;
    let hip = match options.host_ip {
        Some(hip) => hip,
        None => protocol::ensure_subnet_reachable(
            rip,
            DEFAULT_HOST_IP,
            options.interface.as_deref(),
            options.configure_network,
        )?,
    };
    let _ = RESOLVED_RADAR_IP.set(rip);
    let _ = RESOLVED_HOST_IP.set(hip);
    println!("Discovery failed. Falling back to radar={rip}, host={hip}");
    Ok(())
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let options = match parse_args(&args) {
        Ok(options) => options,
        Err(error) => {
            eprintln!("{error}");
            eprintln!("Usage: furuno-server [--radar-ip IP] [--host-ip IP] [--interface NAME] [--configure-network] <mode>");
            std::process::exit(2);
        }
    };

    let mode = options.positional.first().map(|s| s.as_str()).unwrap_or("handshake");

    // Resolve IPs for modes that need the radar
    if let Err(error) = resolve_ips(&options) {
        eprintln!("Startup network configuration failed.\n{error}");
        std::process::exit(2);
    }

    match mode {
        "handshake" => modes::run_handshake(),
        "tx-on" => modes::run_tx_on(),
        "tx-off" => modes::run_tx_off(),
        "echo" => modes::run_echo_listen(),
        "full" => modes::run_full(),
        "command" => modes::run_command_test(),
        "render-live" => {
            let out_path = options.positional.get(1).map(|s| s.as_str()).unwrap_or("radar_live.png");
            modes::run_render_live(out_path);
        }
        "dump-raw" => {
            let count: usize = options.positional.get(1).and_then(|s| s.parse().ok()).unwrap_or(5);
            modes::run_dump_raw(count);
        }
        "test-commands" => testing::run_test_commands(),
        "discover" => modes::run_discover(),
        "validate" => testing::run_validate(),
        "web" => {
            let port: u16 = options.positional.get(1).and_then(|s| s.parse().ok()).unwrap_or(8080);
            let web_dir = options.positional.get(2).map(|s| s.as_str()).unwrap_or("../web");
            web::start_server(radar_ip(), host_ip(), web_dir, port);
        }
        _ => {
            eprintln!("Usage: furuno-server [--radar-ip IP] [--host-ip IP] [--interface NAME] [--configure-network] <mode>");
            eprintln!("Modes: handshake|tx-on|tx-off|echo|full|command|render-live|test-commands|discover|validate|web");
            std::process::exit(1);
        }
    }
}

#[cfg(test)]
mod cli_tests {
    use super::*;

    #[test]
    fn parses_interface_and_opt_in_network_configuration() {
        let args = vec![
            "furuno-server".into(),
            "--radar-ip".into(),
            "172.31.3.226".into(),
            "--host-ip".into(),
            "172.31.1.100".into(),
            "--interface".into(),
            "en5".into(),
            "--configure-network".into(),
            "handshake".into(),
        ];
        let parsed = parse_args(&args).unwrap();

        assert_eq!(parsed.radar_ip, Some(Ipv4Addr::new(172, 31, 3, 226)));
        assert_eq!(parsed.host_ip, Some(Ipv4Addr::new(172, 31, 1, 100)));
        assert_eq!(parsed.interface.as_deref(), Some("en5"));
        assert!(parsed.configure_network);
        assert_eq!(parsed.positional, vec!["handshake"]);
    }

    #[test]
    fn rejects_missing_interface_value() {
        let args = vec!["furuno-server".into(), "--interface".into()];
        assert!(parse_args(&args).is_err());
    }
}
