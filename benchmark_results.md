# Benchmark & Architectural Analysis: C++ vs Rust Radar Web Server

**Date:** 2026-03-06
**Platform:** aarch64 / Debian GNU/Linux 12 (bookworm)
**CPU:** Raspberry Pi 5 Model B Rev 1.1
**RAM:** 16,219 MB

---

## 1. Performance Benchmarks

### Binary Size

| | Size |
|---|---|
| Rust (single static binary) | 3.15 MB |
| C++ binary only | 0.37 MB |
| C++ + libNAVNETDLL.so | 1.64 MB |

The Rust binary is larger because it statically links the Tokio async runtime, Axum web framework, PNG encoder, and the entire reimplemented protocol stack. The C++ binary is small because most logic lives in the proprietary `libNAVNETDLL.so` shared library (1.27 MB).

### Startup Time (binary launch to HTTP 200 on port 8080)

| | Trial 1 | Trial 2 | Trial 3 | Average |
|---|---|---|---|---|
| Rust | 3,814 ms | 3,783 ms | 3,979 ms | **3,858 ms** |
| C++ | 145 ms | 146 ms | 145 ms | **145 ms** |

The Rust client is ~26x slower to start because it performs the full radar handshake synchronously before binding the HTTP port. This includes TCP authentication (port 10010), control channel connection (port 10100), firmware query, and sending all 125 init register reads with response parsing. The C++ SDK defers most initialization to background threads and returns immediately.

### Runtime Resources (idle after init, no TX)

| | RSS (actual RAM) | VSZ (virtual) | CPU | Threads |
|---|---|---|---|---|
| Rust | **3.7 MB** | 336 MB | **0.0%** | **6** |
| C++ | 9.9 MB | 147 MB | **8.2%** | **21** |

Rust uses **63% less physical RAM** and **71% fewer threads**. The C++ process burns 8.2% CPU at idle due to polling loops inside `libNAVNETDLL.so`. The Rust process uses zero CPU at idle because all threads are parked on efficient kernel wait primitives (epoll, futex, hrtimer).

### Disk I/O

| Test | Time | Throughput |
|---|---|---|
| 10 MB sequential write | 37 ms | 270.2 MB/s |

Both programs share the same I/O subsystem; this is a baseline measurement.

### Shared Library Dependencies

| | Count |
|---|---|
| Rust | 5 (libc, libm, libgcc_s, libpthread, ld-linux) |
| C++ | 7 (above + libNAVNETDLL.so + libstdc++) |

The Rust binary has fewer external dependencies and no proprietary shared libraries.

---

## 2. Thread Architecture

### Rust: 6 Threads

| TID | Name | State | Wait Channel | Purpose |
|---|---|---|---|---|
| 657673 | radar-client | sleeping | futex_wait_queue | Main thread (parked after spawning Tokio) |
| 657705 | tokio-rt-worker | sleeping | do_epoll_wait | Tokio worker: HTTP accept + request handling |
| 657706 | tokio-rt-worker | sleeping | futex_wait_queue | Tokio worker: idle (available for async tasks) |
| 657707 | tokio-rt-worker | sleeping | futex_wait_queue | Tokio worker: idle |
| 657708 | tokio-rt-worker | sleeping | futex_wait_queue | Tokio worker: idle |
| 657709 | radar-client | sleeping | hrtimer_nanosleep | Echo listener thread (UDP multicast recv loop) |

**Thread origin in code:**
- **Main thread** (`main.rs:main()` → `web.rs:start_server()`): Spawns Tokio runtime, blocks on server future.
- **Tokio workers** (4 threads, auto-scaled to CPU cores): Created by `#[tokio::main]` macro. Handle all async HTTP requests, spawn_blocking tasks for radar commands.
- **Echo listener** (`web.rs:108 → thread::spawn(echo_listener)`): Dedicated OS thread that loops on UDP `recv_from()`, detects rotations, renders PPI images, writes CSV/PNG when recording.

