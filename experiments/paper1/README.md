# Paper 1 first-experiment toolkit

This directory defines the live measurement contract for Paper 1. It addresses the acquisition interface, settings provenance, settling behavior, persistence, and known-target geometry. It does not define a later gain-response, HDR, or perception benchmark. The machine-readable plan is in `plans/paper1_experiments.json`.

The runner uses the HTTP controller as the command boundary. It logs each request, response, command time, acknowledgement time, and device readback. Current controller builds expose the authoritative fresh snapshot through `/get_current?command=ALL&format=json&fresh=1`; the snapshot includes `settings_readback` and a finite `valid_until` timestamp. Older cached/register endpoints remain compatibility paths for non-characterization runs. A requested value never replaces a readback value. The recording manifest therefore contains both fields, with a match decision left explicit.

The preferred endpoint contract is recorded in `schemas/settings_readback.schema.json`. It returns a verified flag, transition identifier, capture timestamp, per-field verification records, and raw register responses. The runner requires every operational field in the plan before a live phase or control write proceeds. A failed or partial snapshot is retained for diagnosis and then stops the run.

The runner refuses a live capture unless the output directory exists, is writable, and is proved to be on a mounted external volume. A transmitting experiment also requires both `--transmit` and `--physical-ready`. A signal handler and a `finally` block issue TX OFF and recording STOP, and the event log records whether those cleanup requests returned successfully. No command is issued in `--dry-run` mode.

The runner expands each phase's `repeat` count before acquisition. Every executed phase receives a unique ID plus `source_phase_id`, `phase_index`, `repeat_index`, and `repeat_count` fields, so repeated rotations remain separate rows while retaining their source definition. Any non-characterization session with a successful initial readback attempts final restoration even when a phase fails.

Every manifest records the full Git commit, dirty-tree state, SHA-256 values for the runner, analyzer, plan, schemas, and available controller binary, plus an estimated phase runtime and optional raw-packet storage estimate from `metadata.capture_estimate`. Live runs require Git provenance.

## Before the first session

Complete `metadata_template.json` and make a separate target manifest from `target_manifest_template.json` when known targets are available. Record the radar label, firmware, host interface and addresses, mount height, heading reference, coordinate reference, clock synchronization state, weather, scene occlusions, and SHA-256 hashes for every required photo. For a geometry session, record each target's measured range and bearing, reference frame, measurement method, uncertainty, and target photo paths. A target with a placeholder or zero value is not valid ground truth.

The required photographs show the radar and mounting location, label or serial number, Ethernet path, host network settings, radar-view scene, and each target with a scale or surveyed marker. Keep the photos with the session outside the source repository and put their paths and hashes in the metadata file.

## Dry run

Run this command on the Mac after copying the plan and metadata to the external volume. It only validates the plan and writes a manifest.

```sh
python3 experiments/paper1/scripts/capture_runner.py \
  --plan experiments/paper1/plans/paper1_experiments.json \
  --experiment E0_reproducibility_readback \
  --metadata experiments/paper1/metadata_template.json \
  --output-root /Volumes/MacApplications/Radar_papers/data/paper1-experiments \
  --session-id dry-run-001 \
  --dry-run
```

For a live session, start the controller first and select one experiment. The two safety flags are intentionally separate.

```sh
python3 experiments/paper1/scripts/capture_runner.py \
  --plan experiments/paper1/plans/paper1_experiments.json \
  --experiment E1_fixed_scene_repeats \
  --metadata /Volumes/MacApplications/Radar_papers/data/paper1-metadata.json \
  --output-root /Volumes/MacApplications/Radar_papers/data/paper1-experiments \
  --session-id 2026-10-14-fixed-scene-01 \
  --receive-interface 172.31.1.100 \
  --transmit --physical-ready
```

The multicast receiver joins `239.255.0.2:10024` on the radar-facing host address. The runner uses `radar.host_ip` from the metadata file when `--receive-interface` is omitted. Set the option explicitly when the Mac has more than one Ethernet interface.

The one-control indoor response campaign is selected with `--experiment E6_indoor_one_control_response` and requires the completed E5 `session_manifest.json` through `--characterization-report`. When a corrective supplement exists, add it with `--characterization-report-supplement` and optionally write the provenance-preserving merged report with `--merged-characterization-report-out`. The runner filters out rejected or unverified controls using the latest outcome per setting and requested value, records every skipped case with a reason, requests a 30 s recording window and six rotations for each retained condition, and enforces both per-phase and whole-campaign wall-clock deadlines.

