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
  node("settings", 2.75, 3.25, 1.20, [Set])
  node("tx_on", 4.65, 3.25, 1.45, [Transmit])
  node("readback", 6.65, 3.25, 1.35, [Readback])
  node("record", 8.65, 3.25, 1.25, [Record])
  node("stop_drain", 10.75, 3.25, 1.30, [Drain])
  node("standby_b", 13.05, 3.25, 1.25, [Standby])
  line("standby_a.east", "settings.west", stroke: edge, mark: arrow)
  line("settings.east", "tx_on.west", stroke: (paint: garnet, thickness: 0.85pt), mark: tx-arrow)
  line("tx_on.east", "readback.west", stroke: edge, mark: arrow)
  line("readback.east", "record.west", stroke: edge, mark: arrow)
  line("record.east", "stop_drain.west", stroke: edge, mark: arrow)
  line("stop_drain.east", "standby_b.west", stroke: edge, mark: arrow)

  // The multicast stream fans out to independent raw and native receivers.
  node("multicast", 8.65, 1.35, 1.65,
    [Radar #linebreak() multicast], height: 0.48)
  node("raw_receiver", 6.55, 1.35, 1.65, [Raw receiver], height: 0.48)
  node("raw_fifo", 4.45, 1.35, 1.55, [Raw FIFO], height: 0.48)
  node("archive_writer", 2.25, 1.35, 1.90,
    [Archive writer], height: 0.48)
  node("jsonl", 2.25, 0.05, 1.55, [JSONL], height: 0.38)
  node("native_receiver", 10.70, 1.35, 1.90,
    [Native receiver], height: 0.48)
  node("decoder", 12.80, 1.35, 1.35, [Decoder], height: 0.48)
  node("rotation_queue", 14.35, 1.35, 1.65,
    [Rotation queue], height: 0.48)
  node("metadata", 7.65, 0.05, 1.85, [Settings snapshot], height: 0.38)
  node("products", 14.35, 0.00, 1.75,
    [CSV / PNG], height: 0.38)
  line("record.south", "multicast.north", stroke: edge, mark: arrow)
  line("multicast.west", "raw_receiver.east", stroke: edge, mark: arrow)
  line("raw_receiver.west", "raw_fifo.east", stroke: edge, mark: arrow)
  line("raw_fifo.west", "archive_writer.east", stroke: edge, mark: arrow)
  line("archive_writer.south", "jsonl.north", stroke: edge, mark: arrow)
  line("multicast.east", "native_receiver.west", stroke: edge, mark: arrow)
  line("native_receiver.east", "decoder.west", stroke: edge, mark: arrow)
  line("decoder.east", "rotation_queue.west", stroke: edge, mark: arrow)
  line("readback.south", (7.65, 2.35), stroke: dashed-edge)
  line((7.65, 2.35), (7.65, 0.43), stroke: dashed-edge)
  line((7.65, 0.43), "metadata.north", stroke: dashed-edge)
  line("metadata.east", (13.50, 0.05), stroke: dashed-edge)
  line((13.50, 0.05), (13.50, 0.87), stroke: dashed-edge)
  line((13.50, 0.87), "rotation_queue.south", stroke: dashed-edge, mark: dashed-arrow)
  line("rotation_queue.south", "products.north", stroke: edge, mark: arrow)
}))
