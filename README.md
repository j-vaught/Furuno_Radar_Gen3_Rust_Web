# NAVNETDLL

Private Furuno NAVNET integration repository.

## Prerequisites

- Linux ARM (32/64-bit)
- `g++`
- `libboost-all-dev`

Install required packages:

```bash
sudo apt-get update
sudo apt-get install -y g++ libboost-all-dev
```

Optional (only needed for YOLO daemon):

```bash
sudo apt-get install -y python3 python3-pip
python3 -m pip install --upgrade pip
python3 -m pip install numpy pillow onnxruntime
```

Verify required setup:

```bash
./scripts/check_prereqs.sh
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
