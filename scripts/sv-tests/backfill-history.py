#!/usr/bin/env python3
"""Assemble an archive from historical formatter commits and collected snapshots."""

import argparse
import subprocess
from pathlib import Path

from history import backfill, config_id, read_json


def git(formatter, *args):
    return subprocess.check_output(["git", "-C", formatter, *args], text=True).strip()


def commit_metadata(formatter, sha):
    date, subject = git(formatter, "show", "-s", "--format=%cI%x00%s", sha).split("\0")
    return {"sha": sha, "date": date, "subject": subject}


def inventory(formatter, start, head, diagnostics_start):
    start = git(formatter, "rev-parse", f"{start}^{{commit}}")
    diagnostics_start = git(formatter, "rev-parse", f"{diagnostics_start}^{{commit}}")
    lineage = git(formatter, "rev-list", "--reverse", "--first-parent", head).splitlines()
    if start not in lineage or diagnostics_start not in lineage:
        raise ValueError("The release and first diagnostic commit must be on the selected first-parent history")
    first = lineage.index(start)
    first_diagnostic = lineage.index(diagnostics_start)

    commits = []
    for index in range(first, len(lineage)):
        sha = lineage[index]
        commit = commit_metadata(formatter, sha)
        if index < first_diagnostic:
            commit["before_diagnostics"] = True
        if index > first:
            prior = lineage[index - 1]
            merged = git(formatter, "rev-list", "--topo-order", "--reverse", f"{prior}..{sha}").splitlines()
            if len(merged) > 1:
                commit["merged"] = [
                    {**commit_metadata(formatter, side), "parent": git(formatter, "rev-parse", f"{side}^1")}
                    for side in merged if side != sha
                ]
        commits.append(commit)
    return commits


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--formatter", type=Path, default=Path.cwd())
    parser.add_argument("--from", dest="start", default="v0.1.0", help="First release, inclusive")
    parser.add_argument("--through", default="origin/main", help="Last commit, inclusive")
    parser.add_argument("--diagnostics-since", default="f980b1e60d4d022ff866d20dfb95ced08c1b0929")
    parser.add_argument("--snapshots", type=Path, required=True, help="Directory of extracted run JSON files")
    parser.add_argument("--archive", type=Path, required=True, help="New archive directory")
    args = parser.parse_args()

    if not args.snapshots.is_dir():
        parser.error(f"Snapshot directory does not exist: {args.snapshots}")
    commits = inventory(args.formatter, args.start, args.through, args.diagnostics_since)
    snapshots = {}
    for path in sorted(args.snapshots.glob("*.json")):
        snapshot = read_json(path)
        sha = snapshot["sha"]
        if snapshot["config_id"] != config_id(snapshot["config"]):
            parser.error(f"Invalid config ID in {path}")
        if sha in snapshots:
            parser.error(f"Duplicate snapshot for {sha}")
        snapshots[sha] = snapshot
    backfill(args.archive, commits, snapshots)
    main_runs = sum(commit["sha"] in snapshots and not commit.get("before_diagnostics") for commit in commits)
    merged = [side for commit in commits for side in commit.get("merged", [])]
    print(f"Recorded {main_runs}/{len(commits)} main snapshots and "
          f"{sum(side['sha'] in snapshots for side in merged)}/{len(merged)} merged snapshots")


if __name__ == "__main__":
    main()
