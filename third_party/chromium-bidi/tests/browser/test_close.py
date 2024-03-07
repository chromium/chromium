# Copyright 2023 Google LLC.
# Copyright (c) Microsoft Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import pytest
from test_helpers import execute_command


@pytest.mark.asyncio
@pytest.mark.skip(
    reason="TODO: https://github.com/GoogleChromeLabs/chromium-bidi/issues/1964"
)
async def test_browser_close_response_received(websocket):

    # Just wait for the command as it will timeout if we don't receive it
    await execute_command(websocket, {"method": "browser.close", "params": {}})
