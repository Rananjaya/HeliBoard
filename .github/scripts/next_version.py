#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Work out the next RCBBoard release version from the Git history.

Used by .github/workflows/release.yml; see docs/ci-cd.md. Rules:

* Releases are tagged ``rcbboard-vMAJOR.MINOR.PATCH``. The highest such tag that is
  reachable from HEAD is the previous release. Before the first automated release,
  BASELINE_VERSION and BASELINE_COMMIT stand in for it.
* Every non-merge commit since the previous release is checked, so several commits
  and several merged pull requests are all taken into account. Merge commits are
  skipped; the commits they bring in are checked instead.
* A commit only counts if it changes something that ends up in the app. Commits that
  only touch documentation, CI configuration or tests (NON_RELEASE_PREFIXES,
  NON_RELEASE_ROOT_FILES) never cause a release.
* The bump comes from the Conventional Commits header of the commits that count:
  ``type!:`` or a ``BREAKING CHANGE:`` footer -> major, ``feat`` -> minor,
  anything else (fix, perf, refactor, build, chore, non-conventional messages, ...)
  -> patch. The largest bump wins.
* versionCode = MAJOR * 10000 + MINOR * 100 + PATCH. It must be higher than the
  previous release and than MIN_VERSION_CODE (the last manually built release), so
  that existing installations can update.

* With --repo, the previous release tag must have a published GitHub Release. If it
  has none, an earlier release run did not finish and the script stops with recovery
  steps instead of building on top of it (see docs/ci-cd.md, "Recovering a failed release").

Prints a summary and, with --github-output, writes these keys for GitHub Actions:
release (true/false), status (new/existing-tag/no-release), version, version_code,
tag, previous_tag, bump.

``--is-latest X.Y.Z --repo OWNER/NAME`` instead prints ``true`` if X.Y.Z is newer than
every published ``rcbboard-v*`` release and ``false`` otherwise, so that re-running an
older release never marks it as the latest release.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from typing import Callable, Optional

TAG_PREFIX = "rcbboard-v"
# Commit of the manually built RCBBoard release v4.3-beta1 (versionCode 4300).
BASELINE_COMMIT = "8794fa409628f911d57ae438c5f92d4cd5f3c045"
BASELINE_VERSION = (4, 3, 0)
MIN_VERSION_CODE = 4300

NON_RELEASE_PREFIXES = (
    ".github/",
    "docs/",
    "art/",
    "fastlane/",
    "app/src/test/",
    "app/src/androidTest/",
    "app/src/main/jni/tests/",
)
NON_RELEASE_ROOT_FILES = (".gitignore", ".gitattributes", ".editorconfig")

BUMPS = {"patch": 1, "minor": 2, "major": 3}
BUMP_NAMES = {v: k for k, v in BUMPS.items()}
HEADER_RE = re.compile(r"^(?P<type>[A-Za-z]+)(?:\([^)]*\))?(?P<bang>!)?:\s*\S")
BREAKING_RE = re.compile(r"^BREAKING[ -]CHANGE:", re.MULTILINE)
TAG_RE = re.compile(r"^" + re.escape(TAG_PREFIX) + r"(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$")


class VersionError(Exception):
    pass


def gh_api(*args: str) -> subprocess.CompletedProcess:
    """Runs `gh api`; replaced in tests."""
    try:
        return subprocess.run(["gh", "api", *args], capture_output=True, text=True)
    except FileNotFoundError:
        raise VersionError("the GitHub CLI (gh) is needed for --repo / --is-latest but was not found")


def published_release_exists(repo: str, tag: str) -> bool:
    """True if `tag` has a published (non-draft) GitHub Release."""
    result = gh_api(f"repos/{repo}/releases/tags/{tag}", "--jq", ".draft")
    if result.returncode == 0:
        return result.stdout.strip() == "false"
    if "HTTP 404" in result.stderr:
        return False
    raise VersionError(f"could not check the GitHub Release of {tag}: {result.stderr.strip()}")


def published_release_tags(repo: str) -> list[str]:
    """Tag names of all published (non-draft) GitHub Releases."""
    result = gh_api("--paginate", f"repos/{repo}/releases", "--jq", ".[] | select(.draft == false) | .tag_name")
    if result.returncode != 0:
        raise VersionError(f"could not list GitHub Releases: {result.stderr.strip()}")
    return result.stdout.split()


