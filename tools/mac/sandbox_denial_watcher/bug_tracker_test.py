#!/usr/bin/env python3

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for bug_tracker.py."""

from __future__ import annotations

from pathlib import Path
import unittest
from unittest.mock import patch
import urllib.parse

from bug_tracker import (
    _COMPONENT_ID,
    _CONTEXT_PLACEHOLDER,
    _CONTEXT_QUESTION,
    _PRIVACY_REMINDER,
    _format_targets,
    _redact_home_dir,
    get_bug_url,
)
from log_events import SandboxDenial

_FAKE_HOME = "/Users/testuser"


def _denial(
    target: str,
    action: str = "mach-lookup",
    process: str = "Chromium Helper (Renderer)",
) -> SandboxDenial:
    return SandboxDenial(
        event_message=f"Sandbox: {process}(123) deny(1) {action} {target}",
        process_name=process,
        pid=123,
        action=action,
        target=target,
    )


def _parse_url(url: str) -> tuple[urllib.parse.SplitResult, dict[str, str]]:
    parsed = urllib.parse.urlsplit(url)
    params = {
        key: values[0]
        for key, values in urllib.parse.parse_qs(parsed.query).items()
    }
    return parsed, params


class GetBugUrlTest(unittest.TestCase):
    """Verifies the contents of pre-filled bug URLs."""

    def setUp(self) -> None:
        super().setUp()
        for target, return_value in (
            ("bug_tracker.platform.mac_ver", ("15.1", ("", "", ""), "arm64")),
            ("bug_tracker.platform.machine", "arm64"),
            ("bug_tracker.Path.home", Path(_FAKE_HOME)),
        ):
            patcher = patch(target, return_value=return_value)
            patcher.start()
            self.addCleanup(patcher.stop)

    def test_url_targets_new_issue_in_sandbox_component(self):
        parsed, params = _parse_url(get_bug_url(_denial("com.apple.foo")))
        self.assertEqual(parsed.scheme, "https")
        self.assertEqual(parsed.netloc, "issues.chromium.org")
        self.assertEqual(parsed.path, "/issues/new")
        self.assertEqual(params["component"], _COMPONENT_ID)
        self.assertEqual(params["noWizard"], "true")

    def test_title_includes_unexpected_denial_and_target(self):
        _, params = _parse_url(get_bug_url(_denial("com.apple.foo")))
        self.assertEqual(
            params["title"],
            "[macOS Sandbox] Unexpected denial: mach-lookup com.apple.foo",
        )

    def test_description_includes_process_and_event_message(self):
        denial = _denial("com.apple.foo")
        _, params = _parse_url(get_bug_url(denial))
        self.assertIn(
            "Process: Chromium Helper (Renderer)", params["description"]
        )
        self.assertIn(
            f"```\n{denial.event_message}\n```", params["description"]
        )

    def test_batch_title_and_description_with_different_targets(self):
        d1 = _denial(
            "/path/to/crl-set",
            action="file-write-times",
            process="Google Chrome Helper (Aperitif)",
        )
        d2 = _denial(
            "/path/to/manifest.json",
            action="file-write-times",
            process="Google Chrome Helper (Aperitif)",
        )
        _, params = _parse_url(get_bug_url([d1, d2]))
        self.assertEqual(
            params["title"],
            "[macOS Sandbox] Unexpected denial: "
            "file-write-times /path/to/crl-set, /path/to/manifest.json",
        )
        self.assertIn(
            "Process: Google Chrome Helper (Aperitif)", params["description"]
        )
        self.assertIn(d1.event_message, params["description"])
        self.assertIn(d2.event_message, params["description"])
        self.assertIn(
            "Unexpected Sandbox denials detected:",
            params["description"],
        )

    def test_batch_title_with_repeated_same_pattern(self):
        d1 = _denial("com.apple.foo")
        d2 = _denial("com.apple.foo")
        _, params = _parse_url(get_bug_url([d1, d2]))
        self.assertEqual(
            params["title"],
            "[macOS Sandbox] Unexpected denial: mach-lookup com.apple.foo",
        )
        self.assertIn(
            "Unexpected Sandbox denial detected:",
            params["description"],
        )

    def test_batch_title_truncates_long_target_list(self):
        targets = [f"/path/to/resource_{i}" for i in range(10)]
        denials = [_denial(t, action="file-read-data") for t in targets]
        _, params = _parse_url(get_bug_url(denials))
        self.assertEqual(
            params["title"],
            "[macOS Sandbox] Unexpected denial: "
            "file-read-data /path/to/resource_0, ... (+9 more)",
        )

    def test_title_for_targetless_denial(self):
        denial = _denial("", action="process-fork")
        _, params = _parse_url(get_bug_url(denial))
        self.assertEqual(
            params["title"],
            "[macOS Sandbox] Unexpected denial: process-fork",
        )

    def test_batch_description_deduplicates_identical_messages(self):
        d1 = _denial("com.apple.foo")
        d2 = _denial("com.apple.foo")
        _, params = _parse_url(get_bug_url([d1, d2]))
        self.assertEqual(params["description"].count(d1.event_message), 1)

    def test_empty_denials_raises_value_error(self):
        with self.assertRaises(ValueError):
            get_bug_url([])

    def test_description_includes_system_info(self):
        _, params = _parse_url(get_bug_url(_denial("com.apple.foo")))
        self.assertIn("macOS version: 15.1", params["description"])
        self.assertIn("Architecture: arm64", params["description"])

    @patch("bug_tracker.platform.machine", return_value="")
    @patch("bug_tracker.platform.mac_ver", return_value=("", ("", "", ""), ""))
    def test_description_reports_unknown_system_info(
        self, _mock_mac_ver, _mock_machine
    ):
        _, params = _parse_url(get_bug_url(_denial("com.apple.foo")))
        self.assertIn("macOS version: unknown", params["description"])
        self.assertIn("Architecture: unknown", params["description"])

    def test_description_includes_context_placeholder_and_reminder(self):
        _, params = _parse_url(get_bug_url(_denial("com.apple.foo")))
        description = params["description"]
        self.assertIn(
            f"{_CONTEXT_QUESTION}\n{_CONTEXT_PLACEHOLDER}", description
        )
        self.assertIn(_PRIVACY_REMINDER, description)

    def test_home_dir_is_redacted_from_title_and_description(self):
        url = get_bug_url(
            _denial(f"{_FAKE_HOME}/Documents/secret.txt", "file-read-data")
        )
        self.assertNotIn("testuser", url)
        _, params = _parse_url(url)
        self.assertIn("file-read-data ~/Documents/secret.txt", params["title"])
        self.assertIn(
            "file-read-data ~/Documents/secret.txt", params["description"]
        )

    def test_batch_title_truncation_uses_redacted_target_length(self):
        d1 = _denial(
            f"{_FAKE_HOME}/Documents/sub/file_1.txt", action="file-read-data"
        )
        d2 = _denial(
            f"{_FAKE_HOME}/Documents/sub/file_2.txt", action="file-read-data"
        )
        _, params = _parse_url(get_bug_url([d1, d2]))
        self.assertEqual(
            params["title"],
            "[macOS Sandbox] Unexpected denial: "
            "file-read-data ~/Documents/sub/file_1.txt, "
            "~/Documents/sub/file_2.txt",
        )


class FormatTargetsTest(unittest.TestCase):
    """Verifies target list formatting and summarization."""

    def test_empty_targets(self):
        self.assertEqual(_format_targets([]), "")

    def test_single_target(self):
        self.assertEqual(_format_targets(["com.apple.foo"]), "com.apple.foo")

    def test_multiple_targets_within_limit(self):
        self.assertEqual(
            _format_targets(["com.apple.foo", "com.apple.bar"]),
            "com.apple.foo, com.apple.bar",
        )

    def test_multiple_targets_exceeding_limit(self):
        self.assertEqual(
            _format_targets(["/path/1", "/path/2", "/path/3"], max_len=10),
            "/path/1, ... (+2 more)",
        )


class RedactHomeDirTest(unittest.TestCase):
    """Verifies home directory redaction."""

    def test_redacts_only_whole_path_components(self):
        self.assertEqual(
            _redact_home_dir(
                "/Users/bob/x /Users/bobby/y /Users/bob.smith/z /Users/bob",
                home_dir="/Users/bob",
            ),
            "~/x /Users/bobby/y /Users/bob.smith/z ~",
        )

    def test_ignores_trailing_slash_in_home_dir(self):
        self.assertEqual(
            _redact_home_dir("/Users/bob/x", home_dir="/Users/bob/"), "~/x"
        )

    def test_empty_home_dir_leaves_text_unchanged(self):
        self.assertEqual(_redact_home_dir("/a/b", home_dir="/"), "/a/b")


if __name__ == "__main__":
    unittest.main()
