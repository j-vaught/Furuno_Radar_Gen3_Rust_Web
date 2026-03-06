#![allow(dead_code)]

use std::io::{Read, Write};
use std::net::{Ipv4Addr, SocketAddr, TcpStream, UdpSocket};
use std::os::unix::io::AsRawFd;
use std::time::{Duration, Instant};

use crate::protocol;

// --- Error type ---

#[derive(Debug)]
pub enum RadarError {
    AuthFailed(String),
    ConnectionFailed(String),
    IoError(std::io::Error),
    Timeout,
    InvalidState { expected: &'static str, actual: ConnectionState },
    ProtocolError(String),
}

impl From<std::io::Error> for RadarError {
    fn from(e: std::io::Error) -> Self {
        RadarError::IoError(e)
    }
}

impl std::fmt::Display for RadarError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            RadarError::AuthFailed(msg) => write!(f, "Auth failed: {msg}"),
            RadarError::ConnectionFailed(msg) => write!(f, "Connection failed: {msg}"),
            RadarError::IoError(e) => write!(f, "IO error: {e}"),
            RadarError::Timeout => write!(f, "Timeout"),
            RadarError::InvalidState { expected, actual } => {
                write!(f, "Invalid state: expected {expected}, got {actual:?}")
            }
            RadarError::ProtocolError(msg) => write!(f, "Protocol error: {msg}"),
        }
    }
}

// --- Connection state ---

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum ConnectionState {
    Disconnected,
    Authenticated,
    ControlConnected,
    Initialized,
    Transmitting,
}

// --- Parsed responses ---

#[derive(Debug)]
pub enum RadarResponse {
    ClientConnection,
    Keepalive,
    RegisterValue { id: String, values: Vec<String> },
    Unknown(String),
}

// --- RadarConnection ---

pub struct RadarConnection {
    pub radar_ip: Ipv4Addr,
    pub host_ip: Ipv4Addr,
    ctrl: Option<TcpStream>,
    pub state: ConnectionState,
    pub firmware: Option<String>,
    last_keepalive: Instant,
    line_buf: String,
}

impl RadarConnection {
    pub fn new(radar_ip: Ipv4Addr, host_ip: Ipv4Addr) -> Self {
        Self {
            radar_ip,
            host_ip,
            ctrl: None,
            state: ConnectionState::Disconnected,
            firmware: None,
            last_keepalive: Instant::now(),
            line_buf: String::new(),
        }
    }

    pub fn auth(&mut self) -> Result<(), RadarError> {
        let addr = SocketAddr::from((self.radar_ip, protocol::FURUNO_BEACON_PORT));
        println!("Connecting to {addr} for auth...");

        let mut stream = TcpStream::connect_timeout(&addr, Duration::from_secs(5))
            .map_err(|e| RadarError::ConnectionFailed(format!("{e}")))?;

        stream.set_read_timeout(Some(Duration::from_secs(3)))?;

        println!("Sending auth payload ({} bytes)...", protocol::AUTH_PAYLOAD.len());
        stream.write_all(protocol::AUTH_PAYLOAD)?;

        let mut resp = [0u8; 64];
        let n = stream.read(&mut resp)
            .map_err(|e| RadarError::AuthFailed(format!("no response: {e}")))?;

        print!("Auth response ({n} bytes): ");
        for b in &resp[..n] {
            print!("{b:02X} ");
        }
        println!();

        if n >= 10 && resp[0] == 0x09 && resp[1] == 0x01 && resp[9] == 0x64 {
            println!("Auth SUCCESS (status=0x64)");
            self.state = ConnectionState::Authenticated;
            Ok(())
        } else {
            Err(RadarError::AuthFailed("unexpected response".into()))
        }
    }

    pub fn connect_control(&mut self) -> Result<(), RadarError> {
        if self.state == ConnectionState::Disconnected {
            return Err(RadarError::InvalidState {
                expected: "Authenticated",
                actual: self.state,
            });
        }

        let addr = SocketAddr::from((self.radar_ip, protocol::FURUNO_CONTROL_PORT));
        println!("Connecting control channel to {addr}...");

        let stream = TcpStream::connect_timeout(&addr, Duration::from_secs(5))
            .map_err(|e| RadarError::ConnectionFailed(format!("{e}")))?;

        stream.set_read_timeout(Some(Duration::from_millis(500)))?;
        stream.set_nodelay(true)?;

        println!("Control channel connected");
        self.ctrl = Some(stream);
        self.last_keepalive = Instant::now();

        // Read initial messages (expect $N00,CLIENTCONNECTION)
        let responses = self.read_responses(Duration::from_secs(2));
        for resp in &responses {
            match resp {
                RadarResponse::ClientConnection => {
                    println!("  <- CLIENTCONNECTION confirmed");
                }
                RadarResponse::Keepalive => {
                    println!("  <- [keepalive]");
                }
                RadarResponse::RegisterValue { id, values } => {
                    println!("  <- $N{id},{}", values.join(","));
                }
                RadarResponse::Unknown(s) => {
                    println!("  <- {s}");
                }
            }
        }

        self.state = ConnectionState::ControlConnected;
        Ok(())
    }

