#!/usr/bin/env python3

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Watches macOS system logs for unexpected Chrome/Chromium sandbox denials.

Filters out expected/known denials and displays a macOS dialog prompting to file
a pre-filled Chromium Issue Tracker bug when an unexpected denial occurs.

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
import threading
from typing import Sequence, Union
import webbrowser

from bug_tracker import get_bug_url, get_denial_summary, normalize_denials
from log_events import SandboxDenial

# Duration (in seconds) before an unanswered dialog closes itself.
_DIALOG_GIVE_UP_SECONDS = 15

# Duration (in seconds) to collect related denials into a batch before
# prompting.
_BATCH_WINDOW_SECONDS = 1.0

_LOG_PATH = "/usr/bin/log"
_OSASCRIPT_PATH = "/usr/bin/osascript"

_DISMISS_BUTTON_TITLE = "Dismiss"
_FILE_BUG_BUTTON_TITLE = "File bug"

_DIALOG_SCRIPT = (
    "on run argv\n"
    "    set dialogText to item 1 of argv\n"
    "    set dismissTitle to item 2 of argv\n"
    "    set fileBugTitle to item 3 of argv\n"
    "    set giveUpSeconds to (item 4 of argv as integer)\n"
    "    display dialog dialogText buttons {dismissTitle, fileBugTitle} "
    "default button fileBugTitle cancel button dismissTitle "
    'with title "Chrome Sandbox Denial" '
    "giving up after giveUpSeconds\n"
    "end run\n"
)

# Serializes modal dialog display so concurrent unexpected denials do not
# stack dialogs.
_dialog_lock = threading.Lock()

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


def prompt_to_file_bug(
    denial_event: Union[SandboxDenial, Sequence[SandboxDenial]],
) -> bool:
    """Displays a macOS modal dialog prompting the user to file a bug.

    If accepted, opens the pre-filled bug URL in the default browser.
    Returns True if the user opened the bug tracker, or False otherwise.
    """
    denials = normalize_denials(denial_event)
    if not denials:
        return False

    url = get_bug_url(denials)
    logger = logging.getLogger("sandbox_denial_watcher")

    first = denials[0]
    dialog_text = f"{first.process_name}: {get_denial_summary(denials)}"

    with _dialog_lock:
        try:
            res = subprocess.run(
                [
                    _OSASCRIPT_PATH,
                    "-e",
                    _DIALOG_SCRIPT,
                    dialog_text,
                    _DISMISS_BUTTON_TITLE,
                    _FILE_BUG_BUTTON_TITLE,
                    str(_DIALOG_GIVE_UP_SECONDS),
                ],
                capture_output=True,
                encoding="utf-8",
                errors="replace",
                timeout=_DIALOG_GIVE_UP_SECONDS + 5,
                check=False,
            )
        except subprocess.TimeoutExpired:
            logger.debug("Dialog timed out via subprocess timeout")
            return False

        if res.returncode != 0:
            logger.debug(
                "Dialog command exited with code %d: %s",
                res.returncode,
                res.stderr.strip(),
            )
            return False

        logger.debug("Dialog response: %s", res.stdout.strip())
        if f"button returned:{_FILE_BUG_BUTTON_TITLE}" in res.stdout:
            webbrowser.open(url)
            return True
        return False


class SandboxDenialWatcher:
    """Watches macOS system logs for unexpected sandbox denials."""

    def __init__(self) -> None:
        self._logger = logging.getLogger("sandbox_denial_watcher")
        for proc in _INTERESTING_PROCS:
            _validate_process_name(proc)
        self._interesting_procs = _INTERESTING_PROCS
        self._denial_filter = DenialFilter()
        self._prompted_denials: set[tuple[str, str]] = set()
        self._batch_window_seconds = _BATCH_WINDOW_SECONDS
        self._batch_lock = threading.Lock()
        self._active_batches: dict[tuple[str, str], list[SandboxDenial]] = {}

    def _flush_batch(self, batch_key: tuple[str, str]) -> None:
        """Flushes a pending batch of denials and prompts the user."""
        with self._batch_lock:
            denials = self._active_batches.pop(batch_key, None)
            if not denials or batch_key in self._prompted_denials:
                return
            self._prompted_denials.add(batch_key)

        url = get_bug_url(denials)
        self._logger.info("[REPORT] %s", url)
        prompt_to_file_bug(denials)

    def _report_denial(self, denial_event: SandboxDenial) -> None:
        """Logs an unexpected denial and queues it into a batch."""
        self._logger.info("[DENIAL] %s", denial_event.event_message)
        batch_key = (denial_event.process_name, denial_event.action)

        with self._batch_lock:
            if batch_key in self._prompted_denials:
                self._logger.debug(
                    "Skipping bug prompt for already prompted denial: %s",
                    batch_key,
                )
                return

            if batch_key not in self._active_batches:
                self._active_batches[batch_key] = [denial_event]
                timer = threading.Timer(
                    self._batch_window_seconds,
                    self._flush_batch,
                    args=(batch_key,),
                )
                timer.daemon = True
                timer.start()
            else:
                self._active_batches[batch_key].append(denial_event)

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
