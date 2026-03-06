# Comprehensive Test Playbook (`TEST_comp.md`)

This document defines a repeatable, end-to-end validation flow for NAVNETDLL, with heavy focus on settings-panel behavior.

## 1. Hard Rule: `radarNo` Must Always Be `0`

- Every setter/getter command in this plan must use `radarNo=0`.
- Do not test with any other radar number.
- If a command template does not include radar number, add `radarNo=0` when that endpoint supports it.

## 2. Goals

Validate that:
- Build and startup work from the current branch.
- Web server binds and serves expected endpoints.
- Settings panel controls work with valid values.
- Out-of-bounds values are handled predictably.
- Setters can be read back when getter support exists.
- Data capture still produces output files.
- CSV output shifts meaningfully for major setting changes.
- Queue/snapshot metrics advance while capture is active.

## 2.1 Timing Rule

- After every setter command, wait `5` seconds before running the corresponding getter/readback.
- For batch loops, include `sleep 5` between each set/get cycle.
- If a command still appears stale, retry the getter once after another `sleep 5`.

## 2.2 Anti-Stale Validation Rule

- Do not validate a setting with only one valid write.
- For each setting with getter support, run an `A -> B -> A` sequence using two distinct valid values.
- Require each readback to match the expected value for that exact step.
- If readback stays unchanged across A/B/A, mark as failure (likely stale getter or failed setter).

## 3. Preconditions

- Repo root: `/usr/lib/NAVNETDLL`
- Radar hardware/network reachable.
- License files present:
  - `main/runtime/license/vendor.txt`
  - `main/runtime/license/target.dat`
  - `main/runtime/license/Sensor_list.txt`
- SDK library present:
  - `furuno/lib/libNAVNETDLL.so`
  - `furuno/lib/libNAVNETDLL.so.01.95.0000`

## 4. Setup and Launch

Run from repo root:

```bash
cd /usr/lib/NAVNETDLL
./scripts/check_prereqs.sh
pkill -f '/usr/lib/NAVNETDLL/main/bin/navnet_main' || true
ss -ltnp | grep ':8080' || true
./scripts/build.sh
cd main
env LD_LIBRARY_PATH=/usr/lib/NAVNETDLL/furuno/lib \
  ./bin/navnet_main runtime/license/vendor.txt runtime/license/target.dat 172.31.1.100 PC1
```

Expected startup indicators:
- `StartRadarModule : OK!`
- `[WebServer] Listening on port 8080...`
- `Connected!` once radar link is established

From another terminal:

```bash
curl -s 'http://127.0.0.1:8080/get_system_info'
curl -s 'http://127.0.0.1:8080/system_metrics'
```

Expected:
- valid JSON
- metrics keys include `completed_rotations`, `snapshots_produced`, `snapshots_consumed`, `snapshot_queue_high_watermark`

## 5. Baseline Smoke Checks

```bash
curl -s 'http://127.0.0.1:8080/api_contract'
curl -s 'http://127.0.0.1:8080/toggle_echo?radarNo=0&mode=on'
curl -s 'http://127.0.0.1:8080/get_current?radarNo=0&command=ALL'
curl -s -i 'http://127.0.0.1:8080/get_current?radarNo=0&command=CLUTTER'
```

Expected:
- `/api_contract` returns JSON with canonical command list
- TX setter reports return code
- `command=ALL` returns JSON object with per-command sections
- non-canonical command (`CLUTTER`) returns HTTP 400 JSON error

## 6. Settings Panel Coverage Matrix

Use this matrix to test valid, min, max, and out-of-bounds values.

### 6.1 Matrix

