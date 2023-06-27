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

import os

import pytest
import pytest_asyncio
import websockets
from test_helpers import execute_command, get_tree, goto_url, read_JSON_message


@pytest_asyncio.fixture
async def websocket():
    """Return a websocket connection to the browser on localhost."""
    port = os.getenv("PORT", 8080)
    url = f"ws://localhost:{port}"
    async with websockets.connect(url) as connection:
        yield connection


@pytest_asyncio.fixture
async def context_id(websocket):
    """Return the context id from the first browsing context."""
    result = await get_tree(websocket)
    return result["contexts"][0]["context"]


@pytest_asyncio.fixture
async def another_context_id(create_context):
    """Return an additional browsing context id."""
    return await create_context()


@pytest_asyncio.fixture
def create_context(websocket):
    """Return a browsing context factory."""
    async def create_context():
        result = await execute_command(websocket, {
            "method": "browsingContext.create",
            "params": {
                "type": "tab"
            }
        })
        return result['context']

    return create_context


@pytest_asyncio.fixture
async def default_realm(websocket, context_id: str):
    """Return the default realm for the given browsing context."""
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "globalThis",
                "target": {
                    "context": context_id,
                },
                "awaitPromise": True
            }
        })

    return result["realm"]


@pytest_asyncio.fixture
async def sandbox_realm(websocket, context_id: str):
    """Return a sandbox realm for the given browsing context."""
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "globalThis",
                "target": {
                    "context": context_id,
                    "sandbox": 'some_sandbox'
                },
                "awaitPromise": True
            }
        })

    return result["realm"]


@pytest.fixture
def url_same_origin():
    """Return a same-origin URL."""
    return 'about:blank'


# TODO: make offline.
@pytest.fixture(params=[
    'https://example.com/',  # Another domain: Cross-origin
    'data:text/html,<h2>child page</h2>',  # Data URL: Cross-origin
])
def url_cross_origin(request):
    """Return a cross-origin URL."""
    return request.param


# TODO: make offline.
@pytest.fixture(params=[
    'about:blank',  # Same-origin
    'https://example.com/',  # Another domain: Cross-origin
    'data:text/html,<h2>child page</h2>',  # Data URL: Cross-origin
])
def url_all_origins(request):
    """Return an URL exhaustively, including same-origin and cross-origin."""
    return request.param


@pytest.fixture
def read_sorted_messages(websocket):
    """Read the given number of messages from the websocket, and returns them
    in consistent order."""
    async def read_sorted_messages(message_count):
        messages = []
        for _ in range(message_count):
            messages.append(await read_JSON_message(websocket))
        messages.sort(key=lambda x: x["method"]
                      if "method" in x else str(x["id"]) if "id" in x else "")
        return messages

    return read_sorted_messages


@pytest.fixture
def html():
    """Return a factory for HTML data URL with the given content."""
    def html(content=""):
        return f'data:text/html,{content}'

    return html


@pytest.fixture
def iframe():
    """Return a factory for <iframe> with the given src."""
    def iframe(src=""):
        return f'<iframe src="{src}" />'

    return iframe


@pytest.fixture
def html_iframe_same_origin(html, iframe, url_same_origin):
    """Return a page URL with an iframe of the same origin."""
    return html(iframe(url_same_origin))


@pytest_asyncio.fixture
async def iframe_id(websocket, context_id: str, html_iframe_same_origin, html):
    """Navigate to a page with an iframe of the same origin, and return the
    iframe browser context id."""
    await goto_url(websocket, context_id, html_iframe_same_origin)
    result = await get_tree(websocket, context_id)

    iframe_id = result["contexts"][0]["children"][0]["context"]

    # To avoid issue with the events order in headful mode, navigate to some
    # page: https://crbug.com/1353719
    await goto_url(websocket, iframe_id, "data:text/html,<h1>FRAME</h1>")

    return iframe_id
