#!/usr/bin/env python3

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Watches macOS system logs for unexpected Chrome/Chromium sandbox denials.

Filters out expected/known denials and displays a macOS dialog prompting to file
a pre-filled Chromium Issue Tracker bug when an unexpected denial occurs.
Repeated dialogs for the same denial are suppressed after dismissal.

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
from dataclasses import replace
import functools
import logging
import math
from pathlib import Path
import re
import shlex
import subprocess
import sys
import threading
import time
from typing import Callable, Optional, Sequence, Union
import webbrowser

from bug_tracker import (
    get_bug_url,
    get_denial_summary,
    get_seatbelt_profile,
    normalize_denials,
)
from log_events import SandboxDenial

# Duration (in seconds) to suppress repeated notifications for a denial after
# its dialog is dismissed or times out.
_COOLDOWN_SECONDS = 3600

# Duration (in seconds) before an unanswered dialog closes itself.
_DIALOG_GIVE_UP_SECONDS = 15

# Duration (in seconds) to collect related denials into a batch before
# prompting.
_BATCH_WINDOW_SECONDS = 1.0

_LOG_PATH = "/usr/bin/log"
_OSASCRIPT_PATH = "/usr/bin/osascript"
_PS_PATH = "/bin/ps"

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

# Placeholder representing any Chrome/Chromium flavor's Application Support
# directory after target normalization.
_CHROME_APP_SUPPORT_DIR = "<CHROME_APP_SUPPORT_DIR>"

# Default product directory names under ~/Library/Application Support (see
# chrome/common/chrome_paths_mac.mm and docs/user_data_dir.md).
_CHROME_PRODUCT_DIRS = (
    "Chromium",
    "Google/Chrome",
    "Google/Chrome Beta",
    "Google/Chrome Canary",
    "Google/Chrome Dev",
    "Google/Chrome for Testing",
)

_CHROME_APP_SUPPORT_PREFIXES = tuple(
    f"{_HOME_DIR}/Library/Application Support/{product}"
    for product in _CHROME_PRODUCT_DIRS
)

# Placeholder representing any Chrome/Chromium flavor's app bundle directory
# under /Applications after target normalization.
_CHROME_BUNDLE_DIR = "<CHROME_BUNDLE_DIR>"

_CHROME_APP_NAMES = (
    "Chromium.app",
    "Google Chrome.app",
    "Google Chrome Beta.app",
    "Google Chrome Canary.app",
    "Google Chrome Dev.app",
    "Google Chrome for Testing.app",
)

_CHROME_BUNDLE_PREFIXES = tuple(
    f"/Applications/{app_name}" for app_name in _CHROME_APP_NAMES
)

# Placeholder representing the parent directory of a user's Darwin directories
# (`/private/var/folders/<bucket>/<user hash>`) after target normalization.
_DARWIN_USER_DIR = "<DARWIN_USER_DIR>"

# Matches the three Darwin user dirs (user, temp, cache), which live at
# `/private/var/folders/<bucket>/<user hash>/[0|C|T]`, e.g.:
#   /private/var/folders/tr/6v8rtgkn5399hj49myh4zh3m00czwq/T/tmp.123
#   -> <DARWIN_USER_DIR>/T/tmp.123
_DARWIN_USER_DIR_PATTERN = re.compile(
    r"^/private/var/folders/[^/]+/[^/]+(?=/[0CT](?=/|$))"
)

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

# A rule can be either a target pattern string (matching any Seatbelt profile)
# or a `(pattern, profile)` tuple restricting the rule to a specific profile.
RuleType = Union[str, tuple[str, str]]