    pub fn init(&mut self) -> Result<(), RadarError> {
        if self.state != ConnectionState::ControlConnected {
            return Err(RadarError::InvalidState {
                expected: "ControlConnected",
                actual: self.state,
            });
        }

        // Firmware query
        println!("Sending firmware query: $R96,,,,,,,");
        self.send_command("$R96,,,,,,,\r\n")?;
        let responses = self.read_responses(Duration::from_secs(2));
        for resp in &responses {
            match resp {
                RadarResponse::RegisterValue { id, values } => {
                    println!("  <- $N{id},{}", values.join(","));
                    if id == "96" {
                        self.firmware = Some(values.join(","));
                    }
                }
                RadarResponse::Keepalive => println!("  <- [keepalive]"),
                RadarResponse::ClientConnection => println!("  <- CLIENTCONNECTION"),
                RadarResponse::Unknown(s) => println!("  <- {s}"),
            }
        }

        // Send all init register reads
        println!("Sending init register reads...");
        let init_cmds = protocol::init_register_reads();
        let ctrl = self.ctrl.as_mut().ok_or(RadarError::ConnectionFailed("no control stream".into()))?;
        for cmd in &init_cmds {
            ctrl.write_all(cmd.as_bytes())?;
        }

        let responses = self.read_responses(Duration::from_secs(3));
        for resp in &responses {
            match resp {
                RadarResponse::RegisterValue { id, values } => {
                    println!("  <- $N{id},{}", values.join(","));
                }
                RadarResponse::Keepalive => println!("  <- [keepalive]"),
                _ => {}
            }
        }

        println!("Init complete.");
        self.state = ConnectionState::Initialized;
        Ok(())
    }

    pub fn tx_on(&mut self) -> Result<(), RadarError> {
        if self.state != ConnectionState::Initialized && self.state != ConnectionState::Transmitting {
            return Err(RadarError::InvalidState {
                expected: "Initialized or Transmitting",
                actual: self.state,
            });
        }

        println!("Sending TX ON...");
        self.send_command("$S69,2,0,0,0,0,0\r\n")?;
        self.send_command("$S85,0\r\n")?;
        self.send_command("$S86,0\r\n")?;
        self.send_command("$S87,0\r\n")?;
        let responses = self.read_responses(Duration::from_secs(3));
        for resp in &responses {
            if let RadarResponse::RegisterValue { id, values } = resp {
                println!("  <- $N{id},{}", values.join(","));
            }
        }
        println!("TX ON sent.");
        self.state = ConnectionState::Transmitting;
        Ok(())
    }

    pub fn tx_off(&mut self) -> Result<(), RadarError> {
        if self.state != ConnectionState::Initialized && self.state != ConnectionState::Transmitting {
            return Err(RadarError::InvalidState {
                expected: "Initialized or Transmitting",
                actual: self.state,
            });
        }

        println!("Sending TX OFF...");
        self.send_command("$S69,1,0,0,0,0,0\r\n")?;
        self.send_command("$S85,0\r\n")?;
        self.send_command("$S86,0\r\n")?;
        self.send_command("$S87,0\r\n")?;
        let responses = self.read_responses(Duration::from_secs(3));
        for resp in &responses {
            if let RadarResponse::RegisterValue { id, values } = resp {
                println!("  <- $N{id},{}", values.join(","));
            }
        }
        println!("TX OFF sent.");
        self.state = ConnectionState::Initialized;
        Ok(())
    }

    pub fn connect_full(&mut self) -> Result<(), RadarError> {
        self.auth()?;
        println!();
        self.connect_control()?;
        self.init()?;
        Ok(())
    }

    pub fn send_command(&mut self, cmd: &str) -> Result<(), RadarError> {
        let ctrl = self.ctrl.as_mut()
            .ok_or(RadarError::ConnectionFailed("no control stream".into()))?;
        ctrl.write_all(cmd.as_bytes())?;
        Ok(())
    }

    pub fn send_and_wait_ack(
        &mut self,
        cmd: &str,
        expected_prefix: &str,
        timeout: Duration,
    ) -> Result<bool, RadarError> {
        let ctrl = self.ctrl.as_mut()
            .ok_or(RadarError::ConnectionFailed("no control stream".into()))?;

        ctrl.set_nonblocking(false)?;
        ctrl.set_read_timeout(Some(Duration::from_secs(3)))?;
        ctrl.write_all(cmd.as_bytes())?;

        let mut buf = [0u8; 4096];
        let deadline = Instant::now() + timeout;
        let mut all_responses = String::new();

        while Instant::now() < deadline {
            match ctrl.read(&mut buf) {
                Ok(n) if n > 0 => {
                    let text = String::from_utf8_lossy(&buf[..n]);
                    all_responses.push_str(&text);
                    for line in all_responses.lines() {
                        if line.starts_with(expected_prefix) {
                            println!("    ACK: {line}");
                            ctrl.set_nonblocking(true)?;
                            return Ok(true);
                        }
                    }
                }
                _ => {}
            }
        }
        eprintln!(
            "    No ACK for {expected_prefix} (got: {})",
            all_responses.trim().replace('\n', " | ")
        );
        ctrl.set_nonblocking(true)?;
        Ok(false)
    }

