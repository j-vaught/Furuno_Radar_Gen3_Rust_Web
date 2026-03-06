#!/bin/bash
# Furuno radar packet capture script
#
# Usage:
#   ./capture.sh <session_name> <duration_seconds> [filter]
#
# Examples:
#   ./capture.sh steady_state 60
#   ./capture.sh range_changes 120
#   ./capture.sh cold_start 30 "host 172.31.3.226"

set -euo pipefail

IFACE="eth0"
CAPTURE_DIR="$(cd "$(dirname "$0")" && pwd)"
SESSION="${1:?Usage: $0 <session_name> <duration_seconds> [filter]}"
DURATION="${2:?Usage: $0 <session_name> <duration_seconds> [filter]}"
FILTER="${3:-host 172.31.3.226 or net 239.255.0.0/16}"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
FILENAME="${CAPTURE_DIR}/${TIMESTAMP}_${SESSION}.pcap"

echo "=== Radar Packet Capture ==="
echo "Interface:  ${IFACE}"
echo "Session:    ${SESSION}"
echo "Duration:   ${DURATION}s"
echo "Filter:     ${FILTER}"
echo "Output:     ${FILENAME}"
echo ""

# Check if radar app is running
if pgrep -f "navnet_main" > /dev/null 2>&1; then
    RADAR_PID=$(pgrep -f "navnet_main" | head -1)
    echo "Radar app is RUNNING (PID ${RADAR_PID})"
else
    echo "WARNING: Radar app is NOT running"
fi

echo ""
echo "Starting capture at $(date -Iseconds)..."
echo ""

# Capture with large kernel buffer (-B 8192 = 8MB), full packets (-s 0)
# -G duration -W 1 = run for exactly duration seconds then stop
sudo tcpdump -i "${IFACE}" \
    -B 8192 \
    -s 0 \
    -G "${DURATION}" \
    -W 1 \
    -w "${FILENAME}" \
    "${FILTER}" \
    2>&1

echo ""
echo "Capture stopped at $(date -Iseconds)"

# Get stats
PACKETS=$(sudo tcpdump -r "${FILENAME}" 2>/dev/null | wc -l)
SIZE=$(du -h "${FILENAME}" | cut -f1)
SIZE_BYTES=$(stat -c%s "${FILENAME}")

echo ""
echo "=== Capture Complete ==="
echo "File:    ${FILENAME}"
echo "Size:    ${SIZE} (${SIZE_BYTES} bytes)"
echo "Packets: ${PACKETS}"

# Append to session log
cat >> "${CAPTURE_DIR}/session_log.md" << EOF

## ${TIMESTAMP} - ${SESSION}
- **Duration:** ${DURATION}s
- **File:** \`$(basename "${FILENAME}")\`
- **Size:** ${SIZE} (${SIZE_BYTES} bytes)
- **Packets:** ${PACKETS}
- **Filter:** \`${FILTER}\`
- **Radar app running:** $(pgrep -f "navnet_main" > /dev/null 2>&1 && echo "yes (PID $(pgrep -f navnet_main | head -1))" || echo "no")
- **Start time:** $(date -Iseconds)
- **Notes:**

EOF

echo "Session logged to session_log.md"
echo ""
echo "Filename for scripting: ${FILENAME}"
