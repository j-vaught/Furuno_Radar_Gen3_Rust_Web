# Furuno NAVNET Protocol Notes

Generated: 2026-03-06 from live packet captures

## Network Topology

| Host | IP | Role |
|------|-----|------|
| Pi (our app) | 172.31.1.100 | Client |
| Radar unit | 172.31.3.226 | Radar hardware |
| Multicast echo | 239.255.0.2 | Echo data group |
| Broadcast | 172.31.255.255 | ARPA + discovery |

## Traffic Flows (steady state, 30 seconds)

| Flow | Protocol | Port(s) | Count | Rate | Purpose |
|------|----------|---------|-------|------|---------|
| Radar → Multicast (239.255.0.2) | UDP | 10024 → 10024 | 1,871 | ~62/sec | **Echo sweep data** |
| Radar → Pi | TCP | 10100 → 46732 | 61 | ~2/sec | Command/control (NMEA?) |
| Pi → Radar | TCP | 46732 → 10100 | 60 | ~2/sec | ACKs / responses |
| Radar → Broadcast | UDP | 10034 → 10034 | 38 | ~1.3/sec | **ARPA target data** |
| Radar → Broadcast | UDP | 10010 → 10010 | 2 | rare | Discovery/heartbeat |

## Port Map

| Port | Direction | Protocol | Purpose |
|------|-----------|----------|---------|
| 10024 | Radar → Multicast | UDP | Echo data (sweep packets) |
| 10034 | Radar → Broadcast | UDP | ARPA target data |
| 10010 | Radar → Broadcast | UDP | Discovery / alive / FNET |
| 10100 | Radar ↔ Pi | TCP | Command channel (likely NMEA-style) |

**Base port appears to be 10000.** Offsets: +24 echo, +34 ARPA, +10 discovery, +100 TCP control.

## Echo Packets (Port 10024)

- **Source:** 172.31.3.226 (radar)
- **Destination:** 239.255.0.2 (multicast group)
- **Rate:** ~62 packets/sec (~11-14ms between packets)
- **Sizes:** 172-260 bytes (variable, likely depends on echo content)
- **Packet interval changes:** first half ~14ms, second half ~11ms (antenna speed variation?)

### Echo Packet Header (first 32 bytes of UDP payload)

From hex dump of first echo packet (232 bytes UDP payload):
```
Offset  Hex                     Notes
0x00    02 ef                   ??? (possibly packet type 0x02 = echo?)
0x02    00 01                   ??? (constant across packets?)
0x04    00 00 00 00             ??? (padding or sequence?)
0x08    37 0a                   Angle? 0x0A37 = 2615 (of 8192 ticks)
0x0a    64 db                   Heading? 0xDB64 = could be heading ticks
0x0c    02 00                   Heading flag? Range unit?
0x0e    f0 09                   Range? 0x09F0 = 2544
0x10    0a 00                   Scale?
0x12    00 00                   ???
0x14    00 fd 00 d5 ...         Echo data starts here (at offset 0x14 = 20)
```

Second packet (offset 0x00 of UDP payload):
```
02 f0 00 01 00 00 00 00 3e 0a 64 db 02 00 f0 09
```
- Byte 0x01 changes: ef → f0 (could be low byte of angle or sweep count)
- Bytes 0x08-0x09: 37 0a → 3e 0a (angle incrementing: 0x0A37=2615, 0x0A3E=2622, delta=7)

Third packet:
```
02 f1 00 01 00 00 00 00 39 0a 64 db 02 00 f0 09
```
- Byte 0x01: f1 (incrementing: ef, f0, f1 — this is a sequence counter or packet sub-index)
- Bytes 0x08-0x09: 39 0a (angle 0x0A39=2617 — angles are NOT strictly increasing, so this might be structured differently)

**Key observation:** The bytes at offset 0x08-0x09 look like they could be angle in little-endian (0x370a = 0x0A37 LE). Need to cross-reference with `RadarSweepMetadata` struct and `DecodeImoEchoFormat`.

### Echo Data Encoding

Starting at offset ~0x14, the echo data appears to be:
- Pairs of bytes where one is `00` and the other is an intensity value
- `fd` and `fe` appear frequently (possibly special markers: run-length or range markers?)
- Pattern suggests some form of compression (not raw intensity array)

## ARPA Packets (Port 10034)

- **Source:** 172.31.3.226 (radar)
- **Destination:** 172.31.255.255 (broadcast)
- **Rate:** ~1.3/sec
- **Size:** 176 bytes (fixed)
- **Contains ASCII:** `$ARPA,0,0,0032,0000,0032,` — NMEA-style ARPA sentence embedded in binary wrapper

### ARPA Packet Structure (176 bytes UDP payload):
```
Offset  Content
0x00    01 c7 00 01 00 00 00 00    Header (type 0x01 = ARPA?)
0x08    24 41 52 50 41 2c ...      "$ARPA," ASCII NMEA sentence
        Two ARPA sentences per packet
0x30+   Binary ARPA target data
```

## TCP Control Channel (Port 10100)

- Bidirectional TCP connection between radar (10100) and Pi (46732)
- Small packets (~10 bytes)
- Example payload: `24 4e 41 46 2c 32 35 36 0d 0a` = `$NAF,256\r\n`
- This is **NMEA-style framing** with `$` prefix and `\r\n` terminator
- `$NAF` likely = NAVNET Function code message
- Rate: ~2/sec — likely heartbeat/alive check

## Discovery/FNET (Port 10010)

- Only 2 packets in 30 seconds — very infrequent
- Broadcast to 172.31.255.255
- Likely FNET host registration / discovery protocol

## Key Takeaways for Rust Implementation

1. **Echo data is multicast UDP** — just join group 239.255.0.2 on port 10024
2. **ARPA is broadcast UDP** — listen on port 10034
3. **Commands go over TCP** on port 10100, NMEA-style framing
4. **The protocol is relatively simple** — no encryption visible, no complex handshake in steady state
5. **Echo packets have a ~20-byte header** followed by compressed echo data
6. **ARPA packets embed NMEA $ARPA sentences** in a binary wrapper
7. **Need to investigate:** init/handshake sequence (what happens at startup before echoes flow?)
