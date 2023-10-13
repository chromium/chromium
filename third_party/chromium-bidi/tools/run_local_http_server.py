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

import sys
from pathlib import Path

from pytest_httpserver import HTTPServer

# Current directory is not a module, so to import `LocalHttpServer`, its path
# has to be added to `sys.path`. It is done relative to this file's directory.
# The `flake8` is disabled for this reason.
sys.path.append(str(Path(__file__).resolve().parent.parent / 'tests/tools/'))

import local_http_server  # noqa: E402

httpserver = HTTPServer()
httpserver.start()
local_http_server = local_http_server.LocalHttpServer(httpserver)
print(f"""Local http server started...
  - 200: {local_http_server.url_200()}
  - 301 / permanent redirect: {local_http_server.url_permanent_redirect()}
""")
