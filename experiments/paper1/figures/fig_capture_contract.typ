// J.C. Vaught. Figure source for IEEE one-column placement.
#set page(width: 7.16in, height: auto, margin: (x: 0.10in, y: 0.08in))
#set text(font: "New Computer Modern", size: 8pt)
#show math.equation: set text(font: "New Computer Modern Math")
#import "@preview/cetz:0.4.2" as cetz

#align(center, cetz.canvas(length: 1cm, {
  import cetz.draw: *
  let edge = (paint: black, thickness: 0.7pt, cap: "square", join: "miter")
  let garnet = rgb("73000A")
  let arrow = (end: "stealth", fill: black, stroke: 0.25pt + black,
    length: 0.16cm, width: 0.11cm, inset: 0.03cm, anchor: "tip")
  let tx-arrow = (end: "stealth", fill: garnet, stroke: 0.25pt + garnet,
    length: 0.16cm, width: 0.11cm, inset: 0.03cm, anchor: "tip")
  let node(name, x, y, width, label) = {
    rect((x - width / 2, y - 0.38), (x + width / 2, y + 0.38),
      name: name, fill: white, stroke: edge, radius: 0pt)
    content(name + ".center", anchor: "center",
      align(center + horizon, block(width: (width - 0.10) * 1cm, label)))
  }

  // Control state timeline. The recording window begins only after the
  // post-activation readback and settle interval.
  node("standby_a", 1.00, 3.00, 1.35, [Standby])
  node("settings", 3.10, 3.00, 1.45, [Settings])
  node("tx_on", 5.45, 3.00, 1.65, [TX on ACK])
  node("readback", 7.90, 3.00, 1.65, [Readback])
  node("record", 10.35, 3.00, 1.45, [Record])
  node("standby_b", 12.75, 3.00, 1.55, [Standby])
  line("standby_a.east", "settings.west", stroke: edge, mark: arrow)
  line("settings.east", "tx_on.west", stroke: (paint: garnet, thickness: 0.85pt), mark: tx-arrow)
  for (a, b) in (("tx_on", "readback"), ("readback", "record"),
    ("record", "standby_b")) {
    line(a + ".east", b + ".west", stroke: edge, mark: arrow)
  }

  // Independent dataflow branches.
  node("udp", 10.35, 1.00, 1.35, [UDP])
  node("raw", 12.30, 1.65, 1.65, [Raw archive])
  node("decode", 10.35, 0.05, 1.35, [Decode])
  node("save", 12.30, 0.05, 1.55, [CSV + PNG])
  node("metadata", 7.90, 1.00, 1.55, [Metadata])
  line("udp.east", "raw.west", stroke: edge, mark: arrow)
  line("udp.south", "decode.north", stroke: edge, mark: arrow)
  line("decode.east", "save.west", stroke: edge, mark: arrow)
  line("record.south", "udp.north", stroke: edge, mark: arrow)
  line("readback.south", "metadata.north", stroke: edge, mark: arrow)
  line("metadata.south", (7.90, -0.55), stroke: edge)
  line((7.90, -0.55), (12.30, -0.55), stroke: edge)
  line((12.30, -0.55), "save.south", stroke: edge, mark: arrow)
}))
