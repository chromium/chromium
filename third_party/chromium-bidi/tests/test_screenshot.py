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
from test_helpers import (execute_command, goto_url, read_JSON_message,
                          send_JSON_command)

IMAGE_BASE_64 = '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'
GRADIENT_HTML = 'data:image/png;base64,' + IMAGE_BASE_64


@pytest.mark.asyncio
async def test_screenshot_happy_path(websocket, context_id):
    await goto_url(websocket, context_id, GRADIENT_HTML)

    command_result = await execute_command(websocket, {
        "method": "cdp.getSession",
        "params": {
            "context": context_id
        }
    })
    session_id = command_result["cdpSession"]

    # Set a fixed viewport to make the test deterministic.
    await execute_command(
        websocket, {
            "method": "cdp.sendCommand",
            "params": {
                "cdpMethod": "Emulation.setDeviceMetricsOverride",
                "cdpParams": {
                    "width": 200,
                    "height": 200,
                    "deviceScaleFactor": 1.0,
                    "mobile": False,
                },
                "cdpSession": session_id
            }
        })

    await send_JSON_command(
        websocket, {
            "id": 1,
            "method": "browsingContext.captureScreenshot",
            "params": {
                "context": context_id
            }
        })

    resp = await read_JSON_message(websocket)

    assert resp == {'id': 1, 'result': {'data': IMAGE_BASE_64}}
