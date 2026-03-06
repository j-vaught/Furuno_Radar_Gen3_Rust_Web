#!/usr/bin/env python3
"""Live YOLO inference daemon for radar trail images.

Watches trail_rgb/ for new PNGs, runs YOLO detection, draws bounding boxes,
and writes annotated images to detections/ for the web UI to serve.

Usage:
    python3 yolo_inference_daemon.py [--model weights/yolo26_best.onnx] [--conf 0.25]
"""
import argparse
import time
from pathlib import Path

import numpy as np
import onnxruntime as ort
from PIL import Image, ImageDraw


YOLO_SIZE = 640
IMAGE_SIZE = 1735  # native PPI size
COLOURS = [
    (255, 0, 0), (0, 255, 0), (0, 128, 255), (255, 255, 0),
    (255, 0, 255), (0, 255, 255), (128, 255, 0), (255, 128, 0),
]


def build_session(model_path: str) -> ort.InferenceSession:
    so = ort.SessionOptions()
    so.intra_op_num_threads = 2
    so.inter_op_num_threads = 1
    so.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
    return ort.InferenceSession(model_path, sess_options=so, providers=["CPUExecutionProvider"])


def preprocess(img: Image.Image) -> np.ndarray:
    resized = img.resize((YOLO_SIZE, YOLO_SIZE), Image.Resampling.BILINEAR)
    arr = np.array(resized, dtype=np.float32) / 255.0
    return np.transpose(arr, (2, 0, 1))[None, ...]  # [1, 3, 640, 640]


def postprocess(output: np.ndarray, conf_thresh: float, orig_size: int) -> list:
    """Parse YOLO output [1, 300, 6] -> list of (x1, y1, x2, y2, conf, class_id)."""
    preds = output[0]  # [300, 6]
    scale = orig_size / YOLO_SIZE
    detections = []
    for det in preds:
        # output format: x1, y1, x2, y2, conf, class_id
        x1, y1, x2, y2, conf, cls = det
        if conf < conf_thresh:
            continue
        detections.append((
            int(x1 * scale), int(y1 * scale),
            int(x2 * scale), int(y2 * scale),
            float(conf), int(cls)
        ))
    return detections


def draw_detections(img: Image.Image, detections: list) -> Image.Image:
    draw = ImageDraw.Draw(img)
    for x1, y1, x2, y2, conf, cls in detections:
        colour = COLOURS[cls % len(COLOURS)]
        draw.rectangle([x1, y1, x2, y2], outline=colour, width=3)
        label = f"c{cls} {conf:.2f}"
        draw.text((x1, max(0, y1 - 12)), label, fill=colour)
    return img


def find_newest(directory: Path) -> Path | None:
    pngs = list(directory.glob("*.png"))
    if not pngs:
        return None
    return max(pngs, key=lambda p: p.stat().st_mtime)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", default="weights/yolo26_best.onnx")
    parser.add_argument("--trail-dir", default="trail_rgb")
    parser.add_argument("--output-dir", default="detections")
    parser.add_argument("--conf", type=float, default=0.25)
    parser.add_argument("--poll-interval", type=float, default=0.3,
                        help="Seconds between checks for new frames")
    args = parser.parse_args()

    trail_dir = Path(args.trail_dir)
    out_dir = Path(args.output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    print(f"Loading model: {args.model}")
    session = build_session(args.model)
    input_name = session.get_inputs()[0].name
    print(f"Model loaded. Watching {trail_dir}/ (poll every {args.poll_interval}s)")

    last_processed = None

    while True:
        newest = find_newest(trail_dir)
        if newest is None or newest == last_processed:
            time.sleep(args.poll_interval)
            continue

        try:
            t0 = time.perf_counter()
            # Small delay to let the C++ writer finish flushing
            time.sleep(0.05)
            img = Image.open(newest).convert("RGB")
            inp = preprocess(img)
            t1 = time.perf_counter()

            outputs = session.run(None, {input_name: inp})
            t2 = time.perf_counter()

            detections = postprocess(outputs[0], args.conf, img.width)
            annotated = draw_detections(img.copy(), detections)

            # Atomic write: save to temp file then rename to avoid torn reads
            tmp_path = out_dir / "latest.tmp.png"
            out_path = out_dir / "latest.png"
            annotated.save(tmp_path)
            tmp_path.rename(out_path)

            # Also save per-frame copy for review
            annotated.save(out_dir / newest.name)

            t3 = time.perf_counter()
            n = len(detections)
            print(f"{newest.name}: {n} det, "
                  f"resize {(t1-t0)*1000:.0f}ms, "
                  f"infer {(t2-t1)*1000:.0f}ms, "
                  f"total {(t3-t0)*1000:.0f}ms")

            last_processed = newest

        except Exception as e:
            print(f"Error processing {newest}: {e}")
            time.sleep(0.5)
            continue

        time.sleep(args.poll_interval)


if __name__ == "__main__":
    main()
