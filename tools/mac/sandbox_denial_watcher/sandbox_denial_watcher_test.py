#!/usr/bin/env python3

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for sandbox_denial_watcher.py.

Log lines are fed through the same parse, filter, and report steps that
watch_logs() uses, with the macOS dialog and `log stream` process mocked out.
"""

from __future__ import annotations

from dataclasses import replace
import io
import json
import math
import subprocess
import sys
from typing import Optional, Union
import unittest
from unittest.mock import MagicMock, patch

from bug_tracker import get_bug_url
from log_events import SandboxDenial
from sandbox_denial_watcher import (
    _CHROME_APP_NAMES,
    _CHROME_PRODUCT_DIRS,
    _DIALOG_GIVE_UP_SECONDS,
    _DIALOG_SCRIPT,
    _DISMISS_BUTTON_TITLE,
    _FILE_BUG_BUTTON_TITLE,
    _HOME_DIR,
    _INTERESTING_PROCS,
    _OSASCRIPT_PATH,
    _PS_PATH,
    DenialFilter,
    SandboxDenialWatcher,
    _get_process_command_line,
    _matches_action,
    _validate_process_name,
    main,
    prompt_to_file_bug,
)


def _denial_line(
    pattern: str, process: str = "Chromium", pid: int = 100
) -> bytes:
    return json.dumps(
        {
            "eventMessage": f"Sandbox: {process}({pid}) deny(1) {pattern}",
            "processID": pid,
        }
    ).encode("utf-8")


def _denial(
    pattern: str,
    process: str = "Chromium",
    pid: int = 100,
    command_line: Optional[str] = None,
) -> SandboxDenial:
    denial = SandboxDenial.from_json(
        _denial_line(pattern, process=process, pid=pid)
    )
    assert denial is not None
    if command_line is not None:
        denial = replace(denial, command_line=command_line)
    return denial


def _mock_log_process(
    mock_popen: MagicMock,
    stdout: Union[str, bytes],
    returncode: Optional[int] = None,
) -> MagicMock:
    """Configures `mock_popen` to return a fake `log stream` process."""
    proc = MagicMock()
    proc.__enter__.return_value = proc
    proc.__exit__.return_value = False
    stdout_bytes = stdout.encode("utf-8") if isinstance(stdout, str) else stdout
    proc.stdout = io.BytesIO(stdout_bytes)
    proc.returncode = returncode
    proc.poll.side_effect = lambda: proc.returncode

    def _terminate() -> None:
        proc.returncode = -15

    proc.terminate.side_effect = _terminate
    mock_popen.return_value = proc
    return proc


class WatcherTestCase(unittest.TestCase):
    """Base class providing a fresh filter and fake clock per test."""

    # pylint: disable=protected-access

    def setUp(self) -> None:
        super().setUp()
        self.now = 1000.0
        timer_patcher = patch("sandbox_denial_watcher.threading.Timer")
        self.mock_timer = timer_patcher.start()
        self.addCleanup(timer_patcher.stop)
        cmdline_patcher = patch(
            "sandbox_denial_watcher._get_process_command_line",
            return_value=None,
        )
        self.mock_get_cmdline = cmdline_patcher.start()
        self.addCleanup(cmdline_patcher.stop)
        self.denial_filter = self.make_filter()
        self.watcher = SandboxDenialWatcher(denial_filter=self.denial_filter)

    def make_filter(self) -> DenialFilter:
        """Returns a DenialFilter that reads time from `self.now`."""
        return DenialFilter(clock=lambda: self.now)

    def feed(self, *lines: bytes) -> None:
        """Handles each of `lines` as if read from `log stream`."""
        for line in lines:
            self.watcher._handle_log_line(line)
        self.watcher._flush_all_batches()


class ValidationTest(unittest.TestCase):
    """Verifies validation of process names used in log filter predicates."""

    def test_accepts_valid_names(self):
        for name in ("Chromium", "Google Chrome", "MTLCompilerService"):
            _validate_process_name(name)

    def test_rejects_dangerous_characters(self):
        for name in ('bad"proc', "bad\\proc", "bad☃proc"):
            with self.subTest(name=name):
                with self.assertRaises(ValueError):
                    _validate_process_name(name)


class ActionMatchingTest(unittest.TestCase):
    """Verifies action matching for exact actions and parent operations."""

    def test_exact_match(self):
        self.assertTrue(_matches_action("mach-lookup", "mach-lookup"))
        self.assertTrue(_matches_action("file-read-data", "file-read-data"))
        self.assertFalse(_matches_action("mach-lookup", "sysctl-read"))

    def test_parent_operation_wildcard(self):
        self.assertTrue(_matches_action("file-write-data", "file-write*"))
        self.assertTrue(_matches_action("file-write-create", "file-write*"))
        self.assertTrue(_matches_action("file-write", "file-write*"))
        self.assertFalse(_matches_action("file-read-data", "file-write*"))
        self.assertFalse(_matches_action("sysctl-read", "file-write*"))

    def test_midstring_wildcards_and_question_marks_not_treated_as_globs(self):
        self.assertFalse(_matches_action("file-read-data", "file-*-data"))
        self.assertFalse(_matches_action("file-read-data", "file-????-data"))
        self.assertTrue(_matches_action("file-*-data", "file-*-data"))


class FilteringTest(WatcherTestCase):
    """Verifies whether incoming log lines are reported."""

    # pylint: disable=protected-access

    @patch.object(SandboxDenialWatcher, "_report_denial")
    def test_non_denial_or_malformed_log_line_is_not_reported(
        self, mock_report
    ):
        with self.assertLogs("sandbox_denial_watcher", level="DEBUG") as cm:
            self.feed(b"")
            self.feed(b"{not json")
            self.feed(
                json.dumps({"eventMessage": 12345, "processID": 1234}).encode(
                    "utf-8"
                )
            )
            self.feed(
                json.dumps(
                    {
                        "eventMessage": "kernel: normal message without denial",
                        "processID": 1234,
                    }
                ).encode("utf-8")
            )
        mock_report.assert_not_called()
        self.assertIn(
            "Skipping unparsable line: b'{not json'", "\n".join(cm.output)
        )

    @patch.object(SandboxDenialWatcher, "_report_denial")
    def test_non_target_process_denial_is_not_reported(self, mock_report):
        self.feed(
            _denial_line(
                "mach-lookup com.apple.diagnosticd",
                process="siriknowledged",
                pid=500,
            )
        )
        mock_report.assert_not_called()

    @patch.object(SandboxDenialWatcher, "_report_denial")
    def test_known_denials_target_and_subpath_matching(self, mock_report):
        self.mock_get_cmdline.side_effect = lambda pid: (
            "Chrome --type=utility --utility-sub-type=unzip.mojom.Unzipper "
            "--service-sandbox-type=service"
            if pid == 24235
            else "Chrome --type=renderer"
            if pid == 24236
            else None
        )
        unpacker_path = (
            "/private/var/folders/x7/24fct7055y710wkdytb4f8k000ml0d/T/"
            "org.chromium.Chromium.chromecrx_chrome_Unpacker_BeginUnzipping"
            ".12345/file.js"
        )
        self.feed(
            _denial_line(
                "mach-lookup com.apple.diagnosticd", process="Google Chrome"
            ),
            _denial_line(
                "file-read-data /", process="Chromium Helper (Renderer)"
            ),
            _denial_line(
                "file-read-data "
                "/Library/Managed Preferences/com.apple.foo.plist"
            ),
            _denial_line(f"file-write-times {unpacker_path}", pid=24235),
        )
        mock_report.assert_not_called()

        # Subpaths of target rules and sibling directories of subpath
        # rules are still reported.
        self.feed(_denial_line("file-read-data /unexpected/path/to/file"))
        self.feed(
            _denial_line(
                "file-read-data "
                "/Library/Managed PreferencesBackup/com.apple.foo.plist"
            )
        )
        # Denials with a non-matching profile command line are also reported.
        self.feed(_denial_line(f"file-write-times {unpacker_path}", pid=24236))
        self.assertEqual(mock_report.call_count, 3)

    @patch.object(SandboxDenialWatcher, "_report_denial")
    def test_home_dir_denials_are_filtered(self, mock_report):
        self.feed(_denial_line(f"file-read-data {_HOME_DIR}/Desktop"))
        self.feed(_denial_line(f"file-read-data {_HOME_DIR}/Desktop/"))
        self.feed(
            _denial_line(
                f"file-read-data {_HOME_DIR}/Library/Preferences/"
                "com.apple.foo.plist"
            )
        )
        mock_report.assert_not_called()

    @patch.object(SandboxDenialWatcher, "_report_denial")
    def test_unexpected_denial_is_reported(self, mock_report):
        self.feed(
            _denial_line(
                "mach-lookup org.chromium.Chromium.MachPortRendezvousServer.1",
                process="Chromium Helper (Alerts)",
                pid=3456,
            )
        )
        mock_report.assert_called_once()
        denial = mock_report.call_args[0][0]
        self.assertEqual(denial.process_name, "Chromium Helper (Alerts)")
        self.assertEqual(denial.pid, 3456)
        self.assertEqual(denial.action, "mach-lookup")
        self.assertEqual(
            denial.target,
            "org.chromium.Chromium.MachPortRendezvousServer.1",
        )

    @patch("sandbox_denial_watcher.prompt_to_file_bug")
    def test_report_denial_logs_and_prompts(self, mock_prompt):
        denial = _denial("mach-lookup com.apple.foo")
        with self.assertLogs("sandbox_denial_watcher", level="INFO") as cm:
            self.watcher._report_denial(denial)
            self.watcher._flush_batch((denial.process_name, denial.action))
        self.assertEqual(
            cm.output,
            [
                "INFO:sandbox_denial_watcher:[DENIAL] "
                "Sandbox: Chromium(100) deny(1) mach-lookup com.apple.foo",
                f"INFO:sandbox_denial_watcher:[REPORT] {get_bug_url([denial])}",
            ],
        )
        mock_prompt.assert_called_once_with([denial])

    @patch("sandbox_denial_watcher.prompt_to_file_bug", return_value=True)
    def test_report_denial_deduplicates_prompts(self, mock_prompt):
        line = _denial_line("mach-lookup com.apple.foo")
        self.feed(line)
        self.feed(line)
        mock_prompt.assert_called_once_with(
            [_denial("mach-lookup com.apple.foo")]
        )

    @patch("sandbox_denial_watcher.prompt_to_file_bug")
    def test_report_denial_batches_multiple_denials(self, mock_prompt):
        d1 = _denial("file-write-times /path/to/crl-set")
        d2 = _denial("file-write-times /path/to/manifest.json")
        with self.assertLogs("sandbox_denial_watcher", level="INFO") as cm:
            self.watcher._report_denial(d1)
            self.watcher._report_denial(d2)
            self.watcher._flush_batch((d1.process_name, d1.action))
        self.assertEqual(
            cm.output,
            [
                f"INFO:sandbox_denial_watcher:[DENIAL] {d1.event_message}",
                f"INFO:sandbox_denial_watcher:[DENIAL] {d2.event_message}",
                f"INFO:sandbox_denial_watcher:[REPORT] {get_bug_url([d1, d2])}",
            ],
        )
        mock_prompt.assert_called_once_with([d1, d2])

    @patch("sandbox_denial_watcher.prompt_to_file_bug")
    def test_report_denial_attaches_command_line_once_per_batch(
        self, mock_prompt
    ):
        cmd = "Chromium Helper --type=utility --service-sandbox-type=service"
        self.mock_get_cmdline.return_value = cmd
        denial1 = _denial("file-write-times /path/to/crl-set", pid=42)
        denial2 = _denial("file-write-times /path/to/manifest.json", pid=42)
        self.watcher._report_denial(denial1)
        self.watcher._report_denial(denial2)
        self.watcher._flush_batch((denial1.process_name, denial1.action))

        self.mock_get_cmdline.assert_called_once_with(42)
        mock_prompt.assert_called_once_with(
            [replace(denial1, command_line=cmd), denial2]
        )


class ProcessCommandLineTest(unittest.TestCase):
    """Verifies querying process command lines with `/bin/ps`."""

    @patch("sandbox_denial_watcher.subprocess.run")
    def test_returns_stripped_command_line_on_success(self, mock_run):
        mock_run.return_value = MagicMock(
            returncode=0,
            stdout="  Chromium Helper --type=renderer  \n",
        )
        self.assertEqual(
            _get_process_command_line(123),
            "Chromium Helper --type=renderer",
        )
        args, _ = mock_run.call_args
        self.assertEqual(args[0], [_PS_PATH, "-p", "123", "-o", "command="])

    @patch("sandbox_denial_watcher.subprocess.run")
    def test_returns_none_on_nonzero_exit(self, mock_run):
        mock_run.return_value = MagicMock(returncode=1, stdout="")
        self.assertIsNone(_get_process_command_line(123))

    @patch("sandbox_denial_watcher.subprocess.run")
    def test_returns_none_on_subprocess_error(self, mock_run):
        mock_run.side_effect = subprocess.TimeoutExpired(cmd="ps", timeout=2)
        self.assertIsNone(_get_process_command_line(123))


class DenialFilterTest(WatcherTestCase):
    """Verifies DenialFilter's known-denial and suppression bookkeeping."""

    def test_known_denial_match_is_logged(self):
        with self.assertLogs("sandbox_denial_watcher", level="DEBUG") as cm:
            self.assertTrue(
                self.denial_filter.should_ignore(
                    _denial("mach-lookup com.apple.diagnosticd")
                )
            )
        self.assertIn(
            "Filtered known denial matching "
            "'mach-lookup com.apple.diagnosticd'",
            "\n".join(cm.output),
        )

    def test_target_match(self):
        self.assertTrue(
            self.denial_filter.should_ignore(
                _denial("mach-lookup com.apple.diagnosticd")
            )
        )
        self.assertTrue(
            self.denial_filter.should_ignore(
                _denial(
                    "file-issue-extension "
                    "target:/ extension-class:com.apple.app-sandbox.read"
                )
            )
        )
        self.assertFalse(
            self.denial_filter.should_ignore(
                _denial("mach-lookup com.apple.other")
            )
        )
        # Target rules scoped to specific profiles.
        self.assertTrue(
            self.denial_filter.should_ignore(
                _denial(
                    "sysctl-read kern.bootargs",
                    process="Chromium Helper (GPU)",
                )
            )
        )
        self.assertFalse(
            self.denial_filter.should_ignore(
                _denial(
                    "sysctl-read kern.bootargs",
                    process="Chromium Helper (Renderer)",
                )
            )
        )
        audio_cmd = (
            "Chrome --type=utility "
            "--utility-sub-type=audio.mojom.AudioService "
            "--service-sandbox-type=audio"
        )
        self.assertTrue(
            self.denial_filter.should_ignore(
                _denial(
                    "mach-lookup com.apple.audioanalyticsd",
                    command_line=audio_cmd,
                )
            )
        )
        self.assertFalse(
            self.denial_filter.should_ignore(
                _denial(
                    "mach-lookup com.apple.audioanalyticsd",
                    process="Chromium Helper (Renderer)",
                )
            )
        )

    def test_target_match_root_path(self):
        self.assertTrue(
            self.denial_filter.should_ignore(
                _denial(
                    "file-read-data /", process="Chromium Helper (Renderer)"
                )
            )
        )
        self.assertFalse(
            self.denial_filter.should_ignore(_denial("file-read-data /tmp"))
        )
        self.assertFalse(
            self.denial_filter.should_ignore(
                _denial("file-read-data /", process="Chromium Helper (GPU)")
            )
        )

    def test_target_match_directory_with_trailing_slash(self):
        self.assertTrue(
            self.denial_filter.should_ignore(
                _denial(f"file-read-data {_HOME_DIR}/Desktop/")
            )
        )

    def test_subpaths_matching(self):
        self.assertTrue(
            self.denial_filter.should_ignore(
                _denial("file-read-data /Library/Managed Preferences")
            )
        )
        self.assertTrue(
            self.denial_filter.should_ignore(
                _denial("file-read-data /Library/Managed Preferences/")
            )
        )
        self.assertTrue(
            self.denial_filter.should_ignore(
                _denial(
                    "file-read-data "
                    "/Library/Managed Preferences/com.apple.foo.plist"
                )
            )
        )
        self.assertFalse(
            self.denial_filter.should_ignore(
                _denial(
                    "file-read-data "
                    "/Library/Managed PreferencesBackup/com.apple.foo.plist"
                )
            )
        )
        # Subpath rules scoped to specific profiles.
        raw_camera = (
            "/System/Library/CoreServices/RawCamera.bundle/Contents/Info.plist"
        )
        self.assertTrue(
            self.denial_filter.should_ignore(
                _denial(
                    f"file-read-data {raw_camera}",
                    process="Chromium Helper (Renderer)",
                )
            )
        )
        self.assertFalse(
            self.denial_filter.should_ignore(
                _denial(
                    f"file-read-data {raw_camera}",
                    process="Chromium Helper (GPU)",
                )
            )
        )
        odm_path = (
            "/System/Library/ExtensionKit/ExtensionPointDefinitions.sysexpt"
        )
        odm_cmd = (
            "Chrome --type=utility "
            "--utility-sub-type=on_device_model.mojom.OnDeviceModelService "
            "--service-sandbox-type=on_device_model_execution"
        )
        self.assertTrue(
            self.denial_filter.should_ignore(
                _denial(f"file-read-data {odm_path}", command_line=odm_cmd)
            )
        )
        self.assertFalse(
            self.denial_filter.should_ignore(
                _denial(
                    f"file-read-data {odm_path}",
                    command_line="Chrome --type=renderer",
                )
            )
        )

    @patch.dict(
        "sandbox_denial_watcher._KNOWN_SUBPATH_DENIALS",
        {"file-read-data": frozenset(("/Library/Managed Preferences",))},
        clear=True,
    )
    def test_subpaths_matching_without_trailing_slash_in_rule(self):
        self.assertTrue(
            self.denial_filter.should_ignore(
                _denial("file-read-data /Library/Managed Preferences")
            )
        )
        self.assertTrue(
            self.denial_filter.should_ignore(
                _denial(
                    "file-read-data "
                    "/Library/Managed Preferences/com.apple.foo.plist"
                )
            )
        )
        self.assertFalse(
            self.denial_filter.should_ignore(
                _denial(
                    "file-read-data "
                    "/Library/Managed PreferencesBackup/com.apple.foo.plist"
                )
            )
        )

    def test_chrome_app_support_subpath_matching_across_flavors(self):
        app_support = f"{_HOME_DIR}/Library/Application Support"
        for product in _CHROME_PRODUCT_DIRS:
            store = f"{app_support}/{product}/optimization_guide_model_store"
            with self.subTest(product=product):
                self.assertTrue(
                    self.denial_filter.should_ignore(
                        _denial(f"file-write-times {store}")
                    )
                )
                self.assertTrue(
                    self.denial_filter.should_ignore(
                        _denial(f"file-write-times {store}/")
                    )
                )
                self.assertTrue(
                    self.denial_filter.should_ignore(
                        _denial(f"file-write-times {store}/model-info.pb")
                    )
                )
                self.assertFalse(
                    self.denial_filter.should_ignore(
                        _denial(f"file-write-times {store}_other/model-info.pb")
                    )
                )
                self.assertFalse(
                    self.denial_filter.should_ignore(
                        _denial(
                            f"file-write-times {app_support}/{product}Backup/"
                            "optimization_guide_model_store/model-info.pb"
                        )
                    )
                )

    def test_chrome_bundle_subpath_matching_across_flavors(self):
        for app_name in _CHROME_APP_NAMES:
            bundle = f"/Applications/{app_name}"
            with self.subTest(app_name=app_name):
                self.assertTrue(
                    self.denial_filter.should_ignore(
                        _denial(
                            f"file-issue-extension target:{bundle}/Contents/"
                            "Resources"
                        )
                    )
                )
                self.assertFalse(
                    self.denial_filter.should_ignore(
                        _denial(
                            f"file-issue-extension target:{bundle}.bak/"
                            "Contents/Resources"
                        )
                    )
                )

    def test_suppression_applies_across_chrome_flavors(self):
        canary_denial = _denial(
            f"file-read-data {_HOME_DIR}/Library/Application Support/"
            "Google/Chrome Canary/Default/Cookies"
        )
        chromium_denial = _denial(
            f"file-read-data {_HOME_DIR}/Library/Application Support/"
            "Chromium/Default/Cookies"
        )
        self.denial_filter.suppress(canary_denial, 100)
        self.assertTrue(self.denial_filter.should_ignore(chromium_denial))

    def test_darwin_user_dir_target_wildcard_matching(self):
        user1_tmp = "/private/var/folders/tr/6v8rtgkn5399hj49myh4zh3m00czwq/T"
        user2_tmp = "/private/var/folders/lf/6xnpwtqx06xg8dcxbrdlpwqw005_lc/T"
        renderer_proc = "Chromium Helper (Renderer)"
        self.assertTrue(
            self.denial_filter.should_ignore(
                _denial(
                    f"file-write-create {user1_tmp}/tmp.Tq7AOs",
                    process=renderer_proc,
                )
            )
        )
        self.assertTrue(
            self.denial_filter.should_ignore(
                _denial(
                    f"file-write-create {user2_tmp}/tmp.12345",
                    process=renderer_proc,
                )
            )
        )
        self.assertFalse(
            self.denial_filter.should_ignore(
                _denial(
                    f"file-write-create {user1_tmp}/tmp.Tq7AOs/nested",
                    process=renderer_proc,
                )
            )
        )
        self.assertFalse(
            self.denial_filter.should_ignore(
                _denial(
                    f"file-write-create {user1_tmp}/other.Tq7AOs",
                    process=renderer_proc,
                )
            )
        )
        # Matches speech recognition utility process, but not GPU.
        speech_cmd = (
            "Google Chrome Helper --type=utility "
            "--utility-sub-type=media.mojom.SpeechRecognitionService "
            "--service-sandbox-type=speech_recognition"
        )
        self.assertTrue(
            self.denial_filter.should_ignore(
                _denial(
                    f"file-write-create {user1_tmp}/tmp.1UQjEM",
                    process="Google Chrome Helper (Aperitif)",
                    command_line=speech_cmd,
                )
            )
        )
        self.assertFalse(
            self.denial_filter.should_ignore(
                _denial(
                    f"file-write-create {user1_tmp}/tmp.Tq7AOs",
                    process="Chromium Helper (GPU)",
                )
            )
        )

    def test_darwin_user_dir_subpath_wildcard_matching(self):
        user_root = "/private/var/folders/lf/6xnpwtqx06xg8dcxbrdlpwqw005_lc"
        crx_dir = f"{user_root}/T/scoped_dirgy10KM/CRX_INSTALL"
        unzip_dir = (
            f"{user_root}/T/org.chromium.Chromium.chromecrx_chrome"
            "_Unpacker_BeginUnzipping.12345"
        )
        utility_cmd = (
            "Chrome --type=utility --utility-sub-type=unzip.mojom.Unzipper "
            "--service-sandbox-type=service"
        )
        # CRX_INSTALL and unpacker subpaths match file-write* actions for
        # utility processes.
        for action in (
            "file-write-mode",
            "file-write-times",
            "file-write-create",
        ):
            self.assertTrue(
                self.denial_filter.should_ignore(
                    _denial(f"{action} {crx_dir}", command_line=utility_cmd)
                )
            )
        self.assertTrue(
            self.denial_filter.should_ignore(
                _denial(
                    f"file-write-times {unzip_dir}/file.js",
                    command_line=utility_cmd,
                )
            )
        )
        # File reads and non-matching directory patterns are not ignored.
        self.assertFalse(
            self.denial_filter.should_ignore(
                _denial(
                    f"file-read-data {crx_dir}/manifest.json",
                    command_line=utility_cmd,
                )
            )
        )
        self.assertFalse(
            self.denial_filter.should_ignore(
                _denial(
                    f"file-write-mode {crx_dir}_OTHER/manifest.json",
                    command_line=utility_cmd,
                )
            )
        )
        self.assertFalse(
            self.denial_filter.should_ignore(
                _denial(
                    f"file-write-mode {user_root}/T/scoped_dirgy10KM/nested/"
                    "CRX_INSTALL/manifest.json",
                    command_line=utility_cmd,
                )
            )
        )
        self.assertFalse(
            self.denial_filter.should_ignore(
                _denial(
                    f"file-write-times {user_root}/T/other_dir/file.js",
                    command_line=utility_cmd,
                )
            )
        )
        # Denials from non-utility processes are not ignored.
        self.assertFalse(
            self.denial_filter.should_ignore(
                _denial(
                    f"file-write-mode {crx_dir}",
                    command_line="Chrome --type=renderer",
                )
            )
        )

    def test_suppression_applies_across_darwin_user_dirs(self):
        user1_denial = _denial(
            "file-read-data /private/var/folders/tr/"
            "6v8rtgkn5399hj49myh4zh3m00czwq/0/unexpected"
        )
        user2_denial = _denial(
            "file-read-data /private/var/folders/lf/"
            "6xnpwtqx06xg8dcxbrdlpwqw005_lc/0/unexpected"
        )
        self.denial_filter.suppress(user1_denial, 100)
        self.assertTrue(self.denial_filter.should_ignore(user2_denial))

    def test_non_positive_duration_does_not_suppress(self):
        denial = _denial("mach-lookup com.apple.unknown")
        self.denial_filter.suppress(denial, 0)
        self.denial_filter.suppress(denial, -5)
        self.assertFalse(self.denial_filter.should_ignore(denial))

    def test_suppression_is_extended_but_never_shortened(self):
        denial = _denial("mach-lookup com.apple.unknown")
        self.denial_filter.suppress(denial, 100)
        self.denial_filter.suppress(denial, 10)
        self.now += 50
        self.assertTrue(self.denial_filter.should_ignore(denial))

        self.denial_filter.suppress(denial, 200)
        self.now += 100
        self.assertTrue(self.denial_filter.should_ignore(denial))

        self.denial_filter.suppress(denial, math.inf)
        self.now += 1e9
        self.assertTrue(self.denial_filter.should_ignore(denial))

    def test_expired_suppression_is_removed(self):
        denial = _denial("mach-lookup com.apple.unknown")
        self.denial_filter.suppress(denial, 10)
        self.now += 10
        self.assertFalse(self.denial_filter.should_ignore(denial))

        # Once removed, a shorter suppression can be added again.
        self.denial_filter.suppress(denial, 5)
        self.assertTrue(self.denial_filter.should_ignore(denial))

    def test_filters_do_not_share_suppressions(self):
        denial = _denial("mach-lookup com.apple.unknown")
        self.denial_filter.suppress(denial, math.inf)
        self.assertFalse(self.make_filter().should_ignore(denial))


