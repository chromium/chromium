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

import pytest
from permissions import get_origin, query_permission, set_permission
from test_helpers import execute_command, goto_url


@pytest.mark.asyncio
async def test_permissions_set_permission(websocket, context_id, example_url):
    origin = get_origin(example_url)
    await goto_url(websocket, context_id, example_url)
    assert await query_permission(websocket, context_id,
                                  'geolocation') == 'prompt'
    resp = await set_permission(websocket, origin, {'name': 'geolocation'},
                                'granted')
    assert resp == {}
    assert await query_permission(websocket, context_id,
                                  'geolocation') == 'granted'
    resp = await set_permission(websocket, origin, {'name': 'geolocation'},
                                'prompt')
    assert resp == {}
    assert await query_permission(websocket, context_id,
                                  'geolocation') == 'prompt'


@pytest.mark.asyncio
@pytest.mark.skip(reason="See chromium-bidi/issues#1610")
async def test_permissions_set_permission_in_user_context(
        websocket, context_id, example_url):
    await goto_url(websocket, context_id, example_url)

    user_context = await execute_command(websocket, {
        "method": "browser.createUserContext",
        "params": {}
    })

    browsing_context = await execute_command(
        websocket, {
            "method": "browsingContext.create",
            "params": {
                "type": "tab",
                "userContext": user_context["userContext"]
            }
        })

    origin = get_origin(example_url)

    await goto_url(websocket, browsing_context['context'], example_url)

    assert await query_permission(websocket, context_id,
                                  'geolocation') == 'prompt'
    assert await query_permission(websocket, browsing_context['context'],
                                  'geolocation') == 'prompt'

    resp = await set_permission(websocket,
                                origin, {'name': 'geolocation'},
                                'granted',
                                user_context=user_context["userContext"])
    assert resp == {}
    assert await query_permission(websocket, context_id,
                                  'geolocation') == 'prompt'
    assert await query_permission(websocket, browsing_context['context'],
                                  'geolocation') == 'granted'

    resp = await set_permission(websocket,
                                origin, {'name': 'geolocation'},
                                'prompt',
                                user_context=user_context["userContext"])
    assert resp == {}
    assert await query_permission(websocket, context_id,
                                  'geolocation') == 'prompt'
    assert await query_permission(websocket, browsing_context['context'],
                                  'geolocation') == 'prompt'
