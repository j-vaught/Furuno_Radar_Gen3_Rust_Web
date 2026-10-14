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

  // Control state timeline.
  node("standby_a", 1.10, 3.00, 1.45, [Standby])
  node("readback", 3.35, 3.00, 1.75, [Readback])
  node("tx_on", 5.90, 3.00, 1.45, [TX on ACK])
  node("tx_off", 8.45, 3.00, 1.45, [TX off ACK])
  node("standby_b", 11.00, 3.00, 1.55, [Standby])
  for (a, b) in (("standby_a", "readback"), ("readback", "tx_on"),
    ("tx_on", "tx_off"), ("tx_off", "standby_b")) {
    line(a + ".east", b + ".west", stroke: edge, mark: arrow)
  }

  // Independent dataflow branches.
  node("udp", 5.90, 1.00, 1.35, [UDP])
  node("raw", 8.15, 1.55, 1.65, [Raw archive])
  node("decode", 5.90, 0.05, 1.35, [Decode])
  node("save", 8.50, 0.05, 1.55, [CSV + PNG])
  node("metadata", 4.25, 2.10, 1.55, [Metadata])
  line("udp.east", "raw.west", stroke: edge, mark: arrow)
  line("udp.south", "decode.north", stroke: edge, mark: arrow)
  line("decode.east", "save.west", stroke: edge, mark: arrow)
  line("tx_on.south", "udp.north", stroke: (paint: garnet, thickness: 0.85pt), mark: tx-arrow)
  line("readback.south", "metadata.north", stroke: edge, mark: arrow)
  line("metadata.east", (10.20, 2.10), stroke: edge)
  line((10.20, 2.10), (10.20, 0.80), stroke: edge)
  line((10.20, 0.80), "save.east", stroke: edge, mark: arrow)
}))
