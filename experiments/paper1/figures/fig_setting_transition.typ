// J.C. Vaught. Preview-safe template for settings command/readback timing.
#set page(width: 7.16in, height: auto, margin: (x: 0.10in, y: 0.06in))
#set text(font: "New Computer Modern", size: 8pt)
#show math.equation: set text(font: "New Computer Modern Math")
#import "@preview/lilaq:0.6.0" as lq
#let d = json("data/setting_transition.json")

#if d.status == "preview" {
  align(center, block(width: 6.4in, height: 2.0in, stroke: (paint: black, thickness: 0.7pt),
    inset: 0.12in, fill: white,
    align(center + horizon, text(size: 10pt, [PREVIEW. Awaiting verified command and readback history.]))))
} else {
  align(center, grid(
    columns: 2,
    column-gutter: 0.24in,
    lq.diagram(
      lq.plot(d.latency_event_index, d.latency_ms, stroke: 1pt, mark: none),
      width: 3.12in, height: 1.78in,
      xlabel: [Transition index], ylabel: [ACK to readback (ms)],
      xaxis: (subticks: none), yaxis: (subticks: none),
    ),
    lq.diagram(
      lq.plot(d.match_event_index, d.readback_match, stroke: 1pt, mark: "o"),
      width: 3.12in, height: 1.78in,
      xlabel: [Transition index], ylabel: [Readback match],
      xaxis: (subticks: none), yaxis: (ticks: (0, 1), subticks: none),
    ),
  ))
}
