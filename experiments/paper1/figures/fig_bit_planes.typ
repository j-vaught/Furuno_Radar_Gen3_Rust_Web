// J.C. Vaught. Wire-level bit-plane audit and CSV alignment.
#set page(width: 7.16in, height: auto, margin: (x: 0.10in, y: 0.06in))
#set text(font: "New Computer Modern", size: 8pt)
#show math.equation: set text(font: "New Computer Modern Math")
#import "@preview/lilaq:0.6.0" as lq
#let d = json("data/bit_planes.json")

#if d.status == "preview" {
  align(center, block(width: 6.4in, height: 2.0in, stroke: (paint: black, thickness: 0.7pt),
    inset: 0.12in, fill: white,
    align(center + horizon, text(size: 10pt, [PREVIEW. Awaiting wire-level bit audit data.]))))
} else {
  align(center, grid(
    columns: 2,
    column-gutter: 0.24in,
    lq.diagram(
      lq.plot(d.pre_shift_values, d.pre_shift_counts_plot, stroke: 1pt, mark: none),
      width: 3.12in, height: 1.78in,
      xlabel: [Pre-shift code], ylabel: [Count], yscale: "log", ylim: (1, auto),
      xaxis: (subticks: none), yaxis: (subticks: none),
    ),
    lq.diagram(
      lq.plot(d.upper6_values, d.upper6_counts_plot, stroke: 1pt, mark: none),
      width: 3.12in, height: 1.78in,
      xlabel: [Upper-six code], ylabel: [Count], yscale: "log", ylim: (1, auto),
      xaxis: (subticks: none), yaxis: (subticks: none),
    ),
    lq.diagram(
      lq.bar(d.decoded_low2_values, d.decoded_low2_counts, width: 0.72),
      width: 3.12in, height: 1.78in,
      xlabel: [Decoded low-two class], ylabel: [Count],
      xaxis: (ticks: (0, 1, 2, 3), subticks: none), yaxis: (subticks: none),
    ),
    lq.diagram(
      lq.plot(d.alignment_index, d.alignment_fraction, stroke: 1pt, mark: "o"),
      width: 3.12in, height: 1.78in,
      xlabel: [CSV rotation index], ylabel: [Replay match fraction],
      xaxis: (subticks: none), yaxis: (ticks: (0, 1), subticks: none),
    ),
  ))
}
