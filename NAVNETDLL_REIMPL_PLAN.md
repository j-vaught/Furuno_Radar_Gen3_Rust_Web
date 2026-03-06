# NAVNETDLL Rust Reimplementation Plan

## Scope

Replace `furuno/lib/libNAVNETDLL.so` (Furuno proprietary, aarch64 ELF, ~1.3MB, not stripped) with a from-scratch Rust `.so` that is ABI-compatible with the existing C++ codebase.

**Key facts:**
- **6,102 defined functions** in the binary (all GLOBAL visibility)
- 3 public headers: `nndll.h` (1223 lines), `fectype.h` (42 lines), `lasterror.h` (59 lines)
- Binary is **not stripped** — all symbol names intact for disassembly
- Only imports ~164 libc functions (socket ops, string ops, file I/O, pthreads)
- Contains a userspace **ITRON RTOS shim** (`acre_tsk`, `acre_sem`, `acre_flg`, `acre_mbf`)
- Contains **Blowfish encryption** (`bfInitialize`, `bfEncrypt`, `bfDecrypt`, `bfDecode`)

**Radar API surface breakdown:**
| Module | Prefix | Count | Description |
|--------|--------|-------|-------------|
| Radar Commands (Set) | `RmcSet*` | 148 | Send commands to radar hardware |
| Radar Commands (Get) | `RmcGet*` | 139 | Read radar state |
| Radar Commands (Req) | `RmcReq*` | ~10 | Request specific data from radar |
| Radar Module Core | `Rm*` | 67 | Echo pipeline, callbacks, guard zones, heading |
| ARPA Target Tracking | `Rma*` | 13 | Automatic Radar Plotting Aid |
| NAVNET Common | `Nc*` | ~200 | Network stack, NMEA, TCP/IP, servers |
| Internal Support | misc | ~5,500+ | ITRON shim, crypto, protocol, I/O, math |

**Important: the iceberg problem.** Our codebase directly calls ~40 functions, but each of those calls triggers a deep chain of internal functions. For example, `RmcSetRange()` likely calls into command serialization, the ITRON message queue, the network send path, and possibly Blowfish encryption — touching potentially hundreds of internal functions. The 40 "used" functions are the tip; the actual software surface being exercised is far larger.

**Strategy: Comprehensive radar coverage, phased by testability.**
1. Reverse engineer the full protocol and internal architecture (not just the 40 we call today)
2. Implement ALL 148 `RmcSet*` + 139 `RmcGet*` + 67 `Rm*` + 13 `Rma*` functions = **367 radar functions**
3. Prioritize functions that affect radar output/image (echo, gain, range, STC, pulse, clutter, ARPA)
4. Use live radar testing to validate which functions actually change radar behavior
5. Implement `Nc*` networking layer as needed to support the radar functions

---

## API Tiers (by impact on radar output/image)

### Tier 1 — Core Echo & Display (implement first, test first)
These directly affect what you see on screen.

**Echo pipeline (Rm*):**
- `RmInitialModule`, `RmEndModule`
- `RmSetEchoCallbackFunc`, `RmSetTileEchoCallbackFunc`, `RmSetCycleCallbackFunc`
- `RmSetEchoInterpolationFunc`, `RmSetInterpolationSweepNum`, `RmSetEchoSweepTotal`
- `RmSetLowLevelEcho`, `RmGetSweeps`
- `RmSetTxEchoFormat`, `RmGetTxEchoFormat`, `RmTransmitEcho`
- `RmInitializeImoEcho`, `RmEndImoEcho`, `DecodeImoEchoFormat`
- `RmSetRadarEchoProcessMode`

**Image-affecting commands (RmcSet/Get*):**
- Range: `RmcSet/GetRange`, `RmcSet/GetRange_v2`, `RmcSet/GetRangeSelect`
- Gain: `RmcSet/GetGAIN`, `RmcSet/GetCustomGAIN`
- Clutter: `RmcSet/GetACSEA`, `RmcSet/GetCustomACSEA`, `RmcSet/GetACRAIN`, `RmcSet/GetCustomACRAIN`
- STC: `RmcSet/GetNearSTC`, `RmcSet/GetMiddleSTC`, `RmcSet/GetFarSTC`, `RmcSet/GetStcRange`
- Pulse: `RmcSet/GetPulseWidth`, `RmcSet/GetPulseRange`, `RmcReqPulseWidth`
- TX: `RmcSet/GetTxStby` (transmit/standby)
- Display: `RmcSet/GetDispMode`, `RmcSet/GetEchoColorType`, `RmcSet/GetVideoLevel`
- Resolution: `RmcSet/GetBearingResolutionSelect`, `RmcSet/GetHighSensitivityMode`
- Echo processing: `RmcSet/Get2ndEchoReject`, `RmcSet/GetNextEcho`, `RmcSet/GetEchoClear`
- Antenna: `RmcSet/GetAntennaRevolution`, `RmcSet/GetAntennaType`, `RmcSet/GetAntennaSwitch`

