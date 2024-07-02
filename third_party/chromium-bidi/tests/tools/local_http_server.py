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
import ssl
from datetime import datetime
from pathlib import Path
from threading import Event
from typing import Literal

from pytest_httpserver import HTTPServer
from werkzeug.wrappers import Request, Response


class LocalHttpServer:
    """A wrapper of `pytest_httpserver.httpserver` to simplify the usage. Sets
    up common use cases and provides url for them."""

    __http_server: HTTPServer

    __start_time: datetime

    __path_base = "/"
    __path_favicon = "/favicon.ico"
    __path_200 = "/200"
    __path_permanent_redirect = "/301"
    __path_basic_auth = "/401"
    __path_hang_forever = "/hang_forever"
    __path_cacheable = "/cacheable"

    __protocol: Literal['http', 'https']

    content_200: str = 'default 200 page'

    def clear(self):
        self.__http_server.clear()

    def is_running(self):
        return self.__http_server.is_running()

    def stop(self):
        self.hang_forever_stop()
        self.__http_server.stop()

    def __init__(self, protocol: Literal['http', 'https'] = 'http') -> None:
        super().__init__()

        self.__protocol = protocol

        ssl_context = None
        if protocol == 'https':
            ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            cert_file_name = Path(__file__).parent / "cert.pem"
            key_file_name = Path(__file__).parent / "key.pem"
            ssl_context.load_cert_chain(cert_file_name, key_file_name)
        elif protocol != 'http':
            raise ValueError(f"Unsupported protocol: {protocol}")

        self.__http_server = HTTPServer(ssl_context=ssl_context)

        self.__http_server.start()
        self.__http_server.clear()

        self.__start_time = datetime.now()

        def html_doc(content):
            return f"<!DOCTYPE html><html><head><link rel='shortcut icon' href='data:image/x-icon;,' type='image/x-icon'></head><body>{content}</body></html>"

        self.__http_server \
            .expect_request(self.__path_base) \
            .respond_with_data(
                html_doc("I prevent CORS"),
                headers={"Content-Type": "text/html"})

        self.__http_server \
            .expect_request(self.__path_favicon) \
            .respond_with_data(
                "",
                headers={"Content-Type": "image/x-icon"})

        # Set up 200 page.
        self.__http_server \
            .expect_request(self.__path_200) \
            .respond_with_data(
                html_doc(self.content_200),
                headers={"Content-Type": "text/html"})

        # Set up permanent redirect.
        self.__http_server \
            .expect_request(self.__path_permanent_redirect) \
            .respond_with_data('', 301, {"Location": self.url_200()})

        def process_auth(request: Request):
            authorization = request.headers.get("Authorization")
            if authorization is not None:
                # If the authorization is a basic auth, return the decoded.
                if authorization.startswith("Basic ") and len(
                        authorization.split(" ")) == 2:
                    decoded = base64.b64decode(authorization.split(" ")[1])
                    return Response(decoded, 200, content_type="text/html")
                # Otherwise, return them as is with a 500 HTTP code.
                else:
                    return Response(authorization,
                                    500,
                                    content_type="text/html")

            return Response(
                'HTTP Error 401 Unauthorized: Access is denied', 401,
                {"WWW-Authenticate": 'Basic realm="Access to staging site"'})

        self.__http_server \
            .expect_request(self.__path_basic_auth) \
            .respond_with_handler(process_auth)

        self.hang_forever_stop_flag = None

        def hang_forever(_):
            self.hang_forever_stop_flag = Event()
            while not self.hang_forever_stop_flag.is_set():
                self.hang_forever_stop_flag.wait(60)

        self.__http_server.expect_request(self.__path_hang_forever) \
            .respond_with_handler(hang_forever)

        def cache(request: Request):
            content = html_doc(self.content_200)
            if_modified_since = request.headers.get("If-Modified-Since")

            if if_modified_since is not None:
                return Response(content, 304, content_type="text/html")
            else:
                return Response(
                    content,
                    200,
                    content_type="text/html",
                    headers={
                        "Cache-Control": 'public, max-age=31536000',
                        'Last-Modified':
                            self.__start_time.strftime("%Y-%m-%dT%H:%M:%SZ")
                    })

        self.__http_server.expect_request(self.__path_cacheable) \
            .respond_with_handler(cache)

    def hang_forever_stop(self):
        if self.hang_forever_stop_flag is not None:
            self.hang_forever_stop_flag.set()

    def _url_for(self, suffix: str, host: str = 'localhost') -> str:
        """
        Return an url for a given suffix.

        Implementation is the same as the original one, but with a customizable
        host: https://github.com/csernazs/pytest-httpserver/blob/8110d9d543de3b7c151bc1b5c8e85c01b05b226d/pytest_httpserver/httpserver.py#L665
        :param suffix: the suffix which will be added to the base url. It can
            start with ``/`` (slash) or not, the url will be the same.
        :param host: the host to use in the url. Default is ``localhost``.
        :return: the full url which refers to the server
        """
        if not suffix.startswith("/"):
            suffix = "/" + suffix

        host = self.__http_server.format_host(host)

        return "{}://{}:{}{}".format(self.__protocol, host,
                                     self.__http_server.port, suffix)

    def url_base(self, host='localhost') -> str:
        """Returns the url for the base page to navigate and prevent CORS.
        """
        return self._url_for(self.__path_base, host)

    def url_200(self, host='localhost') -> str:
        """Returns the url for the 200 page with the `default_200_page_content`.
        """
        return self._url_for(self.__path_200, host)

    def url_permanent_redirect(self) -> str:
        """Returns the url for the permanent redirect page, redirecting to the
        200 page."""
        return self._url_for(self.__path_permanent_redirect)

    def url_basic_auth(self) -> str:
        """Returns the url for the page with a basic auth."""
        return self._url_for(self.__path_basic_auth)

    def url_hang_forever(self) -> str:
        """Returns the url for the page, request to which will never be finished."""
        return self._url_for(self.__path_hang_forever)

    def url_cacheable(self, host='localhost') -> str:
        """Returns the url for the cacheable page with the `default_200_page_content`."""
        return self._url_for(self.__path_cacheable, host)
