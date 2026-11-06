#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check that a commit may enter the QA or Production stage. Used by qa.yml and release.yml.

The same commit has to pass Development -> QA -> Production (see docs/ci-cd.md):

  stage_gate.py qa      --sha SHA --repo OWNER/NAME   # before a QA build
  stage_gate.py release --sha SHA --repo OWNER/NAME   # before a production release

Both stages require a full 40-character SHA of a commit on origin/develop. QA also needs a
successful Development run for exactly that SHA; Production needs a successful QA run for
exactly that SHA. QA runs are named "QA <sha>" (run-name in qa.yml), which is how they are
found. Nothing is guessed: a short SHA or another commit is never substituted.

With --github-output, writes: sha, run_url (the Development or QA run that was found).
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from typing import Optional

DEVELOP_REF = "origin/develop"
DEVELOPMENT_WORKFLOW = "development.yml"
QA_WORKFLOW = "qa.yml"
SHA_RE = re.compile(r"[0-9a-f]{40}")


class GateError(Exception):
    pass


def gh_api(*args: str) -> subprocess.CompletedProcess:
    """Runs `gh api`; replaced in tests."""
    try:
        return subprocess.run(["gh", "api", *args], capture_output=True, text=True)
    except FileNotFoundError:
        raise GateError("the GitHub CLI (gh) is needed but was not found")


def git(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run(["git", *args], capture_output=True, text=True)


def validate_sha(value: str) -> str:
    # Exact form only (no trimming or case changes): the QA run is named after the raw input,
    # and the release looks for that name, so both must use exactly the same text.
    if not SHA_RE.fullmatch(value):  # fullmatch: "$" alone would accept a trailing newline
        raise GateError(f"'{value}' is not a full 40-character lower-case commit SHA. Copy the full "
                        "SHA from the Development run summary; short SHAs are not accepted.")
    return value


def check_on_develop(sha: str, develop_ref: str = DEVELOP_REF) -> None:
    if git("rev-parse", "--verify", "--quiet", f"{develop_ref}^{{commit}}").returncode != 0:
        raise GateError(f"{develop_ref} is not available; the checkout needs fetch-depth: 0")
    if git("cat-file", "-e", f"{sha}^{{commit}}").returncode != 0:
        raise GateError(f"commit {sha} does not exist in this repository")
    if git("merge-base", "--is-ancestor", sha, develop_ref).returncode != 0:
        raise GateError(f"commit {sha} is not on develop")


def _first_run(result: subprocess.CompletedProcess, what: str) -> Optional[str]:
    if result.returncode != 0:
        raise GateError(f"could not list {what} runs: {result.stderr.strip()}")
    urls = [line.strip() for line in result.stdout.splitlines() if line.strip()]
    return urls[0] if urls else None


def successful_development_run(repo: str, sha: str) -> Optional[str]:
    """URL of a successful Development run on develop that built exactly `sha`, or None."""
    return _first_run(gh_api(
        "--paginate",
        f"repos/{repo}/actions/workflows/{DEVELOPMENT_WORKFLOW}/runs?head_sha={sha}&status=success&per_page=100",
        "--jq", '.workflow_runs[] | select(.head_branch == "develop") | .html_url'), "Development")


def successful_qa_run(repo: str, sha: str) -> Optional[str]:
    """URL of a successful QA run (run name "QA <sha>", started on develop), or None."""
    return _first_run(gh_api(
        "--paginate",
        f"repos/{repo}/actions/workflows/{QA_WORKFLOW}/runs?branch=develop&status=success&per_page=100",
        "--jq", f'.workflow_runs[] | select(.display_title == "QA {sha}") | .html_url'), "QA")


def gate(stage: str, sha_input: str, repo: str, develop_ref: str = DEVELOP_REF) -> dict:
    sha = validate_sha(sha_input)
    check_on_develop(sha, develop_ref)
    if stage == "qa":
        url = successful_development_run(repo, sha)
        if not url:
            raise GateError(f"there is no successful Development run for {sha}. QA only builds "
                            "commits that passed the Development stage; run Actions > Development "
                            "for this commit first (it runs automatically for every push to develop).")
    elif stage == "release":
        url = successful_qa_run(repo, sha)
        if not url:
            raise GateError(f"there is no successful QA run for {sha}. Production only releases "
                            "commits that passed QA; run Actions > QA with this exact SHA, test the "
                            "QA APK, then start the release again.")
    else:
        raise GateError(f"unknown stage {stage}")
    return {"sha": sha, "run_url": url}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("stage", choices=["qa", "release"])
    parser.add_argument("--sha", required=True)
    parser.add_argument("--repo", required=True)
    parser.add_argument("--github-output")
    args = parser.parse_args()
    try:
        result = gate(args.stage, args.sha, args.repo)
    except GateError as error:
        print(f"::error title={'QA' if args.stage == 'qa' else 'Release'} gate::{error}")
        return 1
    previous = "Development" if args.stage == "qa" else "QA"
    print(f"commit {result['sha']} is on develop; successful {previous} run: {result['run_url']}")
    if args.github_output:
        with open(args.github_output, "a", encoding="utf-8") as out:
            out.write(f"sha={result['sha']}\nrun_url={result['run_url']}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
