"""Regression checks for interpreting segmented resonance-angle records."""
import csv
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("tidal_study", Path(__file__).resolve().parents[1]/"tools/run-tidal-study.py")
study = importlib.util.module_from_spec(spec)
spec.loader.exec_module(study)


class ReportTests(unittest.TestCase):
    def reduce(self, phases, segments, times=None):
        rows = []
        for time, phase, segment in zip(times or range(len(phases)), phases, segments):
            row = {key: 0.1 for key in study.FIELDS}
            row.update(time=time, period_ratio=2+time/100, phi_azure_turns=phase,
                       phi_ember_turns=phase, segment_azure=segment, segment_ember=segment,
                       heat=0, mechanical_energy=-2, energy_budget_error=0,
                       angular_momentum_error=0, net_momentum=0, lag_phase_bound=0.1)
            rows.append(row)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/"run.csv"
            with path.open("w", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=rows[0])
                writer.writeheader()
                writer.writerows(rows)
            return study.reduce_csv(path,10)

    def test_phase_reset_is_not_circulation(self):
        report = self.reduce([0.1,0.3,-5,-4.5], [1,1,2,2])
        phase = report["windows"][0]["phi_azure"]
        self.assertIsNone(phase["span_turns"])
        self.assertIsNone(phase["net_turns"])
        self.assertEqual(phase["segments"],2)
        self.assertAlmostEqual(phase["largest_segment_span_turns"],0.5)

    def test_missing_phase_is_not_bounded_libration(self):
        phase = self.reduce([0.1,None,0.2], [1,1,2])["windows"][0]["phi_azure"]
        self.assertIsNone(phase["span_turns"])
        self.assertAlmostEqual(phase["valid_fraction"],2/3)

    def test_windows_keep_first_and_late_statistics_separate(self):
        windows = self.reduce([0,0.1,0.2,0.3,0.4], [1]*5, [0,5,10,15,20])["windows"]
        self.assertEqual(len(windows),2)
        self.assertEqual(windows[0]["samples"],3)
        self.assertAlmostEqual(windows[0]["period_ratio"]["mean"],2.05)
        self.assertAlmostEqual(windows[1]["period_ratio"]["mean"],2.175)

    def test_small_window_spans_do_not_hide_long_term_phase_drift(self):
        report = self.reduce([0,0.1,0.2,3.1,3.2], [1]*5, [0,5,10,15,20])
        self.assertLess(report["windows"][-1]["phi_azure"]["span_turns"],0.2)
        self.assertAlmostEqual(report["overall_phases"]["azure"]["span_turns"],3.2)


if __name__ == "__main__":
    unittest.main()
