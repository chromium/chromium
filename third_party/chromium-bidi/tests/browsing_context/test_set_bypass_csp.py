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
def get_csp_url(html):
    def _get_csp_url(same_origin=True):
        return html(
            "<script>window.cspBypassed = true;</script>",
            same_origin=same_origin,
            headers={"Content-Security-Policy": "default-src 'none'"},
        )

    return _get_csp_url


async def is_csp_bypassed(websocket, context_id):
    resp = await execute_command(
        websocket,
        {
            "method": "script.evaluate",
            "params": {
                "expression": "Boolean(window.cspBypassed)",
                "target": {"context": context_id},
                "awaitPromise": True,
            },
        },
    )
    return resp["result"]["value"]


@pytest.mark.asyncio
async def test_bypass_csp_per_browsing_context(
    websocket, context_id, another_context_id, get_csp_url
):
    csp_url = get_csp_url()
    # Verify initial state has CSP enforced.
    await goto_url(websocket, context_id, csp_url)
    assert not await is_csp_bypassed(websocket, context_id)

    # 1. Enable CSP bypass for `context_id`.
    await execute_command(
        websocket,
        {
            "method": "browsingContext.setBypassCSP",
            "params": {"contexts": [context_id], "bypass": True},
        },
    )
    await goto_url(websocket, context_id, csp_url)
    assert await is_csp_bypassed(websocket, context_id)

    # Verify another top-level context still enforces CSP.
    await goto_url(websocket, another_context_id, csp_url)
    assert not await is_csp_bypassed(websocket, another_context_id)

    # 2. Reset override (None).
    await execute_command(
        websocket,
        {
            "method": "browsingContext.setBypassCSP",
            "params": {"contexts": [context_id], "bypass": None},
        },
    )
    await goto_url(websocket, context_id, csp_url)
    assert not await is_csp_bypassed(websocket, context_id)


@pytest.mark.asyncio
async def test_bypass_csp_per_user_context(
    websocket, user_context_id, create_context, get_csp_url
):
    csp_url = get_csp_url()
    # Enable CSP bypass for the user context.
    await execute_command(
        websocket,
        {
            "method": "browsingContext.setBypassCSP",
            "params": {
                "userContexts": [user_context_id],
                "bypass": True,
            },
        },
    )

    context_id = await create_context(user_context_id)
    await goto_url(websocket, context_id, csp_url)
    assert await is_csp_bypassed(websocket, context_id)

    default_context_id = await create_context()
    await goto_url(websocket, default_context_id, csp_url)
    assert not await is_csp_bypassed(websocket, default_context_id)

    # Reset override for user context.
    await execute_command(
        websocket,
        {
            "method": "browsingContext.setBypassCSP",
            "params": {
                "userContexts": [user_context_id],
                "bypass": None,
            },
        },
    )

    await goto_url(websocket, context_id, csp_url)
    assert not await is_csp_bypassed(websocket, context_id)


@pytest.mark.asyncio
async def test_bypass_csp_globally(websocket, context_id, create_context, get_csp_url):
    csp_url = get_csp_url()
    # Enable CSP bypass globally.
    await execute_command(
        websocket,
        {
            "method": "browsingContext.setBypassCSP",
            "params": {"bypass": True},
        },
    )

    # Verify existing context has CSP bypassed.
    await goto_url(websocket, context_id, csp_url)
    assert await is_csp_bypassed(websocket, context_id)

    # Verify newly created context has CSP bypassed.
    new_context_id = await create_context()
    await goto_url(websocket, new_context_id, csp_url)
    assert await is_csp_bypassed(websocket, new_context_id)

    # Reset global override.
    await execute_command(
        websocket,
        {
            "method": "browsingContext.setBypassCSP",
            "params": {"bypass": None},
        },
    )

    await goto_url(websocket, context_id, csp_url)
    assert not await is_csp_bypassed(websocket, context_id)


@pytest.mark.asyncio
@pytest.mark.parametrize("same_origin", [True, False])
async def test_bypass_csp_iframe(
    websocket, context_id, iframe_id, same_origin, get_csp_url
):
    frame_csp_url = get_csp_url(same_origin=same_origin)

    # 1. Enable CSP bypass on the top-level context before navigating the
    # iframe (for cross-origin, the newly attached OOPIF target gets the
    # bypass via `CdpTarget.#unblock()`).
    await execute_command(
        websocket,
        {
            "method": "browsingContext.setBypassCSP",
            "params": {"contexts": [context_id], "bypass": True},
        },
    )
    await goto_url(websocket, iframe_id, frame_csp_url)
    assert await is_csp_bypassed(websocket, iframe_id)

    # 2. Reset CSP bypass while the iframe target is already attached
    # (applies via `#getAllRelatedCdpTargets()`).
    await execute_command(
        websocket,
        {
            "method": "browsingContext.setBypassCSP",
            "params": {"contexts": [context_id], "bypass": None},
        },
    )
    await goto_url(websocket, iframe_id, frame_csp_url)
    assert not await is_csp_bypassed(websocket, iframe_id)

    # 3. Re-enable CSP bypass while the iframe target is already attached.
    await execute_command(
        websocket,
        {
            "method": "browsingContext.setBypassCSP",
            "params": {"contexts": [context_id], "bypass": True},
        },
    )
    await goto_url(websocket, iframe_id, frame_csp_url)
    assert await is_csp_bypassed(websocket, iframe_id)