# Known expected denials grouped by action. Rules match the target itself (not
# subpaths) and may use `*` wildcards within a single path segment. Action keys
# may use Seatbelt parent operations ending in `*` (e.g. `file-write*`).
_KNOWN_TARGET_DENIALS: dict[str, frozenset[RuleType]] = {
    "sysctl-read": frozenset(
        (
            ("kern.bootargs", "gpu.sb"),
            "kern.willshutdown",
            "machdep.cpu.brand_string",
            "net.inet.ip.portrange.first",
            ("sysctl.proc_translated", "audio.sb"),
        )
    ),
    "file-issue-extension": frozenset(
        ("target:/ extension-class:com.apple.app-sandbox.read",)
    ),
    "file-read-data": frozenset(
        (
            ("/", "renderer.sb"),
            "/Library/Preferences/Logging/com.apple.diagnosticd.filter.plist",
            "/Library/Preferences/com.apple.networkd.plist",
            "/Library/Preferences/com.apple.security.plist",
            "/System/Library/xpc/extensions.plist",
            f"{_HOME_DIR}/Desktop",
        )
    ),
    "file-write-create": frozenset(
        (
            (f"{_DARWIN_USER_DIR}/T/tmp.*", "renderer.sb"),
            (f"{_DARWIN_USER_DIR}/T/tmp.*", "speech_recognition.sb"),
        )
    ),
    "mach-lookup": frozenset(
        (
            "com.apple.CoreServices.coreservicesd",
            "com.apple.DiskArbitration.diskarbitrationd",
            "com.apple.analyticsd",
            ("com.apple.audioanalyticsd", "audio.sb"),
            "com.apple.backupd.sandbox.xpc",
            "com.apple.bsd.dirhelper",
            "com.apple.cmio.registerassistantservice.system-extensions",
            "com.apple.coreservices.quarantine-resolver",
            "com.apple.ctkd.token-client",
            "com.apple.diagnosticd",
            "com.apple.distributed_notifications@Uv3",
            "com.apple.lsd.mapdb",
            ("com.apple.lsd.modifydb", "renderer.sb"),
            ("com.apple.system.notification_center", "utility.sb"),
            "com.apple.system.opendirectoryd.membership",
            "com.apple.tccd",
            "com.apple.tccd.system",
            "com.apple.trustd.agent",
        )
    ),
    "syscall-mig": frozenset(
        (
            ("341", "gpu.sb"),  # mach_voucher_debug_info, used by MTLCompiler
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
# Rules may use `*` wildcards within a single path segment. Action keys may use
# Seatbelt parent operations ending in `*` (e.g. `file-write*`).
_KNOWN_SUBPATH_DENIALS: dict[str, frozenset[RuleType]] = {
    "file-issue-extension": frozenset((f"target:{_CHROME_BUNDLE_DIR}/",)),
    "file-read-data": frozenset(
        (
            "/Library/Managed Preferences/",
            ("/System/Library/CoreServices/RawCamera.bundle/", "renderer.sb"),
            (
                "/System/Library/CoreServices/SystemFolderLocalizations/",
                "on_device_model_execution.sb",
            ),
            (
                "/System/Library/ExtensionKit/"
                "ExtensionPointDefinitions.sysexpt/",
                "on_device_model_execution.sb",
            ),
            "/System/Library/ExtensionKit/ExtensionPoints/",
            "/System/iOSSupport/System/Library/ExtensionKit/ExtensionPoints/",
            "/private/var/db/cmiodalassistants/Library/Preferences/",
            f"{_HOME_DIR}/Library/Preferences/",
        )
    ),
    "file-write*": frozenset(
        (
            (
                f"{_DARWIN_USER_DIR}/T/*_chrome_Unpacker_BeginUnzipping*/",
                "utility.sb",
            ),
            (f"{_DARWIN_USER_DIR}/T/scoped_dir*/CRX_INSTALL/", "utility.sb"),
        )
    ),
    "file-write-times": frozenset(
        (f"{_CHROME_APP_SUPPORT_DIR}/optimization_guide_model_store/",)
    ),
}


def _replace_prefix(
    path: str, prefixes: Sequence[str], placeholder: str
) -> str:
    """Replaces the first matching prefix in `path` with `placeholder`."""
    for prefix in prefixes:
        if path == prefix or path.startswith(f"{prefix}/"):
            return f"{placeholder}{path[len(prefix) :]}"
    return path


def _normalize_target(target: str) -> str:
    """Normalizes a denial target path for matching and suppression."""
    normalized = target.rstrip("/") if target != "/" else "/"
    prefix = ""
    if normalized.startswith("target:"):
        prefix = "target:"
        normalized = normalized[len(prefix) :]
    normalized = _replace_prefix(
        normalized, _CHROME_APP_SUPPORT_PREFIXES, _CHROME_APP_SUPPORT_DIR
    )
    normalized = _replace_prefix(
        normalized, _CHROME_BUNDLE_PREFIXES, _CHROME_BUNDLE_DIR
    )
    normalized = _DARWIN_USER_DIR_PATTERN.sub(_DARWIN_USER_DIR, normalized)
    return f"{prefix}{normalized}"


@functools.lru_cache(maxsize=None)
def _compile_glob_regex(pattern: str, match_subpaths: bool) -> re.Pattern[str]:
    """Compiles a target pattern with `*` wildcards into a regex.

    Each `*` matches zero or more characters within a single path segment
    (`[^/]*`). If `match_subpaths` is True, the pattern matches both the
    directory itself and any path beneath it.
    """
    body = r"[^/]*".join(re.escape(part) for part in pattern.split("*"))
    suffix = r"(?:/.*)?$" if match_subpaths else r"$"
    return re.compile(f"^{body}{suffix}")


def _matches_target_rule(target: str, rule: str) -> bool:
    """Returns True if `target` matches the single-target `rule`."""
    if "*" not in rule:
        return target == rule
    return bool(_compile_glob_regex(rule, match_subpaths=False).match(target))


def _matches_subpath_rule(target: str, prefix: str) -> bool:
    """Returns True if `target` is `prefix` or a path beneath it."""
    dir_path = prefix.rstrip("/")
    if "*" not in dir_path:
        return target == dir_path or target.startswith(f"{dir_path}/")
    return bool(
        _compile_glob_regex(dir_path, match_subpaths=True).match(target)
    )


def _parse_rule(rule: RuleType) -> tuple[str, Optional[str]]:
    """Extracts `(pattern, profile)` from a rule entry.

    A rule can be a string pattern (matching any Seatbelt profile) or a
    `(pattern, profile)` tuple restricting the rule to a specific profile.
    """
    if isinstance(rule, tuple):
        return rule[0], rule[1]
    return rule, None


def _matches_action(denial_action: str, rule_action: str) -> bool:
    """Returns True if `denial_action` matches `rule_action`.

    Supports exact action matching or Seatbelt parent operations ending in `*`
    (such as `file-write*`).
    """
    if rule_action.endswith("*"):
        return denial_action.startswith(rule_action[:-1])
    return denial_action == rule_action


class DenialFilter:
    """Decides which sandbox denials should be reported.

    Combines static known-denial rules with temporary suppressions that are
    added when the user responds to a dialog.
    """

    def __init__(
        self,
        clock: Callable[[], float] = time.monotonic,
        logger: Optional[logging.Logger] = None,
    ) -> None:
        self._clock = clock
        self._logger = logger or logging.getLogger("sandbox_denial_watcher")
        # Maps (action, target) to the clock time at which its suppression
        # expires.
        self._suppressed: dict[tuple[str, str], float] = {}

    def should_ignore(self, denial_event: SandboxDenial) -> bool:
        """Returns True if the denial is expected or currently suppressed."""
        action = denial_event.action
        target = _normalize_target(denial_event.target)
        seatbelt_profile = get_seatbelt_profile(denial_event)

        for rules_by_action, matches_rule in (
            (_KNOWN_TARGET_DENIALS, _matches_target_rule),
            (_KNOWN_SUBPATH_DENIALS, _matches_subpath_rule),
        ):
            for rule_action, rules in rules_by_action.items():
                if not _matches_action(action, rule_action):
                    continue
                for rule in rules:
                    pattern, profile = _parse_rule(rule)
                    if profile is not None and profile != seatbelt_profile:
                        continue
                    if matches_rule(target, pattern):
                        profile_info = f" ({profile})" if profile else ""
                        self._logger.debug(
                            "Filtered known denial matching '%s %s%s': %s",
                            rule_action,
                            pattern,
                            profile_info,
                            denial_event.event_message,
                        )
                        return True

        key = (action, target)
        expires_at = self._suppressed.get(key)
        if expires_at is not None:
            now = self._clock()
            if now >= expires_at:
                del self._suppressed[key]
                return False

            remaining = expires_at - now
            duration_str = (
                "for the remainder of the session"
                if math.isinf(remaining)
                else f"for {remaining:.1f}s"
            )
            denial_str = f"{action} {target}" if target else action
            self._logger.debug(
                "Filtered suppressed denial matching '%s' (%s): %s",
                denial_str,
                duration_str,
                denial_event.event_message,
            )
            return True

        return False

    def suppress(
        self,
        denial_event: Union[SandboxDenial, Sequence[SandboxDenial]],
        duration_seconds: float,
    ) -> None:
        """Suppresses notifications for a denial for the given duration.

        An existing suppression for the same action and target is only ever
        extended, never shortened.
        """
        if duration_seconds <= 0:
            return
        expires_at = (
            math.inf
            if math.isinf(duration_seconds)
            else self._clock() + duration_seconds
        )
        for denial in normalize_denials(denial_event):
            key = (denial.action, _normalize_target(denial.target))
            if self._suppressed.get(key, -math.inf) < expires_at:
                self._suppressed[key] = expires_at


def _validate_process_name(proc: str) -> None:
    if not proc.isascii() or '"' in proc or "\\" in proc:
        raise ValueError(f"Invalid process name: {proc!r}")


def _get_process_command_line(pid: int) -> Optional[str]:
    """Returns the executable path and command-line arguments for `pid`.

    For example:
        '/Applications/Chromium.app/.../Chromium Helper --type=renderer ...'

    Returns None if unavailable (e.g., if `ps` fails or if the process has
    exited).
    """
    logger = logging.getLogger("sandbox_denial_watcher")
    try:
        res = subprocess.run(
            [_PS_PATH, "-p", str(pid), "-o", "command="],
            capture_output=True,
            encoding="utf-8",
            errors="replace",
            timeout=2,
            check=False,
        )
    except (OSError, subprocess.SubprocessError) as e:
        logger.debug("Failed to query command line for pid %d: %s", pid, e)
        return None

    if res.returncode != 0:
        logger.debug("ps exited with code %d for pid %d", res.returncode, pid)
        return None

    return res.stdout.strip() or None


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

    def __init__(
        self,
        denial_filter: Optional[DenialFilter] = None,
        batch_window_seconds: float = _BATCH_WINDOW_SECONDS,
        cooldown_seconds: float = _COOLDOWN_SECONDS,
        interesting_procs: Sequence[str] = _INTERESTING_PROCS,
        logger: Optional[logging.Logger] = None,
    ) -> None:
        self._logger = logger or logging.getLogger("sandbox_denial_watcher")
        self._interesting_procs = tuple(interesting_procs)
        for proc in self._interesting_procs:
            _validate_process_name(proc)
        self._denial_filter = denial_filter or DenialFilter(logger=self._logger)
        self._batch_window_seconds = batch_window_seconds
        self._cooldown_seconds = cooldown_seconds
        self._batch_lock = threading.Lock()
        self._active_batches: dict[tuple[str, str], list[SandboxDenial]] = {}
        self._prompted_denials: set[tuple[str, str]] = set()
        # Caches process command lines per PID with a bounded LRU cache so
        # rapid bursts of denials do not invoke `ps` repeatedly.
        self._get_command_line = functools.lru_cache(maxsize=256)(
            _get_process_command_line
        )

    def _flush_batch(self, batch_key: tuple[str, str]) -> None:
        """Flushes a pending batch of denials and prompts the user."""
        with self._batch_lock:
            denials = self._active_batches.pop(batch_key, None)
            if not denials:
                return
            for denial in denials:
                self._prompted_denials.add(
                    (denial.action, _normalize_target(denial.target))
                )

        url = get_bug_url(denials)
        self._logger.info("[REPORT] %s", url)
        if prompt_to_file_bug(denials):
            self._denial_filter.suppress(denials, math.inf)
        else:
            self._denial_filter.suppress(denials, self._cooldown_seconds)
        with self._batch_lock:
            for denial in denials:
                self._prompted_denials.discard(
                    (denial.action, _normalize_target(denial.target))
                )

    def _flush_all_batches(self) -> None:
        """Flushes all pending batches immediately."""
        with self._batch_lock:
            keys = list(self._active_batches.keys())
        for key in keys:
            self._flush_batch(key)

    def _report_denial(self, denial_event: SandboxDenial) -> None:
        """Logs an unexpected denial and queues it into a batch."""
        self._logger.info("[DENIAL] %s", denial_event.event_message)
        batch_key = (denial_event.process_name, denial_event.action)

        with self._batch_lock:
            key = (denial_event.action, _normalize_target(denial_event.target))
            if key in self._prompted_denials:
                denial_str = (
                    f"{denial_event.action} {key[1]}"
                    if key[1]
                    else denial_event.action
                )
                self._logger.debug(
                    "Skipping bug prompt for already prompted denial: %s",
                    denial_str,
                )
                return

            if batch_key not in self._active_batches:
                # Capture the process command line now (rather than when the
                # batch flushes) in case a short-lived process exits beforehand.
                if not denial_event.command_line:
                    command_line = self._get_command_line(denial_event.pid)
                    if command_line:
                        denial_event = replace(
                            denial_event, command_line=command_line
                        )
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

        if denial_event.command_line is None:
            command_line = self._get_command_line(denial_event.pid)
            if command_line:
                denial_event = replace(denial_event, command_line=command_line)

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
