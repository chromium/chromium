# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import shlex
import contextlib
from enum import Enum
import typing

from . import command_line


class Channel(Enum):
    STABLE = "stable",
    CANARY = "canary",
    DEV = "dev",
    BETA = "beta",
    LOCAL_BUILD = "local-build",
    CHROMIUM = "chromium",

class App:
    """Class to control the chrome app."""

    def __init__(self, channel):
        if not isinstance(channel, Channel):
            raise TypeError(f"channel must be a member of the Channel enum, "
                            f"got {type(channel)}.")

        self.channel = channel

    async def stop(self):
        """Runs the adb shell command to stop an activity."""
        command_parts = ["am", "force-stop", self.package()]
        await command_line.adb_shell(shlex.join(command_parts))

    async def start(
            self,
            activity="org.chromium.chrome.browser.ChromeTabbedActivity",
            action="android.intent.action.VIEW",
            url=None):
        """Runs the adb shell command to start an activity."""
        command_parts = [
            "am", "start", "-n", f"{self.package()}/{activity}", "-a", action
        ]
        if url:
            command_parts.extend(["-d", url])
        await command_line.adb_shell(shlex.join(command_parts))

    def package(self):
        """Returns the package name of the chrome app."""
        match self.channel:
            case Channel.STABLE:
                return "com.android.chrome"
            case Channel.BETA:
                return "com.chrome.beta"
            case Channel.DEV:
                return "com.chrome.dev"
            case Channel.CANARY:
                return "com.chrome.canary"
            case Channel.LOCAL_BUILD:
                return "com.google.android.apps.chrome"
            case Channel.CHROMIUM:
                return "org.chromium.chrome"
            case _:
                raise AssertionError(
                    f"Missing package name for channel: {self.channel}.")


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
