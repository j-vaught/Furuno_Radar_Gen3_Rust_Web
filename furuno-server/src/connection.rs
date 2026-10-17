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

/// One acknowledged register read used to populate recording metadata.
/// `raw` is retained verbatim so later analysis can re-interpret fields when
/// the device protocol semantics are refined.
#[derive(Clone, Debug)]
pub struct RegisterReadback {
    pub requested: String,
    pub register: String,
    pub raw: Option<String>,
    pub values: Vec<String>,
    pub error: Option<String>,
    pub requested_at: String,
    pub read_at: String,
    pub query_duration_ms: u64,
}

#[derive(Clone, Debug, Default)]
pub struct CurrentRadarReadback {
    pub reads: Vec<RegisterReadback>,
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

        let mut stream = connect_from_host(self.host_ip, addr, Duration::from_secs(5))?;

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

        let stream = connect_from_host(self.host_ip, addr, Duration::from_secs(5))?;

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
        self.confirm_tx_command(2)?;
        println!("TX ON acknowledged.");
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
        self.confirm_tx_command(1)?;
        println!("TX OFF acknowledged.");
        self.state = ConnectionState::Initialized;
        Ok(())
    }

    fn confirm_tx_command(&mut self, desired: u16) -> Result<(), RadarError> {
        let command = protocol::cmd_tx_state(desired);
        let (acknowledged, line) = self.send_and_read_ack(
            &command, "$N69,", Duration::from_secs(3),
        )?;
        if !acknowledged {
            return Err(RadarError::Timeout);
        }
        validate_tx_ack(&line, desired)
    }

    pub fn connect_full(&mut self) -> Result<(), RadarError> {
        self.auth()?;
        println!();
        self.connect_control()?;
        self.init()?;
        Ok(())
    }

    /// Read the settings that are persisted alongside an experiment.
    ///
    /// Each request is independent so one unsupported or timed-out register
    /// does not erase the successful reads. Callers must inspect every
    /// `error` and never treat a missing read as a verified value.
    pub fn read_current_settings(&mut self) -> CurrentRadarReadback {
        let mut result = CurrentRadarReadback::default();
        append_register_readback(&mut result, self, "$R62,0,0,0,0\r\n".into(), "$N62,");
        let range = result.reads.last()
            .and_then(|read| read.values.first())
            .and_then(|value| value.parse::<u16>().ok())
            .unwrap_or(0);
        append_register_readback(&mut result, self, "$R63,0,0,0,0,0\r\n".into(), "$N63,");
        append_register_readback(&mut result, self, "$R64,0,0,0,0,0,0\r\n".into(), "$N64,");
        append_register_readback(&mut result, self, "$R65,0,0,0,0,0,0\r\n".into(), "$N65,");
        append_register_readback(&mut result, self, format!("$R68,0,0,{range},0,0\r\n"), "$N68,");
        append_register_readback(&mut result, self, "$R69,0,0,0,0,0,0\r\n".into(), "$N69,");
        append_register_readback(&mut result, self, "$R83,0,0\r\n".into(), "$N83,");
        append_register_readback(&mut result, self, "$R85,0\r\n".into(), "$N85,");
        append_register_readback(&mut result, self, "$R86,0\r\n".into(), "$N86,");
        append_register_readback(&mut result, self, "$R87,0\r\n".into(), "$N87,");
        append_register_readback(&mut result, self, "$R89,0,0\r\n".into(), "$N89,");
        append_register_readback(&mut result, self, "$RED,0,0\r\n".into(), "$NED,");
        append_register_readback(&mut result, self, "$REE,0,0\r\n".into(), "$NEE,");
        append_register_readback(&mut result, self, "$REF,0,0,0\r\n".into(), "$NEF,");
        // Query the interference-rejection subregister. Field 1 is the
        // selector and field 2 is the numeric IR level (0 or 2).
        append_register_readback(&mut result, self, "$R67,0,0,,0\r\n".into(), "$N67,");
        append_register_readback(&mut result, self, "$R67,0,2,,0\r\n".into(), "$N67,");
        append_register_readback(&mut result, self, "$R84,0,0,0\r\n".into(), "$N84,");
        result
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
        let (found, _) = self.send_and_read_ack(cmd, expected_prefix, timeout)?;
        Ok(found)
    }

    /// Send a command and wait for matching $N response. Returns (found, ack_line).
    pub fn send_and_read_ack(
        &mut self,
        cmd: &str,
        expected_prefix: &str,
        timeout: Duration,
    ) -> Result<(bool, String), RadarError> {
        // Keep the control TCP session alive before every potentially slow
        // transaction. This runs while callers hold the connection mutex, so
        // a readback cannot race a heartbeat write on the same stream.
        self.send_keepalive()?;
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
                    // Scan for the expected prefix anywhere in the buffer,
                    // then extract the full line containing it.
                    if let Some(pos) = all_responses.find(expected_prefix) {
                        let from = pos;
                        let to = all_responses[from..].find('\n')
                            .map(|i| from + i)
                            .unwrap_or(all_responses.len());
                        let line = all_responses[from..to].trim().to_string();
                        println!("    ACK: {line}");
                        ctrl.set_nonblocking(true)?;
                        return Ok((true, line));
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
        Ok((false, String::new()))
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
            let ctrl = self
                .ctrl
                .as_mut()
                .ok_or(RadarError::ConnectionFailed("no control stream".into()))?;
            ctrl.write_all(protocol::KEEPALIVE)?;
            self.last_keepalive = Instant::now();
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

fn append_register_readback(
    result: &mut CurrentRadarReadback,
    conn: &mut RadarConnection,
    requested: String,
    prefix: &'static str,
) {
    let requested_at = unix_timestamp_millis();
    let query_started = Instant::now();
    let outcome = conn.send_and_read_ack(&requested, prefix, Duration::from_secs(3));
    let query_duration_ms = query_started.elapsed().as_millis().min(u64::MAX as u128) as u64;
    match outcome {
        Ok((true, raw)) => {
            let (register, values) = protocol::parse_response(&raw)
                .map(|(id, vals)| (
                    format!("$N{id}"),
                    vals.into_iter().map(str::to_string).collect(),
                ))
                .unwrap_or_else(|| (prefix.trim_end_matches(',').to_string(), Vec::new()));
            result.reads.push(RegisterReadback {
                requested,
                register,
                raw: Some(raw),
                values,
                error: None,
                requested_at,
                read_at: unix_timestamp_millis(),
                query_duration_ms,
            });
        }
        Ok((false, detail)) => result.reads.push(RegisterReadback {
            requested,
            register: prefix.trim_end_matches(',').to_string(),
            raw: None,
            values: Vec::new(),
            error: Some(if detail.is_empty() { "timeout waiting for register response".into() } else { detail }),
            requested_at,
            read_at: unix_timestamp_millis(),
            query_duration_ms,
        }),
        Err(error) => result.reads.push(RegisterReadback {
            requested,
            register: prefix.trim_end_matches(',').to_string(),
            raw: None,
            values: Vec::new(),
            error: Some(error.to_string()),
            requested_at,
            read_at: unix_timestamp_millis(),
            query_duration_ms,
        }),
    }
}

fn unix_timestamp_millis() -> String {
    let millis = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis();
    format!("unix_ms:{millis}")
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

fn validate_tx_ack(line: &str, desired: u16) -> Result<(), RadarError> {
    let mut fields = line.trim().split(',');
    let register = fields.next();
    let observed = fields.next().and_then(|value| value.trim().parse::<u16>().ok());
    if register != Some("$N69") || observed != Some(desired) {
        return Err(RadarError::ProtocolError(format!(
            "TX state {desired} was not confirmed by radar response {line:?}"
        )));
    }
    Ok(())
}

/// Validate that a setting acknowledgement corresponds to the command that
/// was sent. A register prefix alone is insufficient because a delayed ACK
/// from an earlier command can otherwise be mistaken for the current state.
pub fn validate_setting_ack(command: &str, response: &str) -> Result<(), RadarError> {
    let parse = |line: &str| -> Option<(String, Vec<i32>)> {
        let fields = line.trim().split(',').collect::<Vec<_>>();
        let id = fields.first()?.strip_prefix('$')?.to_string();
        let values = fields[1..]
            .iter()
            .map(|value| value.trim().parse::<i32>().ok())
            .collect::<Option<Vec<_>>>()?;
        Some((id, values))
    };
    let (command_id, command_values) = parse(command)
        .ok_or_else(|| RadarError::ProtocolError(format!("invalid setting command {command:?}")))?;
    let (response_id, response_values) = parse(response)
        .ok_or_else(|| RadarError::ProtocolError(format!("invalid setting response {response:?}")))?;
    if !response_id.starts_with('N') || response_id[1..] != command_id[1..] {
        return Err(RadarError::ProtocolError(format!(
            "setting response register {response_id:?} does not match command {command_id:?}"
        )));
    }
    let pairs: Vec<(usize, usize)> = match command_id.as_str() {
        // N62 returns three values even though S62 carries trailing protocol
        // placeholders. Only the selected range is the setting being acked.
        "S62" => vec![(0, 0)],
        // N63 retains both manual and automatic values. Confirm the mode and
        // only the value active in that mode; the inactive value is still
        // meaningful metadata and may intentionally be preserved by the radar.
        "S63" => {
            let active_value = if command_values.first() == Some(&0) { 1 } else { 3 };
            vec![(0, 0), (active_value, active_value)]
        }
        // A/C SEA and A/C RAIN each retain inactive values. Confirm the mode,
        // active value, and the explicitly mutable processing-mode fields.
        "S64" => {
            let active_value = if command_values.first() == Some(&0) { 1 } else { 2 };
            vec![(0, 0), (active_value, active_value), (3, 3)]
        }
        "S65" => {
            let active_value = if command_values.first() == Some(&0) { 1 } else { 2 };
            vec![(0, 0), (active_value, active_value), (3, 3), (4, 4)]
        }
        // N67 field 2 is the selected value; field 1 identifies whether the
        // command addresses interference rejection (0) or noise reduction
        // (2). Field 0 and field 3 are retained protocol state.
        "S67" => vec![(1, 1), (2, 2)],
        // S84 field 1 is the mutable antenna-height register; the selector
        // and trailing protocol field are not independent settings.
        "S84" => vec![(1, 1)],
        // Pulse responses echo the range in response field 2; S68 field 1 is
        // the radar/channel selector and remains zero for this controller.
        "S68" => vec![(0, 0), (2, 2)],
        "S69" => vec![(0, 0)],
        "SEE" => vec![(0, 0)],
        "SEF" => vec![(0, 0), (1, 1)],
        _ => (0..command_values.len()).map(|index| (index, index)).collect(),
    };
    for (command_index, response_index) in pairs {
        let expected = command_values.get(command_index).ok_or_else(|| {
            RadarError::ProtocolError(format!("command {command:?} is missing field {command_index}"))
        })?;
        let observed = response_values.get(response_index).ok_or_else(|| {
            RadarError::ProtocolError(format!("response {response:?} is missing field {response_index}"))
        })?;
        if expected != observed {
            return Err(RadarError::ProtocolError(format!(
                "setting acknowledgement mismatch at command field {command_index}: expected {expected}, observed {observed} in {response:?}"
            )));
        }
    }
    Ok(())
}

pub fn create_echo_socket(host_ip: Ipv4Addr) -> UdpSocket {
    try_create_echo_socket(host_ip).expect("Failed to configure echo socket")
}

fn connect_from_host(
    host_ip: Ipv4Addr,
    destination: SocketAddr,
    timeout: Duration,
) -> Result<TcpStream, RadarError> {
    let socket = socket2::Socket::new(
        socket2::Domain::IPV4,
        socket2::Type::STREAM,
        Some(socket2::Protocol::TCP),
    )?;
    socket.bind(&SocketAddr::from((host_ip, 0)).into()).map_err(|e| {
        RadarError::ConnectionFailed(format!("Cannot bind host address {host_ip}: {e}"))
    })?;
    socket.connect_timeout(&destination.into(), timeout).map_err(|e| {
        RadarError::ConnectionFailed(format!("Cannot reach {destination} from {host_ip}: {e}"))
    })?;
    Ok(socket.into())
}

pub fn try_create_echo_socket(host_ip: Ipv4Addr) -> Result<UdpSocket, RadarError> {
    let sock = socket2::Socket::new(
        socket2::Domain::IPV4,
        socket2::Type::DGRAM,
        Some(socket2::Protocol::UDP),
    )?;

    sock.set_reuse_address(true)?;
    unsafe {
        let optval: i32 = 1;
        let result = libc::setsockopt(
            sock.as_raw_fd(),
            libc::SOL_SOCKET,
            libc::SO_REUSEPORT,
            &optval as *const _ as *const libc::c_void,
            std::mem::size_of::<i32>() as u32,
        );
        if result != 0 {
            return Err(std::io::Error::last_os_error().into());
        }
    }

    sock.bind(
        &SocketAddr::from((Ipv4Addr::UNSPECIFIED, protocol::FURUNO_DATA_PORT)).into(),
    )?;

    let echo_socket: UdpSocket = sock.into();
    echo_socket
        .join_multicast_v4(&protocol::FURUNO_MULTICAST, &host_ip)
        .map_err(|e| RadarError::ConnectionFailed(format!(
            "Cannot join radar multicast on host address {host_ip}: {e}"
        )))?;
    echo_socket
        .set_read_timeout(Some(Duration::from_millis(500)))?;

    Ok(echo_socket)
}

#[cfg(test)]
mod socket_tests {
    use super::*;

    #[test]
    fn control_socket_uses_requested_local_address() {
        let listener = std::net::TcpListener::bind((Ipv4Addr::LOCALHOST, 0)).unwrap();
        let stream = connect_from_host(
            Ipv4Addr::LOCALHOST,
            listener.local_addr().unwrap(),
            Duration::from_secs(1),
        ).unwrap();
        assert_eq!(stream.local_addr().unwrap().ip(), Ipv4Addr::LOCALHOST);
    }

    #[test]
    fn missing_local_address_fails_before_connection() {
        let result = connect_from_host(
            Ipv4Addr::new(192, 0, 2, 254),
            SocketAddr::from((Ipv4Addr::LOCALHOST, 1)),
            Duration::from_secs(1),
        );
        assert!(matches!(result, Err(RadarError::ConnectionFailed(message))
            if message.contains("Cannot bind host address")));
    }

    #[test]
    fn tx_ack_requires_the_requested_device_state() {
        assert!(validate_tx_ack("$N69,1,0,0,0,0,0", 1).is_ok());
        assert!(validate_tx_ack("$N69,2,0,0,0,0,0", 2).is_ok());
        assert!(validate_tx_ack("$N69,1,0,0,0,0,0", 2).is_err());
        assert!(validate_tx_ack("$N69,", 1).is_err());
        assert!(validate_tx_ack("$N63,1,0,0,80,0", 1).is_err());
    }

    #[test]
    fn setting_ack_requires_matching_register_values() {
        assert!(validate_setting_ack("$S62,4,0,0\r\n", "$N62,4,0,0").is_ok());
        assert!(validate_setting_ack("$S62,4,0,0\r\n", "$N62,3,0,0").is_err());
        assert!(validate_setting_ack("$S63,1,12,0,80,0\r\n", "$N63,1,12,0,80,0").is_ok());
        assert!(validate_setting_ack("$S63,0,12,0,80,0\r\n", "$N63,0,12,0,99,0").is_ok());
        assert!(validate_setting_ack("$S63,0,12,0,80,0\r\n", "$N63,0,13,0,80,0").is_err());
        assert!(validate_setting_ack("$S64,1,23,45,2,0,0\r\n", "$N64,1,99,45,2,0,0").is_ok());
        assert!(validate_setting_ack("$S65,0,34,56,1,2,0\r\n", "$N65,0,35,56,1,2,0").is_err());
        assert!(validate_setting_ack("$S67,0,0,2,0\r\n", "$N67,0,0,2,0").is_ok());
        assert!(validate_setting_ack("$S67,0,2,1,0\r\n", "$N67,0,2,1,0").is_ok());
        assert!(validate_setting_ack("$S67,0,2,1,0\r\n", "$N67,0,0,1,0").is_err());
        assert!(validate_setting_ack("$S84,0,15,0\r\n", "$N84,0,15,0").is_ok());
        assert!(validate_setting_ack("$S84,0,15,0\r\n", "$N84,0,14").is_err());
        assert!(validate_setting_ack("$S68,5,0,4,0,0\r\n", "$N68,5,0,4,0,0").is_ok());
        assert!(validate_setting_ack("$S68,5,0,4,0,0\r\n", "$N68,5,0,3,0,0").is_err());
        assert!(validate_setting_ack("$S89,1,1\r\n", "$N89,1,1").is_ok());
    }

    #[test]
    fn keepalive_writes_binary_frame_and_updates_clock_after_success() {
        let listener = std::net::TcpListener::bind((Ipv4Addr::LOCALHOST, 0)).unwrap();
        let address = listener.local_addr().unwrap();
        let mut conn = RadarConnection::new(Ipv4Addr::LOCALHOST, Ipv4Addr::LOCALHOST);
        conn.ctrl = Some(TcpStream::connect(address).unwrap());
        conn.last_keepalive = Instant::now() - Duration::from_secs(6);
        let before = conn.last_keepalive;

        conn.send_keepalive().expect("keepalive write");
        let (mut peer, _) = listener.accept().unwrap();
        let mut frame = [0u8; 8];
        peer.read_exact(&mut frame).unwrap();

        assert_eq!(frame, protocol::KEEPALIVE);
        assert!(conn.last_keepalive > before);
    }

    #[test]
    fn keepalive_failure_does_not_advance_clock() {
        let mut conn = RadarConnection::new(Ipv4Addr::LOCALHOST, Ipv4Addr::LOCALHOST);
        conn.last_keepalive = Instant::now() - Duration::from_secs(6);
        let before = conn.last_keepalive;

        assert!(matches!(conn.send_keepalive(), Err(RadarError::ConnectionFailed(_))));
        assert_eq!(conn.last_keepalive, before);
    }

    #[test]
    fn control_transaction_sends_due_keepalive_before_command() {
        let listener = std::net::TcpListener::bind((Ipv4Addr::LOCALHOST, 0)).unwrap();
        let address = listener.local_addr().unwrap();
        let server = std::thread::spawn(move || {
            let (mut peer, _) = listener.accept().unwrap();
            let mut keepalive = [0u8; 8];
            peer.read_exact(&mut keepalive).unwrap();
            assert_eq!(keepalive, protocol::KEEPALIVE);

            let mut command = [0u8; 32];
            let count = peer.read(&mut command).unwrap();
            assert_eq!(&command[..count], b"$R62,0,0,0\r\n");
            peer.write_all(b"$N62,4,0,0\r\n").unwrap();
        });

        let mut conn = RadarConnection::new(Ipv4Addr::LOCALHOST, Ipv4Addr::LOCALHOST);
        conn.ctrl = Some(TcpStream::connect(address).unwrap());
        conn.last_keepalive = Instant::now() - Duration::from_secs(6);
        let result = conn
            .send_and_read_ack("$R62,0,0,0\r\n", "$N62,", Duration::from_secs(1))
            .unwrap();

        assert_eq!(result, (true, "$N62,4,0,0".to_string()));
        server.join().unwrap();
    }

    #[test]
    fn register_readback_records_missing_connection_duration() {
        let mut conn = RadarConnection::new(Ipv4Addr::LOCALHOST, Ipv4Addr::LOCALHOST);
        let mut snapshot = CurrentRadarReadback::default();
        append_register_readback(
            &mut snapshot,
            &mut conn,
            "$R62,0,0,0,0\r\n".into(),
            "$N62,",
        );

        let read = &snapshot.reads[0];
        assert!(read.error.is_some());
        assert!(read.query_duration_ms < 100);
    }

    #[test]
    fn register_readback_records_delayed_ack_duration() {
        let listener = std::net::TcpListener::bind((Ipv4Addr::LOCALHOST, 0)).unwrap();
        let address = listener.local_addr().unwrap();
        let server = std::thread::spawn(move || {
            let (mut peer, _) = listener.accept().unwrap();
            let mut command = [0u8; 32];
            let count = peer.read(&mut command).unwrap();
            assert_eq!(&command[..count], b"$R62,0,0,0,0\r\n");
            std::thread::sleep(Duration::from_millis(75));
            peer.write_all(b"$N62,4,0,0\r\n").unwrap();
        });

        let mut conn = RadarConnection::new(Ipv4Addr::LOCALHOST, Ipv4Addr::LOCALHOST);
        conn.ctrl = Some(TcpStream::connect(address).unwrap());
        let mut snapshot = CurrentRadarReadback::default();
        append_register_readback(
            &mut snapshot,
            &mut conn,
            "$R62,0,0,0,0\r\n".into(),
            "$N62,",
        );

        let read = &snapshot.reads[0];
        assert!(read.error.is_none());
        assert_eq!(read.raw.as_deref(), Some("$N62,4,0,0"));
        assert!(read.query_duration_ms >= 50);
        server.join().unwrap();
    }
}
