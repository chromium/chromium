# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium Issue Tracker bug URL generation for sandbox denials."""

from __future__ import annotations

from pathlib import Path
import platform
import re
from typing import Optional, Sequence, Union
import urllib.parse

from log_events import SandboxDenial

# Chromium Issue Tracker component ID for "Internals > Sandbox" (see
# //sandbox/COMMON_METADATA).
_COMPONENT_ID = "1455745"
_BUG_TITLE_PREFIX = "[macOS Sandbox] Unexpected denial:"
_BUG_TRACKER_URL = "https://issues.chromium.org/issues/new"

# Maximum length of the generated pre-filled issue URL to avoid exceeding
# browser or server URL length limits when many denials are detected.
_MAX_BUG_URL_LENGTH = 8000

# Reserve ~1,000 URL-encoded chars for the base URL, title, and non-log
# sections of the bug description.
_MAX_DENIAL_LOGS_ENCODED_LENGTH = _MAX_BUG_URL_LENGTH - 1000

# Maximum length of the generated pre-filled issue URL to avoid exceeding
# browser or server URL length limits when many denials are detected.
_MAX_BUG_URL_LENGTH = 8000

# Reserve ~1,000 URL-encoded chars for the base URL, title, and non-log
# sections of the bug description.
_MAX_DENIAL_LOGS_ENCODED_LENGTH = _MAX_BUG_URL_LENGTH - 1000

_CONTEXT_QUESTION = "What were you doing when this notification was triggered?"
_CONTEXT_PLACEHOLDER = (
    "<Please describe what you were doing, e.g., the page you were on and the "
    "actions you took. This helps developers reproduce and fix the issue.>"
)
_PRIVACY_REMINDER = (
    "Note to reporter: your home directory has been replaced with '~'. Please "
    "review this report and remove any other personal information before "
    "filing."
)


def _redact_home_dir(text: str, home_dir: Optional[str] = None) -> str:
    """Replaces occurrences of the user's home directory in `text` with '~'.

    Only whole path components are replaced, so a home directory of
    /Users/bob does not affect /Users/bobby or /Users/bob.smith.
    """
    home = (str(Path.home()) if home_dir is None else home_dir).rstrip("/")
    if not home:
        return text
    return re.sub(re.escape(home) + r"(?![\w.-])", "~", text)


def _get_system_info() -> str:
    """Returns a description of the local macOS version and architecture."""
    mac_version = platform.mac_ver()[0] or "unknown"
    architecture = platform.machine() or "unknown"
    return f"macOS version: {mac_version}\nArchitecture: {architecture}"


def normalize_denials(
    denial_event: Union[SandboxDenial, Sequence[SandboxDenial]],
) -> list[SandboxDenial]:
    """Converts a single denial or sequence into a list of denials."""
    if isinstance(denial_event, SandboxDenial):
        return [denial_event]
    return list(denial_event)


def _format_targets(targets: Sequence[str], max_len: int = 80) -> str:
    """Formats denial targets into a concise string.

    For example:
        ['com.apple.foo'] -> 'com.apple.foo'
        ['com.apple.foo', 'com.apple.bar'] -> 'com.apple.foo, com.apple.bar'
        ['/path/1', '/path/2', ...] -> '/path/1, ... (+1 more)'
    """
    joined = ", ".join(targets)
    if len(joined) <= max_len or len(targets) <= 1:
        return joined
    return f"{targets[0]}, ... (+{len(targets) - 1} more)"


def get_denial_summary(denials: Sequence[SandboxDenial]) -> str:
    """Returns a summary of what was denied, e.g. 'mach-lookup <target>'."""
    first = denials[0]
    unique_targets = list(
        dict.fromkeys(_redact_home_dir(d.target) for d in denials if d.target)
    )
    if not unique_targets:
        return first.action
    return f"{first.action} {_format_targets(unique_targets)}"


def _get_bug_title(denials: Sequence[SandboxDenial]) -> str:
    """Generates the bug title for a sequence of related denials."""
    return f"{_BUG_TITLE_PREFIX} {get_denial_summary(denials)}"


# Values of `--service-sandbox-type` that all use the `utility.sb` seatbelt
# profile rather than `<type>.sb` (see sandbox::policy::GetSandboxProfile in
# //sandbox/policy/mac/sandbox_mac.mm).
_UTILITY_SANDBOX_TYPES = frozenset(
    ("platform_runtime", "service", "service_with_jit", "utility")
)


def _parse_switches(command_line: str) -> dict[str, str]:
    """Extracts `--key` and `--key=value` switches from a command line.

    Used to parse the flags provided in a process's CLI invocation, retrieved
    with `ps -p <PID> -o command=`"""
    switches: dict[str, str] = {}
    for token in command_line.split():
        if token.startswith("--"):
            key, _, value = token[2:].partition("=")
            if key:
                switches[key] = value
    return switches


