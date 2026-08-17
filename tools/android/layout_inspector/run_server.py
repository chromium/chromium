#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A web server to access ADB via web UI, while serving local files."""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import threading
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

PNG_SIGNATURE = b'\x89PNG\r\n\x1a\n'
LOCALHOST = '127.0.0.1'
DPI_PER_DP = 160.0
SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
STATIC_DIR = os.path.join(SCRIPT_DIR, 'static')


class AdbClient:
    """Wraps ADB path resolution, shell settings, and command execution."""

    def __init__(self):
        self.adb_path = shutil.which('adb')
        if not self.adb_path:
            print("Error: 'adb' command not found. Is it in your PATH?")
            sys.exit(1)
        self.use_shell = os.name == 'nt' and self.adb_path.lower().endswith(
            ('.bat', '.cmd')
        )

    def run(self, args, **kwargs):
        """Executes an ADB command using the global config."""
        cmd = [self.adb_path]
        if _config.serial:
            cmd.extend(['-s', _config.serial])
        cmd.extend(args)

        kwargs.setdefault('shell', self.use_shell)
        kwargs.setdefault('capture_output', True)
        kwargs.setdefault('check', True)
        return subprocess.run(cmd, **kwargs)

    def get_device(self, serial_arg):
        """Gets the ADB device serial, prompting the user if multiple exist."""
        if serial_arg:
            return serial_arg

        try:
            result = self.run(['devices'], text=True)
        except subprocess.CalledProcessError as e:
            print(f"Error running 'adb devices': {e}")
            sys.exit(1)

        lines = result.stdout.strip().split('\n')[1:]
        devices = []
        for line in lines:
            parts = line.split()
            if len(parts) >= 2 and parts[1] == 'device':
                devices.append(parts[0])

        if not devices:
            print("Error: No ADB devices connected.")
            sys.exit(1)

        if len(devices) == 1:
            return devices[0]

        print("Multiple ADB devices detected:")
        for i, device in enumerate(devices):
            print(f"[{i + 1}] {device}")

        while True:
            try:
                choice = input(f"Select a device (1-{len(devices)}): ")
                index = int(choice) - 1
                if 0 <= index < len(devices):
                    return devices[index]
                print("Invalid selection.")
            except ValueError:
                print("Please enter a number.")
            except (KeyboardInterrupt, EOFError):
                print("\nExiting.")
                sys.exit(1)


class ServerConfig:
    """Global server configurations."""

    def __init__(self):
        # Sub-components.
        self.adb = None

        # User-configurable settings.
        self.serial = None
        self.port = 8000
        self.local = True
        self.use_cache = False

        # Security and imprint state.
        self.require_imprint = True
        self.imprint_lock = threading.Lock()
        self.allowed_client_ip = None
        self.timeout = 120  # Seconds

    @property
    def ip_address(self):
        return LOCALHOST if self.local else ''


# Global states.
_config = ServerConfig()


