# RADAR_web

Furuno DRS4DL+ marine radar interface built in Rust. Provides full radar control, real-time PPI display, echo data recording, and a browser-based web interface.

## Overview

This project is a complete reimplementation of the proprietary Furuno `libNAVNETDLL.so` library in Rust. It handles the full radar lifecycle: authentication, control commands, echo data decoding (4 encoding types), Doppler processing, PPI rendering, and automated calibration sweeps.

## Prerequisites

- Rust toolchain (`rustup` + `cargo`)
- Furuno DRS4DL+ radar connected via Ethernet
- Linux, macOS, or Windows

Install Rust if needed:

```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source ~/.cargo/env
```

## Quick Start

```bash
cd furuno-server
cargo build --release
cargo run --release -- web
```

The web interface is available at `http://localhost:8080`.

The radar is auto-discovered on startup. If auto-discovery fails, specify the IP manually:

```bash
cargo run --release -- --radar-ip 172.31.3.226 web
```

## CLI Modes

```
furuno-server [--radar-ip IP] [--host-ip IP] <mode> [args...]
```

| Mode | Description |
|------|-------------|
| `web [port] [webdir]` | Start the web server (default port 8080) |
| `handshake` | Authenticate and initialize, print firmware info |
| `tx-on` / `tx-off` | Enable or disable the transmitter |
| `echo` | Passively listen for multicast echo packets |
| `full` | Full cycle: init, TX ON, capture one rotation, TX OFF |
| `render-csv <file> [out.png]` | Render a recorded CSV as a PPI image (offline) |
| `render-live [out.png]` | Capture one live rotation and render to PNG |
| `dump-raw [count]` | Dump raw echo packet headers |
| `discover` | Broadcast discovery beacons and listen for radar |
| `validate` | Run the 6-phase validation suite |
| `test-commands` | A/B command verification with PPI captures |
| `calibrate <args>` | Automated multi-parameter calibration sweeps |

## Web Interface

The browser UI provides:

- Real-time PPI radar display
- Radar settings controls (range, gain, pulse width, clutter, STC, Doppler, etc.)
- Echo data recording to CSV with full settings metadata
- Pattern-based calibration execution
- System status and firmware info

## Recorded Data Format

When recording is enabled, each antenna rotation produces:

- `recordings/<timestamp>/csv/rotation_<ts>.csv` -- Echo data with settings metadata
- `recordings/<timestamp>/png/rotation_<ts>.png` -- PPI image
- `recordings/<timestamp>/settings.json` -- Session settings snapshot

CSV files include a metadata header (lines starting with `#`) containing all radar settings at the time of capture, followed by spoke data with per-bin echo amplitudes.

## Project Structure

```
furuno-server/          Rust application
  src/
    main.rs            Entry point, CLI parsing, IP resolution
    protocol.rs        Wire protocol: commands, echo decoders, discovery
    connection.rs      RadarConnection state machine
    web.rs             HTTP server and REST API
    echo.rs            Echo packet parsing and statistics
    render.rs          PPI image rendering
    modes.rs           CLI mode implementations
    testing.rs         Validation and A/B testing
    calibrate.rs       Calibration pattern runner
    tests.rs           27 live integration tests
web/                   Static web frontend (HTML/CSS/JS)
docs/                  LaTeX technical manual with diagrams
```

## Testing

Tests require a live radar connection and must run sequentially:

```bash
cd furuno-server
cargo test -- --test-threads=1
```

## Network

The radar communicates on the `172.31.0.0/16` subnet. Key ports:

| Port | Protocol | Purpose |
|------|----------|---------|
| 10010 | TCP | One-shot authentication |
| 10010 | UDP | Discovery beacon |
| 10024 | UDP multicast (239.255.0.2) | Echo sweep data |
| 10100 | TCP | Persistent control channel |

If the host is not on the radar's subnet, the application automatically adds a secondary IP alias to the Ethernet interface.

## Documentation

See `docs/technical_manual.pdf` for the full 21-page technical reference covering the wire protocol, echo encoding, Doppler processing, system architecture, and API reference.

## Author

J.C. Vaught -- University of South Carolina
