#   Copyright 2025 Google LLC.
#   Copyright (c) Microsoft Corporation.
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import pytest
import pytest_asyncio
from test_helpers import execute_command, goto_url, send_JSON_command

SOME_BIDI_SCREEN_ORIENTATION = {
    "natural": "landscape",
    "type": "portrait-secondary"
}
SOME_WEB_SCREEN_ORIENTATION = {"angle": 270, "type": "portrait-secondary"}

ANOTHER_BIDI_SCREEN_ORIENTATION = {
    "natural": "portrait",
    "type": "landscape-primary"
}
ANOTHER_WEB_SCREEN_ORIENTATION = {"angle": 90, "type": "landscape-primary"}


@pytest_asyncio.fixture
async def initial_screen_orientation(websocket, context_id):
    return await get_screen_orientation(websocket, context_id)


@pytest_asyncio.fixture(autouse=True)
async def activate_context(websocket, context_id):
    # Activation is required, as orientation is only available on an active
    # context.
    await execute_command(websocket, {
        "method": "browsingContext.activate",
        "params": {
            "context": context_id
        }
    })


async def get_screen_orientation(websocket, context_id):
    """
    Returns browsing context's current orientation.
    """
    resp = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": """({
                    angle: screen.orientation.angle,
                    type: screen.orientation.type
                })""",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
            }
        })
    if "result" not in resp:
        return resp

    result = {}
    for key, value in resp["result"]["value"]:
        result[key] = value["value"]
    return result


@pytest.mark.asyncio
async def test_screen_orientation_set_and_clear(
    websocket,
    context_id,
    initial_screen_orientation,
):
    await execute_command(
        websocket, {
            'method': 'emulation.setScreenOrientationOverride',
            'params': {
                'contexts': [context_id],
                'screenOrientation': SOME_BIDI_SCREEN_ORIENTATION
            }
        })

    assert await get_screen_orientation(
        websocket, context_id) == SOME_WEB_SCREEN_ORIENTATION

    await execute_command(
        websocket, {
            'method': 'emulation.setScreenOrientationOverride',
            'params': {
                'contexts': [context_id],
                'screenOrientation': None
            }
        })
    assert await get_screen_orientation(
        websocket, context_id) == initial_screen_orientation


@pytest.mark.asyncio
async def test_screen_orientation_per_user_context(
    websocket,
    user_context_id,
    create_context,
):
    # Set different orientation overrides for different user contexts.
    await execute_command(
        websocket, {
            'method': 'emulation.setScreenOrientationOverride',
            'params': {
                'userContexts': ["default"],
                'screenOrientation': SOME_BIDI_SCREEN_ORIENTATION
            }
        })
    await execute_command(
        websocket, {
            'method': 'emulation.setScreenOrientationOverride',
            'params': {
                'userContexts': [user_context_id],
                'screenOrientation': ANOTHER_BIDI_SCREEN_ORIENTATION
            }
        })

    # Assert the overrides applied for the right contexts.
    browsing_context_id_1 = await create_context()
    emulated_screen_orientation_1 = await get_screen_orientation(
        websocket, browsing_context_id_1)
    assert emulated_screen_orientation_1 == SOME_WEB_SCREEN_ORIENTATION

    browsing_context_id_2 = await create_context(user_context_id)
    emulated_screen_orientation_2 = await get_screen_orientation(
        websocket, browsing_context_id_2)
    assert emulated_screen_orientation_2 == ANOTHER_WEB_SCREEN_ORIENTATION


@pytest.mark.asyncio
async def test_screen_orientation_per_browsing_context(
    websocket,
    context_id,
    another_context_id,
):
    # Set different orientation overrides for different user contexts.
    await execute_command(
        websocket, {
            'method': 'emulation.setScreenOrientationOverride',
            'params': {
                'contexts': [context_id],
                'screenOrientation': SOME_BIDI_SCREEN_ORIENTATION
            }
        })
    await execute_command(
        websocket, {
            'method': 'emulation.setScreenOrientationOverride',
            'params': {
                'contexts': [another_context_id],
                'screenOrientation': ANOTHER_BIDI_SCREEN_ORIENTATION
            }
        })

    assert await get_screen_orientation(
        websocket, context_id) == SOME_WEB_SCREEN_ORIENTATION
    assert await get_screen_orientation(
        websocket, another_context_id) == ANOTHER_WEB_SCREEN_ORIENTATION


@pytest.mark.asyncio
async def test_screen_orientation_iframe(websocket, context_id, iframe_id,
                                         initial_screen_orientation, html):
    await execute_command(
        websocket, {
            'method': 'emulation.setScreenOrientationOverride',
            'params': {
                'contexts': [context_id],
                'screenOrientation': SOME_BIDI_SCREEN_ORIENTATION
            }
        })

    assert await get_screen_orientation(
        websocket, iframe_id) == SOME_WEB_SCREEN_ORIENTATION

    pytest.xfail(
        "TODO: https://github.com/GoogleChromeLabs/chromium-bidi/issues/3532")

    # Move iframe out of process.
    await goto_url(websocket, iframe_id,
                   html("<h1>FRAME</h1>", same_origin=False))

    assert await get_screen_orientation(
        websocket, iframe_id) == SOME_WEB_SCREEN_ORIENTATION

    await execute_command(
        websocket, {
            'method': 'emulation.setScreenOrientationOverride',
            'params': {
                'contexts': [context_id],
                'screenOrientation': ANOTHER_BIDI_SCREEN_ORIENTATION
            }
        })

    assert await get_screen_orientation(
        websocket, iframe_id) == SOME_WEB_SCREEN_ORIENTATION

    await execute_command(
        websocket, {
            'method': 'emulation.setScreenOrientationOverride',
            'params': {
                'contexts': [context_id],
                'screenOrientation': None
            }
        })

    assert await get_screen_orientation(
        websocket, iframe_id) == initial_screen_orientation


@pytest.mark.asyncio
async def test_screen_orientation_with_set_viewport(websocket, context_id,
                                                    initial_screen_orientation,
                                                    read_messages):
    command_id_1 = await send_JSON_command(
        websocket, {
            'method': 'emulation.setScreenOrientationOverride',
            'params': {
                'contexts': [context_id],
                'screenOrientation': SOME_BIDI_SCREEN_ORIENTATION
            }
        })
    command_id_2 = await send_JSON_command(
        websocket, {
            "method": "browsingContext.setViewport",
            "params": {
                "context": context_id,
                "viewport": {
                    "width": 345,
                    "height": 456,
                },
                "devicePixelRatio": 4
            }
        })

    # Wait for both commands to finish.
    await read_messages(2,
                        filter_lambda=lambda x: 'id' in x and x['id'] in
                        [command_id_1, command_id_2])

    # Assert screen orientation was not overridden by setViewport.
    assert await get_screen_orientation(
        websocket, context_id) == SOME_WEB_SCREEN_ORIENTATION

    command_id_1 = await send_JSON_command(
        websocket, {
            'method': 'emulation.setScreenOrientationOverride',
            'params': {
                'contexts': [context_id],
                'screenOrientation': None
            }
        })
    command_id_2 = await send_JSON_command(
        websocket, {
            "method": "browsingContext.setViewport",
            "params": {
                "context": context_id,
                "viewport": None,
                "devicePixelRatio": None
            }
        })

    # Wait for both commands to finish.
    await read_messages(2,
                        filter_lambda=lambda x: 'id' in x and x['id'] in
                        [command_id_1, command_id_2])

    # Assert screen orientation was not overridden by setViewport.
    assert await get_screen_orientation(
        websocket, context_id) == initial_screen_orientation
