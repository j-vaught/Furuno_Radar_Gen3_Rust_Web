// J.C. Vaught. Settings command/readback coverage from analyzer CSV output.
#set page(width: 7.16in, height: auto, margin: (x: 0.10in, y: 0.06in))
#set text(font: "New Computer Modern", size: 8pt)
#show math.equation: set text(font: "New Computer Modern Math")
#import "@preview/lilaq:0.6.0" as lq
#let d = json("data/settings_coverage.json")
#let category-ticks = d.categories.enumerate().map(((index, label)) => (index, label))

#if d.status == "preview" {
  align(center, block(width: 6.4in, height: 2.0in, stroke: (paint: black, thickness: 0.7pt),
    inset: 0.12in, fill: white,
    align(center + horizon, text(size: 10pt, [PREVIEW. Awaiting settings readback coverage data.]))))
} else {
  lq.diagram(
    lq.bar(range(0, d.categories.len()), d.category_counts, width: 0.72),
    width: 6.4in,
    height: 2.0in,
    xlabel: [Schema outcome category],
    ylabel: [Control count],
    xlim: (-0.5, d.categories.len() - 0.5),
    xaxis: (ticks: category-ticks, subticks: none),
    yaxis: (subticks: none),
  )
}
