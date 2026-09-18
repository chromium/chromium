#   Copyright 2026 Google LLC.
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
from test_helpers import execute_command, goto_url

CUSTOM_VIEWPORT_META_WIDTH = 345


@pytest.fixture
def viewport_meta_url(html):
    return html(f'<meta name="viewport" content="width={CUSTOM_VIEWPORT_META_WIDTH}">')


@pytest_asyncio.fixture
async def initial_client_width(websocket, context_id, viewport_meta_url):
    await goto_url(websocket, context_id, viewport_meta_url)
    return await get_client_width(websocket, context_id)


async def get_client_width(websocket, context_id):
    resp = await execute_command(
        websocket,
        {
            "method": "script.evaluate",
            "params": {
                "expression": "document.documentElement.clientWidth",
                "target": {"context": context_id},
                "awaitPromise": True,
            },
        },
    )
    return resp["result"]["value"]


@pytest.mark.asyncio
async def test_viewport_meta_enable_and_clear(
    websocket,
    context_id,
    viewport_meta_url,
    initial_client_width,
):
    # Enable viewport meta tag override
    await execute_command(
        websocket,
        {
            "method": "emulation.setViewportMetaOverride",
            "params": {
                "contexts": [context_id],
                "viewportMeta": True,
            },
        },
    )
    await goto_url(websocket, context_id, viewport_meta_url)
    assert await get_client_width(websocket, context_id) == CUSTOM_VIEWPORT_META_WIDTH

    # Reset override (null)
    await execute_command(
        websocket,
        {
            "method": "emulation.setViewportMetaOverride",
            "params": {
                "contexts": [context_id],
                "viewportMeta": None,
            },
        },
    )
    await goto_url(websocket, context_id, viewport_meta_url)
    assert await get_client_width(websocket, context_id) == initial_client_width
