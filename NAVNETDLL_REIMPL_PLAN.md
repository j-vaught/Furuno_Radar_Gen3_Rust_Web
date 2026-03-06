# Furuno Radar — Rust Client Reimplementation Plan

## Approach

**Goal:** Replace the proprietary `libNAVNETDLL.so` with a clean, standalone Rust library that speaks the radar's UDP protocol directly. Compile for any architecture, full control over echo data processing, no proprietary dependencies.

**Key insight:** The Furuno library is just software between our app and a radar unit on Ethernet. The radar sees UDP packets, not C function calls. We reverse engineer the wire protocol, then build a clean Rust client around it — not a pixel-perfect clone of Furuno's 6,102-function API.

**Validation strategy:** The existing C++ app + real Furuno .so stays intact as our reference oracle. We capture known-good traffic, then validate our Rust client produces byte-identical packets and correctly decodes the same echo data.

**What we already know (from Phase 0.1 symbol analysis):**
- 6,102 defined functions, but the wire protocol is the only thing that matters
- Every command follows: `RmcSet* → rmSubSet* → rmMakeCom* → NcSendRadarCommand → UDP socket`
- 160 IRAD command IDs (0-159) define the command space
- Internal architecture (ITRON shim, Blowfish, state tables) is Furuno overhead we skip entirely
- Echo packets arrive via UDP, decoded by `DecodeImoEchoFormat`
- All symbols intact (not stripped) — targeted disassembly is straightforward

**Estimated total: 40-60 hours**

---

## Phase 0: Capture & Decode Setup (4-6 hours)

### 0.1 — Packet Capture Rig (1-2 hrs)
- Set up `tcpdump` on the radar network interface
- Write a capture script that labels sessions (init, transmit, range-change, etc.)
- Capture a full lifecycle with the real Furuno app:
  - Boot → init → connect → transmit ON
  - Range changes (multiple values)
  - Gain changes (manual + auto)
  - Pulse width, STC, antenna revolution changes
  - Echo data flowing at each setting
  - ARPA target acquisition (if available)
  - Transmit OFF → disconnect
- Capture the same lifecycle 5-7 times for reproducibility
- Output: `reverse/captures/*.pcap`, `reverse/captures/session_log.md`

### 0.2 — Identify Protocol Basics from Pcaps (1-2 hrs)
- Open pcaps in Wireshark, identify:
  - Which ports are used (base port from `NcSetNetworkBasePortNo`)
  - UDP vs TCP (likely UDP for commands + echo, TCP for some control)
  - Broadcast vs unicast vs multicast
  - Packet sizes and frequency (echo packets should be high-frequency)
  - Any obvious framing (magic bytes, length headers)
- Separate traffic into categories: command, echo, heartbeat/alive, NMEA, other
- Output: `reverse/protocol_notes.md`

### 0.3 — Correlate Pcaps with Source Code (1-2 hrs)
- Read through `main.cpp` init sequence alongside the pcap timeline
- Match each `NcSetNetworkIpAddress`, `NcInitialNavnetCommon`, `RmInitialModule` call to packets
- Identify the init handshake: what packets are exchanged before echoes start flowing?
- Output: annotated timeline in `reverse/protocol_notes.md`

---

## Phase 0.5: Raw UDP Smoke Test (0.5 hours)

**Purpose:** Before any disassembly or protocol decoding, answer the single most important architectural question: does the radar broadcast echo data freely, or does it require a handshake first?

### 0.5.1 — Write a Minimal UDP Listener (30 min)
- Write a simple Rust program (or Python script) that:
  - Opens a UDP socket on the radar's echo port (identified from Phase 0.2 or from `NcSetNetworkBasePortNo` defaults)
  - Binds to `0.0.0.0` on that port
  - Loops forever, printing raw hex of every packet received
- Run it with the radar powered on and transmitting (use the existing Furuno app to put the radar in transmit mode)
- Observe:
  - **Bytes flowing?** The radar broadcasts openly. Phase 4 (echo receive) is straightforward — you can decode echoes before fully understanding the init handshake. Phase 3 becomes lower priority.
  - **Nothing received?** The radar requires registration/subscription before it sends echo data. Phase 3 (init handshake) is now **critical path** and must be completed before Phase 4.
