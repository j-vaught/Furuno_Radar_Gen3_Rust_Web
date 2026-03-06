# Packet Capture Session Log

**Radar:** 172.31.3.226 | **Host:** 172.31.1.100 (eth0) | **Hostname:** PC1
**Multicast echo group:** 239.255.0.2:10024
**TCP control:** port 10100

All captures use eth0 interface only (excludes WiFi/tailscale noise).
Default filter: `host 172.31.3.226 or net 239.255.0.0/16`

---

## Session 1: Steady-State Echo (60s passive)
- **File:** `20260306_031720_steady_state_60s.pcap`
- **Duration:** 60s
- **Size:** 3.2M (3,309,681 bytes)
- **Total packets:** 5,122
- **Drops:** 0
- **Filter:** `host 172.31.3.226 or net 239.255.0.0/16`
- **Radar TX:** ON
- **Start:** 2026-03-06T03:17:20
- **Flow breakdown:**
  - 4,805 echo packets (239.255.0.2:10024) - ~80/sec
  - 117 TCP control (radar->host on 10100)
  - 116 TCP ACKs (host->radar)
  - 78 broadcast (10034) - ~1.3/sec
  - 4 broadcast (10010) - rare
  - 2 ARP (1 request + 1 reply)
- **Notes:** Pure passive capture, no user interaction. Echo sizes 576-732 bytes (variable per sweep line).

## Session 2: Steady-State Extended (5min passive)
- **File:** `20260306_031913_steady_state_5min.pcap`
- **Duration:** 300s (5 minutes)
- **Size:** 16M (16,670,095 bytes)
- **Total packets:** 25,861
- **Drops:** 0
- **Filter:** `host 172.31.3.226 or net 239.255.0.0/16`
- **Radar TX:** ON
- **Start:** 2026-03-06T03:19:13
- **Flow breakdown:**
  - 24,206 echo packets (10024) - ~80.7/sec
  - 622 TCP control (10100 radar->host)
  - 621 TCP ACKs (host->radar)
  - 391 broadcast (10034) - ~1.3/sec
  - 19 broadcast (10010) - ~1 every 16s
  - 2 ARP
- **Packets/sec:** avg 86.2, min 6, max 93
- **Notes:** Long capture for timing analysis. 10010 broadcast appears ~every 15s. TCP control at ~2/sec. Zero drops over 5 minutes confirms 8MB buffer sufficient.
