# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import psutil
import asyncio
import signal
import shlex
import contextlib
from enum import StrEnum

from . import command_line
from . import chrome


def _get_listening_ports():
    """Returns a list of TCP ports that are in the 'LISTEN' state."""
    return [
        conn.laddr.port for conn in psutil.net_connections()
        if conn.status == 'LISTEN'
    ]


def _assert_no_listeners_for_ports(ports):
    """Asserts that no programs are listening on the specified ports.

    Args:
        ports: A list of port numbers to check.

    Raises:
        Exception: If any of the ports are in use.
    """
    listening_ports = _get_listening_ports()
    for port in ports:
        if port in listening_ports:
            raise Exception(f"Port {port} is in use by another program. Run " \
                            "ss -lptn 'sport = :{port}'` on your terminal to " \
                            "check which process is listening to the port")


async def _wait_for_ports(ports):
    """Waits until a program is listening on the specified ports.

    Args:
        ports: A list of port numbers to wait for.

    Raises:
        asyncio.CancelledError: If the task is cancelled while waiting.
    """
    try:
        while True:
            listening_ports = _get_listening_ports()
            if all((port in listening_ports) for port in ports):
                return
            await asyncio.sleep(1)  # Check every 1s
    except asyncio.CancelledError:
        raise


HTTP_PORT = 8080  # The port used by WPR for HTTP connections.
HTTPS_PORT = 8081  # The port used by WPR for HTTPS connections.

# Command line configuration for Chrome to use the WPR proxy. Copied from
# https://chromium.googlesource.com/catapult/+/HEAD/web_page_replay_go/README.md#running-on-android
COMMAND_LINE_FLAGS = [
    f"--host-resolver-rules=\"MAP *:80 127.0.0.1:{HTTP_PORT},MAP *:443 127.0.0.1:{HTTPS_PORT},EXCLUDE localhost\"",
    "--ignore-certificate-errors-spki-list=PhrPvGIaAMmd29hj8BCZOq096yj7uMpRNHpn5PDxI6I=,2HcXCSKKJS0lEXLQEWhpHUfGuojiU0tiT5gOF9LP6IQ="
]


class WebPageReplayArchive:
    """A class to manage a Web Page Replay (WPR) archive file.

    This class provides context managers for recording and replaying network
    traffic to the WPR archive file.
    """

    def __init__(self, archive_path):
        """Initializes the WebPageReplayArchive with the path to the archive.

        Args:
            archive_path: The path to the WPR archive file.
        """
        self.archive_path = archive_path

    @contextlib.asynccontextmanager
    async def record(self):
        """A context manager to record network traffic to the archive.

        This starts the WPR server in record mode and configures Chrome to use
        it. The server is stopped when the context is exited.
        """
        async with _server("record", self.archive_path):
            yield

    @contextlib.asynccontextmanager
    async def replay(self):
        """A context manager to replay network traffic from the archive.

        This starts the WPR server in replay mode and configures Chrome to use
        it. The server is stopped when the context is exited.
        """
        async with _server("replay", self.archive_path):
            yield


@contextlib.asynccontextmanager
async def _server(action, archive_path):
    """A context manager to run the Web Page Replay (WPR) server.

    This context manager starts the WPR server in either record or replay mode,
    configures adb to forward the necessary ports to the device, and sets the
    required Chrome command-line flags.

    Args:
        action: The WPR action to perform (record or replay).
        archive_path: The path to the WPR archive file.
    """

    # Configure adb to reverse proxy the ports needed for WPR to the device
    await command_line.run("adb", "reverse", f"tcp:{HTTP_PORT}",
                           f"tcp:{HTTP_PORT}")
    await command_line.run("adb", "reverse", f"tcp:{HTTPS_PORT}",
                           f"tcp:{HTTPS_PORT}")
    _assert_no_listeners_for_ports([HTTP_PORT, HTTPS_PORT])

    async with chrome.additional_command_line_flags(*COMMAND_LINE_FLAGS):
        wpr_task = asyncio.create_task(
            # The WPR server listens for SIGINT (Ctrl-C) to exit gracefully
            command_line.run("go",
                             "run",
                             "-C",
                             "third_party/catapult/web_page_replay_go",
                             "src/wpr.go",
                             action,
                             "--https_cert_file",
                             "wpr_cert.pem,ecdsa_cert.pem",
                             "--https_key_file",
                             "wpr_key.pem,ecdsa_key.pem",
                             f"--http_port={HTTP_PORT}",
                             f"--https_port={HTTPS_PORT}",
                             archive_path,
                             interruption_signal=signal.SIGINT))
        try:
            await _wait_for_ports([8080, 8081])
            yield
        finally:
            wpr_task.cancel()
            try:
                await wpr_task  # Await to allow the task to handle cancellation
            except asyncio.CancelledError:
                pass  # Expected if the task handles cancellation
