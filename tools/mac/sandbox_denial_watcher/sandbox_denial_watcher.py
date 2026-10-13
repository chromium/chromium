#!/usr/bin/env python3

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Watches macOS system logs for unexpected Chrome/Chromium sandbox denials.

Prints each unexpected denial reported by a Chrome/Chromium or related process.

This tool MUST remain compatible with the default macOS Python interpreter
(`/usr/bin/python3`), which is Python 3.9.6 at the time of writing. Do not use
syntax or standard library features introduced in Python 3.10 or later.

Ensure Chrome sandbox denials are being reported by either
    1. Enabling "Mac Sandbox Logging" at chrome://flags/#mac-sandbox-logging
    2. Running with the --enable-sandbox-logging command-line flag

Usage (from the Chromium src directory):
    tools/mac/sandbox_denial_watcher/sandbox_denial_watcher.py [-v]
"""

from __future__ import annotations

import argparse
import logging
from pathlib import Path
import shlex
import subprocess
import sys
from typing import Sequence

from log_events import SandboxDenial

_LOG_PATH = "/usr/bin/log"

_HOME_DIR = Path.home()

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

# Known expected denials grouped by action. Targets are matched exactly.
_KNOWN_EXACT_DENIALS: dict[str, frozenset[str]] = {
    "sysctl-read": frozenset(
        (
            "kern.bootargs",
            "kern.willshutdown",
            "machdep.cpu.brand_string",
            "net.inet.ip.portrange.first",
            "sysctl.proc_translated",
        )
    ),
    "file-read-data": frozenset(
        (
            "/",
            "/Library/Preferences/Logging/com.apple.diagnosticd.filter.plist",
            "/Library/Preferences/com.apple.networkd.plist",
            "/Library/Preferences/com.apple.security.plist",
            "/System/Library/xpc/extensions.plist",
            f"{_HOME_DIR}/Desktop",
        )
    ),
    "mach-lookup": frozenset(
        (
            "com.apple.CoreServices.coreservicesd",
            "com.apple.DiskArbitration.diskarbitrationd",
            "com.apple.analyticsd",
            "com.apple.audioanalyticsd",
            "com.apple.backupd.sandbox.xpc",
            "com.apple.bsd.dirhelper",
            "com.apple.cmio.registerassistantservice.system-extensions",
            "com.apple.ctkd.token-client",
            "com.apple.diagnosticd",
            "com.apple.distributed_notifications@Uv3",
            "com.apple.lsd.mapdb",
            "com.apple.lsd.modifydb",
            "com.apple.system.notification_center",
            "com.apple.system.opendirectoryd.membership",
            "com.apple.tccd",
            "com.apple.tccd.system",
            "com.apple.trustd.agent",
        )
    ),
    "system-info": frozenset(
        (
            "net.link.addr",
            "vfs.disk-space",
        )
    ),
}

# Directory prefixes for operations that allow subpaths (e.g. file reads).
_KNOWN_SUBPATH_DENIALS: dict[str, tuple[str, ...]] = {
    "file-read-data": (
        "/Library/Managed Preferences/",
        "/System/Library/CoreServices/RawCamera.bundle/",
        "/System/Library/ExtensionKit/ExtensionPoints/",
        "/System/iOSSupport/System/Library/ExtensionKit/ExtensionPoints/",
        f"{_HOME_DIR}/Library/Preferences/",
    ),
}


class DenialFilter:
    """Filters out known and expected sandbox denials."""

    def __init__(self) -> None:
        self._logger = logging.getLogger("sandbox_denial_watcher")

    def should_ignore(self, denial_event: SandboxDenial) -> bool:
        """Returns True if the denial is a known expected denial."""
        action = denial_event.action
        target = (
            denial_event.target.rstrip("/")
            if denial_event.target != "/"
            else "/"
        )

        if target in _KNOWN_EXACT_DENIALS.get(action, ()):
            self._logger.debug(
                "Filtered known denial matching '%s %s': %s",
                action,
                target,
                denial_event.event_message,
            )
            return True

        for prefix in _KNOWN_SUBPATH_DENIALS.get(action, ()):
            normalized_prefix = prefix if prefix.endswith("/") else f"{prefix}/"
            if target == normalized_prefix.rstrip(
                "/"
            ) or denial_event.target.startswith(normalized_prefix):
                self._logger.debug(
                    "Filtered known denial matching '%s %s': %s",
                    action,
                    normalized_prefix,
                    denial_event.event_message,
                )
                return True

        return False


def _validate_process_name(proc: str) -> None:
    if not proc.isascii() or '"' in proc or "\\" in proc:
        raise ValueError(f"Invalid process name: {proc!r}")


class SandboxDenialWatcher:
    """Watches macOS system logs for unexpected sandbox denials."""

    def __init__(self) -> None:
        self._logger = logging.getLogger("sandbox_denial_watcher")
        for proc in _INTERESTING_PROCS:
            _validate_process_name(proc)
        self._interesting_procs = _INTERESTING_PROCS
        self._denial_filter = DenialFilter()

    def _report_denial(self, denial_event: SandboxDenial) -> None:
        """Prints a denial to the terminal."""
        self._logger.info("[DENIAL] %s", denial_event.event_message)

    def _handle_log_line(self, line: bytes) -> None:
        """Parses a log line and reports it if it is an unexpected denial."""
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

        if self._denial_filter.should_ignore(denial_event):
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
        """Watches macOS system logs for unexpected sandbox denials.

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
                self._logger.info("Watching for unexpected sandbox denials...")
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
    watcher = SandboxDenialWatcher()
    return watcher.watch_logs()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
