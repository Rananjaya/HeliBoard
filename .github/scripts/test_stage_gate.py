#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Tests for stage_gate.py. Run: python3 .github/scripts/test_stage_gate.py"""
from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stage_gate as sg  # noqa: E402

REPO = "o/r"


class GateTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.old_cwd = os.getcwd()
        os.chdir(self.tmp.name)
        for args in (["init", "-q", "-b", "develop"], ["config", "user.name", "T"],
                     ["config", "user.email", "t@example.invalid"], ["config", "commit.gpgsign", "false"]):
            self.git(*args)
        self.first = self.commit("one")
        self.second = self.commit("two")
        self.git("update-ref", "refs/remotes/origin/develop", self.second)
        self.git("checkout", "-q", "-b", "feature/x")
        self.feature = self.commit("feature only")
        self.git("checkout", "-q", "develop")
        self.calls = []
        self.dev_urls = {}   # sha -> url of a successful Development run
        self.qa_urls = {}    # sha -> url of a successful QA run
        self.api_fails = False
        self.original = sg.gh_api
        sg.gh_api = self.fake_gh

    def tearDown(self):
        sg.gh_api = self.original
        os.chdir(self.old_cwd)
        self.tmp.cleanup()

    def git(self, *args):
        return subprocess.run(["git", *args], check=True, capture_output=True, text=True).stdout.strip()

    def commit(self, message):
        with open("f.txt", "a", encoding="utf-8") as f:
            f.write(message + "\n")
        self.git("add", "f.txt")
        self.git("commit", "-q", "-m", message)
        return self.git("rev-parse", "HEAD")

    def fake_gh(self, *args):
        self.calls.append(args)
        if self.api_fails:
            return subprocess.CompletedProcess(args, 1, "", "gh: Bad credentials (HTTP 401)")
        endpoint = args[1]
        if "development.yml" in endpoint:
            sha = endpoint.split("head_sha=")[1].split("&")[0]
            out = self.dev_urls.get(sha, "")
        else:
            jq = args[-1]
            out = "\n".join(url for sha, url in self.qa_urls.items() if f'"QA {sha}"' in jq)
        return subprocess.CompletedProcess(args, 0, out + "\n" if out else "", "")

    # --- SHA format -------------------------------------------------------------------------

    def test_short_sha_rejected(self):
        with self.assertRaisesRegex(sg.GateError, "full 40-character"):
            sg.gate("qa", self.second[:7], REPO)

    def test_non_hex_rejected(self):
        with self.assertRaisesRegex(sg.GateError, "full 40-character"):
            sg.gate("qa", "g" * 40, REPO)
        with self.assertRaisesRegex(sg.GateError, "full 40-character"):
            sg.gate("qa", "develop", REPO)

    def test_upper_case_and_spaces_are_rejected(self):
        # the QA run name uses the raw input, so only the exact form can be matched later
        self.dev_urls[self.second] = "https://dev/1"
        for value in (self.second.upper(), f" {self.second}", f"{self.second}\n"):
            with self.assertRaisesRegex(sg.GateError, "lower-case"):
                sg.gate("qa", value, REPO)
        self.assertEqual(sg.gate("qa", self.second, REPO)["sha"], self.second)

    # --- on develop ---------------------------------------------------------------------------

    def test_commit_not_on_develop_rejected(self):
        self.dev_urls[self.feature] = "https://dev/1"
        with self.assertRaisesRegex(sg.GateError, "not on develop"):
            sg.gate("qa", self.feature, REPO)

    def test_unknown_commit_rejected(self):
        with self.assertRaisesRegex(sg.GateError, "does not exist"):
            sg.gate("qa", "1" * 40, REPO)

    def test_older_develop_commit_is_on_develop(self):
        self.dev_urls[self.first] = "https://dev/1"
        self.assertEqual(sg.gate("qa", self.first, REPO)["run_url"], "https://dev/1")

    def test_missing_develop_ref(self):
        with self.assertRaisesRegex(sg.GateError, "fetch-depth"):
            sg.gate("qa", self.second, REPO, develop_ref="origin/missing")

    # --- QA needs Development ------------------------------------------------------------------

    def test_qa_requires_successful_development_run(self):
        with self.assertRaisesRegex(sg.GateError, "no successful Development run"):
            sg.gate("qa", self.second, REPO)

    def test_qa_with_development_run(self):
        self.dev_urls[self.second] = "https://dev/7"
        self.assertEqual(sg.gate("qa", self.second, REPO), {"sha": self.second, "run_url": "https://dev/7"})
        endpoint = self.calls[-1][1]
        self.assertIn(f"head_sha={self.second}", endpoint)
        self.assertIn("status=success", endpoint)

    def test_development_run_of_other_commit_does_not_count(self):
        self.dev_urls[self.first] = "https://dev/1"
        with self.assertRaisesRegex(sg.GateError, "no successful Development run"):
            sg.gate("qa", self.second, REPO)

    # --- Release needs QA ----------------------------------------------------------------------

    def test_release_requires_successful_qa_run(self):
        self.dev_urls[self.second] = "https://dev/1"  # Development alone is not enough
        with self.assertRaisesRegex(sg.GateError, "no successful QA run"):
            sg.gate("release", self.second, REPO)

    def test_release_with_qa_run(self):
        self.qa_urls[self.second] = "https://qa/3"
        self.assertEqual(sg.gate("release", self.second, REPO)["run_url"], "https://qa/3")
        self.assertIn("branch=develop", self.calls[-1][1])

    def test_qa_run_of_other_commit_does_not_count(self):
        # develop moved on after QA: QA tested `first`, the release asks for `second`
        self.qa_urls[self.first] = "https://qa/1"
        with self.assertRaisesRegex(sg.GateError, "no successful QA run"):
            sg.gate("release", self.second, REPO)
        self.assertEqual(sg.gate("release", self.first, REPO)["sha"], self.first)

    def test_release_of_feature_commit_rejected_even_with_qa_run(self):
        self.qa_urls[self.feature] = "https://qa/9"
        with self.assertRaisesRegex(sg.GateError, "not on develop"):
            sg.gate("release", self.feature, REPO)

    # --- API failures fail safe ----------------------------------------------------------------

    def test_api_error_fails_safe(self):
        self.api_fails = True
        with self.assertRaisesRegex(sg.GateError, "could not list"):
            sg.gate("release", self.second, REPO)

    def test_missing_gh_fails_safe(self):
        sg.gh_api = self.original
        real_run = sg.subprocess.run

        def run_without_gh(cmd, *args, **kwargs):
            if cmd[0] == "gh":
                raise FileNotFoundError("gh")
            return real_run(cmd, *args, **kwargs)
        sg.subprocess.run = run_without_gh
        try:
            with self.assertRaisesRegex(sg.GateError, "GitHub CLI"):
                sg.gate("qa", self.second, REPO)
        finally:
            sg.subprocess.run = real_run


if __name__ == "__main__":
    unittest.main(verbosity=2)