### Tier 2 — Radar Configuration & Targeting
These affect radar behavior but not directly the echo image.

**ARPA target tracking (Rma*):**
- `RmaInitialArpaModule`, `RmaSetArpaCallbackFunc`, `RmaDeleteArpaInfo`
- `RmaGetArpaInfo`, `RmaGetNextArpaInfo`, `RmaGetNextArpaInfoAll`
- `RmaGetTargetInfo`, `RmaGetNextTargetInfo`
- `RmaGetTargetCpaTcpa`, `RmaGetTargetCpaTcpaAll`, `RmaGetTargetProximity`
- `RmaGetJudgeInfo`, `RmaGetAnalyseInfo`

**ARPA commands (RmcSet/Get*):**
- All `RmcSet/GetArpa*` functions (~48 Set + ~45 Get)
- Target select, acquire, cancel, lost filter, CPA/TCPA, vectors, presets

**Guard zone (Rm*):**
- `RmSetGzTargetZone`, `RmSetGzDetectLevel`, `RmSetGzDetectSize`
- `RmSetGzJudgeType`, `RmSetGzJudgeCount`, `RmSetGzSizeTbl`
- `RmSetGzAlarmCallbackFunc`, `RmResetGzResult`, `RmResetGuardZoneStatus`
- All corresponding `RmGet*` guard zone functions

**Navigation & heading:**
- `RmcSet/GetHeading`, `RmcSet/GetHeadingAdjust`, `RmcSet/GetHeadingSource`
- `RmSet/GetHeadingData`, `RmSet/GetHeadingDataDegree`, `RmSet/GetHeadingStatus`
- `RmcSet/GetBlindSector`, `RmcSet/GetRadarPosition`
- `RmcSet/GetDrift`, `RmcSet/GetSpeedSelect`

### Tier 3 — System, Alarms, Maintenance
Less common but needed for a complete system.

- Alarms: `RmcSet/GetAlarmStatus`, `RmcSet/GetAlarmAck`, `RmcSet/GetAisAlarm`, etc.
- Tune/calibration: `RmcSet/GetTune`, `RmcSet/GetTuneAdjust`, `RmcSet/GetTuneIndicator`
- Hardware: `RmcSet/GetMagnetron`, `RmcSet/GetMagnetronInd`, `RmcSet/GetPM`, `RmcSet/GetPmAdjust`
- System: `RmcSet/GetVersionNo`, `RmcSet/GetConnect`, `RmcSet/GetOnTime`, `RmcSet/GetTxTime`
- Custom pictures: `RmcSet/GetCustomPicture`, `RmcSet/GetCustomPictureAll`, `RmcSet/GetPictureDefault`
- Trails: `RmcSet/GetTrailMode`, `RmcSet/GetTrailProcess`, `RmcSetTrailClear`
- IMO: `RmcSet/GetImoEchoSwitch`, `RmcSet/GetOperatorFitness`
- Misc: `RmcSet/GetMemory`, `RmcSet/GetCustomMemory`, `RmcSet/GetNN3Command`

### Tier 4 — Networking Layer (Nc*)
The plumbing that all of the above depends on. Must be reimplemented but is protocol-level, not radar-feature-level.

