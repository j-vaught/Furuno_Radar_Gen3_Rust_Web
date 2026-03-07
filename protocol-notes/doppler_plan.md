# Doppler / Target Analyzer Reverse Engineering Plan

## Goal

Decode Furuno's Doppler (Target Analyzer / Echo Color) data from the DRS4DL+ radar.
The library exposes `RmcSetEchoColorType(radarNo, mode, sw, autoPicture)` which maps
to wire command `$SEF,{mode},{sw},{autoPicture}`. We need to find where Doppler
information appears in the echo data or callback parameters.

## What We Already Know

| Item | Detail |
|------|--------|
| Wire command | `$SEF,{mode},{sw},{autoPicture}` (ID 0xEF = decimal 239) |
| Callback params | `status` and `h_flg` are currently stored/ignored — likely Doppler carriers |
| Echo header unknowns | `byte[11] bits 7:5`, `byte[13]`, `byte[14:15]` — may carry Doppler flags |
| Navico precedent | HALO encodes Doppler as special amplitude values (14=receding, 15=approaching in 4-bit) |
| Furuno possibilities | Could be in echo amplitudes, callback params, header bits, or a separate UDP channel |

## Approach: Combined C++ Instrumentation + Raw Packet Capture

Run both simultaneously in one session so we can correlate library-decoded output
with raw wire data.

---

## Phase 1: Instrument the C++ App

### 1a. Add Doppler logging to RadarEchoCallback

Modify `main/src/main.cpp` to log `status`, `h_flg`, `heading`, and echo value
distribution on every spoke. Write to a dedicated log file so it doesn't pollute
normal output.

```cpp
// Add at file scope (near other globals):
static std::ofstream dopplerLog;
static std::atomic<bool> dopplerLogging{false};

// In RadarEchoCallback, after the existing code, add:
if (dopplerLogging.load()) {
    // Histogram of echo values (256 bins)
    int hist[256] = {};
    for (int i = 0; i < sweep_len; i++) hist[echo[i]]++;

    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    dopplerLog << ms << ","
               << no << ","
               << status << ","
               << h_flg << ","
               << heading << ","
               << angle << ","
               << sweep_len << ","
               << range << ","
               << scale;
    // Append full histogram
    for (int i = 0; i < 256; i++) dopplerLog << "," << hist[i];
    dopplerLog << "\n";
}
```

### 1b. Add web endpoint to toggle Doppler logging

In `web_server.cpp`, add `/doppler_log?action=start|stop` endpoint that:
- Opens/closes `runtime/output/doppler_log.csv`
- Sets `dopplerLogging` atomic flag
- Writes CSV header on start

### 1c. Add web endpoint to set EchoColorType

In `web_server.cpp`, add `/set_echo_color?mode=X&sw=Y&auto=Z` endpoint:
```cpp
server.resource["^/set_echo_color$"]["GET"] = [](auto response, auto request) {
    auto params = request->parse_query_string();
    int mode = std::stoi(params["mode"]);      // 0=off, 1=target, 2=rain?
    int sw = std::stoi(params["sw"]);           // 0=off, 1=on
    int autoPic = std::stoi(params["auto"]);    // auto picture mode
    int ret = Furuno::RmcSetEchoColorType(0, mode, sw, autoPic);
    // respond with result
};
```

Also add `/get_echo_color` that calls `RmcGetEchoColorType`.

---

## Phase 2: Capture Session

### Sequence (all times approximate):

| Time | Action | Duration |
|------|--------|----------|
| T+0s | Start tcpdump on eth0 (all radar traffic) | continuous |
| T+2s | Start Doppler logging via curl | - |
| T+5s | Baseline: Doppler OFF (`$SEF,0,0,0`) | 30s |
| T+35s | Enable Doppler mode 1 (`$SEF,1,1,0`) | 30s |
| T+65s | Enable Doppler mode 2 (`$SEF,2,1,0`) | 30s |
| T+95s | Try mode 3 if exists (`$SEF,3,1,0`) | 15s |
| T+110s | Try autoPicture=1 (`$SEF,1,1,1`) | 15s |
| T+125s | Disable Doppler (`$SEF,0,0,0`) | 15s |
| T+140s | Stop Doppler logging | - |
| T+142s | Stop tcpdump | - |

