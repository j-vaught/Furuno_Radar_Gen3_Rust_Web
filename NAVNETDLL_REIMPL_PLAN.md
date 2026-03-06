# NAVNETDLL Rust Reimplementation Plan

## Scope

Replace `furuno/lib/libNAVNETDLL.so` (Furuno proprietary, aarch64 ELF, ~1.3MB, not stripped) with a from-scratch Rust `.so` that is ABI-compatible with the existing C++ codebase.

**Key facts:**
- 3,051 exported symbols total, ~986 `Rm*` radar functions
- The existing codebase only calls ~40 unique functions (see "Actually Used API" below)
- 3 public headers: `nndll.h` (1223 lines), `fectype.h` (42 lines), `lasterror.h` (59 lines)
- Binary is **not stripped** — symbol names are intact for disassembly

**Strategy:** Implement only what the codebase actually uses first. Expand coverage later as needed.

---

## Actually Used API (from main.cpp, pattern_control.cpp, web_server.cpp)

### Initialization & Lifecycle
- `fec_entry_function(const char*)` — loads the .so
- `InitialITRON()` — ITRON RTOS init
- `LeClearLastAPIError()` / `LeClearLastBackgroundError()` / `LeGetLastError(INT32)`
- `NcSetNetworkIpAddress(u8,u8,u8,u8)`
- `NcInitialNavnetCommon(u8*, i32*)`
- `NcInitialNmea183Server()`
- `NcInitialNmea0183PortStatus(i32)`

### Radar Module Setup
- `RmGetSensorHostCount(i32*)`
- `RmGetSensorHostName(i32, u8*)`
- `RmSetRadarSource(i32, u8*, i32)`
- `RmGetModuleVersionNo(u8*)`
- `RmSetServerClientMode(i32)`
- `RmInitialModule(u8*, callback)`
- `RmaInitialArpaModule()`
- `RmSetEchoCallbackFunc(callback)`
- `RmSetCycleCallbackFunc(callback)`
- `RmSetEchoInterpolationFunc(i32)`
- `RmSetEchoSweepTotal(i32)`
- `RmSetTileEchoCallbackFunc(callback)`
- `RmGetConnection(i32*)`

### Radar Control (Rmc — Set)
- `RmcSetRange(i32, u16, u16)` / `RmcSetRange_v2(...)`
- `RmcSetGAIN(i32, u16, u16, u16)`
- `RmcSetACSEA(i32, u16, u16, u16, u16)`
- `RmcSetACRAIN(i32, u16, u16, u16, u16, u16)`
- `RmcSetPulseWidth(i32, i16, i16, u16, i32)`
- `RmcSetTxStby(i32, u16, u16, u32, u32)`
- `RmcSetNearSTC(i32, u16)` / `RmcSetMiddleSTC(...)` / `RmcSetFarSTC(...)`
- `RmcSetAntennaRevolution(i32, u16, u16)`
- `RmcSetBearingResolutionSelect(i32, u16)`

### Radar Control (Rmc — Get)
- `RmcGetRange(i32, i16*, u16*)` / `RmcGetRange_v2(...)`
- `RmcGetGAIN(i32, u16*, u16*, u16*)` / `RmcGetCustomGAIN(...)`
- `RmcGetPulseWidth(i32, i16*, i16*, u16*, i32*)`
- `RmcReqPulseWidth(...)`
- `RmcGetNearSTC(...)` / `RmcGetMiddleSTC(...)` / `RmcGetFarSTC(...)`
- `RmcGetAntennaRevolution(i32, u16*, u16*)`
- `RmcGetACSEA(i32, u16*, u16*, u16*, u16*)`
- `RmcGetACRAIN(i32, u16*, u16*, u16*, u16*, u16*)`
- `RmcGetBearingResolutionSelect(i32, u16*)`

---

## Phases & Tasks

### Phase 0: Tooling & Reconnaissance (3-5 hours)

#### 0.1 — Full Symbol Dump & Classification (1-2 hrs)
- Run `nm -D`, `readelf -s`, `objdump -T` on the .so
- Categorize all 3,051 symbols by module prefix (Nc, Rm, Rmc, Rma, fec_, Le, etc.)
- Build a spreadsheet/markdown mapping: symbol -> used/unused -> header declaration
- Output: `reverse/symbols.md`

#### 0.2 — Disassembly Setup (1-2 hrs)
- Install Ghidra or radare2 (r2) on the Pi or a dev machine
- Load the .so into the disassembler, let auto-analysis run
- Verify symbol names resolve correctly (binary is not stripped)
- Bookmark the ~40 actually-used functions for focused analysis
- Output: disassembler project file saved to repo

