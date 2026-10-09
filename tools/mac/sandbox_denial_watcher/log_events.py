# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Parser for macOS Unified Logging sandbox denial events.

Raw log events from `/usr/bin/log stream --style ndjson` contain keys such as:
  - eventMessage: string description of the event (containing sandbox
    denial details)
  - timestamp: ISO 8601 formatted timestamp
  - processImagePath: path to the process executable
  - processID: integer PID of the process that emitted the log entry. For
    sandbox denials this is the kernel (0) or sandboxd, so the PID of the
    denied process is parsed from eventMessage instead.
  - senderImagePath: path to the framework/library emitting the log
  - subsystem, category, messageType, eventType, threadID, userID, etc.
"""

from __future__ import annotations

from dataclasses import dataclass
import json
import re
from typing import Any, Optional

# Matches eventMessage lines like:
#  Sandbox: duetexpertd(1129) deny(1) user-preference-read com.apple.triald
# including the kernel's summaries of repeated denials, like:
#  2 duplicate reports for Sandbox: duetexpertd(1129) deny(1) ...
# The pattern must match from the start of the message.
_DENIAL_PATTERN = re.compile(
    r"(?:\d+ duplicate reports? for )?"
    r"Sandbox:\s+(?P<process>\S.*?)\((?P<pid>\d+)\)\s+deny\(\d+\)\s+"
    r"(?P<action>\S+)(?:\s+(?P<target>\S.*?))?\s*$"
)


@dataclass(frozen=True)
class SandboxDenial:
    """Represents a macOS sandbox denial event parsed from unified logging."""

    event_message: str
    process_name: str
    pid: int
    action: str
    target: str

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> Optional[SandboxDenial]:
        raw_message = data.get("eventMessage", "")
        if not isinstance(raw_message, str) or not raw_message:
            return None
        first_line = raw_message.splitlines()[0]
        match = _DENIAL_PATTERN.match(first_line)
        if not match:
            return None

        process_name = match.group("process")
        pid = int(match.group("pid"))
        action = match.group("action")
        target = match.group("target") or ""

        return cls(
            event_message=first_line,
            process_name=process_name,
            pid=pid,
            action=action,
            target=target,
        )

    @classmethod
    def from_json(cls, line: bytes) -> Optional[SandboxDenial]:
        try:
            data = json.loads(line)
        except (json.JSONDecodeError, UnicodeDecodeError, TypeError):
            return None
        if not isinstance(data, dict):
            return None
        return cls.from_dict(data)