- Output: `reverse/raw_udp_test_results.md` — document what was received (or not), packet sizes, frequency, and the critical path determination

**Why this matters:** This 30-minute test reshapes the entire project schedule. If the radar broadcasts openly, you can parallelize Phases 1-3 freely and start getting echo data almost immediately. If it requires a handshake, Phase 3 becomes a hard blocker and should be prioritized above Phases 1-2.

---

## Phase 1: Decode the Command Protocol (6-8 hours)

### 1.1 — Disassemble `NcSendRadarCommand` (2-3 hrs)
- This is THE function — every `RmcSet*` ends up here
- Load .so in Ghidra/radare2, find this function
- Determine: how is the command buffer structured before it hits the socket?
- What header does it add? (length, source ID, destination, command type?)
- Does it go through encryption (Blowfish) or is it plaintext?
- Cross-reference the bytes against a pcap of a known command (e.g., range change)
- Output: `reverse/command_format.md`

### 1.2 — Disassemble 3-4 `rmMakeCom*` Functions (2-3 hrs)
- Pick commands we actually send: `rmMakeComRange`, `rmMakeComGAIN`, `rmMakeComTxStby`
- For each: what bytes go into the command buffer?
- Map: IRAD_* command ID → byte position, parameter encoding (endianness, scaling)
- Verify against pcap: do the bytes match?
- Output: append to `reverse/command_format.md`

### 1.3 — Generalize the Command Format (1-2 hrs)
- With 3-4 decoded commands, identify the common pattern:
  - Fixed header? Variable payload?
  - Is the IRAD ID a single byte at a fixed offset?
  - Are parameters always UINT16 big-endian? Or mixed?
- Disassemble 2-3 more diverse commands to confirm the pattern holds
- Output: `reverse/command_table.md` — the Rosetta Stone

---

## Phase 2: Decode the Echo Protocol (6-8 hours)

### 2.1 — Isolate Echo Packets in Pcap (1 hr)
- Echo packets should be high-frequency, larger than commands, on a specific port
- Filter and isolate them in Wireshark
- Look for repeating structure: does every packet start with the same magic/header?
- Note packet sizes at different range settings (longer range = more echo samples?)
- Output: sample echo packets annotated

### 2.2 — Disassemble `DecodeImoEchoFormat` (2-3 hrs)
- This is the main echo decoder
- Map the byte layout: where is angle, heading, range, scale, sweep length, echo data?
- Cross-reference with the `RadarSweepMetadata` struct from `nndll.h`:
  ```c
  INT16 angle; UINT16 heading; INT16 headingFlag;
  INT16 radarNo; INT16 range; INT16 scale;
  INT16 sweepLength; INT16 status;
  ```
- Verify against pcap echo packets
- Output: `reverse/echo_format.md`

### 2.3 — Understand Echo Data Encoding (1-2 hrs)
- After the header, what is the echo data?
- Raw intensity values? Compressed? Run-length encoded?
- How many bits per sample? How many samples per sweep?
- Does this change with range or pulse width settings?
- Output: append to `reverse/echo_format.md`

### 2.4 — Understand Tile Echo Format (1-2 hrs)
- Disassemble the tile echo callback path
- How does tile format differ from sweep format?
- Document the tile coordinate system
- Output: append to `reverse/echo_format.md`

---

## Phase 3: Decode Init & Connection Handshake (4-6 hours)

### 3.1 — Disassemble `NcInitialNavnetCommon` (2-3 hrs)
- What sockets are opened? What ports bound?
- Is there a discovery/broadcast phase?
- What's the host registration protocol (the FNET layer)?
- Does the radar require a specific handshake before it starts sending echoes?
- Cross-reference with init packets in pcap
- Output: `reverse/init_handshake.md`

### 3.2 — Disassemble `RmInitialModule` + `RmSetServerClientMode` (1-2 hrs)
- What does "client mode" vs "server mode" mean on the wire?
- What additional sockets/subscriptions does this set up?
- Is there an echo subscription message sent to the radar?
- Output: append to `reverse/init_handshake.md`

### 3.3 — Decode the Keepalive / Alive Check (1 hr)
- `NcGetAliveFlag`, `RmcSetAliveCheck` — what's the heartbeat protocol?
- How often? What packet? Does the radar drop the connection without it?
- Output: append to `reverse/init_handshake.md`

