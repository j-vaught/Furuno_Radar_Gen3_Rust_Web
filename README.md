# NAVNETDLL

Private Furuno NAVNET integration repository.

## Repository Layout

```text
furuno/                  # Vendor SDK headers and shared library
main/                    # Application source, web UI, ML, runtime assets
scripts/                 # Power-user scripts
run_demo.sh              # One-command build + run
data_compressed/         # Compressed data archives
sample/run.sh            # Compatibility wrapper to scripts/run.sh
```

## Prerequisites

- Linux ARM (32/64-bit)
- `g++`
- `libboost-all-dev`

Install Boost:

```bash
sudo apt-get update
sudo apt-get install -y libboost-all-dev
```

## Quick Start (recommended)

From repo root:

```bash
./run_demo.sh
```

This builds and runs the app using:
- `main/runtime/license/vendor.txt`
- `main/runtime/license/target.dat`
- library at `furuno/lib/libNAVNETDLL.so`

Web UI: `http://localhost:8080`

## Power User Commands

Build only:

```bash
./scripts/build.sh
```

Build + run (optional args: `<ip> <host>`):

```bash
./scripts/run.sh
./scripts/run.sh 172.31.1.100 PC1
```

Run YOLO daemon:

```bash
./scripts/run_yolo.sh
```

Legacy compatibility wrapper:

```bash
./sample/run.sh
```

## Runtime Output

Generated output is written under:

```text
main/runtime/output/raw/csv
main/runtime/output/images/unprocessed
main/runtime/output/images/echo_trail
main/runtime/output/images/yolo_detect
```

## Notes

- This repository is private.
- `vendor.txt` and `target.dat` are intentionally tracked in git for this environment.
