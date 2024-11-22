# Copyright 2022 Google LLC.
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
from test_helpers import (ANY_TIMESTAMP, ANY_UUID, read_JSON_message,
                          send_JSON_command, subscribe)


@pytest.mark.asyncio
async def test_browsingContext_domContentLoaded_create_notReceived(
        websocket, assert_no_more_messages):
    await subscribe(websocket, ["browsingContext.domContentLoaded"])

    command_id = await send_JSON_command(websocket, {
        "method": "browsingContext.create",
        "params": {
            "type": "tab"
        }
    })

    response = await read_JSON_message(websocket)
    assert response == {
        'id': command_id,
        'result': {
            'context': ANY_STR,
        },
        'type': 'success',
    }

    await assert_no_more_messages()


@pytest.mark.asyncio
async def test_browsingContext_domContentLoaded_navigate_received(
        websocket, context_id, url_example, assert_no_more_messages,
        read_sorted_messages):
    await subscribe(websocket, ["browsingContext.domContentLoaded"])

    command_id = await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_example,
                "wait": "complete",
                "context": context_id
            }
        })

    messages = await read_sorted_messages(2)
    assert messages == [
        {
            'id': command_id,
            'result': {
                'navigation': ANY_UUID,
                'url': url_example,
            },
            'type': 'success',
        },
        {
            'method': 'browsingContext.domContentLoaded',
            'params': {
                'context': context_id,
                'navigation': ANY_UUID,
                'timestamp': ANY_TIMESTAMP,
                'url': url_example,
            },
            'type': 'event',
        },
    ]

    await assert_no_more_messages()