class SuppressionTest(WatcherTestCase):
    """Verifies how dialog outcomes suppress future notifications."""

    # pylint: disable=protected-access

    @patch(
        "sandbox_denial_watcher.prompt_to_file_bug",
        return_value=False,
    )
    def test_dismissed_denial_respects_cooldown_and_renotifies_after_expiry(
        self, mock_prompt
    ):
        line_pid_1 = _denial_line(
            "mach-lookup com.apple.unrecognized.service", pid=100
        )
        line_pid_2 = _denial_line(
            "mach-lookup com.apple.unrecognized.service", pid=200
        )

        self.now = 1000.0
        self.feed(line_pid_1)
        self.assertEqual(mock_prompt.call_count, 1)

        # Within cooldown window, even from a different PID, no notification.
        self.now = 2800.0
        with self.assertLogs("sandbox_denial_watcher", level="DEBUG") as cm:
            self.feed(line_pid_2)
        self.assertTrue(
            any(
                "Filtered suppressed denial matching "
                "'mach-lookup com.apple.unrecognized.service' (for 1800.0s)"
                in m
                for m in cm.output
            )
        )
        self.assertEqual(mock_prompt.call_count, 1)

        # A prefix-overlapping target is not suppressed by the dismissed entry.
        self.feed(
            _denial_line("mach-lookup com.apple.unrecognized.service.helper")
        )
        self.assertEqual(mock_prompt.call_count, 2)

        # After cooldown window expires, notification triggers again.
        self.now = 4601.0
        self.feed(line_pid_2)
        self.assertEqual(mock_prompt.call_count, 3)

    @patch(
        "sandbox_denial_watcher.prompt_to_file_bug",
        return_value=True,
    )
    def test_filed_bug_suppresses_denial_for_entire_session(self, mock_prompt):
        line = _denial_line("mach-lookup com.apple.filed.service")

        self.now = 1000.0
        self.feed(line)
        self.assertEqual(mock_prompt.call_count, 1)

        # A shorter cooldown does not overwrite a session-long suppression.
        self.denial_filter.suppress(mock_prompt.call_args[0][0], 60.0)

        # Even well past the cooldown window, a filed bug is not re-notified.
        self.now = 999999.0
        with self.assertLogs("sandbox_denial_watcher", level="DEBUG") as cm:
            self.feed(line)
        self.assertTrue(
            any(
                "Filtered suppressed denial matching "
                "'mach-lookup com.apple.filed.service' "
                "(for the remainder of the session)" in m
                for m in cm.output
            )
        )
        self.assertEqual(mock_prompt.call_count, 1)

    @patch("sandbox_denial_watcher.subprocess.run")
    def test_timed_out_dialog_respects_cooldown(self, mock_run):
        mock_run.return_value = MagicMock(
            returncode=0,
            stdout="button returned:, gave up:true\n",
        )
        line = _denial_line("mach-lookup com.apple.timeout.service")
        self.now = 1000.0
        self.feed(line)
        self.assertEqual(mock_run.call_count, 1)

        # Within cooldown window, the denial is suppressed.
        self.now = 2800.0
        self.feed(line)
        self.assertEqual(mock_run.call_count, 1)

        # After cooldown expires, dialog triggers again.
        self.now = 4601.0
        self.feed(line)
        self.assertEqual(mock_run.call_count, 2)


