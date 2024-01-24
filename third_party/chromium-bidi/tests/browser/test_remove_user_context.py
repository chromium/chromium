# Copyright 2024 Google LLC.
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
from test_helpers import (execute_command, send_JSON_command, subscribe,
                          wait_for_event)


@pytest.mark.asyncio
async def test_browser_remove_user_context(websocket):
    result = await execute_command(websocket, {
        "method": "browser.createUserContext",
        "params": {}
    })
    user_context_id = result['userContext']

    result = await execute_command(websocket, {
        "method": "browser.getUserContexts",
        "params": {}
    })
    assert result['userContexts'] == [{
        'userContext': 'default'
    }, {
        'userContext': user_context_id
    }]

    await execute_command(
        websocket, {
            "method": "browser.removeUserContext",
            "params": {
                "userContext": user_context_id
            }
        })

    result = await execute_command(websocket, {
        "method": "browser.getUserContexts",
        "params": {}
    })
    assert result['userContexts'] == [{'userContext': 'default'}]


@pytest.mark.asyncio
async def test_browser_remove_user_context_closes_browsing_context(websocket):
    user_context = await execute_command(websocket, {
        "method": "browser.createUserContext",
        "params": {}
    })

    await subscribe(websocket, ["browsingContext.contextDestroyed"])

    browsing_context = await execute_command(
        websocket, {
            "method": "browsingContext.create",
            "params": {
                "type": "window",
                "userContext": user_context["userContext"]
            }
        })

    tree = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {}
    })

    assert len(tree['contexts']) == 2

    await send_JSON_command(websocket, {
        "method": "browser.removeUserContext",
        "params": user_context
    })

    destroyed = await wait_for_event(websocket,
                                     "browsingContext.contextDestroyed")

    assert destroyed['params']['context'] == browsing_context["context"]

    tree = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {}
    })

    assert len(tree['contexts']) == 1
    assert tree["contexts"][0]["userContext"] == "default"