- Init: `fec_entry_function`, `InitialITRON`, `NcInitialNavnetCommon`, `NcBasicInitializationForModules`
- Network config: `NcSet/GetNetworkIpAddress`, `NcSet/GetNetworkSubnetMask`, `NcSet/GetNetworkDefaultGateway`
- Radar server: `NcInitialRadarServer`, `NcInitialRadarClient`, `NcConnectRadarServer`
- Echo transport: `NcSetRadarEchoCallbackFunc`, `NcSetRadarTileEchoCallbackFunc`, `NcSendRadarEchoData`
- Commands: `NcSendRadarCommand`, `NcSendRadarCommandEx`, `NcReadRadarData`
- NMEA: `NcInitialNmea183Server`, `NcPutNmeaData`, `NcNetworkWriteNMEA0183`, etc.
- Host management: `NcGetSensorHostCount`, `NcHostNameToIpAddr`, `NcGetAliveFlag`, etc.
- Error: `LeClearLastAPIError`, `LeClearLastBackgroundError`, `LeGetLastError`
- ITRON shim: `acre_tsk`, `acre_sem`, `acre_flg`, `acre_mbf`, `c_di`, `c_ei`, `c_enq`, `c_deq`

---

## Phases & Tasks

### Phase 0: Tooling & Reconnaissance (5-7 hours)

#### 0.1 — Full Symbol Dump & Classification (2-3 hrs)
- Run `nm -D`, `readelf -s`, `objdump -T` on the .so
- Categorize all 6,102 functions by module prefix
- Map every `RmcSet*`/`RmcGet*` to its corresponding `IRAD_*` command ID from `nndll.h`
- Identify internal call chains: which internal functions support each public API
- Build a spreadsheet/markdown mapping: symbol -> tier -> header declaration -> IRAD_* ID
- Output: `reverse/symbols.md`

#### 0.2 — Disassembly Setup (1-2 hrs)
- Install Ghidra or radare2 (r2) on the Pi or a dev machine
- Load the .so into the disassembler, let auto-analysis run
- Verify symbol names resolve correctly (binary is not stripped)
- Bookmark Tier 1 functions and their internal call targets
- Output: disassembler project file saved to repo

#### 0.3 — Packet Capture Baseline (1 hr)
- Set up `tcpdump` or Wireshark on the network interface to the radar
- Run the existing C++ app, capture a full init -> transmit -> echo cycle
- Save pcaps to `reverse/captures/`
- Note the base port number, UDP vs TCP, broadcast addresses
- Output: `reverse/captures/baseline.pcap`, `reverse/protocol_notes.md`

#### 0.4 — Live Radar API Audit (1-2 hrs)
- Write a C++ test harness that calls every `RmcSet*` and `RmcGet*` with safe parameters
- Log which calls succeed (return `RM_OK`) vs fail vs have no effect
- Record which commands visibly change radar output (observe echo data changes)
- This tells us which of the 367 radar functions are actually supported by YOUR specific radar hardware
- Output: `reverse/api_audit_results.md`

---

### Phase 1: Understand the Init Sequence (4-6 hours)

#### 1.1 — Disassemble `fec_entry_function` + `InitialITRON` (2-3 hrs)
- These are the entry points — understand what they set up
- Map out global state: mutexes, thread pools, network sockets
- Document the ITRON RTOS emulation layer (`acre_tsk`, `acre_sem`, etc.)
- Trace what threads are spawned, what event loops are created
- **Key question:** how many background threads, and what does each one do?
- Output: `reverse/init_sequence.md`

#### 1.2 — Disassemble `NcInitialNavnetCommon` + Network Init Chain (2-3 hrs)
- Understand how the library configures its network stack
- What sockets does it open? What ports? UDP broadcast?
- Trace the flow: IP set -> common init -> NMEA server init -> radar server/client init -> ready
- How does the Blowfish encryption layer fit in? Is it used for all packets or just licensing?
- Cross-reference with pcap from 0.3
- Output: `reverse/network_init.md`

---

### Phase 2: Understand the Radar Echo Pipeline (6-8 hours)

#### 2.1 — Disassemble `RmInitialModule` and the Full Echo Receive Chain (2-3 hrs)
- How does the library register and invoke echo callbacks?
- What threads are spawned? What's the receive loop?
- Trace the FULL internal call chain from UDP packet arrival to user callback
- Map every intermediate function: packet reassembly, decompression, format conversion
- **This is the iceberg** — expect 50+ internal functions in this path
- Output: `reverse/echo_pipeline.md`

#### 2.2 — Decode the Echo Packet Format (2-3 hrs)
- Use pcap + disassembly of `DecodeImoEchoFormat` to map byte layout
- Document: header fields, sweep angle, heading, echo data array, lengths
- Match against the `RadarSweepMetadata` struct from `nndll.h`
- Understand the difference between IMO echo format and standard format
- Document `RmSetTxEchoFormat` options (echoType, encType, txType)
- Output: `reverse/echo_format.md`

