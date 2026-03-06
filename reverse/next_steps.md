# Next Steps: Rust Radar Client Replacement

## Goal
Replace `libNAVNETDLL.so` entirely with the Rust `radar-client` library.

## What Works Today (in Rust)
- TCP connection + 8-byte authentication handshake
- All `$S` set-command builders (range, gain, pulse, STC, sea/rain clutter, TX, bearing resolution, Doppler/EchoColorType, antenna revolution, bird mode, blind sector, etc.)
- All `$R` query builders
- UDP multicast echo packet receive (239.255.0.2:10024)
- All 4 echo encoding decoders (raw, RLE, delta RLE, 2-bit RLE with prev-spoke copy)
- Echo header parsing (sweep count, sweep length, encoding type)
- Keepalive packet sending
- Beacon auto-discovery (broadcast on 172.31.255.255:10010)
- Calibration pattern system with differential command generation

## Blockers Before Dropping libNAVNETDLL.so

### 1. Init/Handshake Sequence (CRITICAL)
Decode the full startup sequence from process launch to first echo packet.
We have a cold-start pcap (`reverse/captures/20260306_035400_cold_start_init.pcap`)
that captures this. Need to:
- Extract the TCP message exchange on port 10100 during init
- Identify registration messages sent by the library
- Determine what triggers the radar to start sending echo multicast
- Implement the sequence in Rust

### 2. ACK Parsing
Parse `$N` acknowledgment responses from the radar on TCP 10100.
Currently commands are fire-and-forget. Need to:
- Decode `$N` response format
- Match acks to sent commands
- Handle error/retry on NACK

### 3. Full Echo Pipeline
Bridge the gap between raw UDP packets and the callback-equivalent output.
The library's `DecodeImoEchoFormat` (2756 bytes at 0x730a8) does:
- Raw UDP → extract 16-byte header → decode status, h_flg, heading, angle
- Apply echo encoding decoder (types 0-3)
- Deliver via callback with all fields populated

We have the encoding decoders but need:
- Status field extraction from raw UDP header (bit 8 = Doppler, bits 0-2 = pulse type)
- Heading extraction from raw UDP header
- h_flg extraction (always 0 so far, but should still decode it)
- Map raw header bytes to callback parameters

### 4. Doppler Decode Implementation
Implement the confirmed encoding in Rust:
- Check status bit 8 for Doppler active flag
- When active: echo byte bit 7 = Doppler flag, bits 6:0 = intensity
- When inactive: echo byte bits 7:0 = full intensity
- Expose Doppler flag per-sample to rendering layer

### 5. Dual-Pulse Handling
Implement status type awareness:
- Status=1 (long pulse): 17-level coarse quantization
- Status=4 (short pulse): 130-240 level fine quantization
- Range-dependent: dual-pulse at < 6 NM, single at > 16 NM
- Rendering should merge both pulse types appropriately

### 6. Web Server (Optional)
If keeping the HTTP API:
- Port the Boost web server to Rust (axum/actix-web)
- Or keep C++ web server as frontend, Rust as backend library
- All existing endpoints need equivalents

## Recommended Order of Work

1. **Decode init handshake from cold-start pcap** — This unblocks everything
2. **Implement ACK parsing** — Needed for reliable command sending
3. **Complete echo pipeline** (status/heading/h_flg from raw UDP) — Needed for proper rendering
4. **Add Doppler + dual-pulse decode** — Enhancement, not blocker
5. **Web server port** — Can be done incrementally, endpoint by endpoint

## Estimated Scope
Steps 1-3 are the minimum viable replacement. Once those work, you can run
the Rust client standalone and get the same echo data + command control that
`libNAVNETDLL.so` provides today. Steps 4-5 are enhancements. Step 6 is a
separate project.
