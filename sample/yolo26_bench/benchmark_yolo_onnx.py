#!/usr/bin/env python3
import argparse
import json
import platform
import statistics
import time
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

import numpy as np
import onnxruntime as ort
from PIL import Image


DEFAULT_MODEL_URLS = [
    "https://github.com/ultralytics/assets/releases/download/v8.3.0/yolo26n.onnx",
]


def download_model_if_missing(model_path: Path, model_urls: list[str], allow_download: bool) -> str:
    if model_path.exists() and model_path.stat().st_size > 0:
        return f"Using existing model: {model_path}"

    if not allow_download:
        raise RuntimeError(
            f"Model not found at {model_path}. Provide your exported YOLO26 ONNX with --model, "
            "or pass --download if you explicitly want URL download."
        )

    model_path.parent.mkdir(parents=True, exist_ok=True)
    last_error = None
    for url in model_urls:
        try:
            print(f"Trying model download: {url}")
            urllib.request.urlretrieve(url, model_path)
            if model_path.exists() and model_path.stat().st_size > 0:
                return f"Downloaded model from {url} -> {model_path}"
        except Exception as exc:
            last_error = exc
            print(f"Failed: {url} ({exc})")
            continue
    raise RuntimeError(f"Could not download any model. Last error: {last_error}")


def build_session(model_path: Path, threads: int, providers: list[str]) -> ort.InferenceSession:
    so = ort.SessionOptions()
    so.intra_op_num_threads = threads
    so.inter_op_num_threads = 1
    so.execution_mode = ort.ExecutionMode.ORT_SEQUENTIAL
    so.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
    return ort.InferenceSession(str(model_path), sess_options=so, providers=providers)


def detect_providers(mode: str) -> list[str]:
    available = ort.get_available_providers()
    if mode == "cpu":
        return ["CPUExecutionProvider"]
    if mode == "gpu":
        preferred = [p for p in ("TensorrtExecutionProvider", "CUDAExecutionProvider") if p in available]
        if not preferred:
            raise RuntimeError(f"Requested --mode gpu but no GPU provider found. Available: {available}")
        return preferred + ["CPUExecutionProvider"]
    # auto
    preferred = [p for p in ("TensorrtExecutionProvider", "CUDAExecutionProvider") if p in available]
    return preferred + ["CPUExecutionProvider"]


def percentile_ms(values: list[float], q: float) -> float:
    if not values:
        return 0.0
    idx = int(round((q / 100.0) * (len(values) - 1)))
    vals = sorted(values)
    return vals[max(0, min(idx, len(vals) - 1))]


def get_cpu_model() -> str:
    cpuinfo = Path("/proc/cpuinfo")
    if not cpuinfo.exists():
        return platform.processor() or "unknown"
    for line in cpuinfo.read_text(encoding="utf-8", errors="ignore").splitlines():
        if line.lower().startswith("model name"):
            return line.split(":", 1)[1].strip()
    return platform.processor() or "unknown"


def collect_images(image_dir: Path) -> list[Path]:
    exts = ("*.jpg", "*.jpeg", "*.png", "*.bmp", "*.tif", "*.tiff", "*.webp")
    files: list[Path] = []
    for ext in exts:
        files.extend(sorted(image_dir.rglob(ext)))
    return files