def _get_process_and_profile(denial: SandboxDenial) -> tuple[str, str]:
    """Returns `(process_type, seatbelt_profile)` for a sandbox denial.

    Uses Chrome's command-line switches (`--type`, `--utility-sub-type`,
    `--service-sandbox-type`, etc.) when available. Falls back to
    `denial.process_name` if the process exited before we were able to query
    with `ps` or if the process is a system service (e.g., MTLCompilerService).
    """
    switches = (
        _parse_switches(denial.command_line) if denial.command_line else {}
    )
    proc_type = switches.get("type")
    if not proc_type:
        if denial.process_name == "MTLCompilerService":
            return "MTLCompilerService", "gpu.sb"
        # macOS truncates process names in sandbox denial logs to 32 bytes
        # (e.g. "Google Chrome Helper (Aperitif R" or
        # "Chromium Helper (Aperitif Render"), cutting off the end of
        # "(Aperitif Renderer)" and "(Aperitif GPU)". These can be removed when
        # Aperitif is the default.
        if (
            denial.process_name.endswith(" (Renderer)")
            or " (Aperitif R" in denial.process_name
        ):
            proc_type = "renderer"
        elif (
            denial.process_name.endswith(" (GPU)")
            or " (Aperitif G" in denial.process_name
        ):
            proc_type = "gpu-process"
        else:
            return "unknown", "unknown"

    sub_type = (
        switches.get("utility-sub-type") if proc_type == "utility" else None
    )
    process_type = f"utility ({sub_type})" if sub_type else proc_type

    # Mirrors SandboxTypeFromCommandLine in //sandbox/policy/sandbox_type.cc
    # and GetSandboxProfile in //sandbox/policy/mac/sandbox_mac.mm.
    if "no-sandbox" in switches:
        return process_type, "none"
    if proc_type == "renderer":
        return process_type, "renderer.sb"
    if proc_type == "gpu-process":
        profile = "none" if "disable-gpu-sandbox" in switches else "gpu.sb"
        return process_type, profile
    if proc_type == "no-op-for-testing":
        return process_type, "utility.sb"
    if proc_type == "utility":
        sandbox_type = switches.get("service-sandbox-type")
        if not sandbox_type:
            return process_type, "unknown"
        if sandbox_type == "none":
            return process_type, "none"
        if sandbox_type in _UTILITY_SANDBOX_TYPES:
            return process_type, "utility.sb"
        return process_type, f"{sandbox_type}.sb"
    return process_type, "none"


def get_seatbelt_profile(denial: SandboxDenial) -> str:
    """Returns the Seatbelt profile (`.sb`) name for a sandbox denial."""
    return _get_process_and_profile(denial)[1]


def _truncate_denial_logs(
    denial_logs: str,
    max_encoded_len: int = _MAX_DENIAL_LOGS_ENCODED_LENGTH,
) -> str:
    """Truncates `denial_logs` to fit within `max_encoded_len`."""
    if len(urllib.parse.quote_plus(denial_logs)) <= max_encoded_len:
        return denial_logs

    suffix = "\n... (truncated)"
    budget = max_encoded_len - len(urllib.parse.quote_plus(suffix))
    if budget <= 0:
        return "... (truncated)"

    encoded_len = 0
    kept_chars: list[str] = []
    for ch in denial_logs:
        ch_len = len(urllib.parse.quote_plus(ch))
        if encoded_len + ch_len > budget:
            break
        kept_chars.append(ch)
        encoded_len += ch_len

    return "".join(kept_chars).rstrip("\n") + suffix


def _get_bug_body(denials: Sequence[SandboxDenial]) -> str:
    """Generates the bug description for a sequence of related denials."""
    process_type, seatbelt_profile = _get_process_and_profile(denials[0])
    unique_messages = list(dict.fromkeys(d.event_message for d in denials))
    denial_logs = _truncate_denial_logs(
        _redact_home_dir("\n".join(unique_messages))
    )
    header = (
        "Unexpected Sandbox denial detected:"
        if len(unique_messages) == 1
        else "Unexpected Sandbox denials detected:"
    )
    return (
        f"Process type: {process_type}\n"
        f"Seatbelt profile: {seatbelt_profile}\n\n"
        f"{header}\n```\n{denial_logs}\n```\n"
        "\n"
        f"{_get_system_info()}\n"
        "\n"
        f"{_CONTEXT_QUESTION}\n"
        f"{_CONTEXT_PLACEHOLDER}\n"
        "\n"
        f"{_PRIVACY_REMINDER}\n"
    )


def get_bug_url(
    denial_event: Union[SandboxDenial, Sequence[SandboxDenial]],
) -> str:
    """Generates the pre-filled Chromium bug tracker URL."""
    denials = normalize_denials(denial_event)
    if not denials:
        raise ValueError("Cannot generate bug URL for empty denials list")

    params = {
        "noWizard": "true",
        "component": _COMPONENT_ID,
        "title": _get_bug_title(denials),
        "description": _get_bug_body(denials),
    }
    return f"{_BUG_TRACKER_URL}?{urllib.parse.urlencode(params)}"
