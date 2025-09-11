# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import asyncio
import dataclasses
import sys
import shlex
import signal
import os


class CommandException(Exception):
    """Custom exception for command failures."""

    def __init__(self, message, result=None, command=None):
        super().__init__(message)
        self.result = result
        self.command = command

    def __str__(self):
        message = [super().__str__()]
        if self.command:
            message.append(f"Command: {shlex.join(self.command)}")
        if self.result:
            message.append(f"Return Code: {self.result.returncode}")
            if self.result.stdout.strip():
                message.append(f"STDOUT:\n{self.result.stdout.strip()}")
            if self.result.stderr.strip():
                message.append(f"STDERR:\n{self.result.stderr.strip()}")
        return '\n'.join(message)


@dataclasses.dataclass(frozen=True)
class CommandResult:
    """Result of a command execution."""
    stdout: str
    stderr: str
    returncode: int


async def run(command, *args, input="", interruption_signal=signal.SIGKILL):
    """Runs a command on the command line asynchronously.

    Returns stdout, stderr and return code.
    Handles interruptions to kill the subprocess.
    """
    process = None  # Initialize process to None
    try:
        # Put the process in its own process group to kill any subprocess that
        # it might spawn (e.g. when using `go run`) before returning from the
        # function
        process = await asyncio.create_subprocess_exec(
            command,
            *args,
            stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
            preexec_fn=os.setpgrp)
        stdout, stderr = await process.communicate(input=input.encode('utf-8'))
        returncode = process.returncode
        result = CommandResult(stdout.decode(), stderr.decode(), returncode)
        if returncode != 0:
            raise CommandException("Command failed",
                                   result=result,
                                   command=(command, ) + args)
        return result
    except FileNotFoundError:
        raise CommandException("Command not found", command=(command, ) + args)
    except asyncio.CancelledError:
        # Handle cancellation by killing the process group. First, check if
        # process is running and then wait for the process to be killed unless
        # it has already exited.
        try:
            os.killpg(process.pid, interruption_signal)
            await asyncio.wait_for(asyncio.create_task(process.wait()),
                                   timeout=10)
        except ProcessLookupError:
            pass
        raise


def adb_shell(command, *args, **kwargs):
    return run("adb", "shell", command, *args, **kwargs)
