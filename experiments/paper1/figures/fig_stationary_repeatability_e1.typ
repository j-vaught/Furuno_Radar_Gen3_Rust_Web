// J.C. Vaught. E1 stationary repeatability from one fixed-scene export.
#set page(width: 7.16in, height: auto, margin: (x: 0.10in, y: 0.06in))
#set text(font: "New Computer Modern", size: 8pt)
#show math.equation: set text(font: "New Computer Modern Math")
#import "@preview/lilaq:0.6.0" as lq
#let d = json("data/repeatability_e1.json")

#if d.status != "ready" or d.groups.len() == 0 {
  align(center, block(width: 6.4in, height: 2.0in, stroke: (paint: black, thickness: 0.7pt),
    inset: 0.12in, fill: white,
    align(center + horizon, text(size: 10pt,
      [PREVIEW. Awaiting the E1 session summary.]))))
} else {
  let plots = d.groups.map(group => lq.plot(
    group.rotation_index,
    group.zero_fraction.map(value => value * 100),
    label: group.short_label,
    stroke: 1pt,
    mark: none,
  ))
  lq.diagram(
    ..plots,
    width: 6.4in,
    height: 2.1in,
    xlabel: [Rotation index within configuration export],
    ylabel: [Decoded zero fraction (\%)],
    legend: (position: top + right, radius: 0pt),
    xaxis: (subticks: none),
    yaxis: (subticks: none),
  )
}
