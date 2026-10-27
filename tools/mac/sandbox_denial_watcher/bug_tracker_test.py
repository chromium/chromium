#!/usr/bin/env python3

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for bug_tracker.py."""

from __future__ import annotations

from pathlib import Path
from typing import Optional
import unittest
from unittest.mock import patch
import urllib.parse

from bug_tracker import (
    _COMPONENT_ID,
    _CONTEXT_PLACEHOLDER,
    _CONTEXT_QUESTION,
    _MAX_BUG_URL_LENGTH,
    _PRIVACY_REMINDER,
    _format_targets,
    _get_process_and_profile,
    _redact_home_dir,
    get_bug_url,
    get_seatbelt_profile,
)
from log_events import SandboxDenial

_FAKE_HOME = "/Users/testuser"


def _denial(
    target: str,
    action: str = "mach-lookup",
    process: str = "Chromium Helper (Renderer)",
    command_line: Optional[str] = None,
) -> SandboxDenial:
    return SandboxDenial(
        event_message=f"Sandbox: {process}(123) deny(1) {action} {target}",
        process_name=process,
        pid=123,
        action=action,
        target=target,
        command_line=command_line,
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
            "Process type: renderer\nSeatbelt profile: renderer.sb",
            params["description"],
        )
        self.assertIn(
            f"```\n{denial.event_message}\n```", params["description"]
        )

    def test_batch_title_and_description_with_different_targets(self):
        cmd = (
            "/Applications/Google Chrome Canary.app/Contents/MacOS/"
            "Google Chrome Helper (Aperitif) --type=utility "
            "--utility-sub-type=unzip.mojom.Unzipper "
            "--service-sandbox-type=service"
        )
        d1 = _denial(
            "/path/to/crl-set",
            action="file-write-times",
            process="Google Chrome Helper (Aperitif)",
            command_line=cmd,
        )
        d2 = _denial(
            "/path/to/manifest.json",
            action="file-write-times",
            process="Google Chrome Helper (Aperitif)",
            command_line=cmd,
        )
        _, params = _parse_url(get_bug_url([d1, d2]))
        self.assertEqual(
            params["title"],
            "[macOS Sandbox] Unexpected denial: "
            "file-write-times /path/to/crl-set, /path/to/manifest.json",
        )
        self.assertIn(
            "Process type: utility (unzip.mojom.Unzipper)\n"
            "Seatbelt profile: utility.sb",
            params["description"],
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

    def test_process_and_profile_from_command_line(self):
        cases = (
            (
                "Chrome --type=utility --utility-sub-type=audio.mojom.Audio "
                "--service-sandbox-type=audio",
                "audio.mojom.Audio",
                "audio.sb",
            ),
            (
                "Chrome --type=utility --utility-sub-type=net.mojom.Net "
                "--service-sandbox-type=network",
                "net.mojom.Net",
                "network.sb",
            ),
            (
                "Chrome --type=utility --utility-sub-type=unzip.mojom.Unzipper "
                "--service-sandbox-type=service",
                "unzip.mojom.Unzipper",
                "utility.sb",
            ),
            (
                "Chrome --type=utility --utility-sub-type=foo.mojom.Foo "
                "--service-sandbox-type=service_with_jit",
                "foo.mojom.Foo",
                "utility.sb",
            ),
            (
                "Chrome --type=utility --utility-sub-type=foo.mojom.Foo "
                "--service-sandbox-type=platform_runtime",
                "foo.mojom.Foo",
                "utility.sb",
            ),
            (
                "Chrome --type=utility --utility-sub-type=foo.mojom.Foo "
                "--service-sandbox-type=utility",
                "foo.mojom.Foo",
                "utility.sb",
            ),
            (
                "Chrome --type=utility --utility-sub-type=video.mojom.Video "
                "--service-sandbox-type=none",
                "video.mojom.Video",
                "none",
            ),
            (
                "Chrome --type=utility --service-sandbox-type=service",
                None,
                "utility.sb",
            ),
            (
                "Chrome --type=utility --utility-sub-type=foo.mojom.Foo",
                "foo.mojom.Foo",
                "unknown",
            ),
        )
        for cmd, sub_type, expected_profile in cases:
            with self.subTest(cmd=cmd):
                denial = _denial(
                    "com.apple.foo",
                    process="Google Chrome Helper (Aperitif)",
                    command_line=cmd,
                )
                _, params = _parse_url(get_bug_url(denial))
                expected_type = (
                    f"utility ({sub_type})" if sub_type else "utility"
                )
                self.assertIn(
                    f"Process type: {expected_type}\n"
                    f"Seatbelt profile: {expected_profile}",
                    params["description"],
                )

    def test_non_utility_process_and_profile_from_command_line(self):
        cases = (
            ("Chrome --type=renderer", "renderer", "renderer.sb"),
            ("Chrome --type=renderer --no-sandbox", "renderer", "none"),
            ("Chrome --type=gpu-process", "gpu-process", "gpu.sb"),
            (
                "Chrome --type=gpu-process --disable-gpu-sandbox",
                "gpu-process",
                "none",
            ),
            (
                "Chrome --type=no-op-for-testing",
                "no-op-for-testing",
                "utility.sb",
            ),
            ("Chrome --type=relauncher", "relauncher", "none"),
        )
        for cmd, expected_type, expected_profile in cases:
            with self.subTest(cmd=cmd):
                denial = _denial(
                    "com.apple.foo",
                    process="Google Chrome Helper (Aperitif)",
                    command_line=cmd,
                )
                _, params = _parse_url(get_bug_url(denial))
                self.assertIn(
                    f"Process type: {expected_type}\n"
                    f"Seatbelt profile: {expected_profile}",
                    params["description"],
                )

    def test_process_and_profile_fallback_from_process_name(self):
        cases = (
            ("MTLCompilerService", "MTLCompilerService", "gpu.sb"),
            ("Chromium Helper (Renderer)", "renderer", "renderer.sb"),
            (
                "Google Chrome Helper (Aperitif Renderer)",
                "renderer",
                "renderer.sb",
            ),
            ("Google Chrome Helper (Aperitif R", "renderer", "renderer.sb"),
            ("Chromium Helper (Aperitif Render", "renderer", "renderer.sb"),
            ("Chromium Helper (GPU)", "gpu-process", "gpu.sb"),
            ("Google Chrome Helper (Aperitif GPU)", "gpu-process", "gpu.sb"),
            ("Google Chrome Helper (Aperitif G", "gpu-process", "gpu.sb"),
            ("Google Chrome Helper (Aperitif)", "unknown", "unknown"),
        )
        for proc_name, expected_type, expected_profile in cases:
            with self.subTest(proc_name=proc_name):
                denial = _denial("com.apple.foo", process=proc_name)
                _, params = _parse_url(get_bug_url(denial))
                self.assertIn(
                    f"Process type: {expected_type}\n"
                    f"Seatbelt profile: {expected_profile}",
                    params["description"],
                )

    def test_get_seatbelt_profile(self):
        denial_renderer = _denial(
            "com.apple.foo",
            process="Chromium Helper (Renderer)",
        )
        self.assertEqual(get_seatbelt_profile(denial_renderer), "renderer.sb")
        self.assertEqual(
            _get_process_and_profile(denial_renderer),
            ("renderer", "renderer.sb"),
        )

        denial_utility = _denial(
            "com.apple.foo",
            process="Google Chrome Helper (Aperitif)",
            command_line=(
                "Google Chrome Helper --type=utility "
                "--utility-sub-type=unzip.mojom.Unzipper "
                "--service-sandbox-type=service"
            ),
        )
        self.assertEqual(get_seatbelt_profile(denial_utility), "utility.sb")
        self.assertEqual(
            _get_process_and_profile(denial_utility),
            ("utility (unzip.mojom.Unzipper)", "utility.sb"),
        )

        denial_gpu = _denial(
            "com.apple.foo",
            process="MTLCompilerService",
        )
        self.assertEqual(get_seatbelt_profile(denial_gpu), "gpu.sb")

        denial_unknown = _denial(
            "com.apple.foo",
            process="Google Chrome Helper (Aperitif)",
        )
        self.assertEqual(get_seatbelt_profile(denial_unknown), "unknown")

    def test_long_url_truncates_message_block_within_max_length(self):
        denials = [
            _denial(
                f"/private/var/folders/xx/long_directory_name_{i}/file_{i}.txt",
                action="file-read-data",
            )
            for i in range(150)
        ]
        url = get_bug_url(denials)
        self.assertLessEqual(len(url), _MAX_BUG_URL_LENGTH)
        _, params = _parse_url(url)
        description = params["description"]
        self.assertIn("Process type: renderer\n", description)
        self.assertIn("Seatbelt profile: renderer.sb\n", description)
        self.assertIn("\n... (truncated)\n```", description)
        self.assertIn("macOS version: 15.1", description)
        self.assertIn(
            f"{_CONTEXT_QUESTION}\n{_CONTEXT_PLACEHOLDER}", description
        )
        self.assertIn(_PRIVACY_REMINDER, description)


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
