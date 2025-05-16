#  Copyright 2023 Google LLC.
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
from test_helpers import execute_command


async def get_content(websocket, context_id, url):
    """Get the body innerText content from the page with the given url."""
    await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url,
                "wait": "complete",
                "context": context_id
            }
        })

    resp = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "document.body.innerText",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True
            }
        })

    return resp["result"]["value"]


@pytest.mark.asyncio
async def test_local_server_200(websocket, context_id, local_server_http):
    assert await get_content(websocket, context_id, local_server_http.url_200()) \
           == local_server_http.content_200


@pytest.mark.asyncio
async def test_local_server_http_another_host_200(
        websocket, context_id, local_server_http_another_host):
    assert await get_content(websocket, context_id, local_server_http_another_host.url_200()) \
           == local_server_http_another_host.content_200


@pytest.mark.asyncio
async def test_local_server_custom_content(websocket, context_id,
                                           local_server_http):
    some_custom_content = 'some custom content'
    assert await get_content(websocket, context_id, local_server_http.url_200(content=some_custom_content)) \
           == some_custom_content


@pytest.mark.asyncio
async def test_local_server_redirect(websocket, context_id, local_server_http):
    assert await get_content(websocket, context_id,
                             local_server_http.url_permanent_redirect()) \
           == local_server_http.content_200


@pytest.mark.asyncio
async def test_local_server_html(websocket, context_id, html):
    content = "SOME CONTENT"
    assert await get_content(websocket, context_id, html(content)) == content
