# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import signal
import contextlib
import asyncio

from . import command_line


@contextlib.asynccontextmanager
async def caffeinate(interval=30):
    """A context manager to keep a device connected over ADB awake.

    The device is kept awake by simulating keypresses of the F24 key in a loop.

    Args:
        interval: The interval (in seconds) between key events.

    Usage::
        async with colabutils.caffeinate():
            await app.start(url='about:blank')
            fcp_time_ms = await navigation.measure_fcp(app, url="https://google.com")
            print(f"FCP: {fcp_time_ms} ms")
    """
    # Simulate keypresses of the F24 key since it is unlikely that Chrome is
    # listening for it.
    cmd = "while true; do input keyevent 337; sleep {interval}; done"

    caffeinate_task = asyncio.create_task(
        command_line.adb_shell(cmd.format(interval=interval),
                               interruption_signal=signal.SIGINT))
    try:
        yield
    finally:
        caffeinate_task.cancel()
        try:
            await caffeinate_task
        except asyncio.CancelledError:
            pass  # Expected if the task handles cancellation
