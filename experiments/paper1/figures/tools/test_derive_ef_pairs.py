import unittest

from derive_ef_pairs import build_contract


class EFPairsTests(unittest.TestCase):
    def test_ef0_is_paired_with_the_recorded_variant_per_context(self) -> None:
        session = []
        metrics = []
        for phase_id, sha, zero, angles in (
            ("doppler_0_rain_manual_modes_0_1", "a", "0.90", "12"),
            ("doppler_1_rain_manual_modes_0_1", "b", "0.91", "13"),
            ("doppler_0_rain_auto_modes_0_2", "c", "0.92", "14"),
            ("doppler_2_rain_auto_modes_0_2", "d", "0.93", "15"),
        ):
            session.append(
                {
                    "phase_id": phase_id,
                    "csv_sha256": sha,
                    "zero_fraction": zero,
                    "unique_angle_count": angles,
                }
            )
            metrics.append({"csv_sha256": sha, "observed_code_count": "8"})
        contract = build_contract(session, metrics)
        self.assertEqual(contract["reference_rule"], "ef0-per-fixed-context")
        self.assertEqual(
            [(p["reference_label"], p["condition_label"]) for p in contract["pairs"]],
            [("EF0", "EF2"), ("EF0", "EF1")],
        )
        self.assertEqual(contract["pairs"][0]["reference"][0]["rotation_index"], 0)

    def test_missing_companion_is_rejected(self) -> None:
        session = [
            {
                "phase_id": "doppler_0_rain_manual_modes_0_1",
                "csv_sha256": "a",
                "zero_fraction": "0.90",
                "unique_angle_count": "12",
            }
        ]
        metrics = [{"csv_sha256": "a", "observed_code_count": "8"}]
        with self.assertRaises(ValueError):
            build_contract(session, metrics)


if __name__ == "__main__":
    unittest.main()
