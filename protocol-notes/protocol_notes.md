# Furuno NAVNET Wire Protocol Notes

Generated: 2026-03-06 from 7 live packet capture sessions.

## Network Topology

```
  172.31.1.100 (Host/Pi, eth0)          172.31.3.226 (Radar)
  ================================      ================================
  |  navnet_main (our app)       |      |  Furuno DRS4DL+ / NXT        |
  |    TCP client -> 10100       |----->|    TCP server 10100          |
  |    TCP client -> 10010 (init)|----->|    TCP server 10010          |
  |    UDP 10010 (discovery)     |<---->|    UDP 10010 (discovery)     |
  |    UDP multicast listener    |<-----|    UDP 10024 (echo data)     |
  |    SSDP 239.255.255.250:1900|      |    UDP 10034 (ARPA bcast)   |
  |    HTTP :8080 (web UI)       |      |                              |
  |    UPnP :10998               |      |                              |
  ================================      ================================
                                          |
                                          v
                                   239.255.0.2:10024 (multicast echo)
                                   172.31.255.255:10034 (ARPA broadcast)
                                   172.31.255.255:10010 (discovery beacon)
```

## Port Map

| Port | Direction | Protocol | Purpose | Rate |
|------|-----------|----------|---------|------|
| 10024 | Radar -> Multicast (239.255.0.2) | UDP | Echo sweep data | ~80/sec (~12ms) |
| 10034 | Radar -> Broadcast | UDP | ARPA target data | ~1/sec |
| 10010 | Radar -> Broadcast | UDP | Discovery beacon | ~1/16sec |
| 10010 | Host <-> Radar | TCP | One-shot init auth (copyright exchange) | Once at startup |
| 10100 | Host <-> Radar | TCP | Persistent control channel ($S/$N protocol) | ~2/sec + bursts |
| 1900 | Host -> 239.255.255.250 | UDP | SSDP/UPnP announcement | Once at startup |
| 10998 | Host listen | TCP | UPnP root device descriptor | On demand |

## Protocol Format

### Control Channel (TCP 10100) -- NMEA-like ASCII

All messages use ASCII framing similar to NMEA:

```
$<prefix><hex_id>,<param1>,<param2>,...\r\n
```

| Prefix | Direction | Meaning |
|--------|-----------|---------|
| `$S` | Host -> Radar | Send/Set command |
| `$R` | Host -> Radar | Read/Request (used during init) |
| `$N` | Radar -> Host | Notify/Response |

Command IDs are 2-character hex. Parameters are comma-separated decimal values.
Messages terminated with `\r\n` (0x0D 0x0A).

### Binary Keepalive (TCP 10100)

Periodically sent (every ~6-10 seconds):
```
00 00 00 08 00 00 00 00   (8 bytes, first 4 = length field)
```

## Command Reference

### Range ($S62)
```
$S62,<rangeIndex>,0,0,0\r\n
```
- rangeIndex: 0-24 (0=0.125NM, 3=0.75NM, 6=2NM, 12=12NM)
- Response: `$N62,<rangeIndex>,0,0`

### Gain ($S63)
```
$S63,<channel>,<value>,0,0,0\r\n
```
- channel: 0=manual gain, 1=auto gain
- value: 0-255 (manual gain level)
- Response: `$N63,<channel>,<value>,0,0,0`

### STC ($S64)
```
$S64,<channel>,<value>,0,0,0,0\r\n
```
- channel: 0=near, 1=middle, 2=far
- value: STC level
- Response: `$N64,<channel>,<value>,0,0,0,0`

### Pulse Width ($S65)
```
$S65,<channel>,<value>,0,0,0,<p1>,<p2>\r\n
```
- channel: 0=set, 1=reset
- value: pulse width level
- Response: `$N65,...`

### TX On/Off ($S69)
```
$S69,<state>,0,0,0,0,0\r\n
```
- state: 1=TX OFF (standby), 2=TX ON
- TX toggle sends atomic burst: $S69 + $S85 + $S86 + $S87 (all within ~1ms)
- Response: `$N69,<state>,0,0,0,0,0`

### Firmware Version Query ($R96)
```
$R96,,,,,,,\r\n
```
- Response: `$N96,<ver1>,<ver2>,<ver3>,<ver4>,,,`
- Example: `$N96,0359360-01.07,0359358-01.01,0359359-01.02,0359361-01.06,,,`

### Bearing Resolution ($SEE)
```
$SEE,<mode>,0\r\n
```
- mode: 0=Off, 1=Low, 2=Med, 3=High

### Antenna Revolution ($S89)
```
$S89,<rollSpeed>,<hiRpm>\r\n
```

### Sea Clutter ($S85/$S86)
```
$S85,<value>\r\n    (A/C SEA control)
$S86,<value>\r\n    (A/C SEA mode)
```

### Rain Clutter ($S87)
```
$S87,<value>\r\n    (A/C RAIN control)
```

## Periodic Radar Telemetry (Unsolicited $N messages)

