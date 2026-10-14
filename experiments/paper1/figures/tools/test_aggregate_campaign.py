import unittest

from adapt_toolkit import elapsed_ms
from aggregate_campaign import family_for, is_reference_phase, related_reference


class CampaignFamilyTests(unittest.TestCase):
    def setUp(self) -> None:
        self.reference_settings = {
            "gain_auto": False,
            "gain_value": 64,
            "gain_auto_value": 80,
            "doppler": 0,
        }
        self.reference = {
            "phase_id": "response_reference",
            "source_phase_id": "response_reference",
            "requested_settings": self.reference_settings,
        }

    def test_manifest_source_identifies_reference_even_when_settings_match(self) -> None:
        same_settings_variant = {
            "phase_id": "gain_value_64",
            "source_phase_id": "gain_value_64",
            "requested_settings": self.reference_settings.copy(),
        }
        self.assertTrue(is_reference_phase(self.reference))
        self.assertFalse(is_reference_phase(same_settings_variant))
        self.assertEqual(
            family_for(same_settings_variant, self.reference_settings),
            "gain_value",
        )

    def test_manual_and_automatic_gain_families_remain_distinct(self) -> None:
        manual = {
            "phase_id": "gain_value_75",
            "source_phase_id": "gain_value_75",
            "requested_settings": {**self.reference_settings, "gain_value": 75},
        }
        automatic = {
            "phase_id": "gain_auto_value_25",
            "source_phase_id": "gain_auto_value_25",
            "requested_settings": {
                **self.reference_settings,
                "gain_auto": True,
                "gain_auto_value": 25,
            },
        }
        self.assertEqual(family_for(manual, self.reference_settings), "gain_value")
        self.assertEqual(family_for(automatic, self.reference_settings), "gain_auto_value")

    def test_manifest_selected_family_overrides_derived_family(self) -> None:
        doppler = {
            "phase_id": "doppler_0",
            "source_phase_id": "doppler_0",
            "requested_settings": self.reference_settings.copy(),
        }
        self.assertEqual(family_for(doppler, self.reference_settings, "doppler"), "doppler")

    def test_related_reference_uses_explicit_source_prefix(self) -> None:
        variant = {
            "phase_id": "gain_value_64",
            "source_phase_id": "gain_value_64",
            "requested_settings": self.reference_settings.copy(),
        }
        phases = [self.reference, variant]
        settings = {0: self.reference_settings, 1: self.reference_settings.copy()}
        self.assertEqual(related_reference(1, phases, settings), (0, "previous"))

    def test_transition_latency_accepts_unix_millisecond_readback(self) -> None:
        self.assertEqual(elapsed_ms("unix_ms:1000", "unix_ms:2500"), 1500.0)


if __name__ == "__main__":
    unittest.main()
