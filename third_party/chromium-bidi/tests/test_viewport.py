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
from test_helpers import execute_command, goto_url


@pytest.mark.asyncio
async def test_set_viewport(websocket, context_id):
    await goto_url(websocket, context_id, "about:blank")

    await execute_command(
        websocket, {
            "method": "browsingContext.setViewport",
            "params": {
                "context": context_id,
                "viewport": {
                    "width": 300,
                    "height": 300,
                }
            }
        })

    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "({'width': window.innerWidth, 'height': window.innerHeight})",
                "target": {
                    "context": context_id
                },
                "resultOwnership": "none",
                "awaitPromise": True
            }
        })

    assert [["width", {
        "type": "number",
        "value": 300
    }], ["height", {
        "type": "number",
        "value": 300
    }]] == result["result"]["value"]


@pytest.mark.asyncio
async def test_set_viewport_unsupported(websocket, context_id):
    await goto_url(websocket, context_id, "about:blank")

    with pytest.raises(Exception) as exception_info:
        # https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:content/browser/devtools/protocol/emulation_handler.cc;l=232;drc=1890f3f74c8100eb1a3e945d34d6fd576d2a9061
        await execute_command(
            websocket, {
                "method": "browsingContext.setViewport",
                "params": {
                    "context": context_id,
                    "viewport": {
                        "width": 10000001,
                        "height": 10000001,
                    }
                }
            })

    assert {
        "error": "unsupported operation",
        "message": "Provided viewport dimensions are not supported"
    } == exception_info.value.args[0]
