# Codebase Analysis

This document provides a walkthrough of the sample code in this repository, focusing on data
flow, threading behaviour and potential performance bottlenecks. The main interest is the set of
sample programs under the `sample/` directory, especially **Furuno_Sample.cpp**, the optional web
server and pattern control utilities.

## Overview

The sample programs are small C++ applications linking against the `libNAVNETDLL` library. Their
purpose is to connect to a Furuno radar sensor, receive echo data through callbacks and optionally
serve a web interface for controlling the device. A minimal Python environment is also present but
not used by the main C++ logic.

Important source files:

- `sample/Furuno_Sample.cpp` – main example application.
- `sample/web_server.cpp` & `web_server.h` – simple HTTP server (requires Boost.Asio).
- `sample/pattern_control.cpp` – helper routines for automated range/gain patterns.
- `sample/Furuno_Sample_timed.cpp` and `Furuno_Sample_new.cpp` – alternative demos.
- Assets such as `index.html`, `script.js` and `styles.css` used by the web server.

The repository also contains a `UIsample/` directory with an older GUI-based sample which is not
analysed here.

## Initialisation Sequence

`Furuno_Sample.cpp` executes the following high‑level steps at startup:

1. **Read License / Vendor information** from `vendor.txt` and load `target.dat`.
2. **Initialise the NAVNETDLL library** using `Furuno::fec_entry_function`, `InitialITRON`, and
   `Initialize`.
3. **Configure network parameters** via `NcSetNetworkIpAddress`, `NcInitialNavnetCommon` and
   related functions.
4. **Start the radar module** (`StartRadarModule`) which registers several callback functions
   such as `RadarEchoCallback` and `RadarCycleCallback`.
5. **Find the radar sensor** on the network using `RmGetSensorHostCount` and
   `RmGetSensorHostName`.
6. **Begin receiving echo data** once the radar is detected and set as the source with
   `RmSetRadarSource`.
7. **Launch worker threads** for sweep writing and optionally the HTTP server.

After this initial setup the main thread simply sleeps forever while work is performed through
callbacks and background threads.

## Data Flow

### Echo Callback

`RadarEchoCallback` receives raw echo lines from the radar. Each call provides:

- `sweep_len` – number of echo samples (≤1024).
- `angle` – sweep angle in the range `0…8196` (one rotation ≈ 8196 units).
- `status`, `scale`, `range` and additional meta data.
- `echo[]` – array of `sweep_len` intensity values.

The callback appends these values to a global `std::vector<RadarEchoData> radarEchoBuffer`. When a
full rotation is detected (angle wrapped around) the buffer is moved into a `SweepWriteJob` and
pushed onto a queue.

### Sweep Writer Thread

`SweepWriterThread` runs in its own `std::thread`. It waits on a condition variable until a job is
available. For each rotation it:

1. Writes the CSV representation of the echo data (`WriteEchoCSV`).
2. Generates a PNG image by rasterising samples onto a square buffer (`WriteEchoImage`).

Both routines ensure output directories exist. Writing PNGs is relatively expensive because every
pixel is touched in a tight loop before calling `stbi_write_png`. This can become a bottleneck if
rotations arrive faster than they are written.

### Optional Web Server

If compiled with `USE_BOOST_WEB_INTERFACE`, `StartWebServerThread` launches a detached thread that
runs an HTTP server using Boost.Asio (`RunHttpServer`). Each connection is handled synchronously in
the same thread:

1. The request is parsed (only the GET method is supported).
2. Handlers in `web_server.cpp` are invoked to either serve static files, return status information
   or execute radar commands (e.g. set gain, set range, start pattern capture).

Because the server is single threaded, long‑running handlers may block other requests. Some
handlers start additional detached threads – for example `RunRangeGainPattern` is executed in a new
thread when `/run_pattern` is requested.

### Pattern Control

`RunRangeGainPattern` (in `pattern_control.cpp`) automates changes of range and gain. It loops over
a vector of `{range,gain,repeat}` steps while monitoring the current sweep angle to detect when a
full sweep has completed. The function is called in a detached thread so it does not block the web
server.

## Threading Model

The following threads are typically active when running `Furuno_Sample.cpp` with the web interface:

1. **Main thread** – performs initial setup and then sleeps.
2. **Sweep writer thread** – writes CSV and PNG files for each rotation.
3. **Web server thread** – handles HTTP requests sequentially.
4. **Pattern thread(s)** – spawned on demand when a pattern or calibration sequence is initiated.
5. **SDK/internal threads** – the NAVNETDLL library itself creates threads to deliver callbacks.

In practice the application usually has 3–5 threads under normal operation, plus any internal
threads from the SDK. The standard library call `std::thread::hardware_concurrency()` is not used
anywhere; thread count is fixed by the above logic.

## Potential Performance Bottlenecks

- **Disk I/O** – Writing PNG and CSV files for every rotation may overwhelm slower storage,
  especially if the antenna rotates quickly or high resolution images are produced.
- **PNG Generation** – `WriteEchoImage` iterates over every sample and updates individual pixels in
  a memory buffer before compressing with `stbi_write_png`, which is CPU intensive.
- **Single-threaded HTTP server** – all HTTP requests run sequentially. Slow handlers (such as
  starting large patterns) can block further requests until completion.
- **Growing Job Queue** – if `RadarEchoCallback` produces data faster than `SweepWriterThread` can
  write it, the queue `sweepQueue` may grow without bound, potentially exhausting memory.
- **Pattern Sequences** – long calibration patterns in `RunRangeGainPattern` involve many blocking
  sleeps and radar commands. When triggered through the web interface they run in detached threads,
  but heavy use may still load the radar or the host CPU.

## Data Transfer and Storage

All radar communication happens via the NAVNETDLL API. The application sets callback functions
which the SDK calls from its own worker threads. Echo lines are small (≤1024 bytes) and are copied
into memory. Output files are written to the local filesystem in `data/` (CSV) and `frames/` (PNG)
subdirectories. When pattern runs complete these folders are renamed with a suffix (e.g.
`_pattern` or `_calibration`) to preserve previous captures.

## Summary

The sample programs demonstrate how to integrate with a Furuno radar sensor, capture echo data and
serve a minimal web API. The heaviest operations are file generation and pattern execution.
Expensive work is mostly delegated to background threads, but the HTTP server remains
single-threaded. Real-world deployments should monitor disk usage and possibly offload image
encoding to a more efficient implementation if throughput becomes an issue.