---

## Phase 4: Build Minimal Rust Client — Echo Receive (6-8 hours)

### 4.1 — Project Setup (1 hr)
- `cargo init --lib radar-client` in project root
- Add dependencies: `tokio`, `bytes`, `tracing`
- Define core types: `SweepData`, `Range`, `Gain`, `RadarConfig`
- Output: compilable empty crate

### 4.2 — Implement Connection + Init Handshake (2-3 hrs)
- Open UDP sockets on the correct ports
- Send the init/registration packets we decoded in Phase 3
- Send the echo subscription / "I am a client" message
- Implement the keepalive heartbeat
- Output: `radar-client/src/connection.rs`

### 4.3 — Implement Echo Receive + Decode (2-3 hrs)
- Receive echo UDP packets
- Decode header (angle, heading, range, scale, sweep length)
- Decode echo intensity data
- Expose as a stream: `async fn subscribe_echoes() -> impl Stream<Item = Sweep>`
- Output: `radar-client/src/echo.rs`

### 4.4 — First Live Test: Echo Reception (1 hr)
- Write a minimal binary that connects and prints sweep metadata
- Compare angle/heading/range values against Furuno app output
- **Validation:** run both apps simultaneously, diff the echo data
- Output: test binary + results

---

## Phase 5: Build Rust Client — Command Sending (4-6 hours)

### 5.1 — Implement Command Serialization (2-3 hrs)
- Generic command builder: header + IRAD ID + payload
- Implement the commands we actually use:
  - `set_transmit(bool)` — IRAD_TXSTBY (9)
  - `set_range(Range)` — IRAD_RANGE (2)
  - `set_gain(Gain)` — IRAD_GAIN (3)
  - `set_sea_clutter(SeaClutter)` — IRAD_ACSEA (4)
  - `set_rain_clutter(RainClutter)` — IRAD_ACRAIN (5)
  - `set_pulse_width(PulseWidth)` — IRAD_PULSEWIDTH (8)
  - `set_stc(near, mid, far)` — IRAD_NEARSTC/MIDSTC/FARSTC (37-39)
  - `set_antenna_speed(speed)` — IRAD_ANTREVOLUTION (41)
  - `set_bearing_resolution(mode)` — IRAD_BEARINGRESOLUTIONSELECT (142)
- Output: `radar-client/src/commands.rs`

### 5.2 — Implement State Receive (1-2 hrs)
- The radar sends state updates back (same format as commands, opposite direction)
- Parse incoming state packets using the same IRAD ID mapping
- Store current radar state in a shared struct
- `fn get_range() -> Range`, `fn get_gain() -> Gain`, etc.
- Output: `radar-client/src/state.rs`

### 5.3 — Live Test: Command Round-Trip (1-2 hrs)
- Send a range change from Rust, verify radar responds (echo data changes scale)
- Send gain change, verify echo intensity changes
- Toggle transmit on/off
- **Validation:** capture pcap of Rust commands, diff against Furuno pcap byte-for-byte
- Output: test results

---

## Phase 6: Expand Command Coverage (4-6 hours)

### 6.1 — Implement All Image-Affecting Commands (2-3 hrs)
Using the same serialization pattern, add:
- Display mode, echo color, video level
- 2nd echo reject, next echo, echo clear
- Antenna type, switch, height
- High sensitivity mode, observation mode
- Tune, tune adjust
- Custom gain/sea/rain per picture preset
- Output: extend `commands.rs`

### 6.2 — Implement ARPA Commands (1-2 hrs)
- Target select, acquire, cancel
- ARPA display mode, vector, CPA/TCPA
- ARPA preset, all clear
- Output: `radar-client/src/arpa.rs`

### 6.3 — Implement Guard Zone Commands (1 hr)
- Set/get guard zone parameters
- Guard zone alarm detection
- Output: `radar-client/src/guard_zone.rs`

---

## Phase 7: Implement ARPA Target Receive (3-4 hours)

### 7.1 — Decode ARPA Packet Format (1-2 hrs)
- Capture ARPA target data from pcap (need a target in radar view)
- Disassemble `RmaGetArpaInfo` / `RmaGetTargetInfo` to understand the receive format
- Cross-reference with `ARPA`, `ARPA_TARGET`, `ARPA_JUDGE` structs from `nndll.h`
- Output: append to `reverse/protocol_notes.md`

