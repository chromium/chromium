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
import ssl
from pathlib import Path
from uuid import uuid4

import pytest
import pytest_asyncio
import websockets
from pytest_httpserver import HTTPServer
from test_helpers import (execute_command, get_tree, goto_url,
                          read_JSON_message, wait_for_event, wait_for_events)

from tools.http_proxy_server import HttpProxyServer
from tools.local_http_server import LocalHttpServer


@pytest_asyncio.fixture
def local_server(httpserver) -> LocalHttpServer:
    """ Returns an instance of a LocalHttpServer. It can be used for testing
    HTTP requests locally.
    """
    return LocalHttpServer(httpserver)


@pytest_asyncio.fixture
def http_proxy_server() -> HttpProxyServer:
    """ Returns and starts an instance of a HttpProxyServer.
    """
    server = HttpProxyServer()
    server.start()
    return server


@pytest_asyncio.fixture
async def _websocket_connection():
    """ Return a websocket connection to the browser on localhost without an
    active BiDi session.
    """
    port = os.getenv("PORT", 8080)
    url = f"ws://localhost:{port}"
    async with websockets.connect(url) as connection:
        yield connection


@pytest_asyncio.fixture(params=[{"capabilities": {}}])
async def websocket(request, _websocket_connection):
    """Return a websocket with an active BiDi session."""
    capabilities = request.param['capabilities']
    await execute_command(
        _websocket_connection, {
            "method": "session.new",
            "params": {
                "capabilities": {
                    "alwaysMatch": capabilities
                }
            }
        })
    return _websocket_connection


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
    async def create_context(user_context_id=None):
        result = await execute_command(
            websocket, {
                "method": "browsingContext.create",
                "params": {
                    "type": "tab"
                } | ({
                    "userContext": user_context_id
                } if user_context_id is not None else {})
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
    """Return a URL exhaustively, including same-origin and cross-origin."""
    return request.param


@pytest.fixture
def base_url(local_server: LocalHttpServer):
    """Return a generic example URL with status code 200."""
    return local_server.url_base()


@pytest.fixture
def example_url(local_server: LocalHttpServer):
    """Return a generic example URL with status code 200."""
    return local_server.url_200()


@pytest.fixture
def another_example_url(local_server: LocalHttpServer):
    """Return a generic example URL with status code 200, in a domain other than
    the example_url fixture."""
    return local_server.url_200('127.0.0.1')


@pytest.fixture
def auth_required_url(local_server: LocalHttpServer):
    """Return a URL that requires authentication (status code 401).
    Alternatively, any of the following URLs could also be used:
        - "https://authenticationtest.com/HTTPAuth/"
        - "http://the-internet.herokuapp.com/basic_auth"
        - "http://httpstat.us/401"
    """
    return local_server.url_basic_auth()


@pytest.fixture
def hang_url(local_server: LocalHttpServer):
    """Return a URL that hangs forever."""
    try:
        yield local_server.url_hang_forever()
    finally:
        local_server.hang_forever_stop()


@pytest.fixture(scope="session")
def ssl_context_err_cert_authority_invalid():
    """Return a SSL context with an invalid certificate authority."""
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)

    cert_file_name = Path(__file__).parent / "tools" / "cert.pem"
    key_file_name = Path(__file__).parent / "tools" / "key.pem"

    context.load_cert_chain(cert_file_name, key_file_name)

    return context


@pytest.fixture(scope="session")
def https_server_err_cert_authority_invalid(
        ssl_context_err_cert_authority_invalid):
    """Return a HTTPServer instance with SSL enabled.
    See https://pytest-httpserver.readthedocs.io/en/latest/fixtures.html#make-httpserver"""
    server = HTTPServer(ssl_context=ssl_context_err_cert_authority_invalid)
    server.start()

    yield server

    server.clear()
    if server.is_running():
        server.stop()


@pytest.fixture(scope="session")
def bad_ssl_url(https_server_err_cert_authority_invalid):
    """
    Return a URL with an invalid certificate authority from a SSL certificate.
    In Chromium, this generates the following error:

    > Your connection is not private
    > NET::ERR_CERT_AUTHORITY_INVALID
    """
    return https_server_err_cert_authority_invalid.url_for("/")


@pytest.fixture
def cacheable_url(local_server: LocalHttpServer):
    """Return a generic example URL that can be cached."""
    return local_server.url_cacheable()


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


@pytest_asyncio.fixture
async def user_context_id(websocket):
    """Create a new user context and return its id."""
    result = await execute_command(websocket, {
        "method": "browser.createUserContext",
        "params": {}
    })

    return result['userContext']