def parse_tag(name: str) -> Optional[tuple[int, int, int]]:
    match = TAG_RE.match(name)
    return tuple(int(x) for x in match.groups()) if match else None


def is_newest(version: str, published_tags: list[str]) -> bool:
    """True if `version` is newer than every published rcbboard-v* release (other tags are ignored)."""
    own = parse_tag(TAG_PREFIX + version)
    if own is None:
        raise VersionError(f"invalid version {version}")
    others = [v for v in map(parse_tag, published_tags) if v is not None and v != own]
    return all(own > v for v in others)


def orphan_tag_message(tag: str, sha: str) -> str:
    return (f"The newest release tag {tag} (commit {sha[:10]}) has no published GitHub Release, "
            f"so an earlier release run did not finish. Nothing new is released until this is fixed. "
            f"To publish {tag}: open Actions > Release, find the failed run for commit {sha[:10]} "
            f"and choose 'Re-run all jobs'. If that build must not be published: delete any draft "
            f"release for {tag}, delete the tag (git push origin --delete {tag}) and re-run this run. "
            f"See docs/ci-cd.md, 'Recovering a failed release'.")


def git(*args: str) -> str:
    result = subprocess.run(["git", *args], capture_output=True, text=True)
    if result.returncode != 0:
        raise VersionError(f"git {' '.join(args)} failed: {result.stderr.strip()}")
    return result.stdout


def is_ancestor(ancestor: str, rev: str) -> bool:
    return subprocess.run(["git", "merge-base", "--is-ancestor", ancestor, rev],
                          capture_output=True).returncode == 0


def version_code(version: tuple[int, int, int]) -> int:
    major, minor, patch = version
    if minor > 99 or patch > 99:
        raise VersionError(f"{format_version(version)}: MINOR and PATCH must stay below 100 "
                           "for versionCode = MAJOR * 10000 + MINOR * 100 + PATCH")
    return major * 10000 + minor * 100 + patch


def format_version(version: tuple[int, int, int]) -> str:
    return ".".join(map(str, version))


def bump_version(version: tuple[int, int, int], level: int) -> tuple[int, int, int]:
    major, minor, patch = version
    if level == 3:
        return major + 1, 0, 0
    if level == 2:
        return major, minor + 1, 0
    return major, minor, patch + 1


def counts_for_release(path: str) -> bool:
    if path.startswith(NON_RELEASE_PREFIXES):
        return False
    if "/" not in path and (path.endswith(".md") or path in NON_RELEASE_ROOT_FILES):
        return False
    return True


def commit_level(message: str) -> int:
    header = message.split("\n", 1)[0].strip()
    match = HEADER_RE.match(header)
    if match and match.group("bang") or BREAKING_RE.search(message):
        return BUMPS["major"]
    if match and match.group("type").lower() == "feat":
        return BUMPS["minor"]
    return BUMPS["patch"]


def commits_since(base: str, head: str) -> list[tuple[str, str, list[str]]]:
    """Non-merge commits in base..head as (sha, message, changed paths)."""
    out = git("log", "--no-merges", "--no-renames", "--format=%x1e%H%x1f%B%x1f",
              "--name-only", f"{base}..{head}")
    commits = []
    for record in out.split("\x1e")[1:]:
        sha, message, files = record.split("\x1f")
        commits.append((sha, message.strip(), [f for f in files.split("\n") if f.strip()]))
    return commits


def release_tags() -> dict[str, tuple[int, int, int]]:
    tags = {}
    for name in git("tag", "--list", TAG_PREFIX + "*").split():
        match = TAG_RE.match(name)
        if match:
            tags[name] = tuple(int(x) for x in match.groups())
    return tags