class LogStreamTest(WatcherTestCase):
    """Verifies the `log stream` command and the watch loop lifecycle."""

    # pylint: disable=protected-access

    def test_build_log_command(self):
        cmd = self.watcher._build_log_command()
        self.assertEqual(
            cmd[:5],
            ["/usr/bin/log", "stream", "--style", "ndjson", "--predicate"],
        )
        predicate = cmd[5]
        self.assertIn(
            'eventMessage CONTAINS "Sandbox" AND eventMessage CONTAINS "deny"',
            predicate,
        )
        for proc in _INTERESTING_PROCS:
            self.assertIn(f'eventMessage CONTAINS "{proc}"', predicate)

    @patch.object(SandboxDenialWatcher, "_report_denial")
    @patch("sandbox_denial_watcher.subprocess.Popen")
    def test_watch_logs_reports_denials_and_stops_at_end_of_stream(
        self, mock_popen, mock_report
    ):
        proc = _mock_log_process(
            mock_popen,
            _denial_line("mach-lookup com.apple.unexpected") + b"\n",
            returncode=0,
        )

        self.assertEqual(self.watcher.watch_logs(), 0)

        mock_popen.assert_called_once_with(
            self.watcher._build_log_command(),
            stdout=subprocess.PIPE,
        )
        mock_report.assert_called_once()
        proc.__exit__.assert_called_once()
        proc.terminate.assert_not_called()

    @patch.object(
        SandboxDenialWatcher,
        "_handle_log_line",
        side_effect=KeyboardInterrupt,
    )
    @patch("sandbox_denial_watcher.subprocess.Popen")
    def test_watch_logs_stops_on_keyboard_interrupt(
        self, mock_popen, _mock_handle
    ):
        proc = _mock_log_process(
            mock_popen,
            _denial_line("mach-lookup com.apple.unexpected") + b"\n",
        )

        with self.assertLogs("sandbox_denial_watcher", level="INFO") as cm:
            self.assertEqual(self.watcher.watch_logs(), 0)

        self.assertIn("Stopped watching.", "\n".join(cm.output))
        self.assertNotIn("ERROR", "\n".join(cm.output))
        proc.terminate.assert_called_once()

    @patch.object(
        SandboxDenialWatcher,
        "_handle_log_line",
        side_effect=KeyboardInterrupt,
    )
    @patch("sandbox_denial_watcher.subprocess.Popen")
    def test_watch_logs_kills_log_process_if_terminate_times_out(
        self, mock_popen, _mock_handle
    ):
        proc = _mock_log_process(mock_popen, b"")
        proc.wait.side_effect = [
            subprocess.TimeoutExpired(cmd="log", timeout=5),
            0,
        ]

        self.watcher.watch_logs()

        proc.terminate.assert_called_once()
        proc.kill.assert_called_once()

    @patch("sandbox_denial_watcher.subprocess.Popen")
    def test_watch_logs_reports_log_process_failure(self, mock_popen):
        proc = _mock_log_process(mock_popen, b"", returncode=64)

        with self.assertLogs("sandbox_denial_watcher", level="ERROR") as cm:
            self.assertEqual(self.watcher.watch_logs(), 1)

        self.assertEqual(
            cm.output,
            [
                "ERROR:sandbox_denial_watcher:/usr/bin/log exited "
                "unexpectedly with code 64."
            ],
        )
        proc.terminate.assert_not_called()

    @patch("sandbox_denial_watcher.subprocess.Popen")
    def test_watch_logs_reports_log_process_stopped_by_signal(self, mock_popen):
        proc = _mock_log_process(mock_popen, b"", returncode=-15)

        with self.assertLogs("sandbox_denial_watcher", level="ERROR") as cm:
            self.assertEqual(self.watcher.watch_logs(), 1)

        self.assertEqual(
            cm.output,
            [
                "ERROR:sandbox_denial_watcher:/usr/bin/log exited "
                "unexpectedly with code -15."
            ],
        )
        proc.terminate.assert_not_called()

    @unittest.skipUnless(sys.platform == "darwin", "Requires macOS")
    @patch("sandbox_denial_watcher._LOG_PATH", "/usr/bin/false")
    def test_watch_logs_reports_real_process_failure(self):
        with self.assertLogs("sandbox_denial_watcher", level="ERROR") as cm:
            self.assertEqual(self.watcher.watch_logs(), 1)

        self.assertEqual(
            cm.output,
            [
                "ERROR:sandbox_denial_watcher:/usr/bin/false exited "
                "unexpectedly with code 1."
            ],
        )

    @unittest.skipUnless(sys.platform == "darwin", "Requires macOS")
    @patch("sandbox_denial_watcher._LOG_PATH", "/usr/bin/true")
    def test_watch_logs_accepts_real_process_success(self):
        with self.assertLogs("sandbox_denial_watcher", level="INFO") as cm:
            self.assertEqual(self.watcher.watch_logs(), 0)
        self.assertFalse(any("ERROR" in record for record in cm.output))