def main() -> None:
    parser = argparse.ArgumentParser(description="Benchmark YOLO26 ONNX with resize + inference timing.")
    parser.add_argument("--model", default="models/yolo26n.onnx", help="Local YOLO26 ONNX model path.")
    parser.add_argument("--model-url", action="append", default=[], help="Model URL (repeatable).")
    parser.add_argument("--download", action="store_true", help="Allow downloading model if missing.")
    parser.add_argument(
        "--allow-non-yolo26-name",
        action="store_true",
        help="Disable filename guard that requires 'yolo26' in model filename.",
    )
    parser.add_argument("--size", type=int, default=640, help="Square YOLO input size.")
    parser.add_argument("--source-size", type=int, default=1735, help="Square synthetic source image size.")
    parser.add_argument("--image-dir", default="", help="Optional directory of real images to benchmark.")
    parser.add_argument("--warmup", type=int, default=5, help="Warmup iterations.")
    parser.add_argument("--runs", type=int, default=30, help="Measured iterations.")
    parser.add_argument("--threads", type=int, default=1, help="intra_op threads (CPU threads).")
    parser.add_argument("--mode", choices=["auto", "cpu", "gpu"], default="auto", help="Provider mode.")
    args = parser.parse_args()

    model_path = Path(args.model)
    if "yolo26" not in model_path.name.lower() and not args.allow_non_yolo26_name:
        raise RuntimeError(
            f"Refusing non-YOLO26-looking filename: {model_path.name}. "
            "Rename/export as yolo26*.onnx or pass --allow-non-yolo26-name."
        )

    urls = args.model_url if args.model_url else DEFAULT_MODEL_URLS
    msg = download_model_if_missing(model_path, urls, allow_download=args.download)
    print(msg)

    available = ort.get_available_providers()
    providers = detect_providers(args.mode)
    print(f"ONNX Runtime available providers: {available}")
    print(f"ONNX Runtime selected providers: {providers}")

    session = build_session(model_path, threads=args.threads, providers=providers)
    input_meta = session.get_inputs()[0]
    input_name = input_meta.name

    # Build source image pool. If --image-dir is set, preload images to avoid disk I/O noise in timing.
    sources: list[np.ndarray] = []
    if args.image_dir:
        img_dir = Path(args.image_dir)
        if not img_dir.exists():
            raise RuntimeError(f"--image-dir does not exist: {img_dir}")
        image_files = collect_images(img_dir)
        if not image_files:
            raise RuntimeError(f"No images found in --image-dir: {img_dir}")
        for fp in image_files:
            arr = np.array(Image.open(fp).convert("RGB"), dtype=np.uint8)
            sources.append(arr)
        print(f"Loaded {len(sources)} images from {img_dir}")
    else:
        # Synthetic single source, repeatedly resized.
        sources = [np.random.randint(0, 256, (args.source_size, args.source_size, 3), dtype=np.uint8)]
        print(f"Generated synthetic source image: {args.source_size}x{args.source_size}")

    def preprocess_to_nchw(image_hwc: np.ndarray) -> tuple[np.ndarray, float]:
        t0 = time.perf_counter()
        resized = np.array(
            Image.fromarray(image_hwc, mode="RGB").resize((args.size, args.size), Image.Resampling.BILINEAR),
            dtype=np.uint8,
        )
        t1 = time.perf_counter()
        inp = resized.astype(np.float32) / 255.0
        inp = np.transpose(inp, (2, 0, 1))[None, ...]
        return inp, (t1 - t0) * 1000.0

    for i in range(args.warmup):
        warmup_inp, _ = preprocess_to_nchw(sources[i % len(sources)])
        session.run(None, {input_name: warmup_inp})

    resize_ms_samples = []
    inference_ms_samples = []
    end_to_end_ms_samples = []
    for i in range(args.runs):
        t0 = time.perf_counter()
        inp, resize_ms = preprocess_to_nchw(sources[i % len(sources)])
        t1 = time.perf_counter()
        session.run(None, {input_name: inp})
        t2 = time.perf_counter()

        resize_ms_samples.append(resize_ms)
        inference_ms_samples.append((t2 - t1) * 1000.0)
        end_to_end_ms_samples.append((t2 - t0) * 1000.0)

    resize_avg = statistics.mean(resize_ms_samples)
    infer_avg = statistics.mean(inference_ms_samples)
    e2e_avg = statistics.mean(end_to_end_ms_samples)
    fps_infer = 1000.0 / infer_avg if infer_avg > 0 else 0.0
    fps_e2e = 1000.0 / e2e_avg if e2e_avg > 0 else 0.0

    report = {
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "device": {
            "platform": platform.platform(),
            "machine": platform.machine(),
            "cpu_model": get_cpu_model(),
        },
        "model_path": str(model_path),
        "image_source": "directory" if args.image_dir else "synthetic",
        "image_dir": args.image_dir if args.image_dir else None,
        "source_image_count": len(sources),
        "source_image_size": [args.source_size, args.source_size],
        "image_size": [args.size, args.size],
        "warmup_runs": args.warmup,
        "measured_runs": args.runs,
        "threads": args.threads,
        "mode": args.mode,
        "available_providers": available,
        "selected_providers": providers,
        "latency_ms": {
            "downscale_1735_to_640": {
                "avg": round(resize_avg, 3),
                "p50": round(percentile_ms(resize_ms_samples, 50), 3),
                "p90": round(percentile_ms(resize_ms_samples, 90), 3),
                "p95": round(percentile_ms(resize_ms_samples, 95), 3),
                "min": round(min(resize_ms_samples), 3),
                "max": round(max(resize_ms_samples), 3),
            },
            "inference_only": {
                "avg": round(infer_avg, 3),
                "p50": round(percentile_ms(inference_ms_samples, 50), 3),
                "p90": round(percentile_ms(inference_ms_samples, 90), 3),
                "p95": round(percentile_ms(inference_ms_samples, 95), 3),
                "min": round(min(inference_ms_samples), 3),
                "max": round(max(inference_ms_samples), 3),
            },
            "end_to_end_downscale_plus_inference": {
                "avg": round(e2e_avg, 3),
                "p50": round(percentile_ms(end_to_end_ms_samples, 50), 3),
                "p90": round(percentile_ms(end_to_end_ms_samples, 90), 3),
                "p95": round(percentile_ms(end_to_end_ms_samples, 95), 3),
                "min": round(min(end_to_end_ms_samples), 3),
                "max": round(max(end_to_end_ms_samples), 3),
            },
        },
        "fps": {
            "inference_only": round(fps_infer, 3),
            "end_to_end_downscale_plus_inference": round(fps_e2e, 3),
        },
    }

    print(json.dumps(report, indent=2))

    out_dir = Path("results")
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / f"benchmark_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
    out_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"Saved report: {out_path}")


if __name__ == "__main__":
    main()