**Why efficient:** All threads use kernel-level sleep (epoll for I/O readiness, futex for task scheduling, hrtimer for timed waits). No busy-wait loops. CPU usage is exactly 0% when idle.

### C++: 21 Threads

| Wait Channel | Count | Likely Source |
|---|---|---|
| hrtimer_nanosleep | 13 | Furuno SDK polling loops (`usleep`/`nanosleep`) |
| do_select | 6 | Furuno SDK socket `select()` calls (TCP/UDP monitors) |
| futex_wait_queue | 2 | Sweep writer thread + web server accept (condition_variable / mutex) |

**Thread origin in code:**
- **Main thread** (`main.cpp`): Radar module init, callback registration, blocks on main loop.
- **Sweep writer** (`main.cpp:SweepWriterThread()`): Consumer thread for async PNG/CSV writing. Waits on `condition_variable`.
- **Web server** (`web_server.cpp:StartWebServerThread()`): Detached thread running Boost ASIO accept loop.
- **~18 SDK threads** (spawned internally by `libNAVNETDLL.so`): The proprietary library creates threads for UDP multicast listening, TCP control channel management, ARPA processing, echo data buffering, keepalive timers, discovery beacons, and more. The application has no control over these.

**Why wasteful:** 13 of 21 threads are in `hrtimer_nanosleep` — they're polling loops sleeping for short intervals then waking up to check state. This is the classic "sleep-poll" pattern that burns CPU even when nothing is happening. The 8.2% idle CPU comes from these threads constantly waking and re-sleeping.

---

## 3. Architectural Comparison

### Protocol Implementation

| Aspect | C++ | Rust |
|---|---|---|
| Protocol handling | Delegates to proprietary `libNAVNETDLL.so` | Native reimplementation in `protocol.rs` |
| Auth handshake | Hidden inside SDK | Explicit 56-byte payload + response validation |
| Control commands | `Rmc*Set*()` / `Rmc*Get*()` function calls | ASCII `$S`/`$R` commands over raw TCP |
| Echo decoding | SDK decodes and delivers via callback | 4 encoding decoders (raw, RLE, delta-RLE, 2-bit) |
| Discovery | SDK handles internally | UDP broadcast beacon implementation |
| Keepalive | SDK handles internally | 8-byte binary packet every 5 seconds |

The Rust codebase fully understands and controls the wire protocol. The C++ codebase treats the radar as a black box behind the SDK API.

### Web Server

| Aspect | C++ | Rust |
|---|---|---|
| Framework | Boost ASIO (raw TCP) | Axum (async, production-grade) |
| Concurrency model | Single-threaded blocking accept | Tokio multi-threaded async |
| Request routing | String prefix matching | Type-safe route definitions |
| Static file serving | Manual file read + HTTP response | `tower-http::ServeDir` middleware |
| Parameter parsing | Manual query string splitting | Serde automatic deserialization |
| JSON generation | Manual string concatenation | `serde_json::to_string()` |

The C++ web server is a minimal hand-rolled HTTP implementation. The Rust web server uses a mature async framework with proper content negotiation, middleware, and type safety.

### State Management

| Aspect | C++ | Rust |
|---|---|---|
| Radar state | Global variables (no synchronization) | `Arc<AppState>` with `Mutex`/`RwLock`/`Atomic` |
| Settings tracking | Not tracked (query radar each time) | `CurrentSettings` struct updated on each set |
| Connection lifecycle | SDK manages internally | Explicit state machine (`ConnectionState` enum) |
| Error handling | Return codes from SDK, mostly ignored | `Result<T, RadarError>` with typed errors |
| Thread safety | Assumed single callback thread | Enforced by Rust's type system |

### Echo Processing & Rendering

