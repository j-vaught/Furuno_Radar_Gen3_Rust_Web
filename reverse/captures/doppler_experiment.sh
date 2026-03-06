#!/bin/bash
# Doppler / Target Analyzer reverse engineering capture session
# Runs packet capture + C++ callback logging simultaneously
# while cycling through EchoColorType modes.
#
# Prerequisites:
#   - navnet_main must be running with the instrumented build
#   - Radar must be in TX mode (echoes flowing)
#
# Usage: sudo bash doppler_experiment.sh

set -euo pipefail

CAPTURE_DIR="$(cd "$(dirname "$0")" && pwd)"
PCAP_FILE="$CAPTURE_DIR/$(date +%Y%m%d_%H%M%S)_doppler_session.pcap"
LOG_FILE="$CAPTURE_DIR/doppler_experiment_log.txt"
WEB="http://localhost:8080"

log() {
    local msg="[$(date +%H:%M:%S.%3N)] $*"
    echo "$msg"
    echo "$msg" >> "$LOG_FILE"
}

# Start packet capture in background
log "Starting tcpdump -> $PCAP_FILE"
tcpdump -i eth0 -s 0 -B 8192 -w "$PCAP_FILE" \
  'host 172.31.3.226 or net 239.255.0.0/16' &
TCPDUMP_PID=$!
sleep 2

# Start Doppler callback logging
log "Starting Doppler callback logging"
curl -s "$WEB/doppler_log?action=start" > /dev/null

# Read current echo color state
log "Current EchoColorType state:"
curl -s "$WEB/get_current?command=ECHOCOLORTYPE&radarNo=0" | grep -oP '(?<=→ ).*(?=</p>)' | tee -a "$LOG_FILE"

# Phase 1: Baseline (Doppler OFF)
log "PHASE 1: Doppler OFF (baseline) - 30 seconds"
curl -s "$WEB/set_echo_color?mode=0&sw=0&auto=0" > /dev/null
sleep 30

# Phase 2: Mode 1, sw=1 (Target Analyzer)
log "PHASE 2: mode=1, sw=1 (Target Analyzer) - 30 seconds"
curl -s "$WEB/set_echo_color?mode=1&sw=1&auto=0" > /dev/null
sleep 30

# Phase 3: Mode 2, sw=1
log "PHASE 3: mode=2, sw=1 - 30 seconds"
curl -s "$WEB/set_echo_color?mode=2&sw=1&auto=0" > /dev/null
sleep 30

# Phase 4: Mode 3, sw=1 (probe)
log "PHASE 4: mode=3, sw=1 (probe) - 15 seconds"
curl -s "$WEB/set_echo_color?mode=3&sw=1&auto=0" > /dev/null
sleep 15

# Phase 5: Mode 1, sw=1, autoPicture=1
log "PHASE 5: mode=1, sw=1, autoPicture=1 - 15 seconds"
curl -s "$WEB/set_echo_color?mode=1&sw=1&auto=1" > /dev/null
sleep 15

# Phase 6: Doppler OFF again
log "PHASE 6: Doppler OFF (restore) - 15 seconds"
curl -s "$WEB/set_echo_color?mode=0&sw=0&auto=0" > /dev/null
sleep 15

# Stop logging
log "Stopping Doppler callback logging"
curl -s "$WEB/doppler_log?action=stop" > /dev/null

# Stop packet capture
log "Stopping tcpdump"
kill $TCPDUMP_PID 2>/dev/null || true
wait $TCPDUMP_PID 2>/dev/null || true

# Summary
log "Capture complete."
log "  PCAP: $PCAP_FILE"
log "  Doppler CSV: runtime/output/doppler_log.csv"
log "  Experiment log: $LOG_FILE"
ls -lh "$PCAP_FILE" >> "$LOG_FILE"

echo ""
echo "Next: run analyze_doppler.sh to process results"
