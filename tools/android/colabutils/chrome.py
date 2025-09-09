# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import shlex
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
