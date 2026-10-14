// J.C. Vaught. Per-control-family setting-response metrics.
#set page(width: 7.16in, height: auto, margin: (x: 0.10in, y: 0.06in))
#set text(font: "New Computer Modern", size: 8pt)
#show math.equation: set text(font: "New Computer Modern Math")
#import "@preview/lilaq:0.6.0" as lq
#let d = json("data/setting_response.json")
#let family = sys.inputs.at("family", default: "")
#let selected = d.conditions.filter(c => c.label == family)
#let reference = d.conditions.filter(c => c.label == "reference")

#if d.status != "ready" or family == "" or selected.len() == 0 {
  align(center, block(width: 6.4in, height: 2.0in, stroke: (paint: black, thickness: 0.7pt),
    inset: 0.12in, fill: white,
    align(center + horizon, text(size: 10pt,
      [PREVIEW. Awaiting a selected control-family response series.]))))
} else {
  let panel(metric, label) = {
    let ref-plots = if reference.len() > 0 {
      (lq.plot(
        reference.at(0).rotation_index,
        reference.at(0).at(metric),
        label: [Reference],
        stroke: 1pt,
        mark: none,
      ),)
    } else { () }
    let condition-plot = lq.plot(
      selected.at(0).rotation_index,
      selected.at(0).at(metric),
      label: [Condition],
      stroke: 1pt,
      mark: none,
    )
    lq.diagram(
      ..(ref-plots + (condition-plot,)),
      width: 3.12in,
      height: 1.78in,
      xlabel: [Condition index],
      ylabel: label,
      legend: (position: top + right, radius: 0pt),
      xaxis: (subticks: none),
      yaxis: (subticks: none),
    )
  }
  align(center, grid(
    columns: 2,
    column-gutter: 0.24in,
    panel("zero_fraction", [Decoded zero fraction]),
    panel("exported_code_support", [Exported-code support]),
    panel("angular_support", [Angular support]),
  ))
}
