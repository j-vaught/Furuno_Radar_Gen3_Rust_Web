# RADAR_web

Furuno DRS4D-NXT marine radar interface built in Rust. Provides full radar control, real-time PPI display, echo data recording, and a browser-based web interface.

## Overview

This project is a complete reimplementation of the proprietary Furuno `libNAVNETDLL.so` library in Rust. It handles the full radar lifecycle: authentication, control commands, echo data decoding (4 encoding types), Doppler processing, and PPI rendering.

## Prerequisites

- Rust toolchain (`rustup` + `cargo`)
- Furuno DRS4D-NXT radar connected via Ethernet
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

The radar is auto-discovered on startup. If auto-discovery fails, specify both endpoint addresses manually:

```bash
cargo run --release -- --radar-ip 172.31.3.226 --host-ip 172.31.1.100 web
```

Use `--interface NAME` to restrict discovery and subnet checks to one IPv4 interface. The application never changes network settings unless `--configure-network` is supplied together with an explicit interface. On macOS, for example, the opt-in command is `cargo run --release -- --radar-ip 172.31.3.226 --interface en5 --configure-network handshake`. Without that flag, configure the address manually and pass it with `--host-ip`.

### macOS launch profile

Build the native release executable and place it at `runtime/macos/furuno-server`, or set `FURUNO_BINARY` to its absolute path. Run `./run-macos.sh` with MacApplications mounted. This profile uses interface `en5`, radar address `172.31.3.226`, and host address `172.31.1.100`. Configure the Ethernet service with subnet mask `255.255.0.0` before launch. `FURUNO_INTERFACE`, `FURUNO_RADAR_IP`, and `FURUNO_HOST_IP` override those defaults.

The launcher stores recordings at `/Volumes/MacApplications/Radar_papers/data/live/macos-recordings` and refuses to start when the volume is unmounted. `FURUNO_STORAGE_VOLUME` and `FURUNO_RECORDING_ROOT` select another mounted volume and a recording directory on that volume. The server checks that the directory remains writable before saving each rotation.

The settings page keeps command inputs separate from radar state. Use `Read all confirmed settings` or an individual Refresh action to request a fresh radar snapshot. The status page displays a setting only after a response for that field has been parsed and validated; failed, stale, and mode-incomplete fields display as unavailable. The server refreshes the snapshot periodically, gives each readback a capture time, validity deadline, transition identifier, and raw register responses, and records the same structured snapshot in each session. Gain manual and automatic values, sea and rain modes, pulse range, and antenna high-RPM state remain separate fields. Unsupported sweep count and interpolation controls are disabled. Doppler modes 1 and 2 are shown as raw protocol modes until their vendor semantics are verified. Clutter input ranges are left device-specific and must be established by the hardware response. Unavailable macOS resource metrics display `--`.

The CLI, network, socket, and recording-root tests are software-only. The tests in `tests.rs` and the `full`, `validate`, and `test-commands` modes operate real hardware and can enable transmission.

## CLI Modes

```
furuno-server [--radar-ip IP] [--host-ip IP] [--interface NAME] [--configure-network] <mode> [args...]
```

| Mode | Description |
|------|-------------|
| `web [port] [webdir]` | Start the web server (default port 8080) |
| `handshake` | Authenticate and initialize, print firmware info |
| `tx-on` / `tx-off` | Enable or disable the transmitter |
| `echo` | Passively listen for multicast echo packets |
| `full` | Full cycle: init, TX ON, capture one rotation, TX OFF |
| `render-live [out.png]` | Capture one live rotation and render to PNG |
| `dump-raw [count]` | Dump raw echo packet headers |
| `discover` | Broadcast discovery beacons and listen for radar |
| `validate` | Run the 6-phase validation suite |
| `test-commands` | A/B command verification with PPI captures |

## Web Interface

The browser UI provides:

- Real-time PPI radar display (640x640)
- Radar settings controls (range, gain, pulse width, clutter, STC, Doppler, etc.)
- Echo data recording to CSV with full settings metadata
- Pattern-based parameter sweep execution
- System status and firmware info

## Recorded Data Format

When recording is enabled, each antenna rotation produces:

- `recordings/<timestamp>/csv/rotation_<ts>.csv` -- Echo data with settings metadata
- `recordings/<timestamp>/png/rotation_<ts>.png` -- PPI image
- `recordings/<timestamp>/settings.json` -- Session settings snapshot

CSV files include a metadata header (lines starting with `#`) containing the settings readback verification state, capture time, validity deadline, transition identifier, source, raw structured readback JSON, and the values associated with the recorded session, followed by spoke data with per-bin echo amplitudes. `settings_verified,true` means that the server captured and validated every required radar field for that session. A false value is an explicit indication that a setting read failed, became stale, or was not collected and must not be used as experimental ground truth. Rotations spanning a settings transition are discarded so one file cannot silently mix two configurations. The session `settings.json` file retains the structured readback snapshot and command history.

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
    calibrate.rs       Pattern runner
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

If the host is not on the radar's subnet, the application stops and prints the exact manual alias command. Alias creation requires both `--interface NAME` and `--configure-network`; the macOS path uses non-interactive `sudo -n` and fails without a cached authorization.

## Documentation

See `docs/technical_manual.pdf` for the full technical reference covering the wire protocol, echo encoding, Doppler processing, system architecture, and API reference.

## Author

J.C. Vaught -- University of South Carolina