class PromptToFileBugTest(unittest.TestCase):
    """Verifies the macOS dialog invocation and response handling."""

    @patch("sandbox_denial_watcher.subprocess.run")
    @patch("sandbox_denial_watcher.webbrowser.open")
    def test_prompt_files_bug_when_file_bug_clicked(
        self, mock_browser_open, mock_run
    ):
        denial = _denial("mach-lookup com.apple.foo")
        mock_run.return_value = MagicMock(
            returncode=0,
            stdout=f"button returned:{_FILE_BUG_BUTTON_TITLE}, gave up:false\n",
        )
        self.assertTrue(prompt_to_file_bug(denial))
        mock_browser_open.assert_called_once_with(get_bug_url([denial]))

    @patch("sandbox_denial_watcher.subprocess.run")
    @patch("sandbox_denial_watcher.webbrowser.open")
    def test_prompt_dismissed_when_dismiss_clicked(
        self, mock_browser_open, mock_run
    ):
        denial = _denial("mach-lookup com.apple.foo")
        mock_run.return_value = MagicMock(
            returncode=0,
            stdout=f"button returned:{_DISMISS_BUTTON_TITLE}, gave up:false\n",
        )
        self.assertFalse(prompt_to_file_bug(denial))
        mock_browser_open.assert_not_called()

    @patch("sandbox_denial_watcher.subprocess.run")
    @patch("sandbox_denial_watcher.webbrowser.open")
    def test_prompt_times_out_when_dialog_gives_up(
        self, mock_browser_open, mock_run
    ):
        denial = _denial("mach-lookup com.apple.foo")
        mock_run.return_value = MagicMock(
            returncode=0,
            stdout="button returned:, gave up:true\n",
        )
        self.assertFalse(prompt_to_file_bug(denial))
        mock_browser_open.assert_not_called()

    @patch("sandbox_denial_watcher.subprocess.run")
    @patch("sandbox_denial_watcher.webbrowser.open")
    def test_prompt_times_out_when_subprocess_times_out(
        self, mock_browser_open, mock_run
    ):
        denial = _denial("mach-lookup com.apple.foo")
        mock_run.side_effect = subprocess.TimeoutExpired(
            cmd=["osascript"], timeout=20
        )
        self.assertFalse(prompt_to_file_bug(denial))
        mock_browser_open.assert_not_called()

    @patch("sandbox_denial_watcher.subprocess.run")
    @patch("sandbox_denial_watcher.webbrowser.open")
    def test_prompt_dismissed_on_nonzero_exit(
        self, mock_browser_open, mock_run
    ):
        denial = _denial("mach-lookup com.apple.foo")
        mock_run.return_value = MagicMock(
            returncode=1,
            stdout="",
            stderr="execution error: User canceled. (-128)",
        )
        self.assertFalse(prompt_to_file_bug(denial))
        mock_browser_open.assert_not_called()

    @patch("sandbox_denial_watcher.subprocess.run")
    def test_prompt_invokes_osascript_with_expected_arguments(self, mock_run):
        denial = _denial("mach-lookup com.apple.foo")
        mock_run.return_value = MagicMock(returncode=0, stdout="")

        prompt_to_file_bug(denial)

        args, kwargs = mock_run.call_args
        argv = args[0]
        self.assertEqual(argv[0], _OSASCRIPT_PATH)
        self.assertEqual(argv[1], "-e")
        self.assertEqual(argv[2], _DIALOG_SCRIPT)
        self.assertEqual(
            argv[3],
            "Chromium: mach-lookup com.apple.foo",
        )
        self.assertEqual(
            argv[4:],
            [
                _DISMISS_BUTTON_TITLE,
                _FILE_BUG_BUTTON_TITLE,
                str(_DIALOG_GIVE_UP_SECONDS),
            ],
        )
        self.assertEqual(kwargs["encoding"], "utf-8")
        self.assertEqual(kwargs["errors"], "replace")
        self.assertEqual(kwargs["timeout"], _DIALOG_GIVE_UP_SECONDS + 5)

    @patch("sandbox_denial_watcher.subprocess.run")
    def test_prompt_invokes_osascript_with_batch_dialog_text(self, mock_run):
        d1 = _denial("file-write-times /path/to/unpack/crl-set")
        d2 = _denial("file-write-times /path/to/unpack/manifest.json")
        mock_run.return_value = MagicMock(returncode=0, stdout="")

        prompt_to_file_bug([d1, d2])

        args, _ = mock_run.call_args
        self.assertEqual(
            args[0][3],
            "Chromium: file-write-times /path/to/unpack/crl-set, "
            "/path/to/unpack/manifest.json",
        )


