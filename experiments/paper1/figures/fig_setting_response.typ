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
} else {
  let panel(metric, label) = {
    let plots = d.conditions.map(c => lq.plot(
      c.rotation_index,
      c.at(metric),
      label: c.label,
      stroke: 1pt,
      mark: none,
    ))
    lq.diagram(
      ..plots,
      width: 3.12in,
      height: 1.78in,
      xlabel: [Rotation index],
      ylabel: label,
      legend: if d.conditions.len() > 1 { (position: bottom, dy: 75%, radius: 0pt) } else { none },
      xaxis: (subticks: none),
      yaxis: (subticks: none),
    )
  }
  align(center, grid(
    columns: 2,
    column-gutter: 0.24in,
    panel("zero_fraction", [Decoded zero fraction]),
    panel("mean_code", [Decoded mean code]),
    panel("max_code_fraction", [Maximum-code fraction]),
    panel("unique_angle_count", [Unique angle count]),
  ))
}
