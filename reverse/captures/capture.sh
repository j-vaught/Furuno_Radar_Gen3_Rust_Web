#!/bin/bash
# Radar packet capture script
# Captures all traffic on eth0 (radar network interface)
#
# Usage:
#   ./capture.sh <session_name> [duration_seconds]
#
# Examples:
#   ./capture.sh baseline_idle 30
#   ./capture.sh init_sequence 60
#   ./capture.sh range_changes 120
#   ./capture.sh full_lifecycle 300

set -euo pipefail

IFACE="eth0"
CAPTURE_DIR="$(dirname "$0")"
SESSION="${1:?Usage: $0 <session_name> [duration_seconds]}"
DURATION="${2:-60}"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
FILENAME="${CAPTURE_DIR}/${TIMESTAMP}_${SESSION}.pcap"

echo "=== Radar Packet Capture ==="
echo "Interface:  ${IFACE}"
echo "Session:    ${SESSION}"
echo "Duration:   ${DURATION}s"
echo "Output:     ${FILENAME}"
echo ""

# Check if radar app is running
if pgrep -f "navnet_main" > /dev/null 2>&1; then
    echo "Radar app is RUNNING (PID $(pgrep -f navnet_main))"
else
    echo "WARNING: Radar app is NOT running"
fi

echo ""
echo "Starting capture..."
echo "Press Ctrl+C to stop early, or wait ${DURATION}s"
echo ""

sudo tcpdump -i "${IFACE}" -w "${FILENAME}" -G "${DURATION}" -W 1 2>&1

PACKETS=$(tcpdump -r "${FILENAME}" 2>/dev/null | wc -l)
SIZE=$(du -h "${FILENAME}" | cut -f1)

echo ""
echo "=== Capture Complete ==="
echo "File:    ${FILENAME}"
echo "Size:    ${SIZE}"
echo "Packets: ${PACKETS}"

# Append to session log
cat >> "${CAPTURE_DIR}/session_log.md" << EOF

## ${TIMESTAMP} — ${SESSION}
- Duration: ${DURATION}s
- File: \`$(basename "${FILENAME}")\`
- Size: ${SIZE}
- Packets: ${PACKETS}
- Radar app running: $(pgrep -f "navnet_main" > /dev/null 2>&1 && echo "yes" || echo "no")
- Notes:

EOF

echo "Session logged to ${CAPTURE_DIR}/session_log.md"
