# NAVNETDLL Restructure and Safe Migration Plan

## Goals
- Reorganize the repository into a clear long-term structure for maintainability and future custom library development.
- Preserve a working run path after every small migration step.
- Keep sensitive/license files (`vendor.txt`, `target.dat`) in git because this is a private repository.
- Provide both newcomer and power-user entrypoints.

## Current Structure (as of this branch before final migration)

```text
/usr/lib/NAVNETDLL
├── README.md
├── CLAUDE.md
├── include/
│   ├── FECLIB/fectype.h
│   ├── nndll.h
│   └── lasterror.h
├── lib/
│   ├── libNAVNETDLL.so -> libNAVNETDLL.so.01.95.0000
│   └── libNAVNETDLL.so.01.95.0000
├── sample/
│   ├── src/
│   │   ├── Furuno_Sample.cpp
│   │   ├── web_server.cpp
│   │   ├── pattern_control.cpp
│   │   └── stb_image_write.*
│   ├── web/
│   │   ├── index.html
│   │   ├── radar.html
│   │   ├── settings.html
│   │   ├── status.html
│   │   ├── script.js
│   │   └── styles.css
│   ├── ml/
│   │   ├── yolo_inference_daemon.py
│   │   └── weights/yolo26_best.onnx
│   ├── vendor.txt
│   ├── target.dat
│   └── run.sh
└── collected_data/
    └── ... (compressed archives and historical data)
```

## Proposed Target Structure (with hashtag-style change notes)

```text
/usr/lib/NAVNETDLL
├── README.md
├── CLAUDE.md
├── run_demo.sh                        # one-command newcomer flow (build + run + guidance)
├── scripts/                           # power-user scripts callable from repo root
│   ├── build.sh                       # compile/link only
│   ├── run.sh                         # build + run
│   └── run_yolo.sh                    # yolo daemon only
├── furuno/                            # vendor SDK assets isolated from app code
│   ├── include/
│   │   ├── nndll.h                    # edited include path for fectype.h
│   │   ├── lasterror.h
│   │   └── fectype.h                  # moved out of FECLIB/
│   └── lib/
│       ├── libNAVNETDLL.so            # canonical build/link target
│       └── libNAVNETDLL.so.01.95.0000 # SONAME compatibility link for runtime loader
├── main/                              # runtime application bundle (replaces sample/)
│   ├── src/
│   │   ├── main.cpp                   # renamed from Furuno_Sample.cpp
│   │   ├── web_server.cpp
│   │   ├── web_server.h
│   │   ├── pattern_control.cpp
│   │   ├── stb_image_write.cpp
│   │   └── stb_image_write.h
│   ├── web/
│   │   ├── index.html
│   │   ├── radar.html
│   │   ├── settings.html
│   │   ├── status.html
│   │   ├── script.js
│   │   └── styles.css
│   ├── ml/
│   │   ├── radar_yolo_daemon.py       # renamed from yolo_inference_daemon.py
│   │   └── weights/yolo26_best.onnx
│   ├── runtime/
│   │   ├── license/
│   │   │   ├── vendor.txt             # moved from sample/
│   │   │   ├── target.dat             # moved from sample/
│   │   │   └── Sensor_list.txt        # moved from repo root
│   │   └── output/                    # all generated runtime output under one tree
│   │       ├── raw/
│   │       └── images/
│   ├── build/                         # build artifacts
│   └── bin/                           # executable output
├── sample/
│   └── run.sh                         # compatibility wrapper forwarding to scripts/run.sh
└── data_compressed/                   # flat folder for compressed archive artifacts only
```

## Detailed Migration Sequence (smallest viable changes)

### Step 0: Branching and baseline
- Create dedicated working branch for restructure changes.
- Preserve the old runnable command path while introducing new paths.
- Baseline run check before major moves.

Verification:
- `git status --short` clean.
- Existing run command still available.

