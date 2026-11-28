import tempfile
import unittest
from pathlib import Path

from history import backfill, read_history, record
from history_page import render_history
from test_history import snapshot


class HistoryPageTest(unittest.TestCase):
    def test_latest_results_use_the_current_snapshot_even_without_recording_it(self):
        current = snapshot("b", {"Module": ["a", "b"], "Port": ["c"]}, [])
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory)
            page = render_history(archive, "verihogg/verihogg-format", current)
            self.assertIn("2 unsupported constructs", page)
            self.assertIn("History will appear after the first recorded run", page)

            record(archive, snapshot("a", {"Module": ["a"]}, []), None)
            page = render_history(archive, "verihogg/verihogg-format", current)
            self.assertLess(page.index("Latest results"), page.index("Update formatter"))
            self.assertIn("2 unsupported constructs", page)
            self.assertEqual(page.count('href="unsupported.html"'), 1)

    def test_backfill_distinguishes_pre_diagnostics_and_missing_runs(self):
        early = snapshot("a", {}, [])
        missing = snapshot("b", {}, [])
        measured = snapshot("c", {"Module": ["a"]}, [])
        new = snapshot("e", {}, ["a", "b"])
        merge = snapshot("f", {}, ["a", "b"], subject="Merge unchanged formatter")
        merged = snapshot("d", {"Port": ["b"]}, [], subject="Branch-only commit")
        commits = [
            {**early, "before_diagnostics": True},
            missing,
            measured,
            new,
            {**merge, "merged": [{**merged, "parent": missing["sha"]}]},
        ]
        measurements = {run["sha"]: run for run in (measured, new, merge, merged)}
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory)
            backfill(archive, commits, measurements)
            self.assertEqual([run["sha"] for run in read_history(archive)],
                             [measured["sha"], new["sha"], merge["sha"]])
            page = render_history(archive, "verihogg/verihogg-format", merge)
            self.assertIn("Before unsupported-construct diagnostics", page)
            self.assertIn("No measured results", page)
            self.assertNotIn("included by this merge", page)
            self.assertNotIn("Branch-only commit", page)
            self.assertRegex(page, r"1 tested commit with no confirmed support changes</summary>.*Merge unchanged formatter")
            self.assertIn("1 newly supported construct", page)

    def test_history_does_not_label_unverified_diagnostics_as_support(self):
        first = snapshot("a", {"Module": ["a"]}, [])
        second = snapshot("b", {"Port": ["b"]}, [])
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory)
            record(archive, first, None)
            record(archive, second, 1)
            page = render_history(archive, "verihogg/verihogg-format", second)
            self.assertIn("1 diagnostic no longer observed", page)
            self.assertIn("1 new diagnostic", page)
            self.assertEqual(page.count('<details class="diagnostic-change">'), 2)
            self.assertNotIn("newly supported", page)

    def test_records_once_per_commit_and_config_and_resets_baseline(self):
        first = snapshot("a", {"Module": ["a"]}, [])
        second = snapshot("b", {}, ["a"], subject="Support <Module>")
        third = snapshot("c", {}, ["a"])
        same_commit_new_suite = snapshot("c", {"Port": ["c"]}, [], "suite-b")
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory)
            self.assertTrue(record(archive, first, None))
            self.assertTrue(record(archive, second, 2))
            self.assertTrue(record(archive, third, 1))
            self.assertFalse(record(archive, third, 0))
            self.assertTrue(record(archive, same_commit_new_suite, 0))
            page = render_history(archive, "verihogg/verihogg-format", same_commit_new_suite)
            self.assertEqual(len(list((archive / "snapshots").glob("*/*.json"))), 4)
            self.assertIn("Support &lt;Module&gt;", page)
            self.assertIn("1 newly supported construct", page)
            self.assertRegex(
                page,
                r'<h3>.*Support &lt;Module&gt;.*</h3>.*'
                r'<ul class="support-change support-added" role="list" aria-label="1 newly supported construct">'
                r'<li><span aria-hidden="true">\+</span><code>Module</code></li></ul>',
            )
            self.assertIn("1 tested commit with no confirmed support changes", page)
            self.assertIn("2 commits since the previous run", page)
            self.assertIn("Test configuration changed", page)
            self.assertIn("1 unsupported construct. ", page)
            self.assertEqual(page.count('href="unsupported.html"'), 1)
