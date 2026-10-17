// J.C. Vaught. EF paired-condition analyzer metrics.
#set page(width: 7.16in, height: auto, margin: (x: 0.10in, y: 0.06in))
#set text(font: "New Computer Modern", size: 8pt)
#show math.equation: set text(font: "New Computer Modern Math")
#import "@preview/lilaq:0.6.0" as lq
#let d = json("data/ef_paired_response.json")

#if d.status != "ready" or d.pairs.len() != 2 {
  align(center, block(width: 6.4in, height: 2.0in, stroke: (paint: black, thickness: 0.7pt),
    inset: 0.12in, fill: white,
    align(center + horizon, text(size: 10pt,
      [PREVIEW. Awaiting the completed EF paired-condition export.]))))
} else {
  let pair-panel(pair, metric, ylabel, show-legend, show-x, show-y) = {
    let ref = pair.reference
    let cond = pair.condition
    lq.diagram(
      lq.plot(
        ref.map(row => row.rotation_index),
        ref.map(row => row.at(metric)),
        label: [EF0], stroke: 1pt, mark: none,
      ),
      lq.plot(
        cond.map(row => row.rotation_index),
        cond.map(row => row.at(metric)),
        label: pair.condition_label, stroke: 1pt, mark: none,
      ),
      width: 2.02in,
      height: 1.48in,
      xlabel: if show-x { [Rotation index] } else { [] },
      ylabel: if show-y { ylabel } else { [] },
      legend: if show-legend { (position: top + right, radius: 0pt) } else { none },
      xaxis: (subticks: none),
      yaxis: (subticks: none),
    )
  }
  grid(
    columns: 3,
    column-gutter: 0.16in,
    row-gutter: 0.12in,
    pair-panel(d.pairs.at(0), "zero_fraction_pct", [Zero fraction (%)], true, false, true),
    pair-panel(d.pairs.at(0), "observed_code_count", [Observed code count], false, false, false),
    pair-panel(d.pairs.at(0), "unique_angle_count", [Unique angles], false, false, false),
    pair-panel(d.pairs.at(1), "zero_fraction_pct", [Zero fraction (%)], true, true, true),
    pair-panel(d.pairs.at(1), "observed_code_count", [Observed code count], false, true, false),
    pair-panel(d.pairs.at(1), "unique_angle_count", [Unique angles], false, true, false),
  )
}
