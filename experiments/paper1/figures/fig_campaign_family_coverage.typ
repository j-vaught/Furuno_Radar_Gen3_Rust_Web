// J.C. Vaught. Completion status for the planned control-family campaign.
#set page(width: 7.16in, height: auto, margin: (x: 0.10in, y: 0.06in))
#set text(font: "New Computer Modern", size: 8pt)
#show math.equation: set text(font: "New Computer Modern Math")
#import "@preview/lilaq:0.6.0" as lq
#let d = json("data/campaign_family_coverage.json")

#if d.status != "ready" {
  align(center, block(width: 6.4in, height: 2.0in, stroke: (paint: black, thickness: 0.7pt),
    inset: 0.12in, fill: white,
    align(center + horizon, text(size: 10pt,
      [PREVIEW. Awaiting campaign-family completion metadata.]))))
} else {
  lq.diagram(
    lq.bar(range(0, d.status_counts.len()), d.status_counts, width: 0.68),
    width: 6.4in,
    height: 2.0in,
    xlabel: [Campaign-family status],
    ylabel: [Family count],
    xlim: (-0.5, d.status_counts.len() - 0.5),
    xaxis: (ticks: ((0, [Complete]), (1, [Failed]), (2, [Pending])), subticks: none),
    yaxis: (subticks: none),
  )
}