For the no-transmit characterization session, use the indoor metadata template and run the following command. It queries the complete baseline, probes every protocol-valid control value and context, records supported, rejected, clamped, deferred, or verification-failed outcomes, and restores the baseline including auto parameters and pulse width for every range.

```sh
python3 experiments/paper1/scripts/capture_runner.py \
  --plan experiments/paper1/plans/paper1_experiments.json \
  --experiment E5_indoor_standby_control_characterization \
  --metadata experiments/paper1/metadata_template.json \
  --output-root /Volumes/MacApplications/Radar_papers/data/paper1-experiments \
  --session-id 2026-10-14-indoor-controls-01
```

The corrective standby supplement is `E5_IR_noise_height_supplement`. It repeats the selector-sensitive interference-rejection query, noise reduction, and raw N84 antenna-height settings while retaining the original E5 session. Merge its manifest after the supplement finishes. The later outcome for each setting and requested value supersedes the earlier outcome, while the merged manifest retains both source paths and provenance.

```sh
python3 experiments/paper1/scripts/capture_runner.py \
  --plan experiments/paper1/plans/paper1_experiments.json \
  --experiment E5_IR_noise_height_supplement \
  --metadata experiments/paper1/metadata_template.json \
  --output-root /Volumes/MacApplications/Radar_papers/data/paper1-experiments \
  --session-id 2026-10-14-e5-ir-noise-height-01
```

Run `E5_gain_bounded_supplement` before E6. It verifies the installed-unit manual gain values 0, 25, 50, 64, 75, and 100 plus automatic gain values 0, 25, 50, 75, and 100. The E6 reference uses manual gain 64 because the earlier reference value 128 did not persist on the installed radar.

```sh
python3 experiments/paper1/scripts/capture_runner.py \
  --plan experiments/paper1/plans/paper1_experiments.json \
  --experiment E5_gain_bounded_supplement \
  --metadata experiments/paper1/metadata_template.json \
  --output-root /Volumes/MacApplications/Radar_papers/data/paper1-experiments \
  --session-id 2026-10-14-e5-gain-bounded-01
```

If a characterization process stops before restoration, use a new session identifier and the captured baseline file. The recovery command requires a fresh verified readback before each individual write, uses only verified current fields to decide whether a write is safe, records skipped fields from partial responses, and refuses to reuse an existing session directory. It accepts only a standby baseline and never enables transmission. The command still requires the plan and metadata arguments because they supply the provenance and storage record.

```sh
python3 experiments/paper1/scripts/capture_runner.py \
  --plan experiments/paper1/plans/paper1_experiments.json \
  --experiment E0_reproducibility_readback \
  --metadata experiments/paper1/metadata_template.json \
  --output-root /Volumes/MacApplications/Radar_papers/data/paper1-experiments \
  --session-id 2026-10-14-indoor-controls-recovery-01 \
  --baseline /Volumes/MacApplications/Radar_papers/macos-bringup/logs/readback-v2-baseline.json \
  --restore-only
```

Do not run E4 until the target manifest is complete. The runner can store the target manifest path in the metadata record, but it cannot infer a range or bearing from an image or an operator description.

## Experiment sequence

E0 is a standby reproducibility check. It queries the complete supported readback set three times and proves that settings metadata can be saved without transmitting. The useful result is a complete register trace and a settings hash, rather than an image.

E1 is the fixed-scene repeatability experiment. Use one fixed configuration, three or more complete rotations per phase, and a reference phase before and after the condition block. The first and last readbacks must match, and the scene, mount, range, pulse, and processing modes must remain unchanged.

E2 measures transitions. The requested gain and range changes are followed by readback and then by successive rotations. The first stable rotation is identified during analysis; it is not assumed from the command acknowledgement. Keep the reference phase interleaved so scene drift can be separated from transition behavior.

E3 verifies receive and persistence. The runner saves raw UDP payloads with receive timestamps, peer addresses, and base64 payloads while the controller saves CSV and PNG products. The offline analyzer replays the packet framing and decoder counts, checks timestamp ordering, compares raw and CSV hashes, and reports malformed or truncated packets. The existing 10-second smoke capture is a toolkit fixture; its three saved CSV rotations and raw JSONL are not evidence of complete scan coverage.

