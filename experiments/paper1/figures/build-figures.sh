#!/usr/bin/env bash
set -euo pipefail

figure_root=$(cd "$(dirname "$0")" && pwd)
output_dir=${FIGURE_OUTPUT_DIR:-"$figure_root/output"}
figure_data=${1:-}
bit_audit=${2:-}
campaign_manifest=${3:-}
campaign_metrics=${4:-}
characterization=${5:-}
ef_session_summary=${6:-}
ef_rotation_metrics=${7:-}
family_coverage=${8:-}

cd "$figure_root"
if [[ -n "$campaign_manifest" || -n "$campaign_metrics" || -n "$characterization" ]]; then
  campaign_args=(--out data)
  if [[ -n "$campaign_manifest" ]]; then
    campaign_args+=(--manifest "$campaign_manifest")
  fi
  if [[ -n "$campaign_metrics" ]]; then
    campaign_args+=(--rotation-metrics "$campaign_metrics")
  fi
  if [[ -n "$characterization" ]]; then
    campaign_args+=(--recover-characterization "$characterization")
  fi
  python3 tools/aggregate_campaign.py "${campaign_args[@]}"
fi

if [[ -n "$figure_data" || -n "$bit_audit" || -n "$campaign_manifest" || -n "$campaign_metrics" || -n "$characterization" ]]; then
  adapter_args=(--figure-data "${figure_data:-data}" --out data)
  if [[ -n "$campaign_manifest" || -n "$campaign_metrics" || -n "$characterization" ]]; then
    adapter_args+=(--campaign-data data)
  fi
  if [[ -n "$characterization" ]]; then
    adapter_args+=(--characterization "$characterization")
  fi
  if [[ -n "$bit_audit" ]]; then
    adapter_args+=(--bit-audit "$bit_audit")
  fi
  python3 tools/adapt_toolkit.py "${adapter_args[@]}"
fi

if [[ -n "$ef_session_summary" || -n "$ef_rotation_metrics" ]]; then
  if [[ -z "$ef_session_summary" || -z "$ef_rotation_metrics" ]]; then
    echo "both EF session summary and rotation metrics are required" >&2
    exit 2
  fi
  python3 tools/derive_ef_pairs.py \
    --session-summary "$ef_session_summary" \
    --rotation-metrics "$ef_rotation_metrics" \
    --out data/ef_paired_response.json
fi

if [[ -n "$family_coverage" ]]; then
  cp "$family_coverage" data/campaign_family_coverage.json
fi

mkdir -p "$output_dir"
sources=(
  fig_capture_contract
  fig_target_geometry
  fig_live_receive
  fig_live_codes_support
  fig_stationary_repeatability
  fig_stationary_repeatability_e1
  fig_setting_transition
  fig_settings_coverage
  fig_setting_response
  fig_bit_planes
  fig_ef_paired_response
  fig_campaign_family_coverage
  fig_range_bearing_residuals
)
for source in "${sources[@]}"; do
  typst compile "$source.typ" "$output_dir/$source.pdf"
  pdftoppm -png -r 180 -singlefile "$output_dir/$source.pdf" "$output_dir/$source"
done

# Campaign response data is rendered once per control family. Each companion
# figure contains at most the selected condition and the matched reference,
# keeping legends compact when a campaign has many control families.
while IFS=$'\t' read -r family slug; do
  [[ -n "$family" ]] || continue
  output="$output_dir/fig_setting_response_family_${slug}"
  typst compile --input "family=$family" fig_setting_response_family.typ "$output.pdf"
  pdftoppm -png -r 180 -singlefile "$output.pdf" "$output"
done < <(python3 tools/list_setting_families.py --data data/setting_response.json)