| UI Control | Endpoint | Valid Range/Values | Below Min Test | Above Max Test | Readback |
|---|---|---|---|---|---|
| TX ON/OFF | `/toggle_echo` | `mode=on|off` | `mode=invalid` | N/A | `get_current` `ALL` only |
| Range | `/set_range` | panel values: `0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,19,21` with `rangeUnit=0` | `rangeIndex=-1` | `rangeIndex=999` | `command=RANGE` |
| Pulse | `/set_pulse_width` | `pulse=0..5` plus range context | `pulse=-1` | `pulse=6` | `command=PULSE` |
| Gain Manual | `/set_gain` | `status=0`, `manualValue=0..100` | `manualValue=-1` | `manualValue=101` | `command=GAIN` |
| Gain Auto | `/set_gain` | `status=1`, `autoValue=0..100` | `autoValue=-1` | `autoValue=101` | `command=GAIN` |
| STC Near | `/set_near_stc` | `value=0..4` | `value=-1` | `value=5` | `command=STC` |
| STC Middle | `/set_middle_stc` | `value=0..3` | `value=-1` | `value=4` | `command=STC` |
| STC Far | `/set_far_stc` | `value=0..2` | `value=-1` | `value=3` | `command=STC` |
| Antenna Rotation | `/set_antenna_revolution` | `rollSpeed=0..2`, `hiRpm=0..1` | `rollSpeed=-1` | `rollSpeed=3` | `command=ANTREV` |
| RezBoost | `/set_bearing_resolution` | `mode=0..3` | `mode=-1` (server clamp expected) | `mode=99` (server clamp expected) | same endpoint response includes read/set |
| A/C SEA | `/set_acsea` | `status=0|1`, manual/auto `0..100`, `mode=0|1` | manual/auto `-1` | manual/auto `101` | `command=ACSEA` |
| A/C RAIN | `/set_acrain` | `status=0|1`, manual/auto `0..100`, `ftcMode=0..2`, `rainMode=0..2` | manual/auto `-1` | manual/auto `101` | `command=ACRAIN` |
| Sweep Total | `/set_sweep_total` | `mode=0..4` | `mode=-1` | `mode=99` | no getter |
| Interpolation | `/set_interpolation` | `enable=0|1` | `enable=-1` | `enable=2` | no getter |

## 7. Concrete Test Commands

All commands below keep `radarNo=0`.

### 7.1 Range + Pulse

```bash
curl -s 'http://127.0.0.1:8080/set_range?radarNo=0&rangeIndex=2&rangeUnit=0'
sleep 5
curl -s 'http://127.0.0.1:8080/get_current?radarNo=0&command=RANGE'
curl -s 'http://127.0.0.1:8080/set_range?radarNo=0&rangeIndex=-1&rangeUnit=0'
sleep 5
curl -s 'http://127.0.0.1:8080/set_range?radarNo=0&rangeIndex=999&rangeUnit=0'
sleep 5

curl -s 'http://127.0.0.1:8080/set_pulse_width?radarNo=0&pulse=5&rangeIndex=2&rangeUnit=0'
sleep 5
curl -s 'http://127.0.0.1:8080/get_current?radarNo=0&command=PULSE'
curl -s 'http://127.0.0.1:8080/set_pulse_width?radarNo=0&pulse=-1&rangeIndex=2&rangeUnit=0'
sleep 5
curl -s 'http://127.0.0.1:8080/set_pulse_width?radarNo=0&pulse=6&rangeIndex=2&rangeUnit=0'
sleep 5
```

### 7.2 Gain + STC + Clutter

```bash
curl -s 'http://127.0.0.1:8080/set_gain?radarNo=0&status=0&manualValue=0&autoValue=0'
sleep 5
curl -s 'http://127.0.0.1:8080/set_gain?radarNo=0&status=0&manualValue=100&autoValue=0'
sleep 5
curl -s 'http://127.0.0.1:8080/set_gain?radarNo=0&status=0&manualValue=-1&autoValue=0'
sleep 5
curl -s 'http://127.0.0.1:8080/set_gain?radarNo=0&status=0&manualValue=101&autoValue=0'
sleep 5
curl -s 'http://127.0.0.1:8080/get_current?radarNo=0&command=GAIN'

curl -s 'http://127.0.0.1:8080/set_near_stc?radarNo=0&value=0'
sleep 5
curl -s 'http://127.0.0.1:8080/set_middle_stc?radarNo=0&value=0'
sleep 5
curl -s 'http://127.0.0.1:8080/set_far_stc?radarNo=0&value=0'
sleep 5
curl -s 'http://127.0.0.1:8080/get_current?radarNo=0&command=STC'
curl -s 'http://127.0.0.1:8080/set_near_stc?radarNo=0&value=-1'
sleep 5
curl -s 'http://127.0.0.1:8080/set_middle_stc?radarNo=0&value=4'
sleep 5
curl -s 'http://127.0.0.1:8080/set_far_stc?radarNo=0&value=3'
sleep 5

curl -s 'http://127.0.0.1:8080/set_acsea?radarNo=0&status=1&manual=20&auto=30&mode=0'
sleep 5
curl -s 'http://127.0.0.1:8080/get_current?radarNo=0&command=ACSEA'
curl -s 'http://127.0.0.1:8080/set_acsea?radarNo=0&status=1&manual=-1&auto=30&mode=0'
sleep 5
curl -s 'http://127.0.0.1:8080/set_acsea?radarNo=0&status=1&manual=101&auto=30&mode=0'
sleep 5

curl -s 'http://127.0.0.1:8080/set_acrain?radarNo=0&status=1&manual=40&auto=50&ftcMode=0&rainMode=0'
sleep 5
curl -s 'http://127.0.0.1:8080/get_current?radarNo=0&command=ACRAIN'
curl -s 'http://127.0.0.1:8080/set_acrain?radarNo=0&status=1&manual=-1&auto=50&ftcMode=0&rainMode=0'
sleep 5
curl -s 'http://127.0.0.1:8080/set_acrain?radarNo=0&status=1&manual=101&auto=50&ftcMode=0&rainMode=0'
sleep 5
```

