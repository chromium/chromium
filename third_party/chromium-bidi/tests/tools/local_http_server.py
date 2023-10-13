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

from pytest_httpserver import HTTPServer


class LocalHttpServer:
    """A wrapper of `pytest_httpserver.httpserver` to simplify the usage. Sets
    up common use cases and provides url for them."""
    __http_server: HTTPServer
    __path_200 = "/200"
    __path_permanent_redirect = "/301"
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

    def url_200(self) -> str:
        """Returns the url for the 200 page with the `default_200_page_content`.
        """
        return self.__http_server.url_for(self.__path_200)

    def url_permanent_redirect(self) -> str:
        """Returns the url for the permanent redirect page, redirecting to the
        200 page."""
        return self.__http_server.url_for(self.__path_permanent_redirect)