class MainRequestHandler(SimpleHTTPRequestHandler):
    """A request handler to serve files and ADB endpoints."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=STATIC_DIR, **kwargs)

    def _get_density_data(self):
        """Gets the display density factor from the device."""
        result = _config.adb.run(['shell', 'wm', 'density'], text=True)
        # Output can be "Physical density: 480" or include "Override density: 560"
        # We want the last number found in the output.
        densities = re.findall(r':\s*(\d+)', result.stdout)
        if not densities:
            raise ValueError(f'Could not parse density from: {result.stdout}')
        dpi = int(densities[-1])
        return dpi / DPI_PER_DP

    def handle_density(self):
        """Handles the /api/density endpoint."""
        try:
            density_factor = self._get_density_data()
            self.send_response(HTTPStatus.OK)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(
                json.dumps(
                    {'success': True, 'density_factor': density_factor}
                ).encode('utf-8')
            )
        except (
            subprocess.CalledProcessError,
            FileNotFoundError,
            ValueError,
        ) as e:
            self.send_error(
                HTTPStatus.INTERNAL_SERVER_ERROR, f'Failed to get density: {e}'
            )

    def _get_ui_dump_data(self):
        """Gets the UI hierarchy XML from the device."""
        temp_device_path = None

        try:
            # Create a temporary file on the device
            mktemp_result = _config.adb.run(['shell', 'mktemp'], text=True)
            temp_device_path = mktemp_result.stdout.strip()

            # Dump UI to the temporary file on the device
            _config.adb.run(['shell', 'uiautomator', 'dump', temp_device_path])

            # Read the file content
            result = _config.adb.run(
                ['shell', 'cat', temp_device_path], text=True
            )
            return result.stdout
        finally:
            # Clean up the temporary file on the device if it was created
            if temp_device_path:
                _config.adb.run(['shell', 'rm', temp_device_path], check=False)

    def handle_ui_dump(self):
        """Handles the /api/ui-dump endpoint."""
        try:
            xml_content = self._get_ui_dump_data()
            self.send_response(HTTPStatus.OK)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(
                json.dumps({'success': True, 'xml': xml_content}).encode(
                    'utf-8'
                )
            )
        except (subprocess.CalledProcessError, FileNotFoundError) as e:
            self.send_error(
                HTTPStatus.INTERNAL_SERVER_ERROR, f'Failed to get UI dump: {e}'
            )

    def _get_screenshot_data(self):
        """Gets a screenshot from the device."""
        # exec-out is required for binary output without corruption on some
        # systems.
        result = _config.adb.run(['exec-out', 'screencap', '-p'])
        # Devices with multiple screens (e.g., foldable) might be prefixed with
        # warning message. Truncate everything before PNG file signature.
        data = result.stdout
        start = data.find(PNG_SIGNATURE)
        return data[start:] if start >= 0 else data

    def handle_screenshot(self):
        """Handles the /api/screenshot endpoint."""
        try:
            png_data = self._get_screenshot_data()
            self.send_response(HTTPStatus.OK)
            self.send_header('Content-Type', 'image/png')
            self.end_headers()
            self.wfile.write(png_data)
        except (subprocess.CalledProcessError, FileNotFoundError) as e:
            self.send_error(
                HTTPStatus.INTERNAL_SERVER_ERROR,
                f'Failed to get screenshot: {e}',
            )

    def do_GET(self):
        """Handles GET requests."""
        if _config.require_imprint:
            client_ip = self.client_address[0]
            with _config.imprint_lock:
                if _config.allowed_client_ip is None:
                    print(f"Imprinting server to client IP: {client_ip}")
                    _config.allowed_client_ip = client_ip
                elif _config.allowed_client_ip != client_ip:
                    print(
                        f"Blocking request from {client_ip} "
                        f"(Imprinted to {_config.allowed_client_ip})"
                    )
                    self.send_error(
                        HTTPStatus.FORBIDDEN,
                        "Forbidden: Server is imprinted to a different IP.",
                    )
                    return

        if self.path == '/':
            self.path = '/index.html'

        if self.path == '/api/ui-dump.xml':
            self.handle_ui_dump()
            return
        if self.path == '/api/screenshot.png':
            self.handle_screenshot()
            return
        if self.path == '/api/density.json':
            self.handle_density()
            return

        super().do_GET()


class CachingMainRequestHandler(MainRequestHandler):
    """An ADB request handler that caches ADB-related data in memory."""

    # The cache is stored in class fields, which is fine since the server is
    # single-threaded and the cache is read-only after the first hit.
    cached_screenshot = None
    cached_ui_dump = None
    cached_density = None

    def _get_screenshot_data(self):
        """Gets a screenshot, using a cache if available."""
        if not CachingMainRequestHandler.cached_screenshot:
            CachingMainRequestHandler.cached_screenshot = (
                super()._get_screenshot_data()
            )
        return CachingMainRequestHandler.cached_screenshot

    def _get_ui_dump_data(self):
        """Gets the UI hierarchy XML, using a cache if available."""
        if not CachingMainRequestHandler.cached_ui_dump:
            CachingMainRequestHandler.cached_ui_dump = (
                super()._get_ui_dump_data()
            )
        return CachingMainRequestHandler.cached_ui_dump

    def _get_density_data(self):
        """Gets the display density factor, using a cache if available."""
        if not CachingMainRequestHandler.cached_density:
            CachingMainRequestHandler.cached_density = (
                super()._get_density_data()
            )
        return CachingMainRequestHandler.cached_density


def run(handler_class):
    """Starts the HTTP server."""
    server_address = (_config.ip_address, _config.port)
    httpd = ThreadingHTTPServer(server_address, handler_class)
    display_ip = _config.ip_address or LOCALHOST
    print(f'Starting server on http://{display_ip}:{_config.port}')

    if _config.require_imprint:

        def check_imprint_timeout():
            if _config.allowed_client_ip is None:
                print(
                    f"Timeout: No client connected within {_config.timeout}s. "
                    "Exiting."
                )
                httpd.shutdown()

        # Start a timer to shut down if not imprinted within the timeout.
        timer = threading.Timer(_config.timeout, check_imprint_timeout)
        timer.start()

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        if _config.require_imprint and 'timer' in locals():
            timer.cancel()
        httpd.server_close()


def main():
    parser = argparse.ArgumentParser(
        description='Backend server for the Chrome Android Layout Inspector.'
    )
    parser.add_argument(
        '-s',
        '--serial',
        help='Directs ADB commands to the specific device or emulator.',
    )
    parser.add_argument(
        '-p',
        '--port',
        type=int,
        default=_config.port,
        help=f'Port to run the server on (default: {_config.port}).',
    )
    parser.add_argument(
        '--cache',
        action='store_true',
        help='Cache ADB results in memory to speed up front-end debugging.',
    )
    parser.add_argument(
        '--remote',
        action='store_true',
        help='Bind server to all interfaces instead of localhost.',
    )
    parser.add_argument(
        '--open',
        action='store_true',
        help='Disable security imprint and timeout (allow all IPs).',
    )
    args = parser.parse_args()

    # Initialize sub-components.
    _config.adb = AdbClient()

    # Process arguments.
    _config.port = args.port
    _config.use_cache = args.cache
    _config.local = not args.remote
    _config.require_imprint = not args.open
    _config.serial = _config.adb.get_device(args.serial)
    print(f"Using ADB device: {_config.serial}")

    Handler = (
        CachingMainRequestHandler if _config.use_cache else MainRequestHandler
    )
    if _config.use_cache:
        print('Caching is enabled.')

    if _config.require_imprint:
        print(
            f'Security imprint ENABLED. Waiting {_config.timeout}s '
            'for first connection...'
        )
    else:
        print('Security imprint DISABLED. Server is open to all IPs.')
    run(handler_class=Handler)


if __name__ == '__main__':
    main()
