# Technical Manual Restructuring Plan

## Guiding Principle

Top-down flow: user-facing first, developer internals second, deep protocol last.

Two audiences:
1. **New users** — "how do I run this and use it?"
2. **Contributors** — "how do I modify the code, understand the data, or debug the protocol?"
3. **Fork Developers** - "how do I make my own version of this for my application?"

---

## Part I — Overview & Usage

### I.1 Introduction
- What This System Does — one-paragraph summary
- Hardware Overview — DRS4D-NXT specs (X-band, 9.41 GHz, solid-state Doppler, radome enclosure)
- Software Components — furuno-server, web interface, docs
- Reverse Engineering Context — where the protocol knowledge came from

### I.2 Quick Start & Installation
- Prerequisites — Rust toolchain, platform notes
- Building — `cargo build --release`
- First Run — `cargo run --release -- web`, open browser at `http://localhost:8080`
- Auto-Discovery — what happens on startup, manual IP override (`--radar-ip`, `--host-ip`)

### I.3 CLI Modes
- General Syntax — `furuno-server [--radar-ip IP] [--host-ip IP] <mode>`
- `web [port] [webdir]` — start the HTTP server (default port 8080, web files from `../web`). Primary way to operate the radar.
- `discover` — broadcast discovery beacons and listen for radar responses. Use when you don't know the radar's IP.

### I.4 Web Interface
- Starting the Server — port config, static file path, graceful degradation if radar unreachable
- Page Tour — describe each of the 4 HTML pages:
  - index.html — quick controls (TX on/off, range, pulse, gain, RezBoost)
  - radar.html — full-screen PPI display with view mode selector
  - settings.html — detailed parameter panels (clutter, STC, Doppler, antenna)
  - status.html — system metrics, firmware, session stats
- PPI Display — live radar view with history compositing (track length, current/track color controls)

### I.5 Recording & Data Formats
- Starting/Stopping Recording — toggle via web UI or API, automatic directory creation
- Output Directory Structure:
  ```
  recordings/<timestamp>/
  ├── csv/rotation_<ts>.csv
  ├── png/rotation_<ts>.png
  └── settings.json
  ```
- CSV Format — metadata header (`#`-prefixed lines with all settings at capture time), then columnar spoke data: `spoke_index,angle_ticks,angle_deg,heading_ticks,num_bins,bin_0,...,bin_867`
- Settings Snapshot (`settings.json`) — schema: `CurrentSettings` fields, `SettingChange` history array

### I.6 Pattern System
- What Patterns Do — automated parameter sequences, each step held for N rotations
- Two Pattern Formats:
  - **Simple (7-field):** `range,gain,pulse,rezboost,speed,interp,repeat` — all fields required, all values set explicitly
  - **Advanced (13-field):** adds `sea_clutter,rain_clutter,main_bang,interference_rejection,bird_mode,doppler` — fields can be `NaN` to skip (leaves setting unchanged, allows manual override during execution)
- Pattern Queue — load multiple patterns into a queue, each with its own repeat count and label; patterns execute in order; queue can be stopped mid-run
- Loading Patterns — file upload (`.txt`/`.csv`) or manual textarea input; steps separated by semicolons
- NaN Override Behavior — when a pattern step uses `NaN` for a field, that setting remains unlocked and can be changed manually via the web UI while the pattern runs

---

## Part II — Software Internals

### II.1 System Architecture (moved from current section 10)
- Module Layout + dependency figure (existing)
- Module descriptions (existing)
- **NEW: Concurrency Model**:
  - Tokio async runtime for web handlers
  - `spawn_blocking` for radar I/O operations
  - Dedicated `std::thread` for echo listener loop
  - `Mutex<RadarConnection>` enforces single-connection access
- **NEW: Shared State (`AppState`)** — key fields documented with their synchronization primitives:
  - `conn: Mutex<RadarConnection>` — radar connection (exclusive access)
  - `latest_png: RwLock<Vec<u8>>` — most recent PPI image
  - `tx_active: AtomicBool` — TX power state
  - `settings: RwLock<CurrentSettings>` — current radar config
  - `setting_history: RwLock<Vec<SettingChange>>` — change log for recordings
  - etc.

### II.2 Connection State Machine
- States and transitions + figure (existing)
- `RadarError` Enum — `AuthFailed`, `ConnectionFailed`, `IoError`, `Timeout`, `InvalidState`, `ProtocolError`
- Timeout Values — auth: 5s, control: 5s, init registers: 3s, command ack: 3s, keepalive interval: 5-10s
- Graceful Degradation — web server starts even if radar is unreachable, `/reconnect` endpoint for retry

### II.3 PPI Rendering (moved from current section 9)
- Coordinate Transformation (existing)
- Color Mapping (existing but corrected):
  - Actual formula: R = intensity, G = intensity * 180/255, B = intensity * 20/255 (warm-tinted, not pure green)
- Overlay Elements — crosshairs (dark gray RGB 54,54,54 at center), 4 equally-spaced range rings
- Spoke Interpolation — `interpolate_spokes()` function, 6 modes:
  - 0 = 2048 spokes/rotation
  - 1 = 4096
  - 2 = 8192
  - 3 = 1024
  - 4 = 720
  - 5 = OFF (no interpolation)
  - Linear blending algorithm between nearest source spokes
  **SHOULD THIS BE HERE? I think if this is purely software, should be disabled.**
