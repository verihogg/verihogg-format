"""Store formatter snapshots and support changes."""

import hashlib
import json
import re
import subprocess
from collections import defaultdict


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, sort_keys=True,
                               separators=(",", ":")) + "\n", encoding="utf-8")


def config_id(config):
    return hashlib.sha256(json.dumps(config, sort_keys=True).encode()).hexdigest()


def make_snapshot(tests, sha, subject, date, config):
    constructs = defaultdict(set)
    passed = set()
    verified = set()
    for test in tests:
        for key in test.keys:
            category, _, name = key.partition(":")
            if category == "construct":
                constructs[name].add(test.logpath)
        if test.status in {"passed", "warning"} and test.logfile is not None and not test.unparsed:
            # The runner verifies the syntax tree even when other warnings remain.
            verified.add(test.logpath)
            if test.status == "passed":
                passed.add(test.logpath)

    return {
        "sha": sha, "subject": subject, "date": date,
        "config": config, "config_id": config_id(config),
        "constructs": {name: sorted(witnesses) for name, witnesses in constructs.items()},
        "passed": sorted(passed), "verified": sorted(verified),
    }


def compare(before, after):
    def resolved(old, new, checked):
        verified = set(checked)
        return sorted(
            name for name, witnesses in old["constructs"].items()
            if name not in new["constructs"] and verified.issuperset(witnesses)
        )

    return (
        resolved(before, after, after.get("verified", after["passed"])),
        resolved(after, before, before["passed"]),
    )


def snapshot_path(archive, run):
    sha, config_id = run["sha"], run["config_id"]
    if not re.fullmatch(r"[0-9a-f]{40}", sha) or not re.fullmatch(r"[0-9a-f]{64}", config_id):
        raise ValueError("Invalid snapshot identity")
    return archive / "snapshots" / sha / (config_id + ".json")


def load_history(archive):
    path = archive / "history.json"
    if not path.exists():
        return {"version": 1, "runs": []}
    history = read_json(path)
    if history["version"] != 1:
        raise ValueError(f"Unsupported history version in {path}")
    return history


def read_history(archive):
    return load_history(archive)["runs"]


def run_entry(current, previous, distance):
    comparable = previous is not None and previous["config_id"] == current["config_id"]
    added, lost = compare(previous, current) if comparable else ([], [])
    entry = {
        **{key: current[key] for key in ("sha", "date", "subject", "config_id")},
        "base_sha": previous["sha"] if previous else None,
        "commit_count": distance, "baseline": not comparable, "added": added, "lost": lost,
    }
    if comparable:
        before = set(previous["constructs"])
        after = set(current["constructs"])
        entry["diagnostics_removed"] = sorted(before - after - set(added))
        entry["diagnostics_added"] = sorted(after - before - set(lost))
    return entry


def record(archive, current, distance):
    history = load_history(archive)
    runs = history["runs"]
    path = snapshot_path(archive, current)
    if path.exists():
        return False

    previous = read_json(snapshot_path(archive, runs[-1])) if runs else None
    runs.append(run_entry(current, previous, distance))
    write_json(path, current)
    write_json(archive / "history.json", history)
    return True


def backfill(archive, commits, snapshots):
    if archive.exists() and any(archive.iterdir()):
        raise ValueError(f"Backfill output must be empty: {archive}")

    history = {"version": 1, "runs": [], "backfill": []}
    previous = None
    previous_index = None
    for index, commit in enumerate(commits):
        entry = dict(commit)
        history["backfill"].append(entry)
        sha = commit["sha"]
        current = snapshots.get(sha)
        if current is not None and not commit.get("before_diagnostics"):
            distance = index - previous_index if previous is not None else None
            history["runs"].append(run_entry(current, previous, distance))
            write_json(snapshot_path(archive, current), current)
            previous, previous_index = current, index
        if "merged" in commit:
            entry["merged"] = []
            for merged in commit["merged"]:
                side = snapshots.get(merged["sha"])
                if side is not None:
                    parent = snapshots.get(merged["parent"])
                    merged = {**merged, "run": run_entry(side, parent, 1)}
                    write_json(snapshot_path(archive, side), side)
                entry["merged"].append(merged)
    write_json(archive / "history.json", history)


def commit_distance(formatter, base, sha):
    if base == sha:
        return 0
    # Both tested commits came from main. A stale rerun must not append a backwards comparison.
    for distance, commit in enumerate(subprocess.check_output(
        ["git", "-C", formatter, "rev-list", "--first-parent", sha], text=True
    ).splitlines()):
        if commit == base:
            return distance
    raise ValueError(f"Archived commit {base} is outside the first-parent history of {sha}")
