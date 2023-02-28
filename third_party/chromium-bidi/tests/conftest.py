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

import pytest_asyncio
import websockets
from test_helpers import *


@pytest_asyncio.fixture
async def websocket():
    port = os.getenv("PORT", 8080)
    url = f"ws://localhost:{port}"
    async with websockets.connect(url) as connection:
        yield connection


@pytest_asyncio.fixture
async def default_realm(context_id, websocket):
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
async def sandbox_realm(context_id, websocket):
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


@pytest_asyncio.fixture
async def context_id(websocket):
    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {}
    })
    return result["contexts"][0]["context"]


@pytest_asyncio.fixture
async def another_context_id(websocket):
    result = await execute_command(websocket, {
        "method": "browsingContext.create",
        "params": {
            "type": "tab"
        }
    })
    return result['context']


@pytest_asyncio.fixture
async def page_with_nested_iframe_url():
    return f'data:text/html,<h1>MAIN_PAGE</h1>' \
           f'<iframe src="about:blank" />'


@pytest_asyncio.fixture
async def iframe_id(context_id, websocket, page_with_nested_iframe_url):
    await goto_url(websocket, context_id, page_with_nested_iframe_url)
    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {
            "root": context_id
        }
    })

    iframe_id = result["contexts"][0]["children"][0]["context"]

    # To avoid issue with the events order in headful mode, navigate to some
    # page: https://crbug.com/1353719
    await goto_url(websocket, iframe_id, "data:text/html,<h1>FRAME</h1>")

    return iframe_id
