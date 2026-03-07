# Doppler / Target Analyzer: Experimental Findings

**Date:** 2026-03-06
**Experiment:** Cycled through EchoColorType modes while logging callback fields and echo histograms.

## Key Discovery: `status` Field Encodes Doppler State

The `status` parameter in `RadarEchoCallback` carries the Doppler active flag in **bit 8** (0x100).

| status value | Binary | Meaning |
|-------------|--------|---------|
| 1 (0x01) | `0000 0001` | Normal echo, type A (long pulse / coarse) |
| 4 (0x04) | `0000 0100` | Normal echo, type B (short pulse / fine) |
| 257 (0x101) | `1 0000 0001` | Doppler echo, type A |
| 260 (0x104) | `1 0000 0100` | Doppler echo, type B |

Confirmed bit layout of `status`:
```
Bit 8:    Doppler active flag (0=normal, 1=Doppler data in echo bit 7)
Bits 2:0: Pulse type (1=long/coarse, 4=short/fine, interleaved per spoke)
```

## `h_flg` Is NOT Used for Doppler

`h_flg` was 0 in all 138,551 spokes across all modes. Reserved for heading sensor flags or future use.

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

Since ALL 138,551 Doppler-mode spokes showed 0% bit-7 usage (stationary
environment — land, buildings), bit 7 = 1 likely means "target is moving."
Need a moving target to confirm direction encoding.

### Doppler mode differences:
- **mode=0 (off):** Full 8-bit intensity (0-252), top bins at 0 and 252
- **mode=1 (Target Analyzer):** 7-bit intensity (0-124), bit 7 = Doppler flag
- **mode=2:** 7-bit intensity, dominant bin shifts to 128 — different encoding variant

## Dual-Pulse Interleaving (Status 1 vs 4)

### Alternation Pattern
The two status types alternate **perfectly per-spoke**: 4, 1, 4, 1, 4, 1...
(or 260, 257, 260, 257... in Doppler mode).

### Intensity Resolution Difference
- **Status=1 (long pulse):** Only 17 unique intensity values — coarse logarithmic quantization
  `0, 8, 20, 28, 36, 44, 56, 64, 76, 88, 104, 120, 140, 164, 188, 220, 252`
- **Status=4 (short pulse):** 130-240 unique values depending on rez boost — fine linear quantization

### Range Dependence
Dual-pulse is **range-dependent hardware behavior**, not controlled by any software setting:

| Range Index | Distance | Status=1 | Status=4 | Behavior |
|------------|----------|----------|----------|----------|
| 21 | 1/16 NM | 50% | 50% | Dual-pulse |
| 0 | 1/8 NM | 50% | 50% | Dual-pulse |
| 3 | 3/4 NM | 50% | 50% | Dual-pulse |
| 6 | 2 NM | 51% | 49% | Dual-pulse |
| 9 | 6 NM | 69% | 31% | Transitioning |
| 12 | 16 NM | 99.9% | 0.1% | Single pulse only |
| 15 | 48 NM | 99.9% | 0.1% | Single pulse only |

At short ranges (< 6 NM), the radar fires both short and long pulses interleaved.
At long ranges (> 16 NM), only the long pulse is used.

### Rez Boost Effect
Bearing resolution boost does NOT control whether dual-pulse happens, but it
**shifts the ratio** of short vs long pulses and **increases short pulse resolution**:

| Rez Boost | Status=1 % | Status=4 % | Status=4 unique values |
|-----------|-----------|-----------|----------------------|
| Off (0) | 81% | 19% | ~130 |
| Low (1) | 37% | 63% | ~210 |
| Med (2) | 39% | 61% | ~240 |
| High (3) | 36% | 64% | ~240 |

### What Does NOT Affect Dual-Pulse
- **Pulse width setting:** Always 50/50 at all pulse widths (0-3)
- **Gain setting:** Always ~57/43 at all gain levels (0-255)

## Visual Comparison

See plots in `reverse/`:
- `ppi_differential.png` — Side-by-side PPI + differential (Short minus Long)
- `ppi_zoomed_comparison.png` — Zoomed PPI comparison
- `spoke_comparison_1d.png` — 1D spoke overlays showing quantization difference
- `intensity_histogram_comparison.png` — Intensity histograms

## Raw Data

- Callback CSV: `main/runtime/output/doppler_log.csv` (138,551 rows)
- Packet capture: `reverse/captures/20260306_112955_doppler_session.pcap` (10,931 packets)
- Experiment log: `reverse/captures/doppler_experiment_log.txt`
