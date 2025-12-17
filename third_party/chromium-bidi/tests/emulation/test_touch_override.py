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
from test_helpers import execute_command


@pytest_asyncio.fixture
async def initial_max_touch_points(websocket, context_id):
    return await get_max_touch_points(websocket, context_id)


async def get_max_touch_points(websocket, context_id):
    resp = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "navigator.maxTouchPoints",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
            }
        })
    return resp["result"]["value"]


@pytest.mark.asyncio
async def test_set_touch_override_per_browsing_context(
        websocket, context_id, initial_max_touch_points):
    # Set the touch override to 10 for a specific context.
    await execute_command(
        websocket, {
            'method': 'emulation.setTouchOverride',
            'params': {
                'contexts': [context_id],
                'maxTouchPoints': 10
            }
        })
    # Verify the override is applied.
    assert await get_max_touch_points(websocket, context_id) == 10

    # Clear the touch override by setting it to None (null in JSON).
    await execute_command(
        websocket, {
            'method': 'emulation.setTouchOverride',
            'params': {
                'contexts': [context_id],
                'maxTouchPoints': None
            }
        })
    # Verify the override is removed.
    assert await get_max_touch_points(websocket,
                                      context_id) == initial_max_touch_points


@pytest.mark.asyncio
async def test_set_touch_override_per_user_context(websocket, user_context_id,
                                                   create_context,
                                                   initial_max_touch_points):
    await execute_command(
        websocket, {
            'method': 'emulation.setTouchOverride',
            'params': {
                'userContexts': [user_context_id],
                'maxTouchPoints': 7
            }
        })

    context_id = await create_context(user_context_id)
    assert await get_max_touch_points(websocket, context_id) == 7

    default_context_id = await create_context()

    # Verify the override is removed.
    assert await get_max_touch_points(
        websocket, default_context_id) == initial_max_touch_points


@pytest.mark.asyncio
async def test_set_touch_override_globally(websocket, context_id,
                                           create_context,
                                           initial_max_touch_points):
    # Set the touch override globally (no contexts or userContexts specified).
    await execute_command(websocket, {
        'method': 'emulation.setTouchOverride',
        'params': {
            'maxTouchPoints': 5
        }
    })

    # Verify the override is applied to the existing context.
    assert await get_max_touch_points(websocket, context_id) == 5

    # Verify the override is applied to a newly created context.
    new_context_id = await create_context()
    assert await get_max_touch_points(websocket, new_context_id) == 5

    # Clear the global touch override.
    await execute_command(websocket, {
        'method': 'emulation.setTouchOverride',
        'params': {
            'maxTouchPoints': None
        }
    })

    # Verify the override is removed.
    assert await get_max_touch_points(websocket,
                                      context_id) == initial_max_touch_points
