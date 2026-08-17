#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for run_server.py."""

import io
import sys
import unittest
from http import HTTPStatus
from unittest.mock import MagicMock, patch

# Prevent Python from creating a __pycache__ directory in the source tree.
sys.dont_write_bytecode = True
import run_server


class RunServerTest(unittest.TestCase):
    def setUp(self):
        # Suppress print statements during tests.
        self.held_stdout = io.StringIO()
        self.original_stdout = sys.stdout
        sys.stdout = self.held_stdout

        # Reset the global configuration before each test.
        run_server._config = run_server.ServerConfig()
        run_server._config.adb = MagicMock()
        run_server._config.require_imprint = False

    def tearDown(self):
        # Restore original stdout.
        sys.stdout = self.original_stdout

    def create_mock_handler(self, path, client_ip='127.0.0.1'):
        """Creates a partially initialized handler for testing do_GET."""
        handler = run_server.MainRequestHandler.__new__(
            run_server.MainRequestHandler
        )
        handler.path = path
        handler.client_address = (client_ip, 12345)
        handler.send_error = MagicMock()
        handler.send_response = MagicMock()
        handler.send_header = MagicMock()
        handler.end_headers = MagicMock()
        handler.wfile = MagicMock()
        return handler

    # --- Data Parsing Tests ---

    def test_density_parsing_standard(self):
        mock_result = MagicMock()
        mock_result.stdout = "Physical density: 480\n"
        run_server._config.adb.run.return_value = mock_result

        handler = self.create_mock_handler('/api/density.json')
        # 480 / 160.0 = 3.0
        self.assertEqual(handler._get_density_data(), 3.0)

    def test_density_parsing_override(self):
        mock_result = MagicMock()
        mock_result.stdout = "Physical density: 480\nOverride density: 560\n"
        run_server._config.adb.run.return_value = mock_result

        handler = self.create_mock_handler('/api/density.json')
        # 560 / 160.0 = 3.5
        self.assertEqual(handler._get_density_data(), 3.5)

    def test_screenshot_trimming(self):
        # Simulate a warning message prepended to the binary PNG output.
        fake_binary = b'WARNING: Display 1 is off.\n\x89PNG\r\n\x1a\nDATA'
        mock_result = MagicMock()
        mock_result.stdout = fake_binary
        run_server._config.adb.run.return_value = mock_result

        handler = self.create_mock_handler('/api/screenshot.png')
        png_data = handler._get_screenshot_data()
        self.assertEqual(png_data, b'\x89PNG\r\n\x1a\nDATA')

    # --- Security & Routing Tests ---

    @patch('http.server.SimpleHTTPRequestHandler.do_GET')
    def test_allowed_extensions(self, mock_super_do_get):
        for path in ['/index.html', '/style.css', '/app.js', '/model.js']:
            handler = self.create_mock_handler(path)
            handler.do_GET()
            handler.send_error.assert_not_called()
            mock_super_do_get.assert_called()

    def test_root_redirects_to_index(self):
        handler = self.create_mock_handler('/')
        # Prevent it from actually trying to serve the file
        with patch('http.server.SimpleHTTPRequestHandler.do_GET'):
            handler.do_GET()
            self.assertEqual(handler.path, '/index.html')

    def test_security_imprinting(self):
        run_server._config.require_imprint = True

        # First request sets the imprinted IP.
        handler1 = self.create_mock_handler('/index.html', client_ip='10.0.0.1')
        with patch('http.server.SimpleHTTPRequestHandler.do_GET'):
            handler1.do_GET()
            self.assertEqual(run_server._config.allowed_client_ip, '10.0.0.1')
            handler1.send_error.assert_not_called()

        # Request from the same IP is allowed.
        handler2 = self.create_mock_handler('/style.css', client_ip='10.0.0.1')
        with patch('http.server.SimpleHTTPRequestHandler.do_GET'):
            handler2.do_GET()
            handler2.send_error.assert_not_called()

        # Request from a different IP is blocked.
        handler3 = self.create_mock_handler('/index.html', client_ip='10.0.0.2')
        handler3.do_GET()
        handler3.send_error.assert_called_with(
            HTTPStatus.FORBIDDEN,
            "Forbidden: Server is imprinted to a different IP.",
        )


if __name__ == '__main__':
    unittest.main()
