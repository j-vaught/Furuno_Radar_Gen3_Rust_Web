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
`clamped`, `deferred`, `restored`, or `superseded` outcomes. The adapter writes the JSON files imported
by the Typst sources. The future analyzer sources use a top-level `status`
field. `preview` is the only permitted state for an empty input and causes the
Typst source to render an explicit preview panel. A production result sets
`status` to `ready` and supplies the arrays required by its template.

The repeatability adapter emits `runs` and `metric` from rotation index and
`zero_fraction`. The transition adapter emits `latency_event_index`,
`latency_ms`, `event_index`, `match_event_index`, and `readback_match` from the
command/readback CSV.
`latency_ms` is computed from ISO-8601 or `unix_ms:` ACK and readback
timestamps and remains empty when either timestamp is absent. Readback-match
points are retained only when the source field is present, so missing actual
values are not plotted as mismatches.
The E1 repeatability contract is stored separately in
`data/repeatability_e1.json`. It groups the 30-s fixed-scene session summary
by configuration export and resets rotation index within each export. Its
ordinate is decoded zero fraction expressed as a percentage for plotting; it
does not encode surveyed geometry or physical calibration.
The EF paired-response contract is stored in `data/ef_paired_response.json`.
It pairs the observed EF0 record with EF1 or EF2 within the same fixed
processing context, using the explicit `ef0-per-fixed-context` rule. It plots
decoded zero fraction, observed code count, and unique angle count from the
four completed records. The phase labels remain EF0, EF1, and EF2; they do not
establish a manufacturer Doppler or velocity interpretation.
The completed-campaign helper writes `family_coverage.json` with one status per
planned control family. Its complete subset feeds `setting_response.csv`; the
coverage figure reports pending and failed families separately from measured
response values.
The settings coverage adapter emits the same event index and match indicator,
plus setting, requested, and observed labels for audit. It maps only documented
schema outcomes. Missing or unknown outcomes increment `unclassified_count`
and are excluded from category counts. `superseded` is a separate category for
a source record replaced by a corrected run and is never mapped to `deferred`.
The surveyed residual
adapter emits `range_m`, `range_error_m`, `bearing_deg`, and
`bearing_error_deg` only for complete rows.

The setting-response adapter consumes `figure_data/setting_response.csv` or
`setting_effects.csv` and groups rows by `control_family` and `condition`. It
accepts campaign phase rows with `phase_index`, `phase_id`, `control_family`,
`role`, `reference_phase_id`, `reference_relation`, `settings_hash`,
`reference_settings_hash`, `settings_delta_keys`, `rotation_count`,
`decoded_zero_fraction`, `exported_code_support`, and `angular_support`.
The campaign aggregator computes these means from completed rotation metrics
and preserves the phase and settings provenance. It groups the figure output by
control family so the plot does not create one legend entry per setting value.
The build uses the resulting family labels to compile one companion Typst
figure per non-reference family. Each companion selects one family series and
the matched reference series, so a campaign with many control families does
not create a dense multi-family legend. No companion is emitted while the
setting-response contract remains in `preview` state.
The adapter also consumes `control_characterization.csv` for settings coverage;
outcomes are mapped to accepted, rejected, clamped, deferred, restored, and
superseded without treating an unverified command as an accepted response. The bit-plane adapter
consumes the Doppler audit JSON and retains the pre-shift histogram, upper-six
histogram, decoded low-two occupancy, and exact CSV replay match fractions.
Those fields are representation and alignment evidence. They do not establish
velocity or Doppler observability.

The campaign command is `tools/aggregate_campaign.py --manifest
<session_manifest.json> --rotation-metrics <rotation_metrics.csv> --out
<figure_data>`. It writes `setting_response.csv`. For a recovered standby
characterization, pass `--recover-characterization
<characterization_recovered.json>` to write `control_characterization.csv`;
this path contains readback outcomes only and does not create response metrics.
