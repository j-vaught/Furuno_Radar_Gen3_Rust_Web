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

## Session 3: Range Changes (active)
- **File:** `20260306_032603_range_changes.pcap`
- **Duration:** ~70s
- **Size:** 3.0M (3,133,371 bytes)
- **Total packets:** 5,353
- **Drops:** 0
- **Filter:** `host 172.31.3.226 or net 239.255.0.0/16`
- **Radar TX:** ON
- **Command timestamps:**
  - 03:26:03 - Capture start (baseline)
  - 03:26:13 - `set_range rangeIndex=3` (0.75 NM)
  - 03:26:28 - `set_range rangeIndex=6` (2 NM)
  - 03:26:43 - `set_range rangeIndex=12` (12 NM)
  - 03:26:58 - `set_range rangeIndex=0` (0.125 NM, return to default)
  - 03:27:13 - Capture stop
- **Notes:** Range changes should show as TCP command packets from host->radar near the logged timestamps. Echo packet sizes may change with range setting.

## Session 4: Gain Changes (active)
- **File:** `20260306_034641_gain_changes.pcap`
- **Duration:** ~70s
- **Size:** 3.5M (3,705,680 bytes)
- **Total packets:** 6,117
- **Drops:** 0
- **Filter:** `host 172.31.3.226 or net 239.255.0.0/16`
- **Radar TX:** ON
- **Command timestamps:**
  - 03:46:41 - Capture start (baseline)
  - 03:46:51 - `set_gain status=0 manualValue=0` (manual min)
  - 03:47:01 - `set_gain status=0 manualValue=50`
  - 03:47:11 - `set_gain status=0 manualValue=100`
  - 03:47:21 - `set_gain status=0 manualValue=200`
  - 03:47:31 - `set_gain status=0 manualValue=255` (manual max)
  - 03:47:41 - `set_gain status=1` (auto gain)
  - 03:47:51 - Capture stop
- **Notes:** Gain changes are host->radar commands on TCP 10100. Echo intensity should change but packet sizes may stay similar since gain affects echo values, not packet structure.

## Session 5: TX On/Off Toggle (active)
- **File:** `20260306_034817_tx_toggle.pcap`
- **Duration:** ~40s
- **Size:** 1.2M (1,254,131 bytes)
- **Total packets:** 2,080
- **Drops:** 0
- **Filter:** `host 172.31.3.226 or net 239.255.0.0/16`
- **Command timestamps:**
  - 03:48:17 - Capture start (TX on, echoes flowing)
  - 03:48:27 - `toggle_echo mode=off` (standby)
  - 03:48:42 - `toggle_echo mode=on` (TX resume)
  - 03:48:57 - Capture stop
- **Notes:** Packet count much lower than equivalent duration (2080 vs ~3400 expected) - confirms echo packets stop during standby. Look for TX command packets near timestamps and echo gap between 03:48:27-03:48:42.

## Session 6: Multi-Parameter Sweep (active)
- **File:** `20260306_035029_multi_param_sweep.pcap`
- **Duration:** ~120s
- **Size:** 4.6M (4,804,182 bytes)
- **Total packets:** 8,815
- **Drops:** 0
- **Filter:** `host 172.31.3.226 or net 239.255.0.0/16`
- **Command timestamps:**
  - 03:50:29 - Capture start (baseline)
  - 03:50:39 - `set_range rangeIndex=6` (2 NM)
  - 03:50:49 - `set_gain status=0 manualValue=150`
  - 03:50:59 - `set_pulse_width pulse=1 rangeIndex=6` (short-2)
  - 03:51:09 - `set_near_stc value=2`
  - 03:51:19 - `set_middle_stc value=2`
  - 03:51:29 - `set_far_stc value=1`
  - 03:51:39 - `set_antenna_revolution rollSpeed=2 hiRpm=0`
  - 03:51:49 - `set_acsea status=0 manual=100`
  - 03:51:59 - `set_acrain status=0 manual=50 ftcMode=1 rainMode=1`
  - 03:52:09 - `set_bearing_resolution mode=3` (High)
  - 03:52:19 - Reset all to defaults (8 curl commands)
  - 03:52:29 - Capture stop
- **Notes:** Comprehensive command sweep covering 11 parameter types. Each command should produce host->radar TCP payload near its timestamp. Reset burst at 03:52:19 should show 8 rapid commands.

## Session 7: Cold Start Init (app restart)
- **File:** `20260306_035400_cold_start_init.pcap`
- **Duration:** ~67s (from pre-start to post-TX-on)
- **Size:** 1.0M (1,037,153 bytes)
- **Total packets:** 2,172
- **Drops:** 0
- **Filter:** `host 172.31.3.226 or net 239.255.0.0/16 or dst 172.31.255.255`
- **Key events observed:**
  - 03:53:40 - tcpdump starts, radar heartbeat on 10034 already present
  - 03:53:47 - App starts, sends SSDP discovery to 239.255.255.250:1900 (3 packets)
  - 03:53:47 - TCP SYN to 172.31.3.226:10010 (discovery handshake: sends 56 bytes, gets 12 back, connection closed)
  - 03:53:48 - TCP SYN to 172.31.3.226:10100 (persistent control channel opens)
  - 03:53:48 - Radar sends initial 24-byte payload on 10100
  - 03:53:49 - Host sends 12-byte command to radar on 10100
  - 03:54:32 - TX turned on via web UI
  - 03:54:47 - Capture stop (echoes flowing)
- **Notes:** CRITICAL capture. Shows the full init handshake sequence. Init involves: (1) SSDP multicast discovery, (2) TCP connection to port 10010 with 56-byte handshake payload (closed immediately), (3) TCP connection to port 10100 (persistent control channel). No echo data until TX is explicitly enabled.