### 7.2 — Implement ARPA Target Stream (1-2 hrs)
- Parse incoming ARPA packets
- Expose as stream: `async fn subscribe_targets() -> impl Stream<Item = ArpaTarget>`
- Output: extend `radar-client/src/arpa.rs`

---

## Phase 8: Rewrite Application Layer (6-8 hours)

### 8.1 — Port main.cpp Init to Rust Client (1-2 hrs)
- Replace the Furuno init sequence with `RadarConnection::connect()`
- Replace echo callback with the Rust echo stream
- Verify identical behavior

### 8.2 — Port web_server.cpp Controls (2-3 hrs)
- Replace all `Furuno::RmcSet*` / `Furuno::RmcGet*` calls with Rust client methods
- The web server endpoints stay the same, just the backend calls change

### 8.3 — Port pattern_control.cpp (1-2 hrs)
- Replace pattern sweep logic to use Rust client commands
- Verify pattern cycling works identically

### 8.4 — Remove Furuno Dependency (1 hr)
- Remove `furuno/lib/libNAVNETDLL.so` from the link step
- Update `scripts/build.sh`
- Verify clean build with zero Furuno dependencies
- Output: `libNAVNETDLL.so` is no longer needed

---

## Phase 9: Integration Testing & Hardening (3-4 hours)

### 9.1 — Full A/B Comparison (1-2 hrs)
- Run Furuno-backed app and Rust-backed app side by side
- Compare echo data, command responses, state reads
- Diff pcaps between the two

### 9.2 — Edge Cases & Stability (1-2 hrs)
- Radar power cycle during operation
- Network disconnect/reconnect
- Rapid command changes (pattern sweep)
- Long-duration stability (hours)

---

## Time Estimate Summary

| Phase | Description | Est. Hours |
|-------|-------------|-----------|
| 0 | Capture & decode setup | 4-6 |
| 0.5 | Raw UDP smoke test | 0.5 |
| 1 | Decode command protocol | 6-8 |
| 2 | Decode echo protocol | 6-8 |
| 3 | Decode init handshake | 4-6 |
| 4 | Rust client — echo receive | 6-8 |
| 5 | Rust client — commands | 4-6 |
| 6 | Expand command coverage | 4-6 |
| 7 | ARPA target receive | 3-4 |
| 8 | Rewrite app layer | 6-8 |
| 9 | Integration testing | 3-4 |
| **Total** | | **46.5-64.5 hrs** |

---

## Project Structure

```
RADAR_web/
  reverse/                        # RE artifacts (already started)
    symbols.md                    # Symbol classification (done)
    exports.tsv                   # Machine-readable symbols (done)
    all_functions.tsv             # All 6,102 functions (done)
    protocol_notes.md             # Protocol observations
    command_format.md             # Command packet structure
    command_table.md              # IRAD ID -> wire format mapping
    echo_format.md                # Echo packet structure
    init_handshake.md             # Init/connection sequence
    captures/                     # Pcap files
      session_log.md              # Capture session descriptions
  radar-client/                   # The clean Rust library
    Cargo.toml
    src/
      lib.rs                      # Public API
      connection.rs               # UDP socket management, init, keepalive
      echo.rs                     # Echo packet decode, sweep stream
      commands.rs                 # Command serialization + state receive
      arpa.rs                     # ARPA target tracking
      guard_zone.rs               # Guard zone management
      protocol/
        mod.rs                    # Packet framing, header format
        wire.rs                   # Low-level byte serialization
      types.rs                    # Range, Gain, Sweep, ArpaTarget, etc.
      error.rs                    # Error types
  main/                           # Existing C++ app (rewritten in Phase 8)
  furuno/                         # Kept as reference oracle until Phase 9
```

---

## What We Keep from the Old Plan

Everything in `reverse/` carries forward — the symbol classification, IRAD command mapping, internal architecture notes, and function listings are directly useful for targeted disassembly in Phases 1-3. The key insight that `RmcSet → rmSubSet → rmMakeCom → NcSendRadarCommand` is the chain tells us exactly which functions to disassemble to extract the wire format.