| Message | Rate | Content |
|---------|------|---------|
| `$NAF,256` | ~1/sec | Antenna heading/rotation indicator |
| `$NF5,<type>,<value>,0,0,0` | ~1/sec | Radar spoke/scan telemetry (type 3 or 4, value varies) |
| `$NE3` | Periodic | Heartbeat/sync marker |
| `$NE0,0,0,0,0,0,0,0,1` | Periodic | Extended status |
| `$N83,100,1` | Periodic | Gain/threshold report |
| `$N68,0,0,0,0,-2` | Periodic | Status with negative value |
| `$N00,CLIENTCONNECTION,` | Once | Connection established notification |

## Init Sequence (Cold Start)

### Phase 1: Pre-connection
- Radar broadcasts ARPA data on UDP 10034 (~1/sec) regardless of client
- Radar broadcasts discovery beacon on UDP 10010 (~1/16sec): 32 bytes containing "RD003226"

### Phase 2: SSDP Discovery
- Host sends 3 SSDP NOTIFY packets to 239.255.255.250:1900
- SERVER: `FURUNO/1.0 UPnP/1.1 FurunoSensorSDK/1.1`
- LOCATION: `http://172.31.1.100:10998/rootDesc.xml`

### Phase 3: Authentication (TCP 10010)
- Host opens TCP to radar:10010
- Host sends 56-byte binary payload:
  ```
  08 01 00 38 01 00 00 00 00 01 00 00
  COPYRIGHT (C) 2001 FURUNO ELECTRIC CO.,LTD.
  ```
  Header: type=0x08, subtype=0x01, length=0x0038
- Radar responds 12 bytes:
  ```
  09 01 00 0C 01 00 00 00 00 64 00 00
  ```
  Type=0x09, subtype=0x01, length=0x000C, status=0x0064 (100)
- Connection closed immediately (one-shot auth)

### Phase 4: Control Channel (TCP 10100)
- Host opens TCP to radar:10100
- Radar sends: `$N00,CLIENTCONNECTION,\r\n` (24 bytes)
- Radar sends: `$NAF,256\r\n` (antenna heading)
- Host sends: `$R96,,,,,,,\r\n` (firmware query)
- Radar sends binary 8 bytes: `00 00 00 08 00 00 00 00`
- Radar responds: `$N96,0359360-01.07,...`

### Phase 5: Configuration Dump
- Host sends ~60+ `$R<hex_id>` commands in rapid burst (~120ms)
- Registers addressed: 61-B5 range (hex)
- All sent with zero/default values
- Radar responds with `$N<hex_id>` for each, reporting current state
- Key responses include gain=100, timing params, device capabilities

### Phase 6: Steady State
- Periodic `$NAF,256` (1/sec) and `$NF5` telemetry
- Binary keepalive every ~6-10 seconds
- No echo data until TX explicitly enabled via `$S69,2,...`

## Echo Packets (UDP 10024)

### Characteristics
- Rate: ~80 packets/sec (~12ms interval)
- Source: 172.31.3.226:10024
- Destination: 239.255.0.2:10024 (multicast)
- Size: 484-868 bytes (variable, ~625 avg)
- Only present when TX is ON

### Header Structure (18 bytes)

| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0-1 | 2 | Sequence/spoke counter | Increments per packet |
| 2-3 | 2 | Protocol version | Constant `00 01` |
| 4-7 | 4 | Reserved | Constant `00 00 00 00` |
| 8-11 | 4 | Timestamp | Upper bytes vary slowly |
| 12-13 | 2 | Flags/mode | Constant `15 00` |
| 14-15 | 2 | Config identifier | Constant `F0 09` (may change with range) |
| 16-17 | 2 | Azimuth angle | Changes per spoke, wraps around |

### Body Structure
- Variable-length echo amplitude data
- Range bins encoded with sub-headers indicating starting range offset
- Amplitude values: quantized steps (FD, FC, FE, DC, BC, A4, 8C, 78, 68, 58, 4C, 40, 38, 2C, 24, 1C, 14, 08, 00)
- Sparse encoding: only segments with returns are transmitted (explains variable size)
- `FE 00` pairs = no return / noise floor

## Broadcast Packets

### ARPA Data (UDP 10034)
- Rate: ~1/sec
- Size: 65 bytes (standard) or 119 bytes (extended), occasionally 176 bytes
- Format: `$ARPA,0,0,<len>,<seq>,<len>,` followed by binary data
- Contains automatic radar plotting aid target data

### Discovery Beacon (UDP 10010)
- Rate: ~1/16sec
- Size: 32 bytes
- Contains device identifier "RD003226"
- Structure:
  ```
  01 00 00 01 00 00 00 00  (header)
  00 01 00 18 01 00 00 00  (capability/type)
  52 44 30 30 33 32 32 36  ("RD003226")
  01 01 00 02 00 01 00 12  (device type/version)
  ```

## Key Observations

1. The protocol is **NMEA-like ASCII** on TCP, not binary -- much easier to reverse engineer
2. `$S` (send) / `$R` (read) / `$N` (notify) prefix convention
3. Init auth uses a **copyright string** on port 10010 -- simple authentication
4. Radar **echoes every command** back as confirmation
5. TX state: 1=OFF, 2=ON (counterintuitive)
6. Echo data uses **sparse encoding** with variable-length spoke packets
7. All register IDs are 2-char hex in range 0x61-0xB5+ (97-181+ decimal)
8. The 8-byte binary keepalive has a 4-byte length prefix pattern
