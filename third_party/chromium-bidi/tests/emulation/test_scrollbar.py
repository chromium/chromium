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
from test_helpers import execute_command


@pytest_asyncio.fixture
async def get_scrollbar_width(websocket, context_id):

    async def _get_scrollbar_width(context_id=context_id):
        resp = await execute_command(
            websocket,
            {
                "method": "script.evaluate",
                "params": {
                    "expression": """(function() {
                        var outer = document.createElement('div');
                        outer.style.visibility = 'hidden';
                        outer.style.width = '100px';
                        outer.style.height = '100px';
                        outer.style.overflow = 'scroll';
                        document.body.appendChild(outer);
                        var width = outer.offsetWidth - outer.clientWidth;
                        outer.parentNode.removeChild(outer);
                        return width;
                    })()""",
                    "target": {"context": context_id},
                    "awaitPromise": True,
                },
            },
        )
        return resp["result"]["value"]

    return _get_scrollbar_width


@pytest_asyncio.fixture
async def initial_scrollbar_width(context_id, get_scrollbar_width):
    return await get_scrollbar_width(context_id)


@pytest.mark.asyncio
async def test_scrollbar_type_per_browsing_context(
    websocket, context_id, get_scrollbar_width, initial_scrollbar_width
):
    # 1. Set scrollbar type to 'overlay'.
    await execute_command(
        websocket,
        {
            "method": "emulation.setScrollbarTypeOverride",
            "params": {"contexts": [context_id], "scrollbarType": "overlay"},
        },
    )

    # 2. Verify scrollbar width becomes 0.
    assert await get_scrollbar_width(context_id) == 0

    # 3. Set scrollbar type to `classic`.
    await execute_command(
        websocket,
        {
            "method": "emulation.setScrollbarTypeOverride",
            "params": {"contexts": [context_id], "scrollbarType": "classic"},
        },
    )

    # 4. Verify scrollbar width matches initial (if the system default is overlay, then
    # `classic` does not have effect).
    assert await get_scrollbar_width(context_id) == initial_scrollbar_width

    # 5. Reset override (null).
    await execute_command(
        websocket,
        {
            "method": "emulation.setScrollbarTypeOverride",
            "params": {"contexts": [context_id], "scrollbarType": None},
        },
    )

    assert await get_scrollbar_width(context_id) == initial_scrollbar_width


@pytest.mark.asyncio
async def test_scrollbar_type_per_user_context(
    websocket,
    user_context_id,
    create_context,
    get_scrollbar_width,
    initial_scrollbar_width,
):
    # Set scrollbar type to overlay for the user context.
    await execute_command(
        websocket,
        {
            "method": "emulation.setScrollbarTypeOverride",
            "params": {
                "userContexts": [user_context_id],
                "scrollbarType": "overlay",
            },
        },
    )

    context_id = await create_context(user_context_id)
    assert await get_scrollbar_width(context_id) == 0

    default_context_id = await create_context()
    assert await get_scrollbar_width(default_context_id) == initial_scrollbar_width

    # Reset override
    await execute_command(
        websocket,
        {
            "method": "emulation.setScrollbarTypeOverride",
            "params": {
                "userContexts": [user_context_id],
                "scrollbarType": None,
            },
        },
    )

    assert await get_scrollbar_width(context_id) == initial_scrollbar_width


@pytest.mark.asyncio
async def test_scrollbar_type_globally(
    websocket,
    context_id,
    create_context,
    get_scrollbar_width,
    initial_scrollbar_width,
):
    # Set scrollbar type to overlay globally.
    await execute_command(
        websocket,
        {
            "method": "emulation.setScrollbarTypeOverride",
            "params": {"scrollbarType": "overlay"},
        },
    )

    # Verify existing context has overlay scrollbars.
    assert await get_scrollbar_width(context_id) == 0

    # Verify newly created context has overlay scrollbars.
    new_context_id = await create_context()
    assert await get_scrollbar_width(new_context_id) == 0

    # Reset global override.
    await execute_command(
        websocket,
        {
            "method": "emulation.setScrollbarTypeOverride",
            "params": {"scrollbarType": None},
        },
    )

    assert await get_scrollbar_width(context_id) == initial_scrollbar_width
