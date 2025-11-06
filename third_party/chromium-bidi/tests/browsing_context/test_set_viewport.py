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
from anys import ANY_LIST
from test_helpers import execute_command, send_JSON_command

CUSTOM_WIDTH = 345
CUSTOM_HEIGHT = 456
CUSTOM_DPR = 4


async def get_viewport(websocket, context_id):
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "(["
                              "window.innerWidth,"
                              "window.innerHeight,"
                              "window.devicePixelRatio"
                              "])",
                "target": {
                    "context": context_id
                },
                "resultOwnership": "none",
                "awaitPromise": True
            }
        })

    return {
        "width": result["result"]["value"][0]["value"],
        "height": result["result"]["value"][1]["value"],
        "devicePixelRatio": result["result"]["value"][2]["value"]
    }


async def assert_viewport(websocket, context_id, expected_viewport):
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "({"
                              "'width': window.innerWidth, "
                              "'height': window.innerHeight, "
                              "'devicePixelRatio': window.devicePixelRatio"
                              "})",
                "target": {
                    "context": context_id
                },
                "resultOwnership": "none",
                "awaitPromise": True
            }
        })

    expected_result = [
        (["width", {
            "value": expected_viewport["width"],
            "type": "number"
        }] if "width" in expected_viewport else ANY_LIST),
        (["height", {
            "value": expected_viewport["height"],
            "type": "number"
        }] if "height" in expected_viewport else ANY_LIST),
        ([
            "devicePixelRatio", {
                "value": expected_viewport["devicePixelRatio"],
                "type": "number"
            }
        ] if "devicePixelRatio" in expected_viewport else ANY_LIST),
    ]

    assert expected_result == result["result"]["value"]


@pytest.mark.asyncio
async def test_set_viewport_width_height(websocket, context_id):
    await execute_command(
        websocket, {
            "method": "browsingContext.setViewport",
            "params": {
                "context": context_id,
                "viewport": {
                    "width": CUSTOM_WIDTH,
                    "height": CUSTOM_HEIGHT,
                },
                "devicePixelRatio": None
            }
        })

    await assert_viewport(websocket, context_id, {
        "width": CUSTOM_WIDTH,
        "height": CUSTOM_HEIGHT,
    })


@pytest.mark.asyncio
async def test_set_viewport_for_default_user_context(websocket, context_id,
                                                     another_context_id,
                                                     create_context,
                                                     user_context_id):

    context_in_another_user_context = await create_context(user_context_id)
    another_viewport = await get_viewport(websocket,
                                          context_in_another_user_context)

    # Change viewport of the default user context.
    await execute_command(
        websocket, {
            "method": "browsingContext.setViewport",
            "params": {
                "userContexts": ["default"],
                "viewport": {
                    "width": CUSTOM_WIDTH,
                    "height": CUSTOM_HEIGHT,
                },
                "devicePixelRatio": CUSTOM_DPR
            }
        })

    # Assert default user context's viewports are changed.
    await assert_viewport(
        websocket, context_id, {
            "width": CUSTOM_WIDTH,
            "height": CUSTOM_HEIGHT,
            "devicePixelRatio": CUSTOM_DPR
        })
    await assert_viewport(
        websocket, another_context_id, {
            "width": CUSTOM_WIDTH,
            "height": CUSTOM_HEIGHT,
            "devicePixelRatio": CUSTOM_DPR
        })
    # Assert another user context's viewports is untouched.
    await assert_viewport(websocket, context_in_another_user_context,
                          another_viewport)


@pytest.mark.asyncio
async def test_set_viewport_for_custom_user_context(websocket, context_id,
                                                    create_context,
                                                    user_context_id):

    context_in_another_user_context = await create_context(user_context_id)
    another_context_in_another_user_context = await create_context(
        user_context_id)
    default_viewport = await get_viewport(websocket, context_id)

    # Change viewport of the new user context.
    await execute_command(
        websocket, {
            "method": "browsingContext.setViewport",
            "params": {
                "userContexts": [user_context_id],
                "viewport": {
                    "width": CUSTOM_WIDTH,
                    "height": CUSTOM_HEIGHT,
                },
                "devicePixelRatio": CUSTOM_DPR
            }
        })

    # Assert new user context's viewports is untouched.
    await assert_viewport(
        websocket, context_in_another_user_context, {
            "width": CUSTOM_WIDTH,
            "height": CUSTOM_HEIGHT,
            "devicePixelRatio": CUSTOM_DPR
        })
    await assert_viewport(
        websocket, another_context_in_another_user_context, {
            "width": CUSTOM_WIDTH,
            "height": CUSTOM_HEIGHT,
            "devicePixelRatio": CUSTOM_DPR
        })
    # Assert default user context's viewports are changed.
    await assert_viewport(websocket, context_id, default_viewport)


@pytest.mark.asyncio
async def test_set_viewport_dpr(websocket, context_id, html):
    await execute_command(
        websocket, {
            "method": "browsingContext.setViewport",
            "params": {
                "context": context_id,
                "viewport": None,
                "devicePixelRatio": CUSTOM_DPR,
            }
        })

    await assert_viewport(websocket, context_id,
                          {"devicePixelRatio": CUSTOM_DPR})


@pytest.mark.asyncio
@pytest.mark.parametrize("width,height", [
    (300, 10000001),
    (10000001, 300),
    (10000001, 10000001),
],
                         ids=[
                             "very big height",
                             "very big width",
                             "very big width and height",
                         ])
async def test_set_viewport_unsupported(websocket, context_id, width, height):
    with pytest.raises(
            Exception,
            match=str({
                "error": "unsupported operation",
                "message": "Viewport dimension over 10000000 are not supported"
            })):
        # https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:content/browser/devtools/protocol/emulation_handler.cc;l=232;drc=1890f3f74c8100eb1a3e945d34d6fd576d2a9061
        await execute_command(
            websocket, {
                "method": "browsingContext.setViewport",
                "params": {
                    "context": context_id,
                    "viewport": {
                        "width": width,
                        "height": height,
                    },
                    "devicePixelRatio": None
                }
            })


@pytest.mark.asyncio
async def test_set_viewport_with_screen_orientation(websocket, context_id,
                                                    read_messages):
    command_id_1 = await send_JSON_command(
        websocket, {
            "method": "browsingContext.setViewport",
            "params": {
                "context": context_id,
                "viewport": {
                    "width": CUSTOM_WIDTH,
                    "height": CUSTOM_HEIGHT,
                },
                "devicePixelRatio": None
            }
        })

    command_id_2 = await send_JSON_command(
        websocket, {
            'method': 'emulation.setScreenOrientationOverride',
            'params': {
                'contexts': [context_id],
                'screenOrientation': {
                    "natural": "landscape",
                    "type": "portrait-secondary"
                }
            }
        })

    # Wait for both commands to finish.
    await read_messages(2,
                        filter_lambda=lambda x: 'id' in x and x['id'] in
                        [command_id_1, command_id_2])

    # Assert viewport was not overridden by the screen orientation.
    await assert_viewport(websocket, context_id, {
        "width": CUSTOM_WIDTH,
        "height": CUSTOM_HEIGHT,
    })
