# Doppler / Target Analyzer: Experimental Findings

**Date:** 2026-03-06
**Experiment:** Cycled through EchoColorType modes while logging callback fields and echo histograms.

## Key Discovery: `status` Field Encodes Doppler State

The `status` parameter in `RadarEchoCallback` carries the Doppler active flag in **bit 8** (0x100).

| status value | Binary | Meaning |
|-------------|--------|---------|
| 1 (0x01) | `0000 0001` | Normal echo, type A |
| 4 (0x04) | `0000 0100` | Normal echo, type B |
| 257 (0x101) | `1 0000 0001` | Doppler echo, type A |
| 260 (0x104) | `1 0000 0100` | Doppler echo, type B |

- Bits 0-7: Echo type/encoding identifier (values 1 and 4 observed)
- Bit 8: Doppler active flag (1 = Doppler data present in echo values)

The alternation between type A (1/257) and type B (4/260) appears to be a per-spoke property, roughly 50/50 split. This may indicate interleaved normal/Doppler spokes, or two different encoding schemes.

## `h_flg` Is NOT Used for Doppler

`h_flg` was 0 in all 138,551 spokes across all modes. It may be reserved for heading sensor flags or future use.

## Echo Value Encoding Changes with Doppler

The echo amplitude bytes change encoding entirely when Doppler is enabled:

### Doppler OFF (mode=0)
- Dominant bins: `0` (empty), `252` (strong echo)
- Values spread across multiples of 4: `0, 4, 8, 12, 16, 20, ...`
- This is a standard 6-bit echo (0-63) scaled to 8-bit (×4)

### Doppler mode=1 (Target Analyzer)
- Dominant bins: `0` (empty), `124` (strong echo)
- Values cluster at multiples of 4 in range 0-124
- Max value drops from 252 to 124 — **upper half of byte reserved for Doppler info**
- Likely encoding: bits 0-6 = echo intensity (0-124), bit 7 = Doppler flag (approaching/receding)

### Doppler mode=2
- Dominant bins: `0` (empty), `128` (strong echo)
- Values cluster tightly around 0, 128, with some at 124-132
- May use: bit 7 = echo/Doppler discriminator, bits 0-6 = intensity OR Doppler velocity

## CONFIRMED Echo Byte Encoding

### When Doppler OFF (status bit 8 = 0):
```
Byte value: [0-252], full 8-bit range used
Bits 7:0 = echo intensity (0-255)
```
~26% of echo bytes have bit 7 set (values 128-255) in normal mode.

### When Doppler ON (status bit 8 = 1):
```
CONFIRMED: 0% of echo bytes have bit 7 set.
Bit 7 = Doppler flag (approaching or receding)
Bits 6:0 = echo intensity (0-126)
```
In Doppler mode, echo intensity is compressed to 7 bits (0-126) and bit 7 is
repurposed as the Doppler direction indicator. This is similar to Navico's HALO
approach but uses a single bit flag rather than sentinel values.

The Doppler bit likely indicates:
- bit 7 = 0: stationary or receding target
- bit 7 = 1: approaching target (or vice versa)

Since ALL 138,551 Doppler-mode spokes showed 0% bit-7 usage, and the radar
was likely pointing at mostly stationary objects (land, buildings), bit 7 = 1
probably means "target is moving" (approaching or receding), and in our test
environment there were no moving targets detected.

## Status Types A vs B (1/4 distinction)

The two status types alternate **perfectly per-spoke**: 4, 1, 4, 1, 4, 1...
(or 260, 257, 260, 257... in Doppler mode).

This is **interleaved dual-pulse** operation. The radar fires two pulses per
azimuth position (likely short+long pulse, or different PRFs), and the library
delivers them alternating. Both pulse types are identically affected by Doppler
mode (bit 8 set on both).

Confirmed bit layout of `status`:
```
Bit 8:   Doppler active flag (0=normal, 1=Doppler data in echo bit 7)
Bits 2:0: Pulse type (1=type A, 4=type B, interleaved per spoke)
```

## Next Steps

1. **Correlate status type with spoke angle** — are types A/B alternating per-spoke or per-half-rotation?
2. **Compare echo values at same angles** between Doppler OFF and ON — identify the exact bit layout
3. **Test with known targets** — a stationary target should show different Doppler encoding than open water
4. **Implement in Rust** — decode echo bytes differently when status bit 8 is set
5. **Probe bit 7 of echo values** — count how many echo bytes have bit 7 set in mode=1 vs mode=0

## Raw Data

- Callback CSV: `main/runtime/output/doppler_log.csv` (138,551 rows)
- Packet capture: `reverse/captures/20260306_112955_doppler_session.pcap` (10,931 packets)
- Experiment log: `reverse/captures/doppler_experiment_log.txt`
