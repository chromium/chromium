#  Copyright 2024 Google LLC.
#  Copyright (c) Microsoft Corporation.
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

from __future__ import annotations

import http.server
import threading


class _ProxyRequestHandler(http.server.BaseHTTPRequestHandler):
    server: _ProxyHTTPServer

    def log_message(self, format, *args):
        # Suppress logging to keep test output clean
        pass

    def do_GET(self):
        self._handle_proxy()

    def do_POST(self):
        self._handle_proxy()

    def do_PUT(self):
        self._handle_proxy()

    def do_DELETE(self):
        self._handle_proxy()

    def do_HEAD(self):
        self._handle_proxy()

    def do_OPTIONS(self):
        self._handle_proxy()

    def _handle_proxy(self):
        target_url = self.path
        self.server.proxied_urls.append(target_url)

        body = b"<html><body>Proxied response</body></html>"
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=UTF-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


class _ProxyHTTPServer(http.server.ThreadingHTTPServer):
    def __init__(self, server_address, RequestHandlerClass):
        super().__init__(server_address, RequestHandlerClass)
        self.proxied_urls: list[str] = []


class HttpProxyServer:
    """A pure-Python HTTP proxy server for tests.

    Sets up common test use cases and tracks proxied URLs without requiring
    an external Node.js subprocess.
    """

    def __init__(self) -> None:
        self._url = ""
        self._server: _ProxyHTTPServer | None = None
        self._thread: threading.Thread | None = None

    def start(self, host: str = "127.0.0.1"):
        self._server = _ProxyHTTPServer((host, 0), _ProxyRequestHandler)
        port = self._server.server_address[1]
        self._url = f"{host}:{port}"
        self._thread = threading.Thread(target=self._server.serve_forever, daemon=True)
        self._thread.start()

    def stop(self) -> list[str]:
        """Stops the server and returns all URLs that have been proxied."""
        lines: list[str] = []
        if self._server:
            lines = list(self._server.proxied_urls)
            self._server.shutdown()
            self._server.server_close()
            self._server = None
        if self._thread:
            self._thread.join(timeout=2.0)
            self._thread = None
        return lines

    def url(self) -> str:
        """Returns the proxy address without protocol prefix."""
        return self._url
