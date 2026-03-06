# NAVNETDLL

Private Furuno NAVNET integration repository.

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

## Runtime Output

Generated output is written under:

```text
main/runtime/output/raw/csv
main/runtime/output/images/unprocessed
main/runtime/output/images/echo_trail
main/runtime/output/images/yolo_detect
```