#### 0.3 — Packet Capture Baseline (1 hr)
- Set up `tcpdump` or Wireshark on the network interface to the radar
- Run the existing C++ app, capture a full init -> transmit -> echo cycle
- Save pcaps to `reverse/captures/`
- Note the base port number, UDP vs TCP, broadcast addresses
- Output: `reverse/captures/baseline.pcap`, `reverse/protocol_notes.md`

---

### Phase 1: Understand the Init Sequence (4-6 hours)

#### 1.1 — Disassemble `fec_entry_function` + `InitialITRON` (2-3 hrs)
- These are the entry points — understand what they set up
- Map out global state: mutexes, thread pools, network sockets
- Document the ITRON RTOS emulation layer (the `acre_tsk`, `acre_sem`, etc. symbols suggest a userspace ITRON shim)
- Output: `reverse/init_sequence.md`

#### 1.2 — Disassemble `NcSetNetworkIpAddress` + `NcInitialNavnetCommon` (2-3 hrs)
- Understand how the library configures its network stack
- What sockets does it open? What ports? UDP broadcast?
- Trace the flow: IP set -> common init -> NMEA server init -> ready
- Cross-reference with pcap from 0.3
- Output: `reverse/network_init.md`

---

### Phase 2: Understand the Radar Echo Pipeline (6-8 hours)

#### 2.1 — Disassemble `RmInitialModule` + `RmSetEchoCallbackFunc` (2-3 hrs)
- How does the library register and invoke echo callbacks?
- What threads are spawned? What's the receive loop?
- Map the callback signature to the actual packet format
- Output: `reverse/echo_pipeline.md`

#### 2.2 — Decode the Echo Packet Format (2-3 hrs)
- Use pcap + disassembly of `DecodeImoEchoFormat` to map byte layout
- Document: header fields, sweep angle, heading, echo data array, lengths
- Match against the `RadarSweepMetadata` struct from `nndll.h`
- Output: `reverse/echo_format.md`

#### 2.3 — Disassemble the Tile Echo Path (1-2 hrs)
- `RmSetTileEchoCallbackFunc` — how do tile echoes differ from sweep echoes?
- Document the tile coordinate system (xPos, yPos, tileSize, sweepCenter)
- Output: append to `reverse/echo_format.md`

---

### Phase 3: Understand Radar Command Protocol (4-6 hours)

#### 3.1 — Disassemble `RmcSetRange` / `RmcSetGAIN` / `RmcSetTxStby` (2-3 hrs)
- These are the most-used Set commands
- Trace: function call -> command buffer build -> `NcSendRadarCommand`
- Document the command packet format: ID byte (from IRAD_* defines), payload layout
- Cross-reference with pcap
- Output: `reverse/command_format.md`

#### 3.2 — Disassemble `RmcGetRange` / `RmcGetGAIN` and the Get pipeline (2-3 hrs)
- How does the library store received radar state?
- Is it a shared memory table indexed by IRAD_* command IDs?
- Document the state storage mechanism
- Output: append to `reverse/command_format.md`

---

### Phase 4: Scaffold the Rust Crate (3-4 hours)

#### 4.1 — Create the Rust Project Structure (1-2 hrs)
- `cargo init --lib` in `rust/navnet-rs/`
- Set `crate-type = ["cdylib"]` for .so output
- Define the `extern "C"` + `#[no_mangle]` stubs for all ~40 used functions
- Define Rust equivalents of `fectype.h` types and the `ARPA`/`RadarSweepMetadata` structs
- Verify it compiles to a .so with matching symbol names
- Output: compilable skeleton crate

#### 4.2 — Build System Integration (1-2 hrs)
- Modify `scripts/build.sh` to optionally link against the Rust .so
- Set up cross-compilation if developing on non-aarch64
- Add a `Makefile` or `just` recipe: `build-rust` -> `cargo build --release` -> copy .so
- Verify the C++ app loads the Rust .so without crashing (all stubs return OK codes)
- Output: working build pipeline

---

### Phase 5: Implement Init + Network Layer (6-8 hours)

#### 5.1 — Implement ITRON Shim (2-3 hrs)
- Minimal ITRON userspace: `acre_tsk` (create task/thread), `acre_sem` (semaphore), `acre_flg` (event flag), `acre_mbf` (mailbox)
- Map to Rust `std::thread`, `std::sync::Mutex`, channels
- Only implement what the init path actually uses
- Output: `rust/navnet-rs/src/itron.rs`

