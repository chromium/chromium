#  Copyright 2026 Google LLC.
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
import pytest_asyncio
from test_helpers import execute_command, goto_url, subscribe


@pytest_asyncio.fixture
async def is_cache_enabled(
    websocket, local_server_http, local_server_http_another_host
):
    """Returns a helper that checks whether HTTP caching is enabled in a context.

    Instead of subscribing to `network.responseCompleted` (or `goog:cdp.*`
    events) and checking `fromCache`, this helper probes the browser cache
    directly via `fetch()` and `script.callFunction`. Any `session.subscribe`
    call triggers `EventManager.toggleModulesIfNeeded()` ->
    `CdpTarget.toggleNetworkIfNeeded()` (which updates CDP's
    `Network.setCacheDisabled` state), so avoiding subscriptions here allows
    testing `network.setCacheBehavior` both without any active subscriptions
    and across subscribe/unsubscribe transitions.
    """

    async def _fetch_text(context_id, url):
        response = await execute_command(
            websocket,
            {
                "method": "script.callFunction",
                "params": {
                    "functionDeclaration": """
                        async (url) => {
                            const response = await fetch(url);
                            return await response.text();
                        }
                    """,
                    "arguments": [{"type": "string", "value": url}],
                    "awaitPromise": True,
                    "target": {"context": context_id},
                },
            },
        )
        assert response["type"] == "success", f"Unexpected script response: {response}"
        return response["result"]["value"]

    async def _is_cache_enabled(context_id, same_origin=True):
        server = local_server_http if same_origin else local_server_http_another_host
        # Return `responses[0]` on the first HTTP request and `responses[1]` on
        # the second HTTP request that reaches the server. Long-lived
        # `Cache-Control` and `Expires` headers ensure the browser caches the
        # first response by default, and `Access-Control-Allow-Origin: *` allows
        # probing from cross-origin / OOPIF contexts.
        responses = ["first", "second"]
        responses_iter = iter(responses)
        cacheable_url = server.url_200(
            content=lambda: next(responses_iter),
            content_type="text/plain",
            headers={
                "Cache-Control": "public, max-age=31536000",
                "Expires": "Thu, 01 Dec 2100 20:00:00 GMT",
                "Access-Control-Allow-Origin": "*",
            },
        )

        # First fetch warms the browser's HTTP cache with `responses[0]`.
        first_response = await _fetch_text(context_id, cacheable_url)
        assert first_response == responses[0]

        # Fetch the same URL again:
        # - If cache is enabled ("default"), the browser serves `responses[0]`
        #   from its cache without hitting the server.
        # - If cache is bypassed ("bypass" via CDP `Network.setCacheDisabled`),
        #   the browser requests the URL from the server and gets `responses[1]`.
        second_response = await _fetch_text(context_id, cacheable_url)
        return second_response == responses[0]

    return _is_cache_enabled


@pytest.mark.asyncio
async def test_set_cache_behavior_without_network_subscription(
    websocket, context_id, url_base, is_cache_enabled
):
    await goto_url(websocket, context_id, url_base)

    assert await is_cache_enabled(context_id) is True

    await execute_command(
        websocket,
        {
            "method": "network.setCacheBehavior",
            "params": {"cacheBehavior": "bypass"},
        },
    )
    assert await is_cache_enabled(context_id) is False

    await execute_command(
        websocket,
        {
            "method": "network.setCacheBehavior",
            "params": {"cacheBehavior": "default"},
        },
    )
    assert await is_cache_enabled(context_id) is True


