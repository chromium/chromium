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

import asyncio
import os
from uuid import uuid4

import pytest
import pytest_asyncio
import websockets
from test_helpers import (execute_command, get_tree, goto_url,
                          read_JSON_message, wait_for_event, wait_for_events)


@pytest_asyncio.fixture
async def websocket():
    """Return a websocket connection to the browser on localhost."""
    port = os.getenv("PORT", 8080)
    url = f"ws://localhost:{port}"
    async with websockets.connect(url) as connection:
        yield connection
        await execute_command(connection, {
            "method": "browser.close",
            "params": {}
        })


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
                    "sandbox": str(uuid4()),
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
def example_url():
    """Return a generic example URL with status code 200."""
    # TODO: Switch to a local server so that it works off-line.
    # Alternatively: https://www.example.org/
    return "https://www.example.com/"


@pytest.fixture
def another_example_url():
    # TODO: Switch to a local server so that it works off-line.
    """Return a generic example URL with status code 200, in a domain other than the example_url fixture."""
    return "https://www.example.org/"


@pytest.fixture
def auth_required_url():
    """Return a URL that requires authentication (status code 401)."""
    # TODO: Switch to a local server so that it works off-line.
    # All of these URLs work, just pick one.
    # url = "https://authenticationtest.com/HTTPAuth/"
    # url = "http://the-internet.herokuapp.com/basic_auth"
    return "http://httpstat.us/401"


@pytest.fixture
def bad_ssl_url():
    """
    Return a URL with an expired SSL certificate.

    In Chromium, this generates the following error:

    > Your connection is not private
    > NET::ERR_CERT_DATE_INVALID
    """
    # TODO: Switch to a local server so that it works off-line.
    return "https://expired.badssl.com/"


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
def assert_no_more_messages(websocket):
    """Assert that there are no more messages on the websocket."""
    async def assert_no_more_messages(timeout: float | None):
        with pytest.raises(asyncio.TimeoutError):
            await asyncio.wait_for(read_JSON_message(websocket),
                                   timeout=timeout)

    return assert_no_more_messages


# TODO: Rewrite this fixture in terms of assert_no_events_in_queue.
@pytest.fixture
def assert_no_event_in_queue(websocket):
    """Assert that there are no more events of the given type on the websocket within the given timeout."""
    async def assert_no_event_in_queue(event_method: str,
                                       timeout: float | None):
        with pytest.raises(asyncio.TimeoutError):
            await asyncio.wait_for(wait_for_event(websocket, event_method),
                                   timeout=timeout)

    return assert_no_event_in_queue


@pytest.fixture
def assert_no_events_in_queue(websocket):
    """Assert that there are no more events of the given types on the websocket within the given timeout."""
    async def assert_no_events_in_queue(event_methods: list[str],
                                        timeout: float | None):
        with pytest.raises(asyncio.TimeoutError):
            await asyncio.wait_for(wait_for_events(websocket, event_methods),
                                   timeout=timeout)

    return assert_no_events_in_queue


@pytest.fixture
def get_cdp_session_id(websocket):
    """Return the CDP session ID from the given context."""
    async def get_cdp_session_id(context_id: str) -> str:
        result = await execute_command(websocket, {
            "method": "cdp.getSession",
            "params": {
                "context": context_id
            }
        })
        return result["session"]

    return get_cdp_session_id


@pytest.fixture
def query_selector(websocket, context_id):
    """Return an element matching the given selector"""
    async def query_selector(selector: str) -> str:
        result = await execute_command(
            websocket, {
                "method": "script.evaluate",
                "params": {
                    "expression": f"document.querySelector('{selector}')",
                    "target": {
                        "context": context_id
                    },
                    "resultOwnership": "root",
                    "awaitPromise": False,
                }
            })
        return result["result"]

    return query_selector


@pytest.fixture
def activate_main_tab(websocket, context_id, get_cdp_session_id):
    """Actives the main tab"""
    async def activate_main_tab():
        session_id = await get_cdp_session_id(context_id)
        await execute_command(
            websocket, {
                "method": "cdp.sendCommand",
                "params": {
                    "method": "Page.bringToFront",
                    "session": session_id
                }
            })

    return activate_main_tab


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
    await goto_url(websocket, iframe_id, html("<h1>FRAME</h1>"))

    return iframe_id
