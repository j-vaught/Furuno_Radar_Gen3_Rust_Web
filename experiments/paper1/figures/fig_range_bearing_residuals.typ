// J.C. Vaught. Preview-safe template for surveyed residual analysis.
#set page(width: 7.16in, height: auto, margin: (x: 0.10in, y: 0.06in))
#set text(font: "New Computer Modern", size: 8pt)
#show math.equation: set text(font: "New Computer Modern Math")
#import "@preview/lilaq:0.6.0" as lq
#let d = json("data/residuals.json")

#if d.status == "preview" {
  align(center, block(width: 6.4in, height: 2.0in, stroke: (paint: black, thickness: 0.7pt),
    inset: 0.12in, fill: white,
    align(center + horizon, text(size: 10pt, [PREVIEW. Awaiting surveyed target and detection data.]))))
} else {
  align(center, grid(
    columns: 2,
    column-gutter: 0.24in,
    lq.diagram(lq.plot(d.range_m, d.range_error_m, stroke: 1pt, mark: none),
      width: 3.12in, height: 1.78in, xlabel: [Range (m)], ylabel: [Range error (m)],
      xaxis: (subticks: none), yaxis: (subticks: none)),
    lq.diagram(lq.plot(d.bearing_deg, d.bearing_error_deg, stroke: 1pt, mark: none),
      width: 3.12in, height: 1.78in, xlabel: [Bearing (deg)], ylabel: [Bearing error (deg)],
      xaxis: (subticks: none), yaxis: (subticks: none)),
  ))
}