### 7.3 Antenna + RezBoost + Display

```bash
curl -s 'http://127.0.0.1:8080/set_antenna_revolution?radarNo=0&rollSpeed=2&hiRpm=0'
sleep 5
curl -s 'http://127.0.0.1:8080/get_current?radarNo=0&command=ANTREV'
curl -s 'http://127.0.0.1:8080/set_antenna_revolution?radarNo=0&rollSpeed=-1&hiRpm=0'
sleep 5
curl -s 'http://127.0.0.1:8080/set_antenna_revolution?radarNo=0&rollSpeed=3&hiRpm=0'
sleep 5

curl -s 'http://127.0.0.1:8080/set_bearing_resolution?radarNo=0&mode=0'
sleep 5
curl -s 'http://127.0.0.1:8080/set_bearing_resolution?radarNo=0&mode=3'
sleep 5
curl -s 'http://127.0.0.1:8080/set_bearing_resolution?radarNo=0&mode=-1'
sleep 5
curl -s 'http://127.0.0.1:8080/set_bearing_resolution?radarNo=0&mode=99'
sleep 5

curl -s 'http://127.0.0.1:8080/set_sweep_total?mode=0'
sleep 5
curl -s 'http://127.0.0.1:8080/set_sweep_total?mode=4'
sleep 5
curl -s 'http://127.0.0.1:8080/set_sweep_total?mode=-1'
sleep 5
curl -s 'http://127.0.0.1:8080/set_sweep_total?mode=99'
sleep 5

curl -s 'http://127.0.0.1:8080/set_interpolation?enable=0'
sleep 5
curl -s 'http://127.0.0.1:8080/set_interpolation?enable=1'
sleep 5
curl -s 'http://127.0.0.1:8080/set_interpolation?enable=-1'
sleep 5
curl -s 'http://127.0.0.1:8080/set_interpolation?enable=2'
sleep 5
```

## 8. Anti-Stale A/B/A Verification

Use strict checks so stale readbacks do not pass.

