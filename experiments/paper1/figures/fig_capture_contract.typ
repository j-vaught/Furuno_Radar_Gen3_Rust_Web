// J.C. Vaught. Capture contract with bounded post-decode persistence.
#set page(width: 7.16in, height: auto, margin: (x: 0.10in, y: 0.08in))
#set text(font: "New Computer Modern", size: 8pt)
#import "@preview/cetz:0.4.2" as cetz

#align(center, cetz.canvas(length: 1cm, {
  import cetz.draw: *
  let edge = (paint: black, thickness: 0.7pt, cap: "square", join: "miter")
  let dashed-edge = (paint: black, thickness: 0.6pt, dash: "dashed", cap: "square")
  let garnet = rgb("73000A")
  let arrow = (end: "stealth", fill: black, stroke: 0.25pt + black,
    length: 0.16cm, width: 0.11cm, inset: 0.03cm, anchor: "tip")
  let tx-arrow = (end: "stealth", fill: garnet, stroke: 0.25pt + garnet,
    length: 0.16cm, width: 0.11cm, inset: 0.03cm, anchor: "tip")
  let dashed-arrow = (end: "stealth", fill: black, stroke: 0.25pt + black,
    length: 0.14cm, width: 0.10cm, inset: 0.03cm, anchor: "tip")
  let node(name, x, y, width, label, height: 0.38) = {
    rect((x - width / 2, y - height), (x + width / 2, y + height),
      name: name, fill: white, stroke: edge, radius: 0pt)
    content(name + ".center", anchor: "center",
      align(center + horizon, block(width: (width - 0.10) * 1cm, label)))
  }

  // Control state timeline. A timeout while draining is an explicit error.
  node("standby_a", 1.00, 3.25, 1.25, [Standby])
  node("settings", 2.75, 3.25, 1.40, [Apply settings])
  node("tx_on", 4.65, 3.25, 1.45, [TX on ACK])
  node("readback", 6.65, 3.25, 1.60,
    [Fresh #linebreak() verified #linebreak() settings], height: 0.54)
  node("record", 8.65, 3.25, 1.45,
    [Record #linebreak() rotations], height: 0.48)
  node("stop_drain", 10.75, 3.25, 2.40,
    [Stop native #linebreak() drain <= 45 s #linebreak() timeout = error], height: 0.62)
  node("standby_b", 13.05, 3.25, 1.25, [Standby])
  line("standby_a.east", "settings.west", stroke: edge, mark: arrow)
  line("settings.east", "tx_on.west", stroke: (paint: garnet, thickness: 0.85pt), mark: tx-arrow)
  line("tx_on.east", "readback.west", stroke: edge, mark: arrow)
  line("readback.east", "record.west", stroke: edge, mark: arrow)
  line("record.east", "stop_drain.west", stroke: edge, mark: arrow)
  line("stop_drain.east", "standby_b.west", stroke: edge, mark: arrow)

  // The independent raw receiver logs payloads directly; native persistence
  // begins only after decode and uses a bounded queue for CSV/PNG/PPI products.
  node("receive_raw", 8.65, 1.35, 1.65,
    [Python UDP #linebreak() receive_raw], height: 0.48)
  node("raw", 6.25, 1.35, 2.25,
    [Raw archive #linebreak() base64 JSONL #linebreak() session metadata], height: 0.58)
  node("decode", 10.55, 1.35, 1.65,
    [Native decode #linebreak() rotations], height: 0.48)
  node("queue", 12.45, 1.35, 1.75,
    [Native queue #linebreak() 16 jobs], height: 0.50)
  node("metadata", 7.65, 0.05, 2.20,
    [Immutable settings #linebreak() snapshot on job], height: 0.50)
  node("writer", 12.45, 0.00, 1.85,
    [Native writer #linebreak() CSV + PNG + PPI], height: 0.58)
  node("products", 12.45, -1.18, 1.95,
    [CSV + PNG #linebreak() PPI preview], height: 0.48)
  line("record.south", "receive_raw.north", stroke: edge, mark: arrow)
  line("receive_raw.west", "raw.east", stroke: edge, mark: arrow)
  line("receive_raw.east", "decode.west", stroke: edge, mark: arrow)
  line("decode.east", "queue.west", stroke: edge, mark: arrow)
  line("readback.south", (7.65, 2.35), stroke: dashed-edge)
  line((7.65, 2.35), (7.65, 0.55), stroke: dashed-edge)
  line((7.65, 0.55), "metadata.north", stroke: dashed-edge)
  line("metadata.east", (11.25, 0.05), stroke: dashed-edge)
  line((11.25, 0.05), (11.25, 0.75), stroke: dashed-edge)
  line((11.25, 0.75), "queue.south", stroke: dashed-edge, mark: dashed-arrow)
  line("queue.south", "writer.north", stroke: edge, mark: arrow)
  line("writer.south", "products.north", stroke: edge, mark: arrow)
}))
