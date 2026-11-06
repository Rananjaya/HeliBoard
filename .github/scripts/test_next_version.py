#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Tests for next_version.py. Each test builds a small Git repository in a temporary folder.

Run: python3 .github/scripts/test_next_version.py
"""
from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import next_version as nv  # noqa: E402


class Repo:
    def __init__(self, path: str):
        self.path = path
        self.counter = 0
        self.git("init", "-q", "-b", "develop")
        self.git("config", "user.name", "Test")
        self.git("config", "user.email", "test@example.invalid")
        self.git("config", "commit.gpgsign", "false")
        self.git("config", "tag.gpgsign", "false")

    def git(self, *args: str) -> str:
        return subprocess.run(["git", *args], cwd=self.path, check=True, capture_output=True,
                              text=True).stdout.strip()

    def commit(self, message: str, *paths: str) -> str:
        for rel in paths or ("app/src/main/java/File.kt",):
            full = os.path.join(self.path, rel)
            os.makedirs(os.path.dirname(full), exist_ok=True)
            self.counter += 1
            with open(full, "a", encoding="utf-8") as f:
                f.write(f"change {self.counter}\n")
            self.git("add", rel)
        self.git("commit", "-q", "-m", message)
        return self.git("rev-parse", "HEAD")


class NextVersionTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.repo = Repo(self.tmp.name)
        self.baseline = self.repo.commit("feat: initial app")
        self.old_cwd = os.getcwd()
        os.chdir(self.tmp.name)

    def tearDown(self):
        os.chdir(self.old_cwd)
        self.tmp.cleanup()

    def run_nv(self, bump: str = "auto") -> dict:
        return nv.determine("HEAD", bump, baseline_commit=self.baseline,
                            baseline_version=(4, 3, 0), min_version_code=4300)

    def test_first_release_from_baseline_fix_gives_4_3_1(self):
        self.repo.commit("fix(security): prevent overflow", "app/src/main/jni/src/a.cpp")
        self.repo.commit("docs: readme", "README.md")
        r = self.run_nv()
        self.assertEqual((r["release"], r["version"], r["version_code"], r["tag"]),
                         (True, "4.3.1", 40301, "rcbboard-v4.3.1"))
        self.assertEqual(r["bump"], "patch")

    def test_docs_only_changes_do_not_release(self):
        self.repo.commit("docs: update readme", "README.md", "docs/images/a.png", "layouts.md")
        self.repo.commit("feat: new docs page", "docs/new.md")  # type does not matter for docs
        r = self.run_nv()
        self.assertFalse(r["release"])
        self.assertEqual(r["status"], "no-release")

    def test_ci_and_test_only_changes_do_not_release(self):
        self.repo.commit("ci: add workflow", ".github/workflows/ci.yml")
        self.repo.commit("fix: flaky test", "app/src/test/java/T.kt", "app/src/main/jni/tests/t.cpp")
        self.repo.commit("chore: ignore files", ".gitignore")
        self.assertFalse(self.run_nv()["release"])

    def test_chore_touching_app_code_is_patch(self):
        self.repo.commit("chore: clean up links", "app/src/main/java/Constants.kt")
        r = self.run_nv()
        self.assertEqual((r["version"], r["bump"]), ("4.3.1", "patch"))

    def test_non_conventional_app_commit_is_patch(self):
        self.repo.commit("Merge upstream changes and adjust layout", "app/src/main/assets/x.json")
        self.assertEqual(self.run_nv()["version"], "4.3.1")

    def test_multiple_merged_pull_requests_use_largest_bump(self):
        r = self.repo
        r.git("checkout", "-q", "-b", "pr1")
        r.commit("fix: crash in settings")
        r.commit("docs: explain fix", "docs/fix.md")
        r.git("checkout", "-q", "develop")
        r.git("merge", "-q", "--no-ff", "pr1", "-m", "Merge pull request #1 from pr1")
        r.git("checkout", "-q", "-b", "pr2")
        r.commit("feat(layout): add a layout", "app/src/main/assets/layouts/new.txt")
        r.git("checkout", "-q", "develop")
        r.git("merge", "-q", "--no-ff", "pr2", "-m", "Merge pull request #2 from pr2")
        r.git("checkout", "-q", "-b", "pr3")
        r.commit("ci: tweak", ".github/workflows/x.yml")
        r.git("checkout", "-q", "develop")
        r.git("merge", "-q", "--no-ff", "pr3", "-m", "Merge pull request #3 from pr3")
        res = self.run_nv()
        self.assertEqual((res["version"], res["version_code"], res["bump"]), ("4.4.0", 40400, "minor"))
        self.assertEqual(len(res["commits"]), 4)  # merge commits are not counted themselves

    def test_squash_merged_feature_is_minor(self):
        self.repo.commit("feat: Sinhala layout (#7)", "app/src/main/assets/layouts/si.txt")
        self.assertEqual(self.run_nv()["version"], "4.4.0")

    def test_breaking_change_bang_and_footer(self):
        self.repo.commit("refactor!: change settings storage")
        self.assertEqual(self.run_nv()["version"], "5.0.0")

    def test_breaking_change_footer(self):
        self.repo.commit("feat: new backup format\n\nBREAKING CHANGE: old backups cannot be restored")
        r = self.run_nv()
        self.assertEqual((r["version"], r["version_code"]), ("5.0.0", 50000))

    def test_breaking_change_in_docs_only_does_not_release(self):
        self.repo.commit("docs!: rewrite readme", "README.md")
        self.assertFalse(self.run_nv()["release"])

    def test_counts_from_latest_tag(self):
        self.repo.commit("fix: one")
        self.repo.git("tag", "rcbboard-v4.3.1")
        self.repo.commit("fix: two")
        self.repo.git("tag", "rcbboard-v4.3.2")
        self.repo.commit("feat: three")
        r = self.run_nv()
        self.assertEqual((r["previous_tag"], r["version"], r["version_code"]),
                         ("rcbboard-v4.3.2", "4.4.0", 40400))
        self.assertEqual(len(r["commits"]), 1)

    def test_no_change_since_tag_but_new_commit_docs(self):
        self.repo.commit("fix: one")
        self.repo.git("tag", "rcbboard-v4.3.1")
        self.repo.commit("docs: notes", "CONTRIBUTING.md")
        r = self.run_nv()
        self.assertFalse(r["release"])
        self.assertEqual(r["version"], "4.3.1")

    def test_head_already_tagged_is_reported_for_resume(self):
        self.repo.commit("fix: one")
        self.repo.git("tag", "rcbboard-v4.3.1")
        r = self.run_nv()
        self.assertEqual((r["status"], r["tag"], r["release"]), ("existing-tag", "rcbboard-v4.3.1", True))

    def test_tag_on_unmerged_commit_is_not_reused(self):
        r = self.repo
        r.git("checkout", "-q", "-b", "other")
        r.commit("fix: elsewhere")
        r.git("tag", "rcbboard-v4.3.1")
        r.git("checkout", "-q", "develop")
        r.commit("fix: here")
        with self.assertRaisesRegex(nv.VersionError, "already exists"):
            self.run_nv()

    def test_ignores_upstream_style_and_malformed_tags(self):
        self.repo.git("tag", "v4.3-beta1")
        self.repo.git("tag", "rcbboard-v4.3")
        self.repo.git("tag", "rcbboard-v04.3.1")
        self.repo.commit("fix: one")
        self.assertEqual(self.run_nv()["tag"], "rcbboard-v4.3.1")

    def test_forced_bump(self):
        self.repo.commit("docs: only docs", "README.md")
        r = self.run_nv("minor")
        self.assertEqual((r["release"], r["version"]), (True, "4.4.0"))

    def test_baseline_must_be_ancestor(self):
        with self.assertRaisesRegex(nv.VersionError, "baseline"):
            nv.determine("HEAD", "auto", baseline_commit="0" * 40)

    def test_version_code_limits(self):
        self.assertEqual(nv.version_code((4, 3, 1)), 40301)
        with self.assertRaises(nv.VersionError):
            nv.version_code((4, 100, 0))
        self.repo.commit("fix: one")
        self.repo.git("tag", "rcbboard-v4.3.99")
        self.repo.commit("fix: two")
        with self.assertRaisesRegex(nv.VersionError, "below 100"):
            self.run_nv()

    def test_shallow_clone_is_rejected(self):
        self.repo.commit("fix: one")
        self.repo.commit("fix: two")
        shallow = os.path.join(self.tmp.name, "shallow")
        subprocess.run(["git", "clone", "-q", "--depth", "1", "file://" + self.tmp.name, shallow],
                       check=True, capture_output=True)
        os.chdir(shallow)
        with self.assertRaisesRegex(nv.VersionError, "shallow"):
            self.run_nv()


class ReleaseRecoveryTest(unittest.TestCase):
    """Orphan tags: a tag whose release run did not finish. Same repository setup as NextVersionTest."""

    def setUp(self):
        NextVersionTest.setUp(self)  # type: ignore[arg-type]
        self.published = set()

    def tearDown(self):
        NextVersionTest.tearDown(self)  # type: ignore[arg-type]

    def run_nv(self, bump: str = "auto") -> dict:
        return nv.determine("HEAD", bump, baseline_commit=self.baseline, baseline_version=(4, 3, 0),
                            min_version_code=4300, release_exists=lambda tag: tag in self.published)

    def test_orphan_tag_stops_with_recovery_message(self):
        self.repo.commit("fix: one")
        self.repo.git("tag", "rcbboard-v4.3.1")  # tag created, release step failed
        self.repo.commit("fix: two")             # a new fix lands before the re-run
        with self.assertRaises(nv.VersionError) as ctx:
            self.run_nv()
        message = str(ctx.exception)
        self.assertIn("rcbboard-v4.3.1", message)
        self.assertIn("has no published GitHub Release", message)
        self.assertIn("Re-run all jobs", message)
        self.assertIn("git push origin --delete rcbboard-v4.3.1", message)
        self.assertNotIn("\n", message)  # one line, so GitHub shows the whole annotation

    def test_orphan_tag_also_stops_docs_only_pushes(self):
        self.repo.commit("fix: one")
        self.repo.git("tag", "rcbboard-v4.3.1")
        self.repo.commit("docs: notes", "README.md")
        with self.assertRaisesRegex(nv.VersionError, "has no published GitHub Release"):
            self.run_nv()

    def test_published_previous_release_continues(self):
        self.repo.commit("fix: one")
        self.repo.git("tag", "rcbboard-v4.3.1")
        self.published.add("rcbboard-v4.3.1")
        self.repo.commit("fix: two")
        self.assertEqual(self.run_nv()["version"], "4.3.2")

    def test_rerun_of_the_orphaned_commit_resumes(self):
        self.repo.commit("fix: one")
        self.repo.git("tag", "rcbboard-v4.3.1")  # HEAD is the tagged commit: the failed run's re-run
        r = self.run_nv()
        self.assertEqual((r["status"], r["tag"], r["release"]), ("existing-tag", "rcbboard-v4.3.1", True))

    def test_first_release_needs_no_previous_release(self):
        self.repo.commit("fix: one")
        self.assertEqual(self.run_nv()["version"], "4.3.1")

    def test_after_deleting_the_orphan_tag_the_version_is_reused(self):
        self.repo.commit("fix: one")
        self.repo.git("tag", "rcbboard-v4.3.1")
        self.repo.commit("fix: two")
        self.repo.git("tag", "-d", "rcbboard-v4.3.1")
        self.assertEqual(self.run_nv()["tag"], "rcbboard-v4.3.1")


class GitHubApiTest(unittest.TestCase):
    def setUp(self):
        self.original = nv.gh_api

    def tearDown(self):
        nv.gh_api = self.original

    def fake(self, returncode: int, stdout: str = "", stderr: str = ""):
        calls = []

        def gh_api(*args):
            calls.append(args)
            return subprocess.CompletedProcess(args, returncode, stdout, stderr)
        nv.gh_api = gh_api
        return calls

    def test_release_exists(self):
        calls = self.fake(0, "false\n")
        self.assertTrue(nv.published_release_exists("o/r", "rcbboard-v4.3.1"))
        self.assertEqual(calls[0][0], "repos/o/r/releases/tags/rcbboard-v4.3.1")

    def test_draft_release_does_not_count(self):
        self.fake(0, "true\n")
        self.assertFalse(nv.published_release_exists("o/r", "rcbboard-v4.3.1"))

    def test_missing_release(self):
        self.fake(1, stderr="gh: Not Found (HTTP 404)")
        self.assertFalse(nv.published_release_exists("o/r", "rcbboard-v4.3.1"))

    def test_api_error_fails_safe(self):
        self.fake(1, stderr="gh: Bad credentials (HTTP 401)")
        with self.assertRaisesRegex(nv.VersionError, "could not check"):
            nv.published_release_exists("o/r", "rcbboard-v4.3.1")

    def test_list_error_fails_safe(self):
        self.fake(1, stderr="HTTP 500")
        with self.assertRaisesRegex(nv.VersionError, "could not list"):
            nv.published_release_tags("o/r")

    def test_missing_gh_fails_safe(self):
        nv.gh_api = self.original
        old_path = os.environ.get("PATH", "")
        os.environ["PATH"] = tempfile.mkdtemp()  # a folder without gh
        try:
            with self.assertRaisesRegex(nv.VersionError, "GitHub CLI"):
                nv.published_release_exists("o/r", "rcbboard-v4.3.1")
        finally:
            os.environ["PATH"] = old_path

    def test_list_published_tags(self):
        self.fake(0, "rcbboard-v4.3.2\nv4.3-beta1\n")
        self.assertEqual(nv.published_release_tags("o/r"), ["rcbboard-v4.3.2", "v4.3-beta1"])


class LatestTest(unittest.TestCase):
    def test_new_highest_version_is_latest(self):
        self.assertTrue(nv.is_newest("4.3.2", ["rcbboard-v4.3.1", "v4.3-beta1"]))

    def test_first_release_is_latest_despite_manual_release(self):
        self.assertTrue(nv.is_newest("4.3.1", ["v4.3-beta1"]))
        self.assertTrue(nv.is_newest("4.3.1", []))

    def test_older_rerun_is_not_latest(self):
        self.assertFalse(nv.is_newest("4.3.1", ["rcbboard-v4.3.2"]))
        self.assertFalse(nv.is_newest("4.9.0", ["rcbboard-v4.10.0"]))  # numeric, not text order
        self.assertFalse(nv.is_newest("4.3.1", ["rcbboard-v5.0.0", "rcbboard-v4.3.0"]))

    def test_same_version_already_published_is_ignored(self):
        self.assertTrue(nv.is_newest("4.3.1", ["rcbboard-v4.3.1", "rcbboard-v4.3.0"]))

    def test_invalid_version(self):
        with self.assertRaises(nv.VersionError):
            nv.is_newest("4.3", [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