- PNG Encoding — `encode_png_bytes()` for in-memory, `write_png()` for file output

### II.4 REST API Reference (moved from Part I Web Interface section)
- All endpoints grouped by category (existing, with corrections):
  - Power and Connection
  - Settings (all GET with query params)
  - State Queries
  - Echo Image
  - Recording
  - Pattern Execution
- **Fix:** correct query parameter names to match actual code (e.g., `/set_gain` takes `status` and `manualValue`, not just `value`)
- Stub Endpoints — `/set_sweep_total` and `/set_interpolation` are wired but return stub responses

### II.5 CLI Modes (All Other Modes)
- All remaining CLI modes for development, debugging, and verification:
  - `handshake` — authenticate and initialize, print firmware info (default if no mode given)
  - `tx-on` / `tx-off` — enable or disable the transmitter from the command line
  - `full` — full cycle: init, TX ON, capture one rotation, TX OFF
  - `render-live [out.png]` — capture one live rotation and render to PNG
  - `echo` — passively listen for multicast echo packets (no auth needed)
  - `command` — send test commands and display acknowledgments
  - `dump-raw [count]` — dump raw echo packet headers and hex data
  - `test-commands` — A/B command verification with before/after PPI captures
  - `validate` — run the 6-phase validation suite (27 integration tests)

### II.6 Testing & Validation (moved from current section 13)
- Running Tests — `cargo test -- --test-threads=1`, requires live radar (existing)
- Test Categories (existing):
  - Connection (6)
  - TX (3)
  - Settings (14)
  - Echo Impact (2)
  - Render Integration (1)
  - Total: 27
- A/B Command Testing — `testing.rs` methodology: capture PPI before, send command, capture PPI after, compare
- Validation Suite (`validate` mode) — 6-phase automated validation

---

## Part III — Echo Data Pipeline

### III.1 Echo Data Format (existing section 6)
- Packet Structure + figure (existing)
- Echo Metadata — sweep_count, sweep_len, encoding extraction (existing)
- Azimuth and Heading — 8192 ticks/rotation, compass bearing (existing)

### III.2 Echo Encoding Types (promoted from subsections of current section 6)
- Encoding 0: Raw Uncompressed (existing)
- Encoding 1: RLE with Repeat Marker (existing)
- Encoding 2: Delta RLE with Previous Spoke Reference (existing)
- Encoding 3: 2-Bit RLE with Dual Reference (existing)
- Display Value Conversion — `display = wire >> 2` (existing)
- Rotation Detection — echo listener detects wrap-around when azimuth drops by >1000 ticks from previous spoke

### III.3 Doppler Processing (existing section 7, unchanged)
- Status Field Encoding (existing)
- Echo Byte Encoding — normal vs Doppler mode (existing)

### III.4 Dual-Pulse Interleaving (existing section 8, unchanged)
- Pulse Types — long (17 levels) vs short (130-240 levels) (existing)
- Range Dependence (existing)
- Rez Boost Effect (existing)

---

## Part IV — Network & Wire Protocol

### IV.1 Network Architecture (existing section 2)
- Topology + figure (existing)
- Port Map table (existing)
- Auto-Discovery and Network Configuration (existing, expanded):
  - **NEW:** Platform-specific alias commands (Linux: `ip addr add`, macOS: `ifconfig alias`, Windows: placeholder only)
  - **NEW:** Three-stage fallback logic in detail

### IV.2 Control Protocol (existing section 3)
- Message Format — `$S`, `$R`, `$N` prefixes (existing)
- Binary Keepalive + figure (existing)
- Initialization Sequence — 6 phases + figure (existing)
- **NEW: Keepalive in the Echo Listener** — echo thread sends keepalives every 5s even when TX is off, plus `drain_control()` to consume pending responses

### IV.3 Command Reference (existing section 4)
- All existing commands retained
- **Corrections needed:**
  - STC commands (current manual shows `$S85`/`$S86`/`$S87` for both STC AND sea/rain clutter — confusing, needs clarification)
  - Pulse width range: manual says 0-3, code supports 0-5 (S1/S2/M1/M2/M3/L)
  - Interference rejection command ID: manual says `$S88`, code uses `$S67` — verify against protocol.rs

### IV.4 Periodic Telemetry (existing section 5, unchanged)
- Telemetry message table (existing)

### IV.5 Proprietary Library Reference (existing section 15, unchanged)
- libNAVNETDLL.so module breakdown table (existing)

## Key Corrections to Make During Rewrite

1. **Pulse width range**: manual says 0-3, code supports 0-5
2. **Color mapping**: manual says "green-channel intensity", actual is warm-tinted (R=full, G=70%, B=8%)
3. **STC vs clutter command IDs**: current manual reuses $S85/$S86/$S87 for both — needs disambiguation
4. **Web API parameter names**: several endpoints have different query param names than documented
5. **Interference rejection command ID**: verify $S88 vs $S67 against protocol.rs
