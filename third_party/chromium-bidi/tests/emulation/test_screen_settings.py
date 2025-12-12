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
from test_helpers import execute_command, send_JSON_command

SOME_SCREEN_SETTINGS = {
    "width": 100,
    "height": 200,
}

ANOTHER_SCREEN_SETTINGS = {
    "width": 300,
    "height": 400,
}


@pytest_asyncio.fixture
async def initial_screen_settings(websocket, context_id):
    return await get_screen_settings(websocket, context_id)


async def get_screen_settings(websocket, context_id):
    """
    Returns browsing context's current screen settings.
    """
    resp = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": """({
                    width: screen.width,
                    height: screen.height,
                    availWidth: screen.availWidth,
                    availHeight: screen.availHeight,
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
async def test_screen_settings_set_and_clear(
    websocket,
    context_id,
    initial_screen_settings,
):
    await execute_command(
        websocket, {
            'method': 'emulation.setScreenSettingsOverride',
            'params': {
                'contexts': [context_id],
                'screenArea': {
                    'width': 100,
                    'height': 200,
                }
            }
        })

    settings = await get_screen_settings(websocket, context_id)
    assert settings['width'] == 100
    assert settings['height'] == 200

    await execute_command(
        websocket, {
            'method': 'emulation.setScreenSettingsOverride',
            'params': {
                'contexts': [context_id],
                'screenArea': None
            }
        })
    assert await get_screen_settings(websocket,
                                     context_id) == initial_screen_settings


@pytest.mark.asyncio
async def test_screen_settings_screen_area_and_viewport(
        websocket, context_id, read_messages, initial_screen_settings):
    # Set viewport
    command_id_1 = await send_JSON_command(
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

    await read_messages(
        1, filter_lambda=lambda x: 'id' in x and x['id'] == command_id_1)

    # Set Screen Area
    command_id_2 = await send_JSON_command(
        websocket, {
            'method': 'emulation.setScreenSettingsOverride',
            'params': {
                'contexts': [context_id],
                'screenArea': {
                    'width': 600,
                    'height': 800,
                }
            }
        })

    await read_messages(
        1, filter_lambda=lambda x: 'id' in x and x['id'] == command_id_2)

    settings = await get_screen_settings(websocket, context_id)
    assert settings['width'] == 600
    assert settings['height'] == 800

    # Also verify viewport is still set (by checking innerWidth/innerHeight)
    resp = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": """({
                    width: window.innerWidth,
                    height: window.innerHeight,
                    dpr: window.devicePixelRatio,
                })""",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
            }
        })

    viewport_result = {}
    for key, value in resp["result"]["value"]:
        viewport_result[key] = value["value"]

    assert viewport_result['width'] == 345
    assert viewport_result['height'] == 456
    assert viewport_result['dpr'] == 4


@pytest.mark.asyncio
async def test_screen_settings_per_user_context(
    websocket,
    user_context_id,
    create_context,
):
    # Set different screen settings overrides for different user contexts.
    await execute_command(
        websocket, {
            'method': 'emulation.setScreenSettingsOverride',
            'params': {
                'userContexts': ["default"],
                'screenArea': SOME_SCREEN_SETTINGS
            }
        })
    await execute_command(
        websocket, {
            'method': 'emulation.setScreenSettingsOverride',
            'params': {
                'userContexts': [user_context_id],
                'screenArea': ANOTHER_SCREEN_SETTINGS
            }
        })

    # Assert the overrides applied for the right contexts.
    browsing_context_id_1 = await create_context()
    settings_1 = await get_screen_settings(websocket, browsing_context_id_1)
    assert settings_1['width'] == SOME_SCREEN_SETTINGS['width']
    assert settings_1['height'] == SOME_SCREEN_SETTINGS['height']

    browsing_context_id_2 = await create_context(user_context_id)
    settings_2 = await get_screen_settings(websocket, browsing_context_id_2)
    assert settings_2['width'] == ANOTHER_SCREEN_SETTINGS['width']
    assert settings_2['height'] == ANOTHER_SCREEN_SETTINGS['height']


@pytest.mark.asyncio
async def test_screen_settings_per_browsing_context(
    websocket,
    context_id,
    another_context_id,
):
    # Set different screen settings overrides for different browsing contexts.
    await execute_command(
        websocket, {
            'method': 'emulation.setScreenSettingsOverride',
            'params': {
                'contexts': [context_id],
                'screenArea': SOME_SCREEN_SETTINGS
            }
        })
    await execute_command(
        websocket, {
            'method': 'emulation.setScreenSettingsOverride',
            'params': {
                'contexts': [another_context_id],
                'screenArea': ANOTHER_SCREEN_SETTINGS
            }
        })

    settings_1 = await get_screen_settings(websocket, context_id)
    assert settings_1['width'] == SOME_SCREEN_SETTINGS['width']
    assert settings_1['height'] == SOME_SCREEN_SETTINGS['height']

    settings_2 = await get_screen_settings(websocket, another_context_id)
    assert settings_2['width'] == ANOTHER_SCREEN_SETTINGS['width']
    assert settings_2['height'] == ANOTHER_SCREEN_SETTINGS['height']


@pytest.mark.asyncio
async def test_screen_settings_global_fail(websocket, ):
    # Try to set global screen settings override.
    with pytest.raises(Exception, match='invalid argument'):
        await execute_command(
            websocket, {
                'method': 'emulation.setScreenSettingsOverride',
                'params': {
                    'screenArea': SOME_SCREEN_SETTINGS
                }
            })


@pytest.mark.asyncio
async def test_screen_settings_browsing_context_precedence_over_user_context(
    websocket,
    user_context_id,
    create_context,
):
    # Set screen settings override for user context.
    await execute_command(
        websocket, {
            'method': 'emulation.setScreenSettingsOverride',
            'params': {
                'userContexts': [user_context_id],
                'screenArea': SOME_SCREEN_SETTINGS
            }
        })

    browsing_context_id = await create_context(user_context_id)

    # Check that user context settings are applied.
    settings = await get_screen_settings(websocket, browsing_context_id)
    assert settings['width'] == SOME_SCREEN_SETTINGS['width']
    assert settings['height'] == SOME_SCREEN_SETTINGS['height']

    # Set screen settings override for browsing context.
    await execute_command(
        websocket, {
            'method': 'emulation.setScreenSettingsOverride',
            'params': {
                'contexts': [browsing_context_id],
                'screenArea': ANOTHER_SCREEN_SETTINGS
            }
        })

    # Check that browsing context settings take precedence.
    settings = await get_screen_settings(websocket, browsing_context_id)
    assert settings['width'] == ANOTHER_SCREEN_SETTINGS['width']
    assert settings['height'] == ANOTHER_SCREEN_SETTINGS['height']

    # Unset screen settings override for browsing context.
    await execute_command(
        websocket, {
            'method': 'emulation.setScreenSettingsOverride',
            'params': {
                'contexts': [browsing_context_id],
                'screenArea': None
            }
        })

    # Check that it reverts to user context settings.
    settings = await get_screen_settings(websocket, browsing_context_id)
    assert settings['width'] == SOME_SCREEN_SETTINGS['width']
    assert settings['height'] == SOME_SCREEN_SETTINGS['height']
