#!/bin/bash
# Analyze Doppler experiment results
# Reads doppler_log.csv and compares callback fields across phases
#
# Usage: bash analyze_doppler.sh [doppler_log.csv]

set -euo pipefail

CSV="${1:-/home/user/Code_dev/RADAR_web/main/runtime/output/doppler_log.csv}"
LOGFILE="${2:-$(dirname "$0")/doppler_experiment_log.txt}"

if [ ! -f "$CSV" ]; then
    echo "ERROR: $CSV not found"
    exit 1
fi

TOTAL_LINES=$(wc -l < "$CSV")
echo "=== Doppler Experiment Analysis ==="
echo "CSV: $CSV"
echo "Total data lines: $((TOTAL_LINES - 1))"
echo ""

# Extract phase timestamps from experiment log if available
if [ -f "$LOGFILE" ]; then
    echo "=== Phase Timestamps ==="
    grep "PHASE" "$LOGFILE" || echo "(no phase markers found)"
    echo ""
fi

echo "=== Unique 'status' values ==="
tail -n +2 "$CSV" | cut -d',' -f3 | sort -n | uniq -c | sort -rn
echo ""

echo "=== Unique 'h_flg' values ==="
tail -n +2 "$CSV" | cut -d',' -f4 | sort -n | uniq -c | sort -rn
echo ""

echo "=== Status x h_flg cross-tabulation ==="
echo "(count status h_flg)"
tail -n +2 "$CSV" | awk -F',' '{print $3, $4}' | sort | uniq -c | sort -rn
echo ""

echo "=== Unique 'heading' values (sample) ==="
tail -n +2 "$CSV" | cut -d',' -f5 | sort -n | uniq -c | sort -rn | head -20
echo ""

echo "=== Range values seen ==="
tail -n +2 "$CSV" | cut -d',' -f8 | sort -n | uniq -c | sort -rn
echo ""

echo "=== Scale values seen ==="
tail -n +2 "$CSV" | cut -d',' -f9 | sort -n | uniq -c | sort -rn
echo ""

# Echo histogram analysis: look for unusual bins
# Normal echo data uses bins 0-63 (6-bit). If Doppler adds special values
# in higher bins, we'll see them here.
echo "=== Echo amplitude distribution (non-zero bins, summed across all spokes) ==="
tail -n +2 "$CSV" | awk -F',' '
BEGIN { for(i=0;i<256;i++) total[i]=0 }
{
    for(i=10; i<=265; i++) {
        bin = i - 10
        total[bin] += $i
    }
}
END {
    for(i=0;i<256;i++) {
        if(total[i] > 0) printf "  bin %3d: %d\n", i, total[i]
    }
}'
echo ""

# Time-series analysis: split into ~30-second windows and compare
echo "=== Status/h_flg over time (30-second windows) ==="
tail -n +2 "$CSV" | awk -F',' '
NR==1 { first_ms = $1 }
{
    window = int(($1 - first_ms) / 30000)
    key = window ":" $3 ":" $4
    counts[key]++
    windows[window] = 1
}
END {
    for(w=0; w<=10; w++) {
        if(!(w in windows)) continue
        printf "Window %d (T+%ds to T+%ds):\n", w, w*30, (w+1)*30
        for(key in counts) {
            split(key, parts, ":")
            if(parts[1] == w) {
                printf "  status=%s h_flg=%s  count=%d\n", parts[2], parts[3], counts[key]
            }
        }
    }
}'
echo ""

# Look for echo histogram differences between windows
echo "=== Echo histogram per 30-second window (top 10 bins by count) ==="
tail -n +2 "$CSV" | awk -F',' '
NR==1 { first_ms = $1 }
{
    window = int(($1 - first_ms) / 30000)
    for(i=10; i<=265; i++) {
        bin = i - 10
        key = window SUBSEP bin
        hist[key] += $i
    }
    windows[window] = 1
}
END {
    for(w=0; w<=10; w++) {
        if(!(w in windows)) continue
        printf "Window %d:\n", w
        n = 0
        for(b=0; b<256; b++) {
            key = w SUBSEP b
            if((key in hist) && hist[key] > 0) {
                n++
                sorted_val[n] = hist[key]
                sorted_bin[n] = b
            }
        }
        for(rank=1; rank<=10 && rank<=n; rank++) {
            max_val = -1; max_idx = -1
            for(j=1; j<=n; j++) {
                if(sorted_val[j] > max_val) { max_val = sorted_val[j]; max_idx = j }
            }
            if(max_idx > 0) {
                printf "  bin %3d: %d\n", sorted_bin[max_idx], max_val
                sorted_val[max_idx] = -1
            }
        }
    }
}'

echo ""
echo "=== Analysis complete ==="