| Aspect | C++ | Rust |
|---|---|---|
| Echo source | SDK callback delivers decoded data | Raw UDP multicast → 4 custom decoders |
| Regularization | 720 pulses × 868 bins (nearest-pulse) | Direct spoke rendering (no regularization step) |
| PPI rendering | Precomputed LUT (1735×1735) | Per-spoke radial walk (800×800) |
| Image size | 1735×1735 grayscale | 800×800 RGBA |
| Trail mode | Red/green decay overlay | Not implemented |
| Color scheme | Grayscale | Amber (green-tinted) |

The C++ renderer uses a precomputed lookup table for the polar-to-Cartesian transform, which is faster per frame but uses more memory. The Rust renderer walks each spoke radially, which is simpler and produces smaller images.

### Recording & CSV

| Aspect | C++ | Rust |
|---|---|---|
| CSV format | `Status,Scale,Range,Gain,Angle,Echo[0-1023]` | `spoke_index,angle_ticks,angle_deg,heading_ticks,num_bins,bin_0,...,bin_N` |
| Metadata | Inline per-row (status, scale, range, gain) | Comment header (`# key,value`) + `settings.json` |
| Triggering | Always recording when TX on | Manual start/stop via web UI |
| Output structure | Flat `runtime/output/` directory | `recordings/YYYYMMDD_HHMMSS/{csv,png}/` per session |
| Settings snapshot | Not saved | Full settings JSON at session start |

### Code Organization

| Aspect | C++ | Rust |
|---|---|---|
| Source files | 4 files (2,451 lines excl. stb_image) | 10 files (5,469 lines) |
| Modularity | 2 logical modules (main + web) | 10 focused modules |
| Testing | None | 27 integration tests + validation suite |
| CLI modes | 1 (web server only) | 12 (handshake, echo, render, calibrate, etc.) |
| Calibration | Hardcoded presets + web pattern API | CLI-driven parameter sweeps + web pattern API |
| Offline tools | None | CSV-to-PNG rendering, raw packet dump |
| Documentation | None | LaTeX technical manual + protocol notes |

### Dependency Chain

**C++:**
```
Application (2.4K lines)
  └── libNAVNETDLL.so (proprietary, closed-source, 1.3 MB)
       └── Linux sockets (internal)
  └── Boost ASIO (header-only)
  └── stb_image_write (header-only)
```

**Rust:**
```
Application (5.5K lines)
  ├── protocol.rs (wire protocol reimplementation)
  ├── connection.rs (TCP state machine)
  ├── Tokio (async runtime)
  ├── Axum (web framework)
  ├── png crate (image encoding)
  ├── socket2 (multicast UDP)
  ├── if-addrs (cross-platform NIC detection)
  └── serde (serialization)
```

The Rust codebase has **zero proprietary dependencies**. It can be built and deployed on any platform with a Rust toolchain. The C++ codebase requires the Furuno SDK shared library, which is only available for specific Linux targets.

---

## 4. Summary

| Metric | C++ | Rust | Winner |
|---|---|---|---|
| Startup time | 145 ms | 3,858 ms | C++ (26x faster) |
| Idle RAM (RSS) | 9.9 MB | 3.7 MB | Rust (63% less) |
| Idle CPU | 8.2% | 0.0% | Rust |
| Thread count | 21 | 6 | Rust (71% fewer) |
| Binary size (total) | 1.64 MB | 3.15 MB | C++ (48% smaller) |
| Proprietary dependencies | 1 (libNAVNETDLL.so) | 0 | Rust |
| Cross-platform | Linux only (SDK) | Linux, macOS, Windows | Rust |
| Protocol understanding | Black box (SDK) | Full reimplementation | Rust |
| Test coverage | None | 27 tests + validation | Rust |
| CLI versatility | 1 mode | 12 modes | Rust |
| Code lines | 2,451 | 5,469 | C++ (less code) |

The C++ implementation starts faster because the SDK defers initialization, but pays for it with 3.5x more threads and constant idle CPU burn. The Rust implementation takes longer to start because it performs a thorough synchronous handshake, but once running it is significantly more efficient, portable, testable, and maintainable.