```bash
BASE='http://127.0.0.1:8080'

assert_has() {
  local body="$1"
  local needle="$2"
  local label="$3"
  if echo "$body" | grep -q "$needle"; then
    echo "PASS: $label ($needle)"
  else
    echo "FAIL: $label expected [$needle]"
    echo "$body"
  fi
}

# Gain A -> B -> A
curl -s "$BASE/set_gain?radarNo=0&status=0&manualValue=15&autoValue=0" >/dev/null
sleep 5
g1=$(curl -s "$BASE/get_current?radarNo=0&command=GAIN")
assert_has "$g1" "\"manual\":15" "GAIN step A1"

curl -s "$BASE/set_gain?radarNo=0&status=0&manualValue=85&autoValue=0" >/dev/null
sleep 5
g2=$(curl -s "$BASE/get_current?radarNo=0&command=GAIN")
assert_has "$g2" "\"manual\":85" "GAIN step B"

curl -s "$BASE/set_gain?radarNo=0&status=0&manualValue=15&autoValue=0" >/dev/null
sleep 5
g3=$(curl -s "$BASE/get_current?radarNo=0&command=GAIN")
assert_has "$g3" "\"manual\":15" "GAIN step A2"

# Pulse A -> B -> A (with fixed range context)
curl -s "$BASE/set_pulse_width?radarNo=0&pulse=1&rangeIndex=2&rangeUnit=0" >/dev/null
sleep 5
p1=$(curl -s "$BASE/get_current?radarNo=0&command=PULSE")
assert_has "$p1" "\"pulse\":1" "PULSE step A1"

curl -s "$BASE/set_pulse_width?radarNo=0&pulse=5&rangeIndex=2&rangeUnit=0" >/dev/null
sleep 5
p2=$(curl -s "$BASE/get_current?radarNo=0&command=PULSE")
assert_has "$p2" "\"pulse\":5" "PULSE step B"

curl -s "$BASE/set_pulse_width?radarNo=0&pulse=1&rangeIndex=2&rangeUnit=0" >/dev/null
sleep 5
p3=$(curl -s "$BASE/get_current?radarNo=0&command=PULSE")
assert_has "$p3" "\"pulse\":1" "PULSE step A2"

# Range A -> B -> A
curl -s "$BASE/set_range?radarNo=0&rangeIndex=2&rangeUnit=0" >/dev/null
sleep 5
r1=$(curl -s "$BASE/get_current?radarNo=0&command=RANGE")
assert_has "$r1" "\"code\":2" "RANGE step A1"

curl -s "$BASE/set_range?radarNo=0&rangeIndex=10&rangeUnit=0" >/dev/null
sleep 5
r2=$(curl -s "$BASE/get_current?radarNo=0&command=RANGE")
assert_has "$r2" "\"code\":10" "RANGE step B"

curl -s "$BASE/set_range?radarNo=0&rangeIndex=2&rangeUnit=0" >/dev/null
sleep 5
r3=$(curl -s "$BASE/get_current?radarNo=0&command=RANGE")
assert_has "$r3" "\"code\":2" "RANGE step A2"
```

Pass criteria:
- each A/B/A step readback matches the expected value
- no step relies on previous/stale state
- no server crash or hang

## 9. Data Output and Behavior Checks

```bash
ls -1 main/runtime/output/raw/csv | tail -n 5
ls -1 main/runtime/output/images/unprocessed | tail -n 5
ls -1 main/runtime/output/images/echo_trail | tail -n 5
```

Run a simple A/B gain impact check:

```bash
BASE='http://127.0.0.1:8080'
curl -s "$BASE/set_gain?radarNo=0&status=0&manualValue=10&autoValue=0" >/dev/null
sleep 5
low=$(ls -1t /usr/lib/NAVNETDLL/main/runtime/output/raw/csv | head -n1)
awk -F, 'NR>1{for(i=6;i<=NF;i++){s+=$i;n++}} END{print "low_avg=",(n?s/n:0)}' "/usr/lib/NAVNETDLL/main/runtime/output/raw/csv/$low"

curl -s "$BASE/set_gain?radarNo=0&status=0&manualValue=90&autoValue=0" >/dev/null
sleep 5
high=$(ls -1t /usr/lib/NAVNETDLL/main/runtime/output/raw/csv | head -n1)
awk -F, 'NR>1{for(i=6;i<=NF;i++){s+=$i;n++}} END{print "high_avg=",(n?s/n:0)}' "/usr/lib/NAVNETDLL/main/runtime/output/raw/csv/$high"
```

Pass criterion:
- average echo should generally move in expected direction over repeated runs

## 10. Queue/Snapshot Reliability

```bash
curl -s 'http://127.0.0.1:8080/system_metrics'
sleep 5
curl -s 'http://127.0.0.1:8080/system_metrics'
```

Expected:
- `completed_rotations`, `snapshots_produced`, `snapshots_consumed` are non-decreasing
- `snapshots_consumed <= snapshots_produced`
- `snapshot_queue_high_watermark >= 1` under active capture

## 11. Contract Behavior Checks

- `/get_current` is JSON-only.
- Only canonical commands are accepted; invalid command values must return HTTP 400 JSON.
- `SWEEP` should return `null` data fields plus a warning message when getter APIs are unavailable.

## 12. Failure Checklist

If tests fail:
- port conflict: stop stale process on `:8080`
- no data files: confirm `Connected!` and TX ON
- getter mismatch: retry after short delay, then inspect app log
- build/link failures: rerun `./scripts/check_prereqs.sh`

## 13. Test Report Template

Record:
- branch + commit hash
- `/api_contract` response snapshot
- test start/end times
- whether `radarNo=0` was enforced for all setter/getter commands
- per-setting results for valid/min/max/out-of-bounds
- readback consistency notes
- output file counts and CSV A/B summary
- metrics before/after
- anomalies and relevant logs
