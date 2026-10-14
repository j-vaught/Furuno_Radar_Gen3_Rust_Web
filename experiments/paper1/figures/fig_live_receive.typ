// J.C. Vaught. Empirical transport timing from the archived 10 s JSONL capture.
#set page(width: 7.16in, height: auto, margin: (x: 0.10in, y: 0.06in))
#set text(font: "New Computer Modern", size: 8pt)
#show math.equation: set text(font: "New Computer Modern Math")
#import "@preview/lilaq:0.6.0" as lq
#let d = json("data/raw_interarrival.json")

#let panel(x, y, tag, ylabel) = {
  lq.diagram(
    lq.plot(x, y, stroke: 1pt, mark: none),
    width: 3.12in,
    height: 1.76in,
    xlabel: [Packet index],
    ylabel: ylabel,
    xaxis: (subticks: none),
    yaxis: (subticks: none),
  )
  v(-0.06in)
  align(left, text(size: 8pt, tag))
}

#align(center, grid(
  columns: 2,
  column-gutter: 0.24in,
  panel(d.packet_index, d.interval_ms, [(a)], [Inter-arrival (ms)]),
  panel(d.packet_index, d.rate_hz, [(b)], [Packet rate (Hz)]),
))
