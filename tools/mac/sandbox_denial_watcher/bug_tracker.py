# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium Issue Tracker bug URL generation for sandbox denials."""

from __future__ import annotations

from typing import Sequence, Union
import urllib.parse

from log_events import SandboxDenial

# Chromium Issue Tracker component ID for "Internals > Sandbox" (see
# //sandbox/COMMON_METADATA).
_COMPONENT_ID = "1455745"
_BUG_TITLE_PREFIX = "[macOS Sandbox] Unexpected denial:"
_BUG_TRACKER_URL = "https://issues.chromium.org/issues/new"


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
    unique_targets = list(dict.fromkeys(d.target for d in denials if d.target))
    if not unique_targets:
        return first.action
    return f"{first.action} {_format_targets(unique_targets)}"


def _get_bug_title(denials: Sequence[SandboxDenial]) -> str:
    """Generates the bug title for a sequence of related denials."""
    return f"{_BUG_TITLE_PREFIX} {get_denial_summary(denials)}"


def _get_bug_body(denials: Sequence[SandboxDenial]) -> str:
    """Generates the bug description for a sequence of related denials."""
    first = denials[0]
    unique_messages = list(dict.fromkeys(d.event_message for d in denials))
    message_block = "\n".join(unique_messages)
    header = (
        "Unexpected Sandbox denial detected:"
        if len(unique_messages) == 1
        else "Unexpected Sandbox denials detected:"
    )
    return (
        f"Process: {first.process_name}\n\n"
        f"{header}\n```\n{message_block}\n```\n"
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
