#  Copyright 2023 Google LLC.
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

import base64
import http.client
import json
import socket
import ssl
import time
import uuid
from contextlib import closing
from datetime import datetime, timezone
from pathlib import Path
from threading import Event, Thread
from typing import Any, Literal

from flask import Flask
from flask import Response as FlaskResponse
from flask import redirect, request, stream_with_context


# Helper to find a free port
def find_free_port() -> int:
    """Finds and returns an available port number."""
    with closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as s:
        s.bind(('', 0))
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        return s.getsockname()[1]


class LocalHttpServer:
    """
    A Flask-based local HTTP/S server. Sets up common use cases and provides url
    for them.
    """

    # Path constants
    __path_base = "/"
    __path_favicon = "/favicon.ico"
    __path_200 = "/200"
    __path_permanent_redirect = "/301"
    __path_basic_auth = "/401"
    __path_hang_forever = "/hang_forever"
    __path_hang_forever_download = "/hang_forever_download"
    __path_cacheable = "/cacheable"
    __path_echo = "/echo"

    content_200: str = 'default 200 page'

    __app: Flask
    __host: str
    __port: int
    __protocol: Literal['http', 'https']

    __start_time: datetime
    _dynamic_responses: dict[str, Any]
    hang_forever_stop_flag: Event
    _server_thread: Thread | None

    def clear(self) -> None:
        """
        Clears dynamically added responses. Static routes defined at startup remain.
        This differs from pytest-httpserver's clear which removes all expectations.
        """
        self._dynamic_responses.clear()

    def is_running(self) -> bool:
        """Checks if the server thread is alive and the server is responsive."""
        if self._server_thread is not None and self._server_thread.is_alive():
            # Perform a quick check to ensure the server is still responding
            return self._check_server_readiness(timeout_s=0.05)
        return False

    def stop(self) -> None:
        self.hang_forever_stop()
        """Stops the Flask server."""
        if not self._server_thread or not self._server_thread.is_alive():
            # Ensure it's cleaned up if already dead
            self._server_thread = None
            return

        # Terminate server thread.
        self._server_thread.join(timeout=0.1)
        self._server_thread = None

    def __html_doc(self, content: str) -> str:
        return f"<!DOCTYPE html><html><head><link rel='shortcut icon' href='data:image/x-icon;,' type='image/x-icon'></head><body>{content}</body></html>"

    def __init__(self, host: str = 'localhost', ssl_cert_prefix=None) -> None:
        self.__app = Flask(__name__)
        # Important for some Flask behaviors in a test context
        self.__app.testing = True
        self.__host = host
        self.__protocol = 'http' if ssl_cert_prefix is None else "https"
        self.__port = find_free_port()

        ssl_context = None
        if ssl_cert_prefix is not None:
            current_dir = Path(__file__).parent
            cert_file = current_dir / f"certs/{ssl_cert_prefix}.crt"
            key_file = current_dir / f"certs/{ssl_cert_prefix}.key"
            if not cert_file.exists():
                raise FileNotFoundError(
                    f"SSL certificate file not found in {cert_file}")
            if not key_file.exists():
                raise FileNotFoundError(
                    f"SSL key file not found in {key_file}")
            ssl_context = (str(cert_file), str(key_file))

        self.__start_time = datetime.now(timezone.utc)
        self._dynamic_responses = {}
        self.hang_forever_stop_flag = Event()
        self._server_thread = None

        self._setup_routes()
        self._start_server(ssl_context)

    def _setup_routes(self) -> None:
        """Defines all the Flask routes for the server."""

        @self.__app.route(self.__path_base)
        def base_route():
            return FlaskResponse(self.__html_doc("I prevent CORS"),
                                 mimetype="text/html")

        @self.__app.route(self.__path_favicon)
        def favicon_route():
            return FlaskResponse("", mimetype="image/x-icon")

        @self.__app.route(self.__path_200)
        def route_200_default():
            return FlaskResponse(self.__html_doc(self.content_200),
                                 mimetype="text/html")

        @self.__app.route(f"{self.__path_200}/<string:response_id>")
        def route_200_dynamic(response_id: str):
            data = self._dynamic_responses.get(response_id)
            if data:
                return FlaskResponse(data["content"],
                                     mimetype=data["content_type"],
                                     headers=data["headers"])
            return FlaskResponse("Not Found", status=404)

        @self.__app.route(self.__path_echo)
        def process_echo():
            data = {
                "method": request.method,
                "args": request.args,
                "headers": dict(request.headers),
                "origin": request.origin,
                "json": request.json if request.is_json else None,
                "form": request.form if request.form else None,
                "data": request.data.decode('utf-8') if request.data else None,
            }
            return FlaskResponse(json.dumps(data), mimetype="application/json")

        @self.__app.route(self.__path_permanent_redirect)
        def route_permanent_redirect():
            return redirect(self.url_200(), code=301)

        @self.__app.route(self.__path_basic_auth)
        def process_auth():
            authorization = request.headers.get("Authorization")
            if authorization is not None:
                if authorization.startswith("Basic ") and len(
                        authorization.split(" ")) == 2:
                    # If the authorization is a basic auth, return the decoded.
                    decoded = base64.b64decode(authorization.split(" ")[1])
                    return FlaskResponse(decoded,
                                         status=200,
                                         mimetype="text/html")
                else:
                    # Otherwise, return them as is with a 500 HTTP code.
                    return FlaskResponse(authorization,
                                         status=500,
                                         mimetype="text/html")
            # No Authorization header
            return FlaskResponse(
                'HTTP Error 401 Unauthorized: Access is denied',
                status=401,
                mimetype="text/html",
                headers={
                    "WWW-Authenticate": 'Basic realm="Access to staging site"'
                })

        @self.__app.route(self.__path_hang_forever)
        def hang_forever():
            # Reset if called multiple times
            self.hang_forever_stop_flag.clear()
            self.hang_forever_stop_flag.wait()
            return FlaskResponse("Request unblocked.",
                                 status=200,
                                 mimetype="text/html")

        @self.__app.route(self.__path_hang_forever_download)
        def hang_forever_download():

            def content_stream():
                """
                Returns a part of the content, waits for the
                `hang_forever_stop_flag` and then returns the rest.
                """
                yield "CONTENT_START"
                self.hang_forever_stop_flag.clear()
                self.hang_forever_stop_flag.wait()
                return "\nCONTENT_END"

            return FlaskResponse(
                stream_with_context(content_stream()),
                status=200,
                mimetype="text/html",
                headers={
                    'Content-Disposition': 'attachment; filename="partially_downloaded_file.txt"',
                    'Content-Type': 'text/plain',
                })

        @self.__app.route(self.__path_cacheable)
        def cache():
            content = self.__html_doc(self.content_200)
            if_modified_since = request.headers.get("If-Modified-Since")

            if if_modified_since is not None:
                # HTTP 304 responses must not contain a message-body
                return FlaskResponse("", status=304)
            else:
                return FlaskResponse(
                    content,
                    status=200,
                    mimetype="text/html",
                    headers={
                        "Cache-Control": 'public, max-age=31536000',
                        # HTTP spec prefers RFC 1123 date format (GMT)
                        'Last-Modified': self.__start_time.strftime(
                            "%a, %d %b %Y %H:%M:%S GMT")
                    })

    def _check_server_readiness(self, timeout_s: float = 0.1) -> bool:
        """Checks if the server is up and responding to a basic request."""
        try:
            conn: http.client.HTTPConnection | http.client.HTTPSConnection
            if self.__protocol == 'https':
                context = ssl.create_default_context()
                context.check_hostname = False
                # For self-signed certs
                context.verify_mode = ssl.CERT_NONE
                conn = http.client.HTTPSConnection(self.__host,
                                                   self.__port,
                                                   timeout=timeout_s,
                                                   context=context)
            else:
                conn = http.client.HTTPConnection(self.__host,
                                                  self.__port,
                                                  timeout=timeout_s)

            conn.request("GET", self.__path_base)
            response = conn.getresponse()
            # Important to consume the response
            response.read()
            conn.close()
            # Check for any 2xx success
            return 200 <= response.status < 300
        except (ConnectionRefusedError, TimeoutError, OSError,
                http.client.HTTPException, ssl.SSLError):
            return False
        except Exception:
            # Catch any other unexpected errors during check
            return False

    def _wait_for_server_startup(self, max_wait_s: int = 60) -> None:
        """Waits for the Flask server to start by polling a readiness check."""
        start_time_monotonic = time.monotonic()
        while time.monotonic() - start_time_monotonic < max_wait_s:
            if self._check_server_readiness():
                return
            # Short sleep before retrying
            time.sleep(0.1)
        raise RuntimeError(
            f"Flask server failed to start on {self.__protocol}://{self.__host}:{self.__port} within {max_wait_s}s."
        )

    def _start_server(self,
                      ssl_context_config: tuple[str, str] | None) -> None:
        """Starts the Flask development server in a separate thread."""
        if self.is_running():
            return

        kwargs = {
            'host': self.__host,
            'port': self.__port,
            # Should be False for stability and threaded mode
            'debug': False,
            # Reloader must be False when running in a thread
            'use_reloader': False
        }
        if self.__protocol == 'https':
            kwargs['ssl_context'] = ssl_context_config

        def run_server_thread():
            try:
                self.__app.run(**kwargs)
            except Exception as e:
                # This might catch errors like "address already in use" if
                # find_free_port failed, though it's unlikely.
                print(f"Error running Flask server thread: {e}")

        self._server_thread = Thread(target=run_server_thread, daemon=True)
        self._server_thread.start()
        self._wait_for_server_startup()

    def hang_forever_stop(self):
        # Release any hanging requests
        self.hang_forever_stop_flag.set()

    def _build_url(self, path: str) -> str:
        """Constructs a full URL for a given path on this server."""
        return f"{self.__protocol}://{self.__host}:{self.__port}{path}"

    def origin(self) -> str:
        """Returns the origin (scheme://host:port) of the server."""
        return f"{self.__protocol}://{self.__host}:{self.__port}"

    def url_base(self) -> str:
        """Returns the URL for the base page (used to prevent CORS issues)."""
        return self._build_url(self.__path_base)

    def url_200(self,
                content: str | None = None,
                content_type: str = "text/html",
                headers: dict[str, str] | None = None) -> str:
        """
        Returns a URL that serves a 200 response.
        If 'content' is provided, a unique URL is generated for that specific content.
        Otherwise, returns the URL for the default 200 page.
        """
        if headers is None:
            headers = {}

        if content is not None:
            response_id = str(uuid.uuid4())

            final_content = content
            if content_type == "text/html":
                # Wrap in basic HTML structure if serving HTML, as per original logic
                final_content = self.__html_doc(content)

            self._dynamic_responses[response_id] = {
                "content": final_content,
                "content_type": content_type,
                # User-provided headers
                "headers": headers
            }
            path = f"{self.__path_200}/{response_id}"
            return self._build_url(path)

        return self._build_url(self.__path_200)

    def url_echo(self) -> str:
        """Returns the URL for the base page (used to prevent CORS issues)."""
        return self._build_url(self.__path_echo)

    def url_permanent_redirect(self) -> str:
        """Returns the URL for a page that permanently redirects to the default 200 page."""
        return self._build_url(self.__path_permanent_redirect)

    def url_basic_auth(self) -> str:
        """Returns the URL for a page protected by Basic authentication."""
        return self._build_url(self.__path_basic_auth)

    def url_hang_forever(self) -> str:
        """Returns the URL for a page that will hang until `hang_forever_stop()` is called."""
        return self._build_url(self.__path_hang_forever)

    def url_hang_forever_download(self) -> str:
        """Returns the URL for a page that will hang until `hang_forever_stop()` is called."""
        return self._build_url(self.__path_hang_forever_download)

    def url_cacheable(self) -> str:
        """Returns the URL for a cacheable page (using Last-Modified and If-Modified-Since)."""
        return self._build_url(self.__path_cacheable)
