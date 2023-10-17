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
import time

from pytest_httpserver import HTTPServer
from werkzeug.wrappers import Request, Response


class LocalHttpServer:
    """A wrapper of `pytest_httpserver.httpserver` to simplify the usage. Sets
    up common use cases and provides url for them."""
    __http_server: HTTPServer
    __path_200 = "/200"
    __path_permanent_redirect = "/301"
    __path_basic_auth = "/401"
    __path_hang_forever = "/hang_forever"
    default_200_page_content: str = 'default 200 page'

    def __init__(self, http_server: HTTPServer) -> None:
        super().__init__()
        self.__http_server = http_server

        # Setting up 200 page.
        self.__http_server \
            .expect_request(self.__path_200) \
            .respond_with_data(
                f"<html><body>{self.default_200_page_content}</body></html>",
                headers={"Content-Type": "text/html"})

        # Setting up permanent redirect.
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
                '', 401,
                {"WWW-Authenticate": 'Basic realm="Access to staging site"'})

        self.__http_server \
            .expect_request(self.__path_basic_auth) \
            .respond_with_handler(process_auth)

        def hang_forever(request):
            while True:
                time.sleep(60)
            raise Exception("Should not reach here")

        self.__http_server.expect_request(self.__path_hang_forever) \
            .respond_with_handler(hang_forever)

    def url_200(self) -> str:
        """Returns the url for the 200 page with the `default_200_page_content`.
        """
        return self.__http_server.url_for(self.__path_200)

    def url_permanent_redirect(self) -> str:
        """Returns the url for the permanent redirect page, redirecting to the
        200 page."""
        return self.__http_server.url_for(self.__path_permanent_redirect)

    def url_basic_auth(self) -> str:
        """Returns the url for the page with a basic auth."""
        return self.__http_server.url_for(self.__path_basic_auth)

    def url_hang_forever(self) -> str:
        """Returns the url for the page, request to which will never be finished."""
        return self.__http_server.url_for(self.__path_hang_forever)