def determine(head: str = "HEAD", forced_bump: str = "auto",
              baseline_commit: str = BASELINE_COMMIT,
              baseline_version: tuple[int, int, int] = BASELINE_VERSION,
              min_version_code: int = MIN_VERSION_CODE,
              release_exists: Optional[Callable[[str], bool]] = None) -> dict:
    if git("rev-parse", "--is-shallow-repository").strip() == "true":
        raise VersionError("the clone is shallow; check out with fetch-depth: 0")
    head_sha = git("rev-parse", "--verify", f"{head}^{{commit}}").strip()
    tags = release_tags()
    tag_commits = {name: git("rev-parse", f"{name}^{{commit}}").strip() for name in tags}

    # HEAD already released (for example a re-run after the release step failed).
    at_head = sorted((tags[n], n) for n, sha in tag_commits.items() if sha == head_sha)
    if at_head:
        version, name = at_head[-1]
        return {"release": True, "status": "existing-tag", "version": format_version(version),
                "version_code": version_code(version), "tag": name, "previous_tag": "",
                "bump": "none", "commits": []}

    merged = [(v, n) for n, v in tags.items() if is_ancestor(tag_commits[n], head_sha)]
    if merged:
        previous_version, previous_tag = max(merged)
        base = previous_tag
        if release_exists is not None and not release_exists(previous_tag):
            raise VersionError(orphan_tag_message(previous_tag, tag_commits[previous_tag]))
    else:
        if not is_ancestor(baseline_commit, head_sha):
            raise VersionError(f"no {TAG_PREFIX}* tag and the baseline commit {baseline_commit} "
                               "is not an ancestor of HEAD")
        previous_version, previous_tag, base = baseline_version, "", baseline_commit

    level = 0
    report = []
    for sha, message, files in commits_since(base, head_sha):
        counts = any(counts_for_release(f) for f in files)
        commit_bump = commit_level(message) if counts else 0
        level = max(level, commit_bump)
        report.append((sha[:10], BUMP_NAMES.get(commit_bump, "none"), message.split("\n", 1)[0]))

    if forced_bump != "auto":
        if forced_bump not in BUMPS:
            raise VersionError(f"unknown bump '{forced_bump}'")
        level = BUMPS[forced_bump]

    result = {"previous_tag": previous_tag, "commits": report}
    if level == 0:
        result.update(release=False, status="no-release", version=format_version(previous_version),
                      version_code=version_code(previous_version), tag="", bump="none")
        return result

    version = bump_version(previous_version, level)
    code = version_code(version)
    floor = max(min_version_code, version_code(previous_version) if previous_tag else min_version_code)
    if code <= floor:
        raise VersionError(f"versionCode {code} is not higher than {floor}")
    tag = TAG_PREFIX + format_version(version)
    if tag in tags:
        raise VersionError(f"tag {tag} already exists on another commit ({tag_commits[tag][:10]})")
    taken = [n for n, v in tags.items() if version_code(v) == code]
    if taken:
        raise VersionError(f"versionCode {code} is already used by {taken[0]}")
    result.update(release=True, status="new", version=format_version(version), version_code=code,
                  tag=tag, bump=BUMP_NAMES[level])
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--head", default="HEAD")
    parser.add_argument("--bump", default="auto", choices=["auto", *BUMPS])
    parser.add_argument("--github-output", help="file to append GitHub Actions outputs to")
    parser.add_argument("--repo", help="OWNER/NAME; check GitHub Releases with `gh` (needs GH_TOKEN)")
    parser.add_argument("--is-latest", metavar="VERSION",
                        help="print true/false: is VERSION newer than every published release (needs --repo)")
    args = parser.parse_args()
    try:
        if args.is_latest:
            if not args.repo:
                raise VersionError("--is-latest needs --repo")
            print(str(is_newest(args.is_latest, published_release_tags(args.repo))).lower())
            return 0
        checker = (lambda tag: published_release_exists(args.repo, tag)) if args.repo else None
        result = determine(args.head, args.bump, release_exists=checker)
    except VersionError as error:
        print(f"::error title=Version::{error}")
        return 1

    for sha, bump, header in result["commits"]:
        print(f"  {sha}  {bump:<5}  {header}")
    print(f"previous: {result['previous_tag'] or 'baseline ' + format_version(BASELINE_VERSION)}")
    print(f"status: {result['status']}, bump: {result['bump']}, version: {result['version']} "
          f"(versionCode {result['version_code']}), tag: {result['tag'] or '-'}")
    if args.github_output:
        with open(args.github_output, "a", encoding="utf-8") as out:
            for key in ("release", "status", "version", "version_code", "tag", "previous_tag", "bump"):
                value = result[key]
                out.write(f"{key}={str(value).lower() if isinstance(value, bool) else value}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
