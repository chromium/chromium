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

from subprocess import PIPE, Popen


class HttpProxyServer():
    """A wrapper of `tools/http-proxy.mjs` to simplify the usage. Sets
    up common use cases and provides url for them."""

    def __init__(self) -> None:
        self._url = ""

    def start(self):
        self._process = Popen(["node", "tools/http-proxy.mjs"],
                              stdout=PIPE,
                              shell=False)
        # Wait for the proxy to start.
        node_output_line = self._process.stdout.readline().decode(
            "utf-8").strip()
        # Assert the prefix is present before removing it.
        assert node_output_line.startswith("Listening on ")
        self._url = node_output_line.removeprefix("Listening on ").strip()

    def stop(self):
        """
        Stops the server and reads all URLs that have been proxied by the server.
        """
        lines = []
        self._process.terminate()
        while self._process.stdout.peek().decode("utf-8").strip() != '':
            line = self._process.stdout.readline().decode("utf-8").strip()
            lines.append(line)
        return lines

    def url(self):
        """
        Returns the proxy address without protocol prefix. Available after the
        `start` call has succeeded.
        """
        return self._url