#### 5.2 — Implement `fec_entry_function` + `InitialITRON` (1-2 hrs)
- Initialize global state, logging, thread pool
- Output: `rust/navnet-rs/src/init.rs`

#### 5.3 — Implement Network Configuration + `NcInitialNavnetCommon` (2-3 hrs)
- `NcSetNetworkIpAddress`, `NcInitialNavnetCommon`, `NcInitialNmea183Server`
- Open the correct UDP sockets, bind to the correct ports
- Set up the receive loop thread(s)
- Output: `rust/navnet-rs/src/network.rs`

---

### Phase 6: Implement Radar Echo Receive (4-6 hours)

#### 6.1 — Implement Echo Receive + Callback Dispatch (2-3 hrs)
- `RmInitialModule`, `RmSetEchoCallbackFunc`, `RmSetTileEchoCallbackFunc`
- Receive UDP echo packets, decode header, invoke registered callback
- Output: `rust/navnet-rs/src/radar/echo.rs`

#### 6.2 — Implement Sweep Interpolation + Total (1-2 hrs)
- `RmSetEchoInterpolationFunc`, `RmSetEchoSweepTotal`
- Output: extend `echo.rs`

#### 6.3 — First Live Test: Echo Reception (1 hr)
- Swap in the Rust .so, boot the app, verify echo callbacks fire
- Compare sweep data against known-good captures
- Output: test results documented

---

### Phase 7: Implement Radar Commands (4-6 hours)

#### 7.1 — Implement Rmc Set Commands (2-3 hrs)
- All the `RmcSet*` functions: build command packets, send via UDP
- Use the documented command format from Phase 3
- Output: `rust/navnet-rs/src/radar/commands.rs`

#### 7.2 — Implement Rmc Get Commands + State Storage (2-3 hrs)
- All the `RmcGet*` functions: parse incoming state, store in shared table
- Output: extend `commands.rs`

---

### Phase 8: Implement Supporting APIs (2-3 hours)

#### 8.1 — Error API (30 min)
- `LeClearLastAPIError`, `LeClearLastBackgroundError`, `LeGetLastError`
- Simple thread-local or atomic error codes

#### 8.2 — Sensor Discovery (1 hr)
- `RmGetSensorHostCount`, `RmGetSensorHostName`, `RmSetRadarSource`
- `RmGetConnection`, `RmGetModuleVersionNo`

#### 8.3 — ARPA Module Stub (1 hr)
- `RmaInitialArpaModule` — currently used but ARPA data isn't consumed
- Stub it to return OK, implement later if needed

---

### Phase 9: Integration Testing (3-4 hours)

#### 9.1 — Full Lifecycle Test (2 hrs)
- Boot with Rust .so, init, connect to radar, receive echoes, send commands
- Compare behavior against original .so side by side

#### 9.2 — Stress & Edge Cases (1-2 hrs)
- Radar disconnect/reconnect
- Invalid parameters
- Multiple rapid command changes (pattern_control.cpp sweep patterns)

---

## Time Estimate Summary

| Phase | Description | Est. Hours |
|-------|-------------|-----------|
| 0 | Tooling & Recon | 3-5 |
| 1 | Init Sequence RE | 4-6 |
| 2 | Echo Pipeline RE | 6-8 |
| 3 | Command Protocol RE | 4-6 |
| 4 | Rust Scaffold | 3-4 |
| 5 | Init + Network Impl | 6-8 |
| 6 | Echo Receive Impl | 4-6 |
| 7 | Command Impl | 4-6 |
| 8 | Supporting APIs | 2-3 |
| 9 | Integration Testing | 3-4 |
| **Total** | | **39-56 hrs** |

## File Structure (planned)

```
RADAR_web/
  reverse/                    # Reverse engineering notes & artifacts
    symbols.md                # Full symbol classification
    init_sequence.md          # Init flow documentation
    network_init.md           # Network setup documentation
    echo_format.md            # Echo packet format
    command_format.md         # Command packet format
    protocol_notes.md         # General protocol observations
    captures/                 # Packet captures
  rust/
    navnet-rs/
      Cargo.toml
      src/
        lib.rs                # extern "C" exports
        init.rs               # fec_entry_function, InitialITRON
        itron.rs              # ITRON RTOS shim
        network.rs            # Nc* network functions
        error.rs              # Le* error functions
        radar/
          mod.rs
          echo.rs             # Echo receive + callbacks
          commands.rs         # Rmc Set/Get commands
          arpa.rs             # ARPA module
          discovery.rs        # Sensor host discovery
        types.rs              # fectype.h equivalents, structs
```