@pytest.mark.asyncio
async def test_set_cache_behavior_per_context_persists_after_subscribe(
    websocket, context_id, another_context_id, url_base, is_cache_enabled
):
    await goto_url(websocket, context_id, url_base)
    await goto_url(websocket, another_context_id, url_base)

    await execute_command(
        websocket,
        {
            "method": "network.setCacheBehavior",
            "params": {
                "cacheBehavior": "bypass",
                "contexts": [context_id],
            },
        },
    )

    assert await is_cache_enabled(context_id) is False
    assert await is_cache_enabled(another_context_id) is True

    # Subscribing and unsubscribing triggers `toggleNetworkIfNeeded` and should
    # not reset the per-context cache behavior.
    subscription = await subscribe(websocket, ["network.responseCompleted"])
    assert await is_cache_enabled(context_id) is False
    assert await is_cache_enabled(another_context_id) is True

    await execute_command(
        websocket,
        {
            "method": "session.unsubscribe",
            "params": {"subscriptions": [subscription["subscription"]]},
        },
    )
    assert await is_cache_enabled(context_id) is False
    assert await is_cache_enabled(another_context_id) is True


@pytest.mark.asyncio
async def test_set_cache_behavior_iframe(
    websocket, context_id, iframe_id, html, is_cache_enabled
):
    await execute_command(
        websocket,
        {
            "method": "network.setCacheBehavior",
            "params": {
                "cacheBehavior": "bypass",
                "contexts": [context_id],
            },
        },
    )

    assert await is_cache_enabled(iframe_id) is False

    # Move iframe out of process (OOPIF) and verify cache behavior persists.
    await goto_url(websocket, iframe_id, html(same_origin=False))
    assert await is_cache_enabled(iframe_id, same_origin=False) is False

    # Updating top-level context cache behavior should also update the OOPIF target.
    await execute_command(
        websocket,
        {
            "method": "network.setCacheBehavior",
            "params": {
                "cacheBehavior": "default",
                "contexts": [context_id],
            },
        },
    )
    assert await is_cache_enabled(iframe_id, same_origin=False) is True


@pytest.mark.asyncio
async def test_set_cache_behavior_per_context_before_global(
    websocket, context_id, another_context_id, url_base, is_cache_enabled
):
    await goto_url(websocket, context_id, url_base)
    await goto_url(websocket, another_context_id, url_base)

    await execute_command(
        websocket,
        {
            "method": "network.setCacheBehavior",
            "params": {
                "cacheBehavior": "bypass",
                "contexts": [context_id],
            },
        },
    )
    assert await is_cache_enabled(context_id) is False
    assert await is_cache_enabled(another_context_id) is True

    # This is the currently specified behavior: setting global cache behavior
    # clears per-context cache behavior overrides.
    # See https://github.com/w3c/webdriver-bidi/issues/1170.
    await execute_command(
        websocket,
        {
            "method": "network.setCacheBehavior",
            "params": {"cacheBehavior": "default"},
        },
    )
    assert await is_cache_enabled(context_id) is True
    assert await is_cache_enabled(another_context_id) is True

    await execute_command(
        websocket,
        {
            "method": "network.setCacheBehavior",
            "params": {
                "cacheBehavior": "default",
                "contexts": [context_id],
            },
        },
    )
    await execute_command(
        websocket,
        {
            "method": "network.setCacheBehavior",
            "params": {"cacheBehavior": "bypass"},
        },
    )
    assert await is_cache_enabled(context_id) is False
    assert await is_cache_enabled(another_context_id) is False


@pytest.mark.asyncio
async def test_set_cache_behavior_per_context_after_global(
    websocket, context_id, another_context_id, url_base, is_cache_enabled
):
    await goto_url(websocket, context_id, url_base)
    await goto_url(websocket, another_context_id, url_base)

    # This is the currently specified behavior: per-context cache behavior
    # overrides the global cache behavior when set after the global one.
    # See https://github.com/w3c/webdriver-bidi/issues/1170.
    await execute_command(
        websocket,
        {
            "method": "network.setCacheBehavior",
            "params": {"cacheBehavior": "bypass"},
        },
    )
    assert await is_cache_enabled(context_id) is False
    assert await is_cache_enabled(another_context_id) is False

    await execute_command(
        websocket,
        {
            "method": "network.setCacheBehavior",
            "params": {
                "cacheBehavior": "default",
                "contexts": [context_id],
            },
        },
    )
    assert await is_cache_enabled(context_id) is True
    assert await is_cache_enabled(another_context_id) is False
