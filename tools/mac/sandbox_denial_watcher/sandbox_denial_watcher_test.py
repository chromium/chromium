#!/usr/bin/env python3

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for sandbox_denial_watcher.py.

Log lines are fed through the same parse and report steps that watch_logs()
uses, with the `log stream` process mocked out.
"""

import io
import json
import subprocess
import sys
from typing import Optional, Union
import unittest
from unittest.mock import MagicMock, patch

from log_events import SandboxDenial
from sandbox_denial_watcher import (
    SandboxDenialWatcher,
    _INTERESTING_PROCS,
    _validate_process_name,
    main,
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


def _denial(pattern: str, process: str = "Chromium") -> SandboxDenial:
    denial = SandboxDenial.from_json(_denial_line(pattern, process))
    assert denial is not None
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
    """Base class for tests that feed log lines to the watcher."""

    # pylint: disable=protected-access

    def setUp(self) -> None:
        super().setUp()
        self.watcher = SandboxDenialWatcher()

    def feed(self, *lines: bytes) -> None:
        """Handles each of `lines` as if read from `log stream`."""
        for line in lines:
            self.watcher._handle_log_line(line)


class ValidationTest(unittest.TestCase):
    """Verifies validation of process names used in log filter predicates."""

    def test_accepts_valid_names(self):
        for name in ("Chromium", "Google Chrome", "MTLCompilerService"):
            _validate_process_name(name)

    def test_rejects_dangerous_characters(self):
        for name in ('bad"proc', "bad\\proc", "bad\u2603proc"):
            with self.subTest(name=name):
                with self.assertRaises(ValueError):
                    _validate_process_name(name)


class FilteringTest(WatcherTestCase):
    """Verifies whether incoming log lines are reported."""

    # pylint: disable=protected-access

    @patch.object(SandboxDenialWatcher, "_report_denial")
    def test_non_denial_or_malformed_log_line_is_not_reported(
        self, mock_report
    ):
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
    def test_target_process_denial_is_reported(self, mock_report):
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

    def test_report_denial_prints_event_message(self):
        denial = _denial("mach-lookup com.apple.foo")
        with self.assertLogs("sandbox_denial_watcher", level="INFO") as cm:
            self.watcher._report_denial(denial)
        self.assertEqual(
            cm.output,
            [
                "INFO:sandbox_denial_watcher:[DENIAL] "
                "Sandbox: Chromium(100) deny(1) mach-lookup com.apple.foo"
            ],
        )


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