#### 2.3 — Disassemble the Tile Echo Path (1-2 hrs)
- `RmSetTileEchoCallbackFunc` — how do tile echoes differ from sweep echoes?
- Document the tile coordinate system (xPos, yPos, tileSize, sweepCenter)
- Output: append to `reverse/echo_format.md`

#### 2.4 — Understand Echo Interpolation Internals (1 hr)
- `RmSetEchoInterpolationFunc`, `RmSetInterpolationSweepNum`, `RmSetEchoSweepTotal`
- How does the library generate interpolated sweeps between real ones?
- This directly affects image quality — need to understand the algorithm
- Output: append to `reverse/echo_pipeline.md`

---

### Phase 3: Understand Radar Command Protocol (6-8 hours)

#### 3.1 — Disassemble the Command Send Path (2-3 hrs)
- Pick 3-4 `RmcSet*` functions of varying complexity (e.g., `RmcSetRange`, `RmcSetGAIN`, `RmcSetArpaDispMode`)
- Trace: function call -> command buffer build -> `NcSendRadarCommand` -> socket send
- Document the command packet format: how IRAD_* command IDs map to wire bytes, payload layout
- **Key question:** is there a single uniform command format, or do different commands have different wire formats?
- Cross-reference with pcap
- Output: `reverse/command_format.md`

#### 3.2 — Disassemble the Command Receive + State Storage Path (2-3 hrs)
- How does the library store received radar state?
- Is it a shared memory table indexed by IRAD_* command IDs?
- Trace from UDP receive -> state update -> `RmcGet*` read
- How does the command callback (`RmSetCommandCallbackFunc`) get invoked?
- Output: append to `reverse/command_format.md`

#### 3.3 — Map ALL RmcSet/Get Functions to Wire Format (2 hrs)
- Now that we understand the pattern, systematically document all 148 `RmcSet*` commands
- For each: IRAD_* ID, payload byte layout, parameter encoding
- Most will follow the same pattern — flag the ones that don't
- Output: `reverse/command_table.md` (the "Rosetta Stone" for the whole project)

---

### Phase 4: Scaffold the Rust Crate (4-6 hours)

#### 4.1 — Create the Rust Project Structure (2-3 hrs)
- `cargo init --lib` in `rust/navnet-rs/`
- Set `crate-type = ["cdylib"]` for .so output
- Define `extern "C"` + `#[no_mangle]` stubs for ALL 367 radar functions + Nc* + Le* + init
- Define Rust equivalents of `fectype.h` types, `ARPA`, `ARPA_TARGET`, `ARPA_JUDGE`, `ARPA_ANALYSE_DATA`, `RadarSweepMetadata` structs
- Use `cbindgen` or manual FFI to ensure ABI compatibility
- Verify it compiles to a .so with matching symbol names
- Output: compilable skeleton crate with every export stubbed

#### 4.2 — Build System Integration (1-2 hrs)
- Modify `scripts/build.sh` to optionally link against the Rust .so
- Set up cross-compilation if developing on non-aarch64
- Add a `Makefile` or `just` recipe: `build-rust` -> `cargo build --release` -> copy .so
- Verify the C++ app loads the Rust .so without crashing (all stubs return OK codes)
- Output: working build pipeline

#### 4.3 — Logging & Diagnostics Layer (1 hr)
- Add `tracing` or `env_logger` to the Rust crate
- Every stubbed function logs when called and with what parameters
- This becomes a powerful diagnostic: run the real app against the stub .so to see the exact call sequence
- Output: instrumented stub .so

---

### Phase 5: Implement Networking Layer (8-10 hours)

#### 5.1 — Implement ITRON Shim (2-3 hrs)
- Userspace ITRON: `acre_tsk` (create task/thread), `acre_sem` (semaphore), `acre_flg` (event flag), `acre_mbf` (mailbox/message buffer)
- `c_di`/`c_ei` (disable/enable interrupts → mutex lock/unlock)
- `c_enq`/`c_deq` (queue operations)
- Map to Rust `std::thread`, `std::sync::Mutex`, `crossbeam` channels
- Output: `rust/navnet-rs/src/itron.rs`