Raw capture uses a bounded in-memory FIFO and a separate archive writer so storage latency cannot shorten the receive window. The receiver records host timestamps before enqueueing each datagram, requests a 2 MiB socket receive buffer, and records the actual buffer size. Queue overflow, archive write errors, and an incomplete bounded drain fail the session. The final manifest reports received, written, pending, overflow, and write-error counts, and the session cannot be marked complete until the archive writer has drained.

E4 measures range and bearing only after the operator supplies surveyed target fields. The analyzer will report residuals with the supplied measurement and survey uncertainties. It will not invent a target coordinate, a range-bin conversion, an angular origin, or a mounting heading.

E5 characterizes the HTTP control surface while the radar remains in standby. It covers every source-table range probe, pulse value per range, manual and automatic gain and clutter parameters, STC, RezBoost, interference rejection, bird mode, Doppler, main bang, and antenna speed with high-RPM combinations. The source-table values are probes rather than guarantees of installed-unit acceptance. Each probe is followed by a fresh aggregate readback and classified from the observed response. The baseline is restored even after a mid-probe failure, and the final readback must match before the session is accepted.

E6 is the transmitting indoor response plan. It changes one operational control at a time from a confirmed baseline, records each condition for at least 30 s after settling, interleaves reference conditions, and restores the baseline after every condition and again during final cleanup. Indoor weather is held constant. Sea, rain, and Doppler values are recorded as internal processing modes rather than outdoor-weather variation.

For each transmitting phase, the runner verifies the requested state in standby, enables TX, obtains a fresh TX-start readback, waits the configured settling interval after TX is active, obtains a second readback, and only then starts recording. Gain, clutter, and antenna settings are sent as one complete register payload when a phase changes that register; unchanged payloads are logged and skipped from the fresh readback.

The TX gates require a transmitting readback state of 2 or 3, a successful recording-start response, and a `get_system_info` response with recording active. The system-info snapshot must also report `settings_verified,true` and match every requested setting at recording start and during every status poll. During acquisition the runner stops on an explicit recording, settings drift, or controller disconnect. After stopping, it requires the requested number of CSV rotations and checks each `furuno-web-2` CSV header for `settings_verified,true`, nonempty output, and recording start and stop boundaries. An active phase is retained in the failure manifest until its output checks and baseline restoration complete.

The current finite E6 matrix expands to 197 phases after the merged characterization reports filter unsupported outcomes. At the configured 5 s settling interval and 30 s recording window, the runner reports the 6,895 s window-and-settle lower bound. Control writes, readback, and baseline restoration add time and are not treated as zero-cost. Fill `metadata.timing_estimate` from a measured phase or dry run before a long campaign; the runner adds those per-phase values to its runtime estimate and records the basis in `session_manifest.json`. The wall-clock deadline still protects the run, so use a fresh session identifier and a deliberately larger timeout only after confirming available storage and operator coverage.

Before resuming the full E6 family campaign after a controller or rotation-writer update, run `E7_rotation_writer_validation`. It uses four complete 30 s windows with 5 s post-TX settling and six required rotations per window. The sequence covers the strict manual-gain-64 baseline, manual gain 100 at range index 0, manual gain 64 at range index 6, and a final baseline reference. The range-transition phase is a guard condition, so the run must stop if its complete readback or six-rotation requirement fails. Use a new session identifier, `--transmit`, and `--physical-ready`; the runner still restores and verifies the original strict baseline on every exit.

Run E7 with a fresh session directory after the native controller build is deployed.

```sh
python3 experiments/paper1/scripts/capture_runner.py \
  --plan experiments/paper1/plans/paper1_experiments.json \
  --experiment E7_rotation_writer_validation \
  --metadata /Volumes/MacApplications/Radar_papers/data/paper1-metadata.json \
  --output-root /Volumes/MacApplications/Radar_papers/data/paper1-experiments \
  --session-id 2026-10-14-e7-rotation-writer \
  --http-timeout 60 \
  --transmit --physical-ready
```

For a long E6 campaign, batch one control family at a time with `--control-family`, for example `--control-family gain_value`. The runner retains the complete reference settings and inserts a reference phase before and after every condition in that family. Each completed phase is checkpointed through an atomic manifest replacement with its actual readback and recording file paths, so a server-side analysis can begin while later batches remain pending.

The campaign driver performs that batching sequence and stops on the first failed child or failed final restoration. It writes `campaign_manifest.json` after each family and stores each child log beside its session directory.

