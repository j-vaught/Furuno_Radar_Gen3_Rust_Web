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

### II.1 System Architecture Overview
High-level introduction only — points readers to detailed sections. No deep dives here.
- Brief module list with one-line descriptions and "see §II.X" pointers
- Three new architecture diagrams (all created from scratch):
  1. **System-level block diagram** — radar hardware ↔ Rust server ↔ browser. Shows the physical and network boundaries.
  2. **Module-level diagram** — how the `.rs` files relate to each other (`main.rs`, `protocol.rs`, `connection.rs`, `web.rs`, `echo.rs`, `render.rs`, `modes.rs`, `testing.rs`)
  3. **Data flow diagram** — echo packet arrives on UDP → `decode_echo_packet()` → `Spoke` structs → `render_ppi()` → PNG bytes → `/newest_png` → browser canvas
- Concurrency model summary — Tokio async for HTTP, dedicated echo listener thread, `Mutex<RadarConnection>` for single-connection enforcement. Details in relevant sections.

### II.2 PPI Rendering (`render.rs`)
Most commonly modified subsystem — changing colors, overlays, image size, etc.
- Coordinate Transformation — compass bearing to math angle, range bin to pixel
- Color Mapping (corrected from current manual):
  - Actual formula: R = intensity, G = intensity * 180/255, B = intensity * 20/255 (warm-tinted, not pure green)
- Overlay Elements — crosshairs (dark gray RGB 54,54,54 at center), 4 equally-spaced range rings
- PNG Encoding — `encode_png_bytes()` for in-memory, `write_png()` for file output
- How to modify — changing image size, color palette, adding new overlays

### II.3 Web Server (`web.rs`)
How the HTTP server works and how to extend it.
- Framework — Axum 0.7 + Tokio async runtime
- Shared State (`AppState`) — key fields and their synchronization primitives:
  - `conn: Mutex<RadarConnection>` — radar connection (exclusive access)
  - `latest_png: RwLock<Vec<u8>>` — most recent PPI image
  - `tx_active: AtomicBool` — TX power state
  - `settings: RwLock<CurrentSettings>` — current radar config
  - `setting_history: RwLock<Vec<SettingChange>>` — change log for recordings
- Route registration — how routes are wired in `start_server()`
- Handler pattern — `async fn handler(State(state), Query(params)) -> impl IntoResponse`
- Echo listener thread — dedicated `std::thread` that receives UDP multicast, decodes spokes, renders PPI, updates `latest_png`
- Static file serving — `ServeDir` fallback for the `web/` directory
- How to add a new endpoint — step-by-step: define params struct, write handler, register route

### II.4 REST API Reference
All endpoints with correct parameter names (verified against code).
- Power and Connection
- Settings (all GET with query params)
- State Queries
- Echo Image
- Recording
- Pattern Execution
- Stub Endpoints — `/set_sweep_total` and `/set_interpolation` are wired but return stub responses

### II.5 Recording System (`web.rs` internals)
How recording works under the hood.
- Echo listener integration — rotation detection triggers CSV + PNG write
- Directory creation — timestamped folder with `csv/` and `png/` subdirectories
- CSV writing — metadata header generation from `CurrentSettings`, spoke-by-spoke output
- Settings snapshots — `settings.json` schema, `ManualRecordingSettingsSnapshot` vs `PatternRecordingSettingsSnapshot`
- Setting change history — how `SettingChange` entries accumulate during a session

### II.6 Pattern Execution Engine (`web.rs` internals)
How patterns run internally.
- Step parsing — `parse_pattern_steps()`: 7-field (simple) vs 13-field (advanced) format
- Differential command generation — only changed parameters sent to radar
- Single pattern execution — `execute_pattern()` loop: apply step, wait N rotations, advance
- Queue execution — `handle_run_pattern_queue()`: base64 decode, iterate items, per-item recording sessions
- NaN override logic — `pattern_step_allows_override()`: fields set to `NaN` remain unlocked for manual changes
- Stop mechanism — `pattern_stop: AtomicBool` checked between steps and rotations

### II.7 CLI Modes (All Other Modes)
All CLI modes beyond `web` and `discover` (documented in Part I).
- `handshake` — authenticate and initialize, print firmware info (default if no mode given)
- `tx-on` / `tx-off` — enable or disable the transmitter from the command line
- `full` — full cycle: init, TX ON, capture one rotation, TX OFF
- `render-live [out.png]` — capture one live rotation and render to PNG
- `echo` — passively listen for multicast echo packets (no auth needed)
- `command` — send test commands and display acknowledgments
- `dump-raw [count]` — dump raw echo packet headers and hex data
- `test-commands` — A/B command verification with before/after PPI captures
- `validate` — run the 6-phase validation suite (27 integration tests)

### II.8 Connection State Machine (`connection.rs`)
- States — `Disconnected`, `Authenticated`, `ControlConnected`, `Initialized`, `Transmitting`
- Transitions + figure (new diagram)
- `RadarError` Enum — `AuthFailed`, `ConnectionFailed`, `IoError`, `Timeout`, `InvalidState`, `ProtocolError`
- Timeout Values — auth: 5s, control: 5s, init registers: 3s, command ack: 3s, keepalive interval: 5-10s
- Graceful Degradation — web server starts even if radar is unreachable, `/reconnect` endpoint for retry
- Keepalive handling — echo thread sends keepalives every 5s, `drain_control()` consumes pending responses

### II.9 Testing & Validation (`testing.rs`, `tests.rs`)
- Running Tests — `cargo test -- --test-threads=1`, requires live radar, sequential only (single connection)
- Test Categories:
  - Connection (6) — auth, control channel, init, firmware, keepalive, echo socket
  - TX (3) — TX on/off cycling, echo reception, full rotation collection
  - Settings (14) — set-and-read verification for each parameter
  - Echo Impact (2) — gain and range changes produce measurable echo differences
  - Render Integration (1) — full pipeline: UDP → decode → render → PNG
- A/B Command Testing — `testing.rs` methodology: capture PPI before, send command, capture PPI after, compare
- Validation Suite (`validate` mode) — 6-phase automated verification

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
