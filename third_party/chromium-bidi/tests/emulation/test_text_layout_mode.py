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
from test_helpers import execute_command, goto_url


@pytest.fixture
def text_autosizing_url(html):
    return html("""
        <div id="unadjusted" style="font-size: 16px; text-size-adjust: none;">Text</div>
        <div id="adjusted" style="font-size: 16px; text-size-adjust: 200%;">Text</div>
    """)


async def is_text_autosizing_enabled(websocket, context_id):
    resp = await execute_command(
        websocket,
        {
            "method": "script.evaluate",
            "params": {
                "expression": """(function() {
                    const unadjusted = parseFloat(
                        getComputedStyle(document.getElementById('unadjusted')).fontSize);
                    const adjusted = parseFloat(
                        getComputedStyle(document.getElementById('adjusted')).fontSize);
                    return adjusted === unadjusted * 2;
                })()""",
                "target": {"context": context_id},
                "awaitPromise": True,
            },
        },
    )
    return resp["result"]["value"]


@pytest.mark.asyncio
async def test_text_layout_mode_per_browsing_context(
    websocket, context_id, text_autosizing_url
):
    # Verify initial state has text autosizing disabled on desktop.
    await goto_url(websocket, context_id, text_autosizing_url)
    assert not await is_text_autosizing_enabled(websocket, context_id)

    # 1. Enable text autosizing ('mobile').
    await execute_command(
        websocket,
        {
            "method": "emulation.setTextLayoutModeOverride",
            "params": {"contexts": [context_id], "textLayoutMode": "mobile"},
        },
    )
    await goto_url(websocket, context_id, text_autosizing_url)
    assert await is_text_autosizing_enabled(websocket, context_id)

    # 2. Reset override (None).
    await execute_command(
        websocket,
        {
            "method": "emulation.setTextLayoutModeOverride",
            "params": {"contexts": [context_id], "textLayoutMode": None},
        },
    )
    await goto_url(websocket, context_id, text_autosizing_url)
    assert not await is_text_autosizing_enabled(websocket, context_id)


@pytest.mark.asyncio
async def test_text_layout_mode_per_user_context(
    websocket, user_context_id, create_context, text_autosizing_url
):
    # Set text layout mode to 'mobile' for the user context.
    await execute_command(
        websocket,
        {
            "method": "emulation.setTextLayoutModeOverride",
            "params": {
                "userContexts": [user_context_id],
                "textLayoutMode": "mobile",
            },
        },
    )

    context_id = await create_context(user_context_id)
    await goto_url(websocket, context_id, text_autosizing_url)
    assert await is_text_autosizing_enabled(websocket, context_id)

    default_context_id = await create_context()
    await goto_url(websocket, default_context_id, text_autosizing_url)
    assert not await is_text_autosizing_enabled(websocket, default_context_id)

    # Reset override for user context.
    await execute_command(
        websocket,
        {
            "method": "emulation.setTextLayoutModeOverride",
            "params": {
                "userContexts": [user_context_id],
                "textLayoutMode": None,
            },
        },
    )

    await goto_url(websocket, context_id, text_autosizing_url)
    assert not await is_text_autosizing_enabled(websocket, context_id)


@pytest.mark.asyncio
async def test_text_layout_mode_globally(
    websocket, context_id, create_context, text_autosizing_url
):
    # Set text layout mode to 'mobile' globally.
    await execute_command(
        websocket,
        {
            "method": "emulation.setTextLayoutModeOverride",
            "params": {"textLayoutMode": "mobile"},
        },
    )

    # Verify existing context has text autosizing enabled.
    await goto_url(websocket, context_id, text_autosizing_url)
    assert await is_text_autosizing_enabled(websocket, context_id)

    # Verify newly created context has text autosizing enabled.
    new_context_id = await create_context()
    await goto_url(websocket, new_context_id, text_autosizing_url)
    assert await is_text_autosizing_enabled(websocket, new_context_id)

    # Reset global override.
    await execute_command(
        websocket,
        {
            "method": "emulation.setTextLayoutModeOverride",
            "params": {"textLayoutMode": None},
        },
    )

    await goto_url(websocket, context_id, text_autosizing_url)
    assert not await is_text_autosizing_enabled(websocket, context_id)