```sh
python3 experiments/paper1/scripts/run_indoor_campaign.py \
  --plan experiments/paper1/plans/paper1_experiments.json \
  --metadata /Volumes/MacApplications/Radar_papers/data/paper1-metadata.json \
  --characterization-report /Volumes/MacApplications/Radar_papers/data/paper1-experiments/2026-10-14-indoor-controls-01/session_manifest.json \
  --output-root /Volumes/MacApplications/Radar_papers/data/paper1-experiments \
  --campaign-id 2026-10-14-e6-indoor-all \
  --transmit --physical-ready
```

Use `--family gain_value` or repeat `--family` to select a subset after checking the master manifest. The driver requires matching initial and final verified settings with `tx_state=1` for every completed child and rejects cleanup errors even when the child process exits successfully.

Each generated E6 condition reapplies the complete reference settings before applying its one requested variant and any explicit context fields. Gain transitions stage the inactive manual or automatic register when needed, then select the requested mode. The runner reads the complete setting snapshot afterward and stops before recording if any requested field is missing or differs. This keeps an automatic gain, range, pulse, clutter, and antenna state from carrying over from the preceding condition.

The runner skips an idempotent scalar command when the fresh verified pre-apply readback already matches the requested value, and records the skipped field and its readback source in `events.jsonl`. A pulse command remains required after a range transition because pulse values are stored per range. Doppler still receives a fresh readback before mode-sensitive gain, sea, and rain payloads are built. Complete post-apply, post-TX, recording, and final-restoration checks remain required.

Before restarting the stopped characterization, save the existing session directory and its event log. Select a new session identifier, confirm that the baseline file reports standby, inspect the Ethernet and recording paths, and keep the antenna sector clear. Run the standby recovery command first when the earlier session ended before its restoration report. Treat a partial recovery as a stop condition. Preserve its manifest and repair the controller or obtain a new verified baseline before beginning E5 again. These steps describe the required evidence path and do not add measurements that were not recorded.

E5 provides exhaustive finite standby read/write/readback coverage. E6 uses representative low, interior, and high numeric levels plus every discrete mode that E5 reports as usable. This separates control-interface coverage from the longer response experiment and avoids treating source-table limits as installed-hardware acceptance.

## Offline analysis

The analyzer emits numeric CSV and JSON only. It does not draw plots. Its `figure_data` directory follows `schemas/figure_data_contract.json`, so the figure worker can import the tables into Typst/CeTZ/Lilaq. Run it against the existing smoke capture as follows.

```sh
python3 experiments/paper1/scripts/analyze_capture.py \
  --session /Volumes/MacApplications/Radar_papers/macos-bringup/logs/tx-test-20261014T042023Z \
  --raw /Volumes/MacApplications/Radar_papers/macos-bringup/logs/tx-test-20261014T042023Z/echo-udp-payloads.jsonl \
  --rotations /Volumes/MacApplications/Radar_papers/data/live/macos-recordings/20261014_042023 \
  --out /Volumes/MacApplications/Radar_papers/macos-bringup/logs/tx-test-20261014T042023Z/paper1-analysis
```

The smoke capture currently yields 1,024 raw UDP payloads, 10,375 replayed spokes, and three CSV rotation files. The analyzer reports one truncated sweep in the raw fixture and separates spoke-start truncation from payload truncation. It follows the Rust decoder's encoding-2 first-sweep seed rule. These values are integrity evidence for the tool path, not a claim of full-scan completeness or calibrated physical performance.

The analyzer writes `figure_data/control_characterization.csv` and a raw JSON companion from the E5 manifest. It also writes `figure_data/raw_replay_summary.csv` with explicit truncation fields. These numeric files are the data contract for the Typst/CeTZ/Lilaq figure sources.

When analysis runs on a compute server, the analyzer reads the manifest's declared rotation files instead of globbing unrelated CSVs. Map the Mac's recorded absolute path to its server mirror with `--path-map`, for example `--path-map /Volumes/MacApplications/Radar_papers/data=/mnt/12TB_Drive/dspire/atlantis-master/jvaught2/radar-papers-20261012/data`. Missing manifest-declared rotations stop analysis so a partial copy cannot silently produce a zero-file or incomplete result. Each output row retains both the original `csv_path` and the local `analysis_csv_path`.

Major analysis belongs on COMECH-2080 or another compute server. Keep only manifests, hashes, compact summaries, and selected review files on the MacApplications volume. The raw UDP and rotation files remain external-volume data.