### Commands:

```bash
# Terminal 1: packet capture
sudo tcpdump -i eth0 -s 0 -B 8192 -w reverse/captures/doppler_session.pcap \
  'host 172.31.3.226 or net 239.255.0.0/16'

# Terminal 2: control sequence
curl "http://localhost:8080/doppler_log?action=start"
sleep 5

# Baseline (Doppler off)
curl "http://localhost:8080/set_echo_color?mode=0&sw=0&auto=0"
sleep 30

# Mode 1 (target analyzer)
curl "http://localhost:8080/set_echo_color?mode=1&sw=1&auto=0"
sleep 30

# Mode 2 (rain mode?)
curl "http://localhost:8080/set_echo_color?mode=2&sw=1&auto=0"
sleep 30

# Mode 3 (probe)
curl "http://localhost:8080/set_echo_color?mode=3&sw=1&auto=0"
sleep 15

# Auto picture
curl "http://localhost:8080/set_echo_color?mode=1&sw=1&auto=1"
sleep 15

# Off again
curl "http://localhost:8080/set_echo_color?mode=0&sw=0&auto=0"
sleep 15

curl "http://localhost:8080/doppler_log?action=stop"
```

---

## Phase 3: Simultaneous Rust Raw Capture

Run our Rust client in `dump-raw` mode during the same session to capture the raw
UDP echo packets with full headers, so we can inspect the unknown header bytes.

```bash
cd /home/user/Code_dev/RADAR_web/radar-client
source ~/.cargo/env && cargo run -- dump-raw --duration 150 --output reverse/captures/doppler_raw_echoes.bin
```

This gives us the raw 16-byte headers that the library strips before calling the callback.

---

## Phase 4: Analysis

### 4a. C++ callback analysis (doppler_log.csv)

Compare the three phases:
1. **status field**: Does it change between Doppler off/on? What values appear?
2. **h_flg field**: Same question. This is the most likely Doppler carrier.
3. **Echo histogram**: Do new amplitude values appear when Doppler is on?
   - If Furuno uses Navico-style encoding, we'd see values in ranges that don't
     appear in normal echo data (e.g., specific sentinel values)
   - Compare histogram shape between off/mode1/mode2

### 4b. Raw packet analysis

Compare UDP echo packets between Doppler off/on:
1. **Header byte[11] bits 7:5**: Do they change?
2. **Header byte[13]**: Does it change?
3. **Header bytes[14:15]**: Do they change?
4. **Echo payload**: Byte-level comparison of matching spoke angles
5. **Packet size**: Does it change? (Doppler data might add extra bytes)
6. **New UDP flows**: Does enabling Doppler open a new port/multicast group?

### 4c. TCP command capture

From the pcap, extract TCP packets on port 10100 to see:
1. The exact `$SEF` commands we sent
2. Any `$N` ack responses with extra data
3. Any spontaneous messages from the radar after Doppler enable

### Analysis script: `reverse/captures/analyze_doppler.sh`

```bash
#!/bin/bash
# Compare echo packets between Doppler phases using timestamps
# Extract TCP commands to verify $SEF was sent/acked
# Histogram analysis of echo values per phase
```

---

## Expected Outcomes

**Best case:** `h_flg` or `status` directly encodes Doppler state per-spoke, and
echo values contain Doppler-encoded amplitudes (like Navico's approach). We can
decode it immediately.

**Medium case:** Doppler info is in the raw UDP header bytes that the library strips.
We can decode from raw packets but need to bypass the library.

**Worst case:** Doppler data arrives on a separate channel (different port/multicast)
that we haven't been capturing. The captures will reveal this if it happens.

---

## Files Modified

- `main/src/main.cpp` — Add Doppler logging globals and callback instrumentation
- `main/src/web_server.cpp` — Add `/set_echo_color`, `/get_echo_color`, `/doppler_log` endpoints
- `reverse/captures/doppler_session.pcap` — Raw packet capture
- `reverse/captures/doppler_raw_echoes.bin` — Rust raw echo dump
- `runtime/output/doppler_log.csv` — C++ callback field log
- `reverse/captures/analyze_doppler.sh` — Analysis script
