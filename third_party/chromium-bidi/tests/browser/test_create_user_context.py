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
from anys import ANY_STR
from test_helpers import execute_command


@pytest.mark.asyncio
async def test_browser_create_user_context(websocket):
    result = await execute_command(websocket, {
        "method": "browser.getUserContexts",
        "params": {}
    })

    assert result['userContexts'] == [{'userContext': 'default'}]

    result = await execute_command(websocket, {
        "method": "browser.createUserContext",
        "params": {}
    })

    user_context_id = result['userContext']

    assert user_context_id != 'default'
    assert user_context_id == ANY_STR

    result = await execute_command(websocket, {
        "method": "browser.getUserContexts",
        "params": {}
    })

    def user_context(x):
        return x["userContext"]

    assert sorted(result['userContexts'],
                  key=user_context) == sorted([{
                      'userContext': 'default'
                  }, {
                      'userContext': user_context_id
                  }],
                                              key=user_context)


@pytest.mark.asyncio
async def test_browser_create_user_context_proxy_server(
        websocket, http_proxy_server):

    # Localhost URLs are not proxied.
    example_url = "http://example.com"

    user_context = await execute_command(
        websocket, {
            "method": "browser.createUserContext",
            "params": {
                "goog:proxyServer": http_proxy_server.url()
            }
        })

    browsing_context = await execute_command(
        websocket, {
            "method": "browsingContext.create",
            "params": {
                "type": "tab",
                "userContext": user_context["userContext"]
            }
        })

    await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": example_url,
                "wait": "complete",
                "context": browsing_context["context"]
            }
        })

    assert http_proxy_server.stop()[0] == "http://example.com/"