#### 5.2 — Implement Core Network Stack (3-4 hrs)
- `fec_entry_function` + `InitialITRON` — global state init
- `NcSetNetworkIpAddress`, `NcSetNetworkSubnetMask`, `NcSetNetworkDefaultGateway`, `NcSetNetworkBasePortNo`
- `NcInitialNavnetCommon` — open UDP sockets, bind ports, start receive threads
- `NcMakeBroadcastAddress`
- Host table management: `NcHostNameToIpAddr`, `NcGetTargetHostName`, `NcDeleteHostTableByIP`
- Output: `rust/navnet-rs/src/network.rs`

#### 5.3 — Implement Radar Server/Client Connection (2-3 hrs)
- `NcInitialRadarServer`, `NcInitialRadarClient`, `NcConnectRadarServer`, `NcConnectRadarServerEx`
- `NcUnconnectRadarServer`, `NcEndRadarClient`
- `NcSendRadarCommand`, `NcSendRadarCommandEx`, `NcReadRadarData`
- `NcSendRadarEchoData`, `NcSetRadarEchoCallbackFunc`, `NcSetRadarTileEchoCallbackFunc`
- `NcSetRadarConnectionFunc`
- Output: `rust/navnet-rs/src/network/radar.rs`

---

### Phase 6: Implement Tier 1 — Core Echo & Display (8-10 hours)

#### 6.1 — Implement Echo Receive + Callback Dispatch (3-4 hrs)
- `RmInitialModule`, `RmEndModule`
- `RmSetEchoCallbackFunc`, `RmSetTileEchoCallbackFunc`, `RmSetCycleCallbackFunc`
- Receive UDP echo packets, decode header, invoke registered callbacks
- `DecodeImoEchoFormat`, `RmInitializeImoEcho`, `RmEndImoEcho`
- Output: `rust/navnet-rs/src/radar/echo.rs`

#### 6.2 — Implement Echo Processing (2-3 hrs)
- `RmSetEchoInterpolationFunc`, `RmSetInterpolationSweepNum`, `RmSetEchoSweepTotal`
- `RmSetLowLevelEcho`, `RmSetRadarEchoProcessMode`
- `RmGetSweeps`
- `RmSetTxEchoFormat`, `RmGetTxEchoFormat`, `RmTransmitEcho`
- Output: extend `echo.rs`

#### 6.3 — Implement All Tier 1 RmcSet/Get Commands (2-3 hrs)
- Range, Gain, Clutter (AcSea/AcRain), STC, Pulse, TxStby, Display mode
- Echo color, video level, resolution boost, 2nd echo reject
- Antenna revolution/type/switch
- All the corresponding Get functions
- These all follow the same pattern: serialize -> send command packet
- Output: `rust/navnet-rs/src/radar/commands.rs`

#### 6.4 — First Live Test: Echo + Commands (1 hr)
- Swap in Rust .so, boot app, verify echo callbacks fire with correct data
- Send range/gain/tx commands, verify radar responds
- Output: test results documented

---

### Phase 7: Implement Tier 2 — ARPA, Guard Zones, Navigation (6-8 hours)

#### 7.1 — Implement ARPA Module (3-4 hrs)
- `RmaInitialArpaModule`, `RmaSetArpaCallbackFunc`
- `RmaGetArpaInfo`, `RmaGetNextArpaInfo`, `RmaGetNextArpaInfoAll`
- `RmaGetTargetInfo`, `RmaGetNextTargetInfo`, `RmaDeleteArpaInfo`
- `RmaGetTargetCpaTcpa`, `RmaGetTargetCpaTcpaAll`, `RmaGetTargetProximity`
- `RmaGetJudgeInfo`, `RmaGetAnalyseInfo`
- All `RmcSet/GetArpa*` commands (~93 functions)
- Output: `rust/navnet-rs/src/radar/arpa.rs`

#### 7.2 — Implement Guard Zones (1-2 hrs)
- All `RmSetGz*` / `RmGetGz*` / `RmResetGz*` functions
- `RmSetGzAlarmCallbackFunc`, `RmGetGuardZoneStatus`, `RmResetGuardZoneStatus`
- Output: `rust/navnet-rs/src/radar/guard_zone.rs`

#### 7.3 — Implement Navigation & Heading Commands (1-2 hrs)
- Heading, blind sector, radar position, drift, speed
- All corresponding Set/Get pairs
- Output: extend `commands.rs`

---

### Phase 8: Implement Tier 3 — System, Alarms, Maintenance (4-6 hours)

