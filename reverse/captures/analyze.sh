#!/bin/bash
# Analyze a pcap file and print summary statistics
#
# Usage:
#   ./analyze.sh <pcap_file>
#
# Uses only tcpdump (no tshark/wireshark needed)

set -uo pipefail

PCAP="${1:?Usage: $0 <pcap_file>}"

if [ ! -f "$PCAP" ]; then
    echo "ERROR: File not found: $PCAP"
    exit 1
fi

echo "=============================================="
echo "  PCAP Analysis: $(basename "$PCAP")"
echo "=============================================="
echo ""

# Basic file info
SIZE=$(du -h "$PCAP" | cut -f1)
SIZE_BYTES=$(stat -c%s "$PCAP")
echo "File size: ${SIZE} (${SIZE_BYTES} bytes)"
echo ""

# Total packets and timestamps
echo "--- Packet Summary ---"
TOTAL=$(sudo tcpdump -r "$PCAP" 2>/dev/null | wc -l)
echo "Total packets: ${TOTAL}"

if [ "$TOTAL" -eq 0 ]; then
    echo "No packets to analyze."
    exit 0
fi

FIRST_TS=$(sudo tcpdump -r "$PCAP" -c 1 2>/dev/null | awk '{print $1}')
LAST_TS=$(sudo tcpdump -r "$PCAP" 2>/dev/null | tail -1 | awk '{print $1}')
echo "First packet: ${FIRST_TS}"
echo "Last packet:  ${LAST_TS}"
echo ""

# Unique flows: src.port > dst.port with counts
echo "--- Flow Summary (src > dst, packet counts) ---"
sudo tcpdump -nn -r "$PCAP" 2>/dev/null | \
    sed -E 's/^[0-9:.]+\s+//' | \
    awk '{
        # Extract protocol and endpoints
        if ($1 == "IP") {
            src = $2; dst = $4;
            gsub(/:$/, "", dst);
            flow = src " > " dst;
            counts[flow]++;
        } else if ($1 == "IP6") {
            src = $2; dst = $4;
            gsub(/:$/, "", dst);
            flow = "IPv6 " src " > " dst;
            counts[flow]++;
        } else {
            flow = $0;
            counts[flow]++;
        }
    }
    END {
        for (f in counts) {
            printf "%7d  %s\n", counts[f], f;
        }
    }' | sort -rn | head -30
echo ""

# Port breakdown
echo "--- Port Breakdown ---"
sudo tcpdump -nn -r "$PCAP" 2>/dev/null | \
    grep -oP '\.\d+[: >]' | \
    grep -oP '\d+' | \
    sort | uniq -c | sort -rn | head -20
echo ""

# Protocol breakdown
echo "--- Protocol Breakdown ---"
echo "UDP packets:"
UDP_COUNT=$(sudo tcpdump -r "$PCAP" udp 2>/dev/null | wc -l)
echo "  ${UDP_COUNT}"
echo "TCP packets:"
TCP_COUNT=$(sudo tcpdump -r "$PCAP" tcp 2>/dev/null | wc -l)
echo "  ${TCP_COUNT}"
echo "IGMP packets:"
IGMP_COUNT=$(sudo tcpdump -r "$PCAP" igmp 2>/dev/null | wc -l)
echo "  ${IGMP_COUNT}"
echo "ARP packets:"
ARP_COUNT=$(sudo tcpdump -r "$PCAP" arp 2>/dev/null | wc -l)
echo "  ${ARP_COUNT}"
echo "Other: $((TOTAL - UDP_COUNT - TCP_COUNT - IGMP_COUNT - ARP_COUNT))"
echo ""

# Packet size distribution per flow
echo "--- Packet Sizes per Port (UDP only) ---"
sudo tcpdump -nn -r "$PCAP" udp 2>/dev/null | \
    awk '{
        # Get destination port and length
        for (i=1; i<=NF; i++) {
            if ($i == "length") {
                len = $(i+1)+0;
                # Find dst port
                dst = $4;
                gsub(/:$/, "", dst);
                split(dst, parts, ".");
                port = parts[length(parts)];
                if (port != "") {
                    count[port]++;
                    total[port] += len;
                    if (!(port in min_val) || len < min_val[port]) min_val[port] = len;
                    if (!(port in max_val) || len > max_val[port]) max_val[port] = len;
                }
                break;
            }
        }
    }
    END {
        printf "%-8s %8s %8s %8s %8s\n", "Port", "Count", "Min", "Avg", "Max";
        printf "%-8s %8s %8s %8s %8s\n", "----", "-----", "---", "---", "---";
        for (p in count) {
            avg = total[p] / count[p];
            printf "%-8s %8d %8d %8.0f %8d\n", p, count[p], min_val[p], avg, max_val[p];
        }
    }' | sort -t' ' -k2 -rn
echo ""

# Packets per second
echo "--- Packets Per Second ---"
sudo tcpdump -nn -r "$PCAP" 2>/dev/null | \
    awk '{
        split($1, ts, ".");
        sec = ts[1];
        counts[sec]++;
    }
    END {
        n = 0; sum = 0; min_v = 999999; max_v = 0;
        for (s in counts) {
            n++;
            sum += counts[s];
            if (counts[s] < min_v) min_v = counts[s];
            if (counts[s] > max_v) max_v = counts[s];
        }
        if (n > 0) {
            printf "Seconds with packets: %d\n", n;
            printf "Avg packets/sec: %.1f\n", sum/n;
            printf "Min packets/sec: %d\n", min_v;
            printf "Max packets/sec: %d\n", max_v;
        }
    }'
echo ""
echo "=============================================="
echo "  Analysis complete"
echo "=============================================="
