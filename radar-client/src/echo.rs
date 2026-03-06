use std::io::Write;
use std::net::UdpSocket;
use std::time::Duration;

use crate::protocol::{self, Spoke};
use crate::connection::RadarConnection;

pub struct EchoHeader {
    pub sequence: u16,
    pub timestamp: u32,
    pub flags: u16,
    pub config: u16,
    pub azimuth_deg: f64,
}

pub fn parse_echo_header(data: &[u8]) -> EchoHeader {
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

pub struct EchoStats {
    pub num_spokes: usize,
    pub num_nonzero_spokes: usize,
    pub mean_amplitude: f64,
    pub max_amplitude: u8,
    pub nonzero_bins_pct: f64,
    pub sweep_len: u32,
    pub encoding: u8,
    pub sweep_count_per_pkt: u32,
}

impl std::fmt::Display for EchoStats {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "spokes={} nonzero={} mean_amp={:.1} max_amp={} nz_bins={:.1}% sweep_len={} enc={} sweeps/pkt={}",
            self.num_spokes, self.num_nonzero_spokes, self.mean_amplitude,
            self.max_amplitude, self.nonzero_bins_pct, self.sweep_len, self.encoding, self.sweep_count_per_pkt)
    }
}

pub fn collect_rotation_stats(
    echo_socket: &UdpSocket,
    conn: &mut RadarConnection,
    prev_spoke_data: &mut Vec<u8>,
) -> (Vec<Spoke>, EchoStats) {
    let mut buf = [0u8; 2048];
    let mut spokes: Vec<Spoke> = Vec::new();
    let mut prev_az: Option<u16> = None;
    let mut rotation_count = 0u32;
    let start = std::time::Instant::now();
    let mut last_sweep_len: u32 = 0;
    let mut last_encoding: u8 = 0;
    let mut last_sweep_count: u32 = 0;

    loop {
        match echo_socket.recv_from(&mut buf) {
            Ok((len, _)) => {
                if len < 20 { continue; }
                let az = u16::from_le_bytes([buf[16], buf[17]]);

                let meta = protocol::parse_echo_metadata(&buf[..len]);
                last_sweep_len = meta.sweep_len;
                last_encoding = meta.encoding;
                last_sweep_count = meta.sweep_count;

                if let Some(prev) = prev_az {
                    if az < prev.saturating_sub(1000) {
                        rotation_count += 1;
                        if rotation_count >= 2 { break; }
                    }
                }
                prev_az = Some(az);

                if rotation_count >= 1 {
                    protocol::decode_echo_packet(&buf[..len], prev_spoke_data, &mut spokes);
                } else {
                    let mut discard = Vec::new();
                    protocol::decode_echo_packet(&buf[..len], prev_spoke_data, &mut discard);
                }
            }
            Err(_) => {
                if start.elapsed() > Duration::from_secs(15) {
                    eprintln!("    Timeout collecting rotation");
                    break;
                }
            }
        }
        conn.drain_control();
        let _ = conn.send_keepalive();
    }

    let num_spokes = spokes.len();
    let mut total_amp: u64 = 0;
    let mut total_bins: u64 = 0;
    let mut nonzero_bins: u64 = 0;
    let mut max_amp: u8 = 0;
    let mut nonzero_spokes = 0usize;

    for spoke in &spokes {
        let has_nonzero = spoke.echo.iter().any(|&b| b > 0);
        if has_nonzero { nonzero_spokes += 1; }
        for &b in &spoke.echo {
            total_bins += 1;
            total_amp += b as u64;
            if b > 0 { nonzero_bins += 1; }
            if b > max_amp { max_amp = b; }
        }
    }

    let mean_amp = if total_bins > 0 { total_amp as f64 / total_bins as f64 } else { 0.0 };
    let nz_pct = if total_bins > 0 { nonzero_bins as f64 / total_bins as f64 * 100.0 } else { 0.0 };

    let stats = EchoStats {
        num_spokes,
        num_nonzero_spokes: nonzero_spokes,
        mean_amplitude: mean_amp,
        max_amplitude: max_amp,
        nonzero_bins_pct: nz_pct,
        sweep_len: last_sweep_len,
        encoding: last_encoding,
        sweep_count_per_pkt: last_sweep_count,
    };

    (spokes, stats)
}

pub fn write_echo_csv(path: &str, spokes: &[Spoke]) {
    let mut f = std::io::BufWriter::new(
        std::fs::File::create(path).expect("Failed to create CSV file"),
    );

    let max_bins = spokes.iter().map(|s| s.echo.len()).max().unwrap_or(0);
    write!(f, "spoke_index,angle_ticks,angle_deg,heading_ticks,num_bins").unwrap();
    for i in 0..max_bins {
        write!(f, ",bin_{i}").unwrap();
    }
    writeln!(f).unwrap();

    for (idx, spoke) in spokes.iter().enumerate() {
        let angle_deg = spoke.angle_ticks as f64 / 8192.0 * 360.0;
        write!(
            f,
            "{},{},{:.2},{},{}",
            idx, spoke.angle_ticks, angle_deg, spoke.heading_ticks, spoke.echo.len()
        )
        .unwrap();
        for &b in &spoke.echo {
            write!(f, ",{b}").unwrap();
        }
        for _ in spoke.echo.len()..max_bins {
            write!(f, ",0").unwrap();
        }
        writeln!(f).unwrap();
    }
}

pub fn timestamp_str() -> String {
    let now = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap();
    let secs = now.as_secs();
    let s = secs % 60;
    let m = (secs / 60) % 60;
    let h = (secs / 3600) % 24;
    let days = secs / 86400;
    let mut y = 1970u32;
    let mut remaining = days;
    loop {
        let days_in_year = if y % 4 == 0 && (y % 100 != 0 || y % 400 == 0) { 366 } else { 365 };
        if remaining < days_in_year { break; }
        remaining -= days_in_year;
        y += 1;
    }
    let leap = y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
    let month_days: [u64; 12] = [
        31,
        if leap { 29 } else { 28 },
        31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    ];
    let mut mo = 0;
    for (i, &md) in month_days.iter().enumerate() {
        if remaining < md {
            mo = i + 1;
            break;
        }
        remaining -= md;
    }
    let d = remaining + 1;
    format!("{y}{mo:02}{d:02}_{h:02}{m:02}{s:02}")
}

pub fn elapsed_str(start: std::time::Instant) -> String {
    let secs = start.elapsed().as_secs_f64();
    format!("{secs:6.1}s")
}