#### 8.1 — Alarms & Tune (2-3 hrs)
- Alarm status/ack, AIS alarm, tune/adjust/indicator
- Magnetron, PM adjust, hardware monitoring

#### 8.2 — Trails, IMO, Custom Pictures, Misc (2-3 hrs)
- Trail mode/process/clear
- IMO echo switch, operator fitness
- Custom picture/memory, NN3 command
- Version, connect, on-time, tx-time

---

### Phase 9: Implement Nc* Extended APIs (4-6 hours)

#### 9.1 — NMEA 0183 (2-3 hrs)
- `NcInitialNmea183Server`, `NcPutNmeaData`, `NcNetworkWriteNMEA0183`, `NcNetworkReadNMEA0183`
- NMEA SP, NMEA SP WIFI, NMEA F (heading/heave/wind/roll-pitch)
- NMEA 2000

#### 9.2 — System, Database, Keyboard, Relay, TCP/IP (2-3 hrs)
- `NcInitialSystemServer`, `NcConnectSystemServer`, `NcSendSystemCommand`
- Database, settings, keyboard, relay, MC, CIF, GMM, log, MCU

---

### Phase 10: Integration Testing & Validation (4-6 hours)

#### 10.1 — Full Lifecycle Test (2-3 hrs)
- Boot with Rust .so, init, connect to radar, receive echoes, send commands
- Compare behavior against original .so side by side
- Validate every Tier 1 and Tier 2 function against live radar

#### 10.2 — Comprehensive Command Audit (1-2 hrs)
- Run the API audit harness from Phase 0.4 against the Rust .so
- Compare results to the original .so results
- Every function that worked on the original must work identically on Rust

#### 10.3 — Stress & Edge Cases (1-2 hrs)
- Radar disconnect/reconnect
- Invalid parameters
- Multiple rapid command changes (pattern_control.cpp sweep patterns)
- Long-duration stability test

---

## Time Estimate Summary

| Phase | Description | Est. Hours |
|-------|-------------|-----------|
| 0 | Tooling & Recon + Live API Audit | 5-7 |
| 1 | Init Sequence RE | 4-6 |
| 2 | Echo Pipeline RE | 6-8 |
| 3 | Command Protocol RE | 6-8 |
| 4 | Rust Scaffold (ALL 367+ functions) | 4-6 |
| 5 | Networking Layer Impl | 8-10 |
| 6 | Tier 1: Core Echo & Display | 8-10 |
| 7 | Tier 2: ARPA, Guard Zones, Nav | 6-8 |
| 8 | Tier 3: System, Alarms, Maint | 4-6 |
| 9 | Nc* Extended APIs | 4-6 |
| 10 | Integration Testing | 4-6 |
| **Total** | | **59-81 hrs** |

---

## File Structure (planned)

```
RADAR_web/
  reverse/                        # Reverse engineering notes & artifacts
    symbols.md                    # Full symbol classification (6,102 functions)
    api_audit_results.md          # Live radar function test results
    init_sequence.md              # Init flow documentation
    network_init.md               # Network setup documentation
    echo_pipeline.md              # Full echo receive chain (50+ internal funcs)
    echo_format.md                # Echo packet format (sweep + tile)
    command_format.md             # Command send/receive protocol
    command_table.md              # ALL 148 RmcSet commands: IRAD ID, wire format, params
    protocol_notes.md             # General protocol observations
    captures/                     # Packet captures
  rust/
    navnet-rs/
      Cargo.toml
      src/
        lib.rs                    # All extern "C" exports (367+ functions)
        types.rs                  # fectype.h equivalents, ARPA, RadarSweepMetadata structs
        init.rs                   # fec_entry_function, InitialITRON
        itron.rs                  # ITRON RTOS shim (tasks, semaphores, flags, mailboxes)
        error.rs                  # Le* error functions
        network/
          mod.rs                  # Nc* core network stack
          radar.rs                # Radar server/client connection
          nmea.rs                 # NMEA 0183/2000 servers
          system.rs               # System/DB/Key/Relay/TCP servers
        radar/
          mod.rs
          echo.rs                 # Echo receive, decode, interpolation, callbacks
          commands.rs             # ALL RmcSet/Get commands (287 functions)
          arpa.rs                 # ARPA module (Rma* + RmcSet/GetArpa*, 106 functions)
          guard_zone.rs           # Guard zone (RmSetGz/GetGz*, ~20 functions)
          discovery.rs            # Sensor host discovery, connection status
```