### Step 1: Least-damaging data-only move
- Move compressed archives into `data_compressed/` as a flat list.
- No source, include, or runtime path changes.

Verification:
- Confirm compressed files are present in `data_compressed/`.
- App run path unchanged.

### Step 2: Introduce newcomer + power-user scripts at root
- Add `run_demo.sh` (newcomer wrapper).
- Add `scripts/build.sh`, `scripts/run.sh`, `scripts/run_yolo.sh`.
- Keep `sample/run.sh` as compatibility wrapper.

Verification:
- `./run_demo.sh` starts build+run flow.
- `./scripts/build.sh` builds binary.
- `./scripts/run.sh` runs app.
- `sample/run.sh` still runs by forwarding.

### Step 3: Move application code from `sample/` to `main/`
- Move tracked source/web/ml/license assets to `main/`.
- Rename `Furuno_Sample.cpp` -> `main.cpp`.
- Rename `yolo_inference_daemon.py` -> `radar_yolo_daemon.py`.

Verification:
- Build script compiles from `main/src`.
- Run script executes binary successfully.
- Web endpoints still serve static assets.

### Step 4: Normalize runtime data locations
- Switch runtime outputs from `raw/` + `images/` to `main/runtime/output/raw` + `main/runtime/output/images`.
- Update code paths in `main.cpp`, `web_server.cpp`, and `pattern_control.cpp`.
- Update YOLO daemon defaults to new output paths.

Verification:
- CSV/PNG output appears under `main/runtime/output/...`.
- `/newest_png`, `/newest_trail`, `/newest_detection` still work.
- Pattern archival targets new output directories.

### Step 5: Move license files into `main/runtime/license`
- Place `vendor.txt`, `target.dat`, `Sensor_list.txt` together.
- Update run scripts to pass new paths.
- Keep fallback lookup in code for old `Sensor_list.txt` during transition.

Verification:
- License load succeeds at startup.
- Sensor list check still prints expected sensors.

### Step 6: Move SDK assets under `furuno/`
- Move `include/` and `lib/` to `furuno/include` and `furuno/lib`.
- Update build flags (`-I`, `-L`) and `LD_LIBRARY_PATH` in scripts.

Verification:
- Build and link succeed using `furuno/*` paths.
- Runtime library loads correctly.

### Step 7: Remove FECLIB nesting
- Move `fectype.h` to `furuno/include/fectype.h`.
- Update `nndll.h` include from `FECLIB/fectype.h` to `fectype.h`.
- Remove empty FECLIB directory.

Verification:
- Clean compile with updated header include chain.

### Step 8: Library filename normalization
- Keep canonical binary as `furuno/lib/libNAVNETDLL.so`.
- Keep SONAME compatibility path `furuno/lib/libNAVNETDLL.so.01.95.0000` as a symlink.
- Update scripts/docs to treat `libNAVNETDLL.so` as canonical while preserving runtime compatibility.

Verification:
- Build links with `-lNAVNETDLL`.
- Runtime loads library via `LD_LIBRARY_PATH`.

### Step 9: Final cleanup + docs alignment
- Update README commands to root-level scripts.
- Keep `sample/run.sh` wrapper until explicitly removed.
- Confirm no stale paths to `sample/`, `include/`, `lib/`, `raw/`, or `images/` remain.

Verification:
- `./run_demo.sh` works.
- `./scripts/build.sh`, `./scripts/run.sh`, and `./scripts/run_yolo.sh` work.
- `sample/run.sh` wrapper still works.

## Regression Checklist (run after each structural step)
- Build completes with zero link errors.
- App initializes NAVNET module without path-related failures.
- License files are found and parsed.
- Web server serves HTML/CSS/JS.
- Image endpoints return latest generated images.
- Pattern run still archives outputs.

## Rollback Strategy
- Keep each migration in small commits.
- If a step fails, revert only that commit.
- Do not combine path changes and behavior changes in one commit unless required.