@patch("sandbox_denial_watcher.logging.basicConfig")
class MainTest(unittest.TestCase):
    """Verifies the command-line entry point."""

    @patch("sandbox_denial_watcher.sys.platform", "linux")
    @patch.object(SandboxDenialWatcher, "watch_logs")
    def test_main_fails_when_not_on_macos(self, mock_watch, _mock_config):
        with self.assertLogs("sandbox_denial_watcher", level="ERROR") as cm:
            self.assertEqual(main([]), 1)

        self.assertEqual(
            cm.output,
            ["ERROR:sandbox_denial_watcher:This tool only runs on macOS."],
        )
        mock_watch.assert_not_called()

    @patch("sandbox_denial_watcher.sys.platform", "darwin")
    @patch.object(SandboxDenialWatcher, "watch_logs", return_value=1)
    def test_main_returns_watch_logs_result(self, mock_watch, _mock_config):
        self.assertEqual(main([]), 1)
        mock_watch.assert_called_once_with()

    @patch("sandbox_denial_watcher.sys.platform", "darwin")
    @patch.object(SandboxDenialWatcher, "watch_logs", return_value=0)
    def test_main_parses_verbose_flag(self, _mock_watch, mock_config):
        self.assertEqual(main(["-v"]), 0)
        mock_config.assert_called_once()
        self.assertEqual(mock_config.call_args[1]["level"], 10)  # DEBUG


if __name__ == "__main__":
    unittest.main()
