// J.C. Vaught. Setting-response metrics from matched analyzer conditions.
#set page(width: 7.16in, height: auto, margin: (x: 0.10in, y: 0.06in))
#set text(font: "New Computer Modern", size: 8pt)
#show math.equation: set text(font: "New Computer Modern Math")
#import "@preview/lilaq:0.6.0" as lq
#let d = json("data/setting_response.json")

#if d.status == "preview" {
  align(center, block(width: 6.4in, height: 2.0in, stroke: (paint: black, thickness: 0.7pt),
    inset: 0.12in, fill: white,
    align(center + horizon, text(size: 10pt, [PREVIEW. Awaiting matched setting-response data.]))))
} else if d.conditions.filter(c => c.label == "reference").len() == 0 {
  align(center, block(width: 6.4in, height: 2.0in, stroke: (paint: black, thickness: 0.7pt),
    inset: 0.12in, fill: white,
    align(center + horizon, text(size: 10pt,
      [PREVIEW. Awaiting a reference series for the campaign overview.]))))
} else {
  let reference = d.conditions.filter(c => c.label == "reference").at(0)
  let panel(metric, label) = {
    let plots = (lq.plot(
      reference.rotation_index,
      reference.at(metric),
      label: [Reference],
      stroke: 1pt,
      mark: none,
    ),)
    lq.diagram(
      ..plots,
      width: 3.12in,
      height: 1.78in,
      xlabel: [Condition index],
      ylabel: label,
      legend: none,
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
