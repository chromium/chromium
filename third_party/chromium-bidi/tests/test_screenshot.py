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
from anys import ANY_STR
from test_helpers import goto_url, read_JSON_message, send_JSON_command

GRADIENT_HTML = 'data:text/html,<body style="background: linear-gradient(to bottom, #ffffff 0%, #000000 100%); background-blend-mode: difference;">'


@pytest.mark.asyncio
async def test_screenshot_happy_path(websocket, context_id):
    await goto_url(websocket, context_id, GRADIENT_HTML)

    await send_JSON_command(
        websocket, {
            "id": 1,
            "method": "browsingContext.captureScreenshot",
            "params": {
                "context": context_id
            }
        })

    resp = await read_JSON_message(websocket)

    # TODO: Compare the screenshot data to a golden of fixed dimensions.
    assert resp == {'id': 1, 'result': {'data': ANY_STR, }}
