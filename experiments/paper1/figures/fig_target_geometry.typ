// J.C. Vaught. Symbolic geometry only. Survey distances are supplied later.
#set page(width: 3.45in, height: auto, margin: (x: 0.08in, y: 0.08in))
#set text(font: "New Computer Modern", size: 8pt)
#show math.equation: set text(font: "New Computer Modern Math")
#import "@preview/cetz:0.4.2" as cetz

#align(center, cetz.canvas(length: 1cm, {
  import cetz.draw: *
  let edge = (paint: black, thickness: 0.75pt, cap: "square", join: "miter")
  let garnet = rgb("73000A")
  let arrow = (end: "stealth", fill: garnet, stroke: 0.25pt + garnet,
    length: 0.17cm, width: 0.12cm, inset: 0.03cm, anchor: "tip")
  circle((4.25, 3.00), radius: 0.38, fill: white, stroke: edge)
  content((4.25, 3.00), anchor: "center", [Radar])
  circle((9.15, 5.15), radius: 0.31, fill: garnet, stroke: edge)
  content((9.15, 5.72), anchor: "south", [Target])
  line((4.63, 3.00), (9.15, 5.15), stroke: edge, mark: arrow)
  line((4.25, 3.00), (6.25, 3.00), stroke: (paint: black, thickness: 0.55pt), mark: arrow)
  content((5.25, 2.80), anchor: "north", [$x$])
  content((6.90, 4.02), anchor: "center", [$r$])
  content((5.25, 3.36), anchor: "south", [reference axis])
  arc((4.25, 3.00), radius: 1.18, start: 0deg, stop: 24deg,
    stroke: (paint: garnet, thickness: 0.85pt), mark: arrow)
  content((5.44, 3.22), anchor: "west", [$\beta$])
  line((9.15, 5.15), (9.15, 1.05), stroke: (paint: black, thickness: 0.55pt, dash: "dashed"))
  content((9.42, 3.10), anchor: "west", [survey line])
}))
