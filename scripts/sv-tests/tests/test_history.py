import hashlib
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

from history import compare, make_snapshot, read_history, record


def snapshot(sha, constructs, passed, config="suite-a", subject="Update formatter"):
    return {
        "sha": sha * 40, "config_id": hashlib.sha256(config.encode()).hexdigest(),
        "date": "2026-11-25T10:00:00Z",
        "subject": subject, "constructs": constructs, "passed": passed,
    }


class HistoryTest(unittest.TestCase):
    def test_snapshot_path_rejects_imported_paths(self):
        current = snapshot("a", {}, [])
        current["config_id"] = "nested/invalid"
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory)
            with self.assertRaisesRegex(ValueError, "Invalid snapshot identity"):
                record(archive, current, None)
            self.assertEqual(list(archive.iterdir()), [])

    def test_snapshot_uses_construct_diagnostics_and_readable_passing_tests(self):
        def test_case(logpath, status, keys, logfile=True):
            return SimpleNamespace(logpath=logpath, status=status, keys=keys,
                                   logfile=logfile, unparsed=False)

        result = make_snapshot([
            test_case("logs/a", "warning", ["construct:Module", "token:AndKeyword"]),
            test_case("logs/b", "passed", []),
            test_case("logs/c", "passed", [], logfile=None),
        ], "a" * 40, "Update formatter", "2026-11-25T10:00:00Z", {"suite": "a"})
        self.assertEqual(result["constructs"], {"Module": ["logs/a"]})
        self.assertEqual(result["passed"], ["logs/b"])

    def test_only_confirms_changes_with_passing_witnesses(self):
        before = snapshot("a", {"Module": ["a", "b"], "Missing": ["x"]}, ["c"])
        after = snapshot("b", {"Port": ["c"], "Added test": ["d"]}, ["a", "b"])
        self.assertEqual(compare(before, after), (["Module"], ["Port"]))
        after["constructs"]["Module"] = ["new"]
        self.assertEqual(compare(before, after), ([], ["Port"]))

    def test_accepts_checks_that_pass_with_other_known_warnings(self):
        before = snapshot("a", {"Import": ["a"], "Static": ["b"], "Missing": ["c"]}, [])
        tests = [
            SimpleNamespace(logpath="a", status="warning", keys=["construct:Other"],
                            logfile=True, unparsed=False),
            SimpleNamespace(logpath="b", status="warning", keys=[],
                            logfile=True, unparsed=True),
            SimpleNamespace(logpath="c", status="failed", keys=[],
                            logfile=True, unparsed=False),
        ]
        after = make_snapshot(tests, "b" * 40, "Update formatter",
                              "2026-11-25T10:00:00Z", {"suite": "a"})

        self.assertEqual(after["passed"], [])
        self.assertEqual(compare(before, after), (["Import"], []))

    def test_records_once_per_commit_and_config(self):
        first = snapshot("a", {"Module": ["a"]}, [])
        second = snapshot("b", {}, ["a"])
        new_config = snapshot("b", {"Port": ["b"]}, [], "suite-b")
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory)
            self.assertTrue(record(archive, first, None))
            self.assertTrue(record(archive, second, 2))
            self.assertFalse(record(archive, second, 0))
            self.assertTrue(record(archive, new_config, 0))
            runs = read_history(archive)
            self.assertEqual(len(runs), 3)
            self.assertEqual(runs[1]["added"], ["Module"])
            self.assertTrue(runs[2]["baseline"])
            self.assertEqual(len(list((archive / "snapshots").glob("*/*.json"))), 3)


if __name__ == "__main__":
    unittest.main()
