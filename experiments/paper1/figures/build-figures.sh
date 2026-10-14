#!/usr/bin/env bash
set -euo pipefail

figure_root=$(cd "$(dirname "$0")" && pwd)
output_dir=${FIGURE_OUTPUT_DIR:-"$figure_root/output"}
figure_data=${1:-}
bit_audit=${2:-}

cd "$figure_root"
if [[ -n "$figure_data" || -n "$bit_audit" ]]; then
  adapter_args=(--figure-data "${figure_data:-/dev/null}" --out data)
  if [[ -n "$bit_audit" ]]; then
    adapter_args+=(--bit-audit "$bit_audit")
  fi
  python3 tools/adapt_toolkit.py "${adapter_args[@]}"
fi

mkdir -p "$output_dir"
sources=(
  fig_capture_contract
  fig_target_geometry
  fig_live_receive
  fig_live_codes_support
  fig_stationary_repeatability
  fig_setting_transition
  fig_settings_coverage
  fig_setting_response
  fig_bit_planes
  fig_range_bearing_residuals
)
for source in "${sources[@]}"; do
  typst compile "$source.typ" "$output_dir/$source.pdf"
  pdftoppm -png -r 180 -singlefile "$output_dir/$source.pdf" "$output_dir/$source"
done
