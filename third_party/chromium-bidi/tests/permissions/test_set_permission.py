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
from permissions import query_permission, set_permission
from test_helpers import execute_command, get_origin, goto_url


@pytest.mark.asyncio
async def test_permissions_set_permission(websocket, context_id, url_example,
                                          test_chromedriver_mode):
    if test_chromedriver_mode:
        pytest.xfail(reason="ChromeDriver handles permissions differently")

    origin = get_origin(url_example)
    await goto_url(websocket, context_id, url_example)
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
        websocket, context_id, url_example, create_context):
    await goto_url(websocket, context_id, url_example)

    user_context_id = (await execute_command(websocket, {
        "method": "browser.createUserContext",
        "params": {}
    }))["userContext"]

    another_browsing_context_id = await create_context(
        user_context_id=user_context_id)
    origin = get_origin(url_example)

    await goto_url(websocket, another_browsing_context_id, url_example)

    # Both contexts have the same default permission state (prompt).
    assert await query_permission(websocket, context_id,
                                  'geolocation') == 'prompt'
    assert await query_permission(websocket, another_browsing_context_id,
                                  'geolocation') == 'prompt'

    # Permission changes in one context do not affect another.
    resp = await set_permission(websocket,
                                origin, {'name': 'geolocation'},
                                'granted',
                                user_context=user_context_id)
    assert resp == {}
    assert await query_permission(websocket, context_id,
                                  'geolocation') == 'prompt'
    assert await query_permission(websocket, another_browsing_context_id,
                                  'geolocation') == 'granted'

    # Permission can be set back to the original value.
    resp = await set_permission(websocket,
                                origin, {'name': 'geolocation'},
                                'prompt',
                                user_context=user_context_id)
    assert resp == {}
    assert await query_permission(websocket, context_id,
                                  'geolocation') == 'prompt'
    assert await query_permission(websocket, another_browsing_context_id,
                                  'geolocation') == 'prompt'


@pytest.mark.asyncio
@pytest.mark.parametrize("same_origin", [True, False])
async def test_permissions_set_permission_per_top_level_origin(
        websocket, context_id, url_example, iframe_id, html, same_origin):
    origin = get_origin(url_example)
    frame_url = html(same_origin=same_origin)
    await goto_url(websocket, iframe_id, frame_url)

    # Default permission is `prompt`.
    assert await query_permission(websocket, iframe_id,
                                  'storage-access') == 'prompt'

    # Set permissions for the same origin. Should be ignored in iframe, as it
    # has a different origin.
    resp = await set_permission(websocket, origin, {'name': 'storage-access'},
                                'granted')
    assert resp == {}

    # Assert the iframe's permission depending on `same_origin`.
    assert (await query_permission(websocket, iframe_id, 'storage-access')
            == 'granted') if same_origin else 'prompt'

    # Set permission for the iframe within top-level origin.
    resp = await set_permission(websocket,
                                origin, {'name': 'storage-access'},
                                'granted',
                                embedded_origin=frame_url)
    assert resp == {}
    # Assert the permission is applied in the iframe.
    assert await query_permission(websocket, iframe_id,
                                  'storage-access') == 'granted'

    # Reset permission.
    resp = await set_permission(websocket, origin, {'name': 'storage-access'},
                                'prompt')
    assert resp == {}
    # Assert permission is reset to the default.
    assert await query_permission(websocket, context_id,
                                  'storage-access') == 'prompt'