    pub fn read_responses(&mut self, timeout: Duration) -> Vec<RadarResponse> {
        let ctrl = match self.ctrl.as_mut() {
            Some(c) => c,
            None => return Vec::new(),
        };

        let _ = ctrl.set_read_timeout(Some(timeout));
        let mut buf = [0u8; 4096];
        let mut responses = Vec::new();

        loop {
            match ctrl.read(&mut buf) {
                Ok(0) => break,
                Ok(n) => {
                    let data = &buf[..n];

                    // Check for binary keepalive
                    if n == 8 && data == protocol::KEEPALIVE {
                        responses.push(RadarResponse::Keepalive);
                        let _ = ctrl.set_read_timeout(Some(Duration::from_millis(200)));
                        continue;
                    }

                    // Parse ASCII messages
                    let text = String::from_utf8_lossy(data);
                    self.line_buf.push_str(&text);

                    // Process complete lines
                    while let Some(pos) = self.line_buf.find('\n') {
                        let line: String = self.line_buf.drain(..=pos).collect();
                        let line = line.trim();
                        if line.is_empty() {
                            continue;
                        }
                        responses.push(parse_response_line(line));
                    }

                    // Shorter timeout for subsequent reads
                    let _ = ctrl.set_read_timeout(Some(Duration::from_millis(200)));
                }
                Err(_) => break,
            }
        }

        // Process any remaining data in the line buffer
        if !self.line_buf.is_empty() {
            let remaining = std::mem::take(&mut self.line_buf);
            let line = remaining.trim();
            if !line.is_empty() {
                responses.push(parse_response_line(line));
            }
        }

        responses
    }

    pub fn send_keepalive(&mut self) -> Result<(), RadarError> {
        if self.last_keepalive.elapsed() > Duration::from_secs(5) {
            if let Some(ctrl) = self.ctrl.as_mut() {
                let _ = ctrl.write_all(protocol::KEEPALIVE);
                self.last_keepalive = Instant::now();
            }
        }
        Ok(())
    }

    pub fn drain_control(&mut self) {
        if let Some(ctrl) = self.ctrl.as_mut() {
            let mut buf = [0u8; 4096];
            let _ = ctrl.read(&mut buf);
        }
    }

    pub fn ctrl_stream(&mut self) -> Result<&mut TcpStream, RadarError> {
        self.ctrl.as_mut()
            .ok_or(RadarError::ConnectionFailed("no control stream".into()))
    }

    pub fn set_nonblocking(&mut self, nonblocking: bool) -> Result<(), RadarError> {
        if let Some(ctrl) = self.ctrl.as_mut() {
            ctrl.set_nonblocking(nonblocking)?;
        }
        Ok(())
    }
}

// --- Response parsing ---

fn parse_response_line(line: &str) -> RadarResponse {
    if let Some((id, values)) = protocol::parse_response(line) {
        if id == "00" && values.first().map(|v| v.contains("CLIENTCONNECTION")).unwrap_or(false) {
            return RadarResponse::ClientConnection;
        }
        return RadarResponse::RegisterValue {
            id: id.to_string(),
            values: values.iter().map(|s| s.to_string()).collect(),
        };
    }
    if line.starts_with('$') {
        RadarResponse::Unknown(line.to_string())
    } else {
        RadarResponse::Unknown(line.to_string())
    }
}

// --- Echo socket helper ---

pub fn create_echo_socket(host_ip: Ipv4Addr) -> UdpSocket {
    let sock = socket2::Socket::new(
        socket2::Domain::IPV4,
        socket2::Type::DGRAM,
        Some(socket2::Protocol::UDP),
    )
    .expect("Failed to create echo socket");

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

    let bind_result = sock.bind(
        &SocketAddr::from((Ipv4Addr::UNSPECIFIED, protocol::FURUNO_DATA_PORT)).into(),
    );
    if bind_result.is_err() {
        sock.bind(
            &SocketAddr::from((protocol::FURUNO_MULTICAST, protocol::FURUNO_DATA_PORT)).into(),
        )
        .expect("Failed to bind echo socket");
    }

    let echo_socket: UdpSocket = sock.into();
    echo_socket
        .join_multicast_v4(&protocol::FURUNO_MULTICAST, &host_ip)
        .expect("Failed to join multicast group");
    echo_socket
        .set_read_timeout(Some(Duration::from_millis(500)))
        .unwrap();

    echo_socket
}
