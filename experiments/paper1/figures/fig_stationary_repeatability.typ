// J.C. Vaught. Preview-safe template for stationary repeatability analysis.
#set page(width: 7.16in, height: auto, margin: (x: 0.10in, y: 0.06in))
#set text(font: "New Computer Modern", size: 8pt)
#show math.equation: set text(font: "New Computer Modern Math")
#import "@preview/lilaq:0.6.0" as lq
#let d = json("data/repeatability.json")

#if d.status == "preview" {
  align(center, block(width: 6.4in, height: 2.0in, stroke: (paint: black, thickness: 0.7pt),
    inset: 0.12in, fill: white,
    align(center + horizon, text(size: 10pt, [PREVIEW. Awaiting stationary analyzer output.]))))
} else {
  lq.diagram(
    lq.plot(d.runs, d.metric, stroke: 1pt, mark: none),
    width: 6.4in,
    height: 2.0in,
    xlabel: [Rotation index],
    ylabel: [Decoded zero fraction],
    xaxis: (subticks: none),
    yaxis: (subticks: none),
  )
}
