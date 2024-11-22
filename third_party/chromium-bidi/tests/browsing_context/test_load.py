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
from test_helpers import (ANY_TIMESTAMP, ANY_UUID, read_JSON_message,
                          send_JSON_command, subscribe)


@pytest.mark.asyncio
async def test_browsingContext_noInitialLoadEvents(websocket, html,
                                                   assert_no_more_messages):
    # Due to the nature, the test does not always fail, even if the
    # implementation does not guarantee the initial context to be fully loaded.
    # The test asserts there was no initial "browsingContext.load" emitted
    # during the following steps:
    # 1. Subscribe for the "browsingContext.load" event.
    # 2. Get the currently open context.
    # 3. Navigate to some url.
    # 4. Verify the new page is loaded.

    url = html("<h2>test</h2>")

    await send_JSON_command(
        websocket, {
            "id": 1,
            "method": "session.subscribe",
            "params": {
                "events": ["browsingContext.load"]
            }
        })

    resp = await read_JSON_message(websocket)
    assert resp["id"] == 1

    await send_JSON_command(websocket, {
        "id": 2,
        "method": "browsingContext.getTree",
        "params": {}
    })

    resp = await read_JSON_message(websocket)
    assert resp[
        "id"] == 2, "The message should be result of command `browsingContext.getTree` with `id`: 2"
    context_id = resp["result"]["contexts"][0]["context"]

    await send_JSON_command(
        websocket, {
            "id": 3,
            "method": "browsingContext.navigate",
            "params": {
                "url": url,
                "wait": "none",
                "context": context_id
            }
        })

    resp = await read_JSON_message(websocket)
    assert resp["id"] == 3
    navigation = resp["result"]["navigation"]

    # Wait for the navigated page to be loaded.
    resp = await read_JSON_message(websocket)
    assert {
        'type': 'event',
        'method': 'browsingContext.load',
        'params': {
            'context': context_id,
            'navigation': navigation,
            'timestamp': ANY_TIMESTAMP,
            'url': url
        }
    } == resp
    assert_no_more_messages()


@pytest.mark.asyncio
async def test_browsingContext_load_properNavigation(websocket, context_id,
                                                     url_example,
                                                     read_sorted_messages,
                                                     assert_no_more_messages):
    await subscribe(websocket, "browsingContext.load")

    command_id = await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_example,
                "wait": "none",
                "context": context_id
            }
        })

    [command_result, load_event] = await read_sorted_messages(2)
    assert [command_result, load_event] == [
        {
            'id': command_id,
            'result': {
                'navigation': ANY_UUID,
                'url': url_example,
            },
            'type': 'success',
        },
        {
            'method': 'browsingContext.load',
            'params': {
                'context': context_id,
                'navigation': ANY_UUID,
                'timestamp': ANY_TIMESTAMP,
                'url': url_example,
            },
            'type': 'event',
        },
    ]

    assert command_result['result']['navigation'] == load_event['params'][
        'navigation']

    await assert_no_more_messages()
