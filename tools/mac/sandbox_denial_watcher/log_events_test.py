#!/usr/bin/env python3

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for log_events.py."""

import json
import unittest

from log_events import SandboxDenial


def _event_line(message: str) -> bytes:
    return json.dumps({"eventMessage": message, "processID": 1}).encode("utf-8")


class SandboxDenialParsingTest(unittest.TestCase):
    """Verifies parsing of sandbox denials from `log stream` ndjson lines."""

    def test_parses_denial_with_target(self):
        message = "Sandbox: Chromium(100) deny(1) mach-lookup com.apple.foo"
        self.assertEqual(
            SandboxDenial.from_json(_event_line(message)),
            SandboxDenial(
                event_message=message,
                process_name="Chromium",
                pid=100,
                action="mach-lookup",
                target="com.apple.foo",
            ),
        )

    def test_parses_denial_without_target(self):
        denial = SandboxDenial.from_json(
            _event_line("Sandbox: Chromium Helper(100) deny(1) process-fork")
        )
        self.assertIsNotNone(denial)
        self.assertEqual(denial.action, "process-fork")
        self.assertEqual(denial.target, "")

    def test_parses_denial_count_greater_than_one(self):
        denial = SandboxDenial.from_json(
            _event_line("Sandbox: Chromium(100) deny(12) sysctl-read kern.foo")
        )
        self.assertIsNotNone(denial)
        self.assertEqual(denial.action, "sysctl-read")
        self.assertEqual(denial.target, "kern.foo")

    def test_parses_process_name_with_parenthesized_words(self):
        denial = SandboxDenial.from_json(
            _event_line(
                "Sandbox: Chromium Helper (Renderer)(77) deny(1) "
                "mach-lookup com.apple.foo"
            )
        )
        self.assertIsNotNone(denial)
        self.assertEqual(denial.process_name, "Chromium Helper (Renderer)")
        self.assertEqual(denial.pid, 77)

    def test_parses_process_name_with_parenthesized_digits(self):
        denial = SandboxDenial.from_json(
            _event_line(
                "Sandbox: Chromium Helper (1)(4321) deny(1) "
                "file-read-data /tmp/foo"
            )
        )
        self.assertIsNotNone(denial)
        self.assertEqual(denial.process_name, "Chromium Helper (1)")
        self.assertEqual(denial.pid, 4321)
        self.assertEqual(denial.target, "/tmp/foo")

    def test_parses_target_containing_spaces(self):
        denial = SandboxDenial.from_json(
            _event_line(
                "Sandbox: Chromium(100) deny(1) file-read-data "
                "/Library/Managed Preferences/com.apple.foo.plist"
            )
        )
        self.assertIsNotNone(denial)
        self.assertEqual(
            denial.target, "/Library/Managed Preferences/com.apple.foo.plist"
        )

    def test_multiline_event_message_keeps_first_line(self):
        denial = SandboxDenial.from_json(
            _event_line(
                "Sandbox: Chromium(100) deny(1) mach-lookup com.apple.foo\n"
                "Process: Chromium [100]\nThread 0:\n0 libsystem_kernel.dylib"
            )
        )
        self.assertIsNotNone(denial)
        self.assertEqual(
            denial.event_message,
            "Sandbox: Chromium(100) deny(1) mach-lookup com.apple.foo",
        )

    def test_parses_duplicate_report_summaries(self):
        for message in (
            "1 duplicate report for Sandbox: Chromium(100) deny(1) "
            "mach-lookup com.apple.foo",
            "4 duplicate reports for Sandbox: Chromium(100) deny(1) "
            "mach-lookup com.apple.foo",
        ):
            with self.subTest(message=message):
                denial = SandboxDenial.from_json(_event_line(message))
                self.assertIsNotNone(denial)
                self.assertEqual(denial.event_message, message)
                self.assertEqual(denial.process_name, "Chromium")
                self.assertEqual(denial.action, "mach-lookup")
                self.assertEqual(denial.target, "com.apple.foo")

    def test_rejects_denial_text_not_at_start_of_message(self):
        self.assertIsNone(
            SandboxDenial.from_json(
                _event_line(
                    "Quoting Sandbox: Chromium(100) deny(1) mach-lookup foo"
                )
            )
        )

    def test_rejects_non_denial_input(self):
        for line in (
            b"",
            b"   ",
            b"not json",
            b"{not json",
            b"[1, 2]",
            b"123",
            b'"just a string"',
            b"true",
            b"{}",
            json.dumps({"eventMessage": 12345}).encode("utf-8"),
            json.dumps({"eventMessage": ""}).encode("utf-8"),
            _event_line("kernel: normal message without denial"),
        ):
            with self.subTest(line=line):
                self.assertIsNone(SandboxDenial.from_json(line))

    def test_matching_is_case_sensitive(self):
        self.assertIsNone(
            SandboxDenial.from_json(
                _event_line("sandbox: Chromium(100) DENY(1) mach-lookup foo")
            )
        )


if __name__ == "__main__":
    unittest.main()
