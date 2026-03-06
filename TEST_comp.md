# Comprehensive Test Playbook (`TEST_comp.md`)

This document defines a repeatable, end-to-end test workflow for NAVNETDLL.

## 1. Goals

Validate that:
- Build and startup work from current branch.
- Web server binds to the expected port.
- Radar control endpoints execute and return expected status.
- Key settings updates (TX, gain, range, pulse) are observable.
- Data capture produces CSV/PNG outputs.
- CSV output changes meaningfully when settings change.
- System metrics include queue/snapshot diagnostics.

## 2. Preconditions

- Repository root: `/usr/lib/NAVNETDLL`
- Branch under test checked out.
- Radar hardware/network reachable.
- License files present:
  - `main/runtime/license/vendor.txt`
  - `main/runtime/license/target.dat`
  - `main/runtime/license/Sensor_list.txt`
- SDK library present:
  - `furuno/lib/libNAVNETDLL.so`
  - `furuno/lib/libNAVNETDLL.so.01.95.0000`

## 3. Environment Setup

Run:

```bash
cd /usr/lib/NAVNETDLL
./scripts/check_prereqs.sh
```

Expected:
- Required checks all `[OK]`.
- If optional YOLO deps missing, only `[OPTIONAL-MISSING]` lines should appear.

## 4. Clean Start Procedure

1. Stop old processes and free port:

```bash
pkill -f '/usr/lib/NAVNETDLL/main/bin/navnet_main' || true
pkill -f '/usr/lib/NAVNETDLL/sample/main' || true
ss -ltnp | grep ':8080' || true
```

2. Build:

```bash
./scripts/build.sh
```

Expected: `Build complete: .../main/bin/navnet_main`

3. Start app in foreground (for logs):

```bash
cd main
env LD_LIBRARY_PATH=/usr/lib/NAVNETDLL/furuno/lib \
  ./bin/navnet_main runtime/license/vendor.txt runtime/license/target.dat 172.31.1.100 PC1
```

Expected startup indicators:
- `StartRadarModule : OK!`
- `[WebServer] Listening on port 8080...`
- eventually `Connected!` (if radar link is established)

## 5. Port + Health Checks

From a second terminal:

```bash
curl -s 'http://127.0.0.1:8080/get_system_info'
curl -s 'http://127.0.0.1:8080/system_metrics'
```

Expected:
- Valid JSON responses.
- `system_metrics` includes:
  - `completed_rotations`
  - `snapshots_produced`
  - `snapshots_consumed`
  - `snapshot_queue_high_watermark`

## 6. Command/Control Functional Tests

### 6.1 TX control

```bash
curl -s 'http://127.0.0.1:8080/toggle_echo?radarNo=0&mode=on'
curl -s 'http://127.0.0.1:8080/get_current?radarNo=0&command=TX'
```

Expected:
- `RmcSetTxStby returned: 0` in toggle response.
- `get_current TX` endpoint responds without errors.

### 6.2 Gain control + readback

```bash
curl -s 'http://127.0.0.1:8080/set_gain?radarNo=0&status=0&manualValue=12&autoValue=30'
curl -s 'http://127.0.0.1:8080/get_current?radarNo=0&command=GAIN'
```

Expected:
- Setter returns `0`.
- Getter reports `RmcGetGAIN returned 0` and values consistent with recent command.

### 6.3 Range control + readback

```bash
curl -s 'http://127.0.0.1:8080/set_range?radarNo=0&rangeIndex=2&rangeUnit=0'
curl -s 'http://127.0.0.1:8080/get_current?radarNo=0&command=RANGE'
```

Expected:
- Setter returns `0`.
- Getter reports `RmcGetRange_v2 returned 0` with updated code/unit.

### 6.4 Pulse control + readback

```bash
curl -s 'http://127.0.0.1:8080/set_pulse_width?radarNo=0&pulse=5&rangeIndex=2&rangeUnit=0'
curl -s 'http://127.0.0.1:8080/get_current?radarNo=0&command=PULSE'
```

Expected:
- Setter returns `0`.
- Preferred: `RmcGetPulseWidth returned 0 ...`.
- If non-zero return occurs, response should include request/retry diagnostics and fallback last-set context.

## 7. Output Data Validation

Check output directories:

```bash
ls -1 main/runtime/output/raw/csv | tail -n 5
ls -1 main/runtime/output/images/unprocessed | tail -n 5
ls -1 main/runtime/output/images/echo_trail | tail -n 5
```

Expected:
- New files appear after TX ON and connection.

### 7.1 CSV integrity sanity

```bash
latest=$(ls -1t main/runtime/output/raw/csv | head -n 1)
awk -F, 'NR==1{print "header:",$0} NR>1{rows++; for(i=6;i<=NF;i++){sum+=$i;n++}} END{print "rows=",rows,"avg_echo=",(n?sum/n:0)}' "main/runtime/output/raw/csv/$latest"
```

Expected:
- Header present.
- `rows > 0`.
- Average echo finite and non-negative.

## 8. Setting-Impact A/B Test (CSV response)

Goal: verify output shifts with meaningful settings changes.

1. Keep range fixed; vary gain (e.g., 10 vs 90).
2. Capture one fresh CSV after each change.
3. Compare `avg_echo` and optionally median/percentiles.

Suggested command skeleton:

```bash
# Example placeholders; execute in sequence with waits for new CSV files.
# set_gain low -> sample csv stats
# set_gain high -> sample csv stats
# compare low vs high avg_echo
```

Pass criterion:
- Difference is directionally consistent over repeated trials (single-frame noise is expected).

## 9. Queue/Snapshot Reliability Checks

Poll metrics twice after TX ON:

```bash
curl -s 'http://127.0.0.1:8080/system_metrics'
sleep 2
curl -s 'http://127.0.0.1:8080/system_metrics'
```

Expected:
- `completed_rotations` non-decreasing.
- `snapshots_produced` non-decreasing.
- `snapshots_consumed` non-decreasing.
- `snapshots_consumed <= snapshots_produced`.
- `snapshot_queue_high_watermark >= 1` during active capture.

## 10. Failure-Mode Checklist

If tests fail:
- Port conflict: check and kill stale process on `:8080`.
- No data files: verify `Connected!` appears in app log and TX is ON.
- Getter mismatch: verify setter returns `0`, then retry getter after short delay.
- Build/link error: re-run `./scripts/check_prereqs.sh` and inspect missing libs.

## 11. Test Report Template

For each run, record:
- Commit hash/branch.
- Start/stop timestamps.
- Radar connectivity observed (`Connected!` yes/no).
- Endpoint results (TX/GAIN/RANGE/PULSE).
- Output counts (`csv/png/trail`) before/after.
- Metrics snapshots before/after.
- A/B setting impact summary.
- Any anomalies + logs.
