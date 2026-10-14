import base64
import json
import tempfile
import unittest
from pathlib import Path

from scripts.doppler_audit import (
    SpokeRecord,
    _decode_sweep,
    compare_csv,
    remap_csv_paths,
    replay_raw,
)


def packet(encoding: int, sweep_len: int, sweeps: list[tuple[int, int, bytes]]) -> bytes:
    header = bytearray(16)
    header[9] = len(sweeps) << 1
    header[10] = sweep_len & 0xFF
    header[11] = ((sweep_len >> 8) & 0x07) | (encoding << 3)
    body = bytearray()
    for angle, heading, encoded in sweeps:
        body.extend(angle.to_bytes(2, "little"))
        body.extend(heading.to_bytes(2, "little"))
        body.extend(encoded)
        body.extend(b"\x00" * ((-len(encoded)) % 4))
    return bytes(header + body)


class DopplerAuditTests(unittest.TestCase):
    def test_remaps_capture_host_csv_prefix(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "capture-host" / "csv"
            target = root / "analysis-host" / "csv"
            source.mkdir(parents=True)
            target.mkdir(parents=True)
            requested = source / "rotation.csv"
            mapped = target / "rotation.csv"
            mapped.write_text("data", encoding="utf-8")
            self.assertEqual(
                remap_csv_paths([requested], [f"{source}={target}"]),
                [mapped],
            )

    def test_csv_alignment_prefers_a_contiguous_duplicate_signature_run(self) -> None:
        spokes = [
            SpokeRecord(0, index, 0, 0, 3, 1, [value], True, index, index)
            for index, value in enumerate([0, 4, 8, 0, 4, 12])
        ]
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "rotation.csv"
            path.write_text("Angle\tEchoValues\n0\t0\n0\t1\n0\t3\n", encoding="utf-8")
            result = compare_csv([path], spokes)[0]
        self.assertEqual(result["matched_row_count"], 3)
        self.assertEqual(result["first_match_replay_index"], 3)
        self.assertEqual(result["last_match_replay_index"], 5)
        self.assertTrue(result["match_indices_contiguous"])

    def test_encoding_zero_preserves_raw_values_and_shift_is_separate(self) -> None:
        decoded = _decode_sweep(bytes([0, 1, 2, 252]), 0, 4, [])
        self.assertEqual(decoded.values, [0, 1, 2, 252])
        self.assertEqual([value >> 2 for value in decoded.values], [0, 0, 0, 63])
        self.assertEqual([value & 3 for value in decoded.values], [0, 1, 2, 0])
        self.assertTrue(decoded.complete)

    def test_encoding_one_rle(self) -> None:
        decoded = _decode_sweep(bytes([12, 7, 0, 0]), 1, 4, [])
        self.assertEqual(decoded.values, [12, 12, 12, 12])
        self.assertEqual(decoded.repeat_count, 1)
        self.assertTrue(decoded.complete)

    def test_encoding_two_carries_previous_spoke(self) -> None:
        decoded = _decode_sweep(bytes([9, 0, 0, 0]), 2, 4, [4, 8, 12, 16])
        self.assertEqual(decoded.values, [4, 8, 12, 16])
        self.assertEqual(decoded.previous_copy_count, 1)

    def test_encoding_three_carries_previous_and_repeats(self) -> None:
        copy_previous = _decode_sweep(bytes([14, 0, 0, 0]), 3, 3, [4, 8, 12])
        self.assertEqual(copy_previous.values, [4, 8, 12])
        copy_then_repeat = _decode_sweep(bytes([18, 13, 0, 0]), 3, 7, [4, 8, 12, 16])
        self.assertEqual(copy_then_repeat.values, [4, 8, 12, 16, 16, 16, 16])

    def test_replay_keeps_previous_state_across_packets(self) -> None:
        first = packet(3, 4, [(100, 7, bytes([4, 8, 12, 16]))])
        second = packet(3, 4, [(104, 7, bytes([18]))])
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "raw.jsonl"
            path.write_text(
                "".join(
                    json.dumps(
                        {
                            "received_unix_ns": index,
                            "payload_b64": base64.b64encode(payload).decode(),
                        }
                    )
                    + "\n"
                    for index, payload in enumerate((first, second))
                ),
                encoding="utf-8",
            )
            state, _ = replay_raw(path)
        self.assertEqual(state.decoded_spoke_count, 2)
        self.assertEqual(state.spokes[-1].values, [4, 8, 12, 16])
        self.assertEqual(sum(state.low2.values()), 8)

    def test_replay_counts_all_packet_encodings(self) -> None:
        payloads = [
            packet(0, 4, [(1, 2, bytes([0, 4, 8, 12]))]),
            packet(1, 4, [(2, 3, bytes([4, 7]))]),
            packet(2, 4, [(3, 4, bytes([4, 8, 12, 16])), (4, 5, bytes([7]))]),
            packet(3, 4, [(5, 6, bytes([4, 8, 12, 16]))]),
        ]
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "raw.jsonl"
            path.write_text(
                "".join(
                    json.dumps(
                        {
                            "received_unix_ns": index,
                            "payload_b64": base64.b64encode(payload).decode(),
                        }
                    )
                    + "\n"
                    for index, payload in enumerate(payloads)
                ),
                encoding="utf-8",
            )
            state, _ = replay_raw(path)
        self.assertEqual(state.encodings, {0: 1, 1: 1, 2: 1, 3: 1})
        self.assertEqual(state.decoded_spoke_count, 5)


if __name__ == "__main__":
    unittest.main()
