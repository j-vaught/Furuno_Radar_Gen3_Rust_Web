// J.C. Vaught. Empirical decoded-cell support from the three recorded CSV rotations.
#set page(width: 7.16in, height: auto, margin: (x: 0.10in, y: 0.06in))
#set text(font: "New Computer Modern", size: 8pt)
#show math.equation: set text(font: "New Computer Modern Math")
#import "@preview/lilaq:0.6.0" as lq
#let d = json("data/decoded_support.json")
#let r0 = d.records.at(0)
#let r1 = d.records.at(1)
#let r2 = d.records.at(2)

#let panel(tag, series, xlab, ylab, legend-pos, dots) = {
  let s0 = series.at(0)
  let s1 = series.at(1)
  let s2 = series.at(2)
  let make-plot(series, label, dash) = if dots {
    lq.scatter(series.at(0), series.at(1), label: label, mark: "o")
  } else {
    lq.plot(series.at(0), series.at(1), label: label, stroke: (thickness: 1.1pt, dash: dash), mark: none)
  }
  lq.diagram(
    make-plot(s0, [Rotation 1], "solid"),
    make-plot(s1, [Rotation 2], "dashed"),
    make-plot(s2, [Rotation 3], "dotted"),
    width: 3.12in,
    height: 1.78in,
    xlabel: xlab,
    ylabel: ylab,
    legend: (position: legend-pos, radius: 0pt),
    yscale: if ylab == [Count] { "log" } else { "linear" },
    ylim: if ylab == [Count] { (1, auto) } else { auto },
    xaxis: (subticks: none),
    yaxis: (subticks: none),
  )
  v(-0.06in)
  align(left, text(size: 8pt, tag))
}

#align(center, grid(
  columns: 2,
  column-gutter: 0.24in,
  panel([(a)], (
    (r0.code_values_nonzero, r0.code_counts_nonzero),
    (r1.code_values_nonzero, r1.code_counts_nonzero),
    (r2.code_values_nonzero, r2.code_counts_nonzero),
  ), [Decoded cell code], [Count], bottom + center, true),
  panel([(b)], (
    (r0.angle_bin_deg, r0.angle_bin_counts),
    (r1.angle_bin_deg, r1.angle_bin_counts),
    (r2.angle_bin_deg, r2.angle_bin_counts),
  ), [Bearing bin (deg)], [Rows], top + right, false),
))
