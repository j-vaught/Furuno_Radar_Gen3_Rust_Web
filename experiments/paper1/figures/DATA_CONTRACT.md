# Figure data contracts

The live transport source is `data/raw_interarrival.json`. It must contain
`source`, `sha256`, `packets`, `duration_s`, `packet_index`, `interval_ms`, and
`rate_hz`. The packet index, interval, and rate arrays must have equal length.
The current capture contains 1,024 UDP payloads and 9.595979 s of archived
timestamps.

The decoded support source is `data/decoded_support.json`. Every member of
`records` must contain `name`, `rows`, `bins`, `code_values`, `code_counts`,
`code_values_nonzero`, `code_counts_nonzero`, `angle_bins`, and
`angle_bin_counts`. The code and count arrays must have the same length, and
the angle and count arrays must have the same length. The nonzero arrays are
the exact positive-count subset used by the logarithmic code plot, so omitted
zero-count bins are not rendered at a fabricated plotting floor. The current
capture has three rotations with 3,427, 2,690, and 2,691 decoded rows.

The adapter in `tools/adapt_toolkit.py` consumes the analyzer's
`figure_data/session_summary.csv`, `figure_data/setting_transition.csv`, and
`figure_data/geometry_residuals.csv` files. When present, an optional
`figure_data/settings_coverage.csv` supplies explicit `accepted`, `rejected`,
`clamped`, `deferred`, and `restored` outcomes. The adapter writes the JSON files imported
by the Typst sources. The future analyzer sources use a top-level `status`
field. `preview` is the only permitted state for an empty input and causes the
Typst source to render an explicit preview panel. A production result sets
`status` to `ready` and supplies the arrays required by its template.

The repeatability adapter emits `runs` and `metric` from rotation index and
`zero_fraction`. The transition adapter emits `latency_event_index`,
`latency_ms`, `event_index`, and `readback_match` from the command/readback CSV.
`latency_ms` is computed from the ISO-8601 ACK and readback timestamps and
remains empty when either timestamp is absent.
The settings coverage adapter emits the same event index and match indicator,
plus setting, requested, and observed labels for audit. The surveyed residual
adapter emits `range_m`, `range_error_m`, `bearing_deg`, and
`bearing_error_deg` only for complete rows.

The setting-response adapter consumes `figure_data/setting_response.csv` or
`setting_effects.csv` and groups rows by `control_family` and `condition`. It
requires rotation index plus decoded zero fraction, decoded mean code,
maximum-code fraction, and unique angular support. The bit-plane adapter
consumes the Doppler audit JSON and retains the pre-shift histogram, upper-six
histogram, decoded low-two occupancy, and exact CSV replay match fractions.
Those fields are representation and alignment evidence. They do not establish
velocity or Doppler observability.
