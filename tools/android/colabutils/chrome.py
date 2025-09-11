# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import shlex
import contextlib
from enum import Enum, StrEnum

from . import command_line


class Channel(StrEnum):
    STABLE = "com.android.chrome"
    CANARY = "com.chrome.canary"
    DEV = "com.chrome.dev"
    BETA = "com.chrome.beta"
    LOCAL_BUILD = "com.google.android.apps.chrome"
    CHROMIUM = "org.chromium.chrome"


class App:
    """Class to control the chrome app."""

    def __init__(self, channel):
        self.package = str(channel)

    async def stop(self):
        """Runs the adb shell command to stop an activity."""
        command_parts = ["am", "force-stop", self.package]
        await command_line.adb_shell(shlex.join(command_parts))

    async def start(
            self,
            activity="org.chromium.chrome.browser.ChromeTabbedActivity",
            action="android.intent.action.VIEW",
            url=None):
        """Runs the adb shell command to start an activity."""
        command_parts = [
            "am", "start", "-n", f"{self.package}/{activity}", "-a", action
        ]
        if url:
            command_parts.extend(["-d", url])
        await command_line.adb_shell(shlex.join(command_parts))


@contextlib.asynccontextmanager
async def additional_command_line_flags(*args):
    """A context manager to temporarily add flags to chrome-command-line."""
    # Ensure the file exists before reading it.
    await command_line.adb_shell("touch",
                                 "/data/local/tmp/chrome-command-line")
    old_flags_str = (await command_line.adb_shell(
        "cat", "/data/local/tmp/chrome-command-line")).stdout.strip()

    try:
        # The command-line file must start with a placeholder token. If the file
        # is empty or doesn't start with one, we add it.
        old_flags_list = shlex.split(old_flags_str) if old_flags_str else ['_']
        new_flags_list = old_flags_list + list(args)
        new_flags_str = shlex.join(new_flags_list)
        await command_line.adb_shell(
            "cat > /data/local/tmp/chrome-command-line", input=new_flags_str)
        yield
    finally:
        await command_line.adb_shell(
            "cat > /data/local/tmp/chrome-command-line", input=old_flags_str)
