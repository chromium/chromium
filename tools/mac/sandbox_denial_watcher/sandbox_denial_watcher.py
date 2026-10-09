#!/usr/bin/env python3

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Watches macOS system logs for Chrome/Chromium sandbox denials.

Prints each sandbox denial reported by a Chrome or Chromium process.

Ensure Chrome sandbox denials are being reported by either
    1. Enabling "Mac Sandbox Logging" at chrome://flags/#mac-sandbox-logging
    2. Running with the --enable-sandbox-logging command-line flag

Usage (from the Chromium src directory):
    tools/mac/sandbox_denial_watcher/sandbox_denial_watcher.py [-v]
"""

import argparse
import logging
import shlex
import subprocess
import sys

from typing import Optional, Sequence

from log_events import SandboxDenial

_LOG_PATH = "/usr/bin/log"

# Prefixes of the process names whose denials are reported.
_INTERESTING_PROCS = (
    "Chromium",
    "Google Chrome",
    # MTLCompilerService inherits the GPU process's Seatbelt profile, so its
    # denials reflect Chrome's GPU process sandbox policy. Other apps that use
    # Metal run their own MTLCompilerService instances, and denial messages do
    # not identify the client app, so some of these denials may not come from
    # Chrome.
    "MTLCompilerService",
)


def _validate_process_name(proc: str) -> None:
    if not proc.isascii() or '"' in proc or "\\" in proc:
        raise ValueError(f"Invalid process name: {proc!r}")


class SandboxDenialWatcher:
    """Watches macOS system logs for Chrome/Chromium sandbox denials."""

    def __init__(
        self,
        logger: Optional[logging.Logger] = None,
        interesting_procs: tuple[str, ...] = _INTERESTING_PROCS,
    ) -> None:
        self._logger = logger or logging.getLogger("sandbox_denial_watcher")
        for proc in interesting_procs:
            _validate_process_name(proc)
        self._interesting_procs = interesting_procs

    def _report_denial(self, denial_event: SandboxDenial) -> None:
        """Prints a denial to the terminal."""
        self._logger.info("[DENIAL] %s", denial_event.event_message)

    def _handle_log_line(self, line: bytes) -> None:
        """Parses a log line and reports it if it is a Chrome sandbox denial."""
        denial_event = SandboxDenial.from_json(line)
        if not denial_event:
            self._logger.debug("Skipping unparsable line: %r", line)
            return

        if not denial_event.process_name.startswith(self._interesting_procs):
            self._logger.debug(
                "Skipping denial for non-target process: %s",
                denial_event.process_name,
            )
            return

        self._report_denial(denial_event)

    def _build_log_command(self) -> list[str]:
        """Returns the `log stream` command that emits candidate denials."""
        proc_filter = " OR ".join(
            f'eventMessage CONTAINS "{proc}"'
            for proc in self._interesting_procs
        )
        predicate = (
            'eventMessage CONTAINS "Sandbox" AND eventMessage CONTAINS "deny" '
            f"AND ({proc_filter})"
        )
        return [
            _LOG_PATH,
            "stream",
            "--style",
            "ndjson",
            "--predicate",
            predicate,
        ]

    def watch_logs(self) -> int:
        """Watches macOS system logs for sandbox denials.

        Returns 1 if `log stream` exits unexpectedly with an error, otherwise 0.
        """
        cmd = self._build_log_command()
        self._logger.debug("Log command: %s", shlex.join(cmd))
        interrupted = False
        with subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
        ) as proc:
            try:
                self._logger.info("Watching for sandbox denials...")
                self._logger.info(
                    'Ensure "Mac Sandbox Logging" is enabled at '
                    "chrome://flags/#mac-sandbox-logging."
                )
                self._logger.info("Press Ctrl+C to stop.")

                assert proc.stdout is not None
                for line in proc.stdout:
                    self._handle_log_line(line)
            except KeyboardInterrupt:
                interrupted = True
                self._logger.info("Stopped watching.")
            finally:
                if proc.poll() is None:
                    proc.terminate()
                    try:
                        proc.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        proc.kill()
                        proc.wait()

        assert proc.returncode is not None
        if not interrupted and proc.returncode != 0:
            self._logger.error(
                "%s exited unexpectedly with code %d.",
                _LOG_PATH,
                proc.returncode,
            )
            return 1
        return 0


def main(args: Sequence[str]) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="Enable verbose debug logging",
    )
    parsed = parser.parse_args(args)

    logging.basicConfig(
        level=logging.DEBUG if parsed.verbose else logging.INFO,
        format=(
            "%(asctime)s [%(levelname)s] %(message)s"
            if parsed.verbose
            else "%(message)s"
        ),
    )
    logger = logging.getLogger("sandbox_denial_watcher")
    if sys.platform != "darwin":
        logger.error("This tool only runs on macOS.")
        return 1
    watcher = SandboxDenialWatcher(logger=logger)
    return watcher.watch_logs()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
