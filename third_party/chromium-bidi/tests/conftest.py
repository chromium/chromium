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
import logging
import os
from asyncio import Timeout
from collections.abc import Callable, Generator
from uuid import uuid4

import pytest
import pytest_asyncio
import websockets
from test_helpers import (AnyExtending, execute_command, get_tree, goto_url,
                          merge_dicts_recursively, read_JSON_message,
                          send_JSON_command, stabilize_key_values,
                          wait_for_event, wait_for_events)

from tools.http_proxy_server import HttpProxyServer
from tools.local_http_server import LocalHttpServer

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

GOOD_SSL_CERT_SPKI = "QQDsUATYj6FX2oHvQ5/cyDW9CutD2sp9z+qeLfNGHHw="


@pytest_asyncio.fixture(scope='session')
def local_server_http() -> Generator[LocalHttpServer, None, None]:
    """
    Returns an instance of a LocalHttpServer without SSL pointing to localhost.
    """
    server = LocalHttpServer()
    yield server

    server.clear()
    if server.is_running():
        server.stop()
        return


@pytest_asyncio.fixture(scope='session')
def local_server_http_another_host() -> Generator[LocalHttpServer, None, None]:
    """
    Returns an instance of a LocalHttpServer without SSL pointing to `127.0.0.1`
    """
    server = LocalHttpServer('127.0.0.1')
    yield server

    server.clear()
    if server.is_running():
        server.stop()
        return


@pytest_asyncio.fixture(scope='session')
def local_server_bad_ssl() -> Generator[LocalHttpServer, None, None]:
    """ Returns an instance of a LocalHttpServer with bad SSL certificate. """
    server = LocalHttpServer(ssl_cert_prefix="ssl_bad")
    yield server

    server.clear()
    if server.is_running():
        server.stop()
        return


@pytest_asyncio.fixture(scope='session')
def local_server_good_ssl() -> Generator[LocalHttpServer, None, None]:
    """ Returns an instance of a LocalHttpServer with a valid SSL certificate. """
    server = LocalHttpServer(ssl_cert_prefix="ssl_good")
    yield server

    server.clear()
    if server.is_running():
        server.stop()
        return


@pytest_asyncio.fixture
def http_proxy_server() -> HttpProxyServer:
    """ Returns and starts an instance of a HttpProxyServer.
    """
    server = HttpProxyServer()
    server.start()
    return server


@pytest_asyncio.fixture
async def test_headless_mode():
    """Return the headless mode to use for the test. The default is "new" mode."""
    maybe_headless = os.getenv("HEADLESS")
    return maybe_headless if maybe_headless in ["old", "new", "false"
                                                ] else "new"


@pytest_asyncio.fixture
async def test_chromedriver_mode():
    """Return the headless mode to use for the test. The default is "new" mode."""
    maybe_chromedriver = os.getenv("CHROMEDRIVER")
    return maybe_chromedriver == "true"


@pytest_asyncio.fixture
async def capabilities(request):
    if not hasattr(request, 'param') or request.param is None:
        return {}
    return request.param


@pytest_asyncio.fixture
async def current_test_name(request):
    if request and request.node and request.node.name:
        # Add test name to ease log analyse.
        return request.node.name
    return "::NO_TEST"


@pytest_asyncio.fixture
async def websocket(test_headless_mode, capabilities, current_test_name):
    """Connects to endpoint, creates a session and returns a websocket connection."""
    async def create_session(connection):
        """
        Creates a new session on the given connection. It can time out due to
        GitHub infra issues.
        """
        default_capabilities = {
            "webSocketUrl": True,
            "goog:chromeOptions": {
                "args": [
                    # Required for navigating to `local_server_good_ssl`.
                    f"--ignore-certificate-errors-spki-list={GOOD_SSL_CERT_SPKI}",
                    "--disable-infobars",
                    # Required to prevent automatic switch to https.
                    "--disable-features=HttpsFirstBalancedModeAutoEnable,HttpsUpgrades,LocalNetworkAccessChecks",
                    # Required for bluetooth testing.
                    "--enable-features=WebBluetooth",
                    # Prevent throttling.
                    "--disable-background-networking",
                    "--disable-background-timer-throttling",
                    "--disable-backgrounding-occluded-windows",
                ]
            },
            # Add test name to ease log analyse.
            "goog:pytest_name": current_test_name
        }

        maybe_browser_bin = os.getenv("BROWSER_BIN")
        if maybe_browser_bin:
            default_capabilities["goog:chromeOptions"][
                "binary"] = maybe_browser_bin

        if test_headless_mode != "false":
            if test_headless_mode == "old":
                default_capabilities["goog:chromeOptions"]["args"].extend([
                    # No need in `--headless=old` flag, as it will be handled by
                    # `BROWSER_BIN` environment variable.
                    "--hide-scrollbars",
                    "--mute-audio"
                ])
            else:
                # Default to new headless mode.
                default_capabilities["goog:chromeOptions"]["args"].append(
                    "--headless=new")

        session_capabilities = merge_dicts_recursively(default_capabilities,
                                                       capabilities)

        await execute_command(connection, {
            "method": "session.new",
            "params": {
                "capabilities": {
                    "alwaysMatch": session_capabilities
                }
            }
        },
                              timeout=40)

    async def connect_and_create_new_session():
        """
        Tries to connect to websocket and to create a new session. If session
        creation timed out, retries several time to avoid infra CI issues.
        """
        current_attempt = 0
        max_attempt = 5
        while True:
            connection = await connect_websocket()
            try:
                await create_session(connection)
                return connection
            except (asyncio.exceptions.CancelledError,
                    asyncio.TimeoutError) as e:
                # Timeout during creating session is expected due to infra issues.
                current_attempt = current_attempt + 1
                await connection.close()
                if current_attempt >= max_attempt:
                    raise
                else:
                    logger.info(
                        f"Error during creating session. Attempts: {current_attempt}/{max_attempt}. {e}"
                    )
            except Exception:
                await connection.close()
                raise

    async def connect_websocket():
        """ Return a websocket connection to the browser on localhost without an
        active BiDi session.
        """
        port = os.getenv("PORT", 8080)
        url = f"ws://localhost:{port}/session"
        return await websockets.connect(url)

    _websocket_connection = await connect_and_create_new_session()

    yield _websocket_connection

    try:
        # End session after each test.
        await execute_command(_websocket_connection, {
            "method": "session.end",
            "params": {}
        })
    except websockets.exceptions.ConnectionClosedError:
        # The session can be already closed if the test did it,
        # or if the last tab was closed. Details:
        # https://w3c.github.io/webdriver/#dfn-close-window
        # TODO: revisit after BiDi specification is clarified:
        #  https://github.com/w3c/webdriver-bidi/issues/187
        pass
    except RuntimeError:
        # There can be another coroutine waiting for websocket messages, which
        # is fine.
        pass
    except Timeout:
        # Node runner can fail sending session.end response. Ignore the error.
        pass
    except Exception as e:
        logger.info(f"Error during closing session. {e}")
        raise

    try:
        # Close websocket connection.
        await _websocket_connection.close()
    except websockets.exceptions.ConnectionClosedError:
        # The connection can be already closed if the test did it,
        # or if the last tab was closed. Details:
        # https://w3c.github.io/webdriver/#dfn-close-window
        # TODO: revisit after BiDi specification is clarified:
        #  https://github.com/w3c/webdriver-bidi/issues/187
        pass
    except Exception as e:
        logger.info(
            f"Error during closing websocket connection. {type(e)} {e}")
        pass


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
    async def create_context(user_context_id=None, context_type='tab'):
        result = await execute_command(
            websocket, {
                "method": "browsingContext.create",
                "params": {
                    "type": context_type
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


@pytest.fixture(params=[
    'url_example', 'url_example_another_origin', 'html', 'about:blank'
])
def url_all_origins(request, url_example, url_example_another_origin, html):
    if request.param == 'url_example':
        return url_example
    if request.param == 'url_example_another_origin':
        return url_example_another_origin
    if request.param == 'html':
        return html('data:text/html,<h2>some page</h2>')
    if request.param == 'about:blank':
        return 'about:blank'
    raise ValueError(f"Unknown parameter: {request.param}")


@pytest.fixture
def url_base(local_server_http):
    """Return a generic example URL with status code 200."""
    return local_server_http.url_base()


@pytest.fixture
def url_example(local_server_http):
    """Return a generic example URL with status code 200."""
    return local_server_http.url_200()


@pytest.fixture
def url_example_another_origin(local_server_http_another_host):
    """Return a generic example URL with status code 200, in a domain other than
    the example_url fixture."""
    return local_server_http_another_host.url_200()


@pytest.fixture
def url_auth_required(local_server_http):
    """Return a URL that requires authentication (status code 401).
    Alternatively, any of the following URLs could also be used:
        - "https://authenticationtest.com/HTTPAuth/"
        - "http://the-internet.herokuapp.com/basic_auth"
        - "http://httpstat.us/401"
    """
    return local_server_http.url_basic_auth()


@pytest.fixture
def url_hang_forever(local_server_http):
    """Return a URL that hangs forever."""
    try:
        yield local_server_http.url_hang_forever()
    finally:
        local_server_http.hang_forever_stop()


@pytest.fixture(scope="session")
def url_bad_ssl(local_server_bad_ssl):
    """
    Return a URL with an invalid certificate authority from a SSL certificate.
    In Chromium, this generates the following error:

    > Your connection is not private
    > NET::ERR_CERT_AUTHORITY_INVALID
    """
    return local_server_bad_ssl.url_200()


@pytest.fixture(scope="session")
def url_secure_context(local_server_good_ssl):
    return local_server_good_ssl.url_200()


@pytest.fixture
def url_cacheable(local_server_http):
    """Return a generic example URL that can be cached."""
    return local_server_http.url_cacheable()


@pytest.fixture
def get_url_echo(local_server_http, local_server_http_another_host):
    def get_url_echo(same_origin=True):
        if same_origin:
            return local_server_http.url_echo()
        return local_server_http_another_host.url_echo()

    return get_url_echo


@pytest.fixture
def url_echo(get_url_echo):
    """Returns a URL that, when fetched, echoes back the details of the request."""
    return get_url_echo()


@pytest.fixture
def url_permanent_redirect(local_server_http):
    return local_server_http.url_permanent_redirect()


@pytest.fixture
def read_messages(websocket, read_all_messages):
    """
    Reads the specified number of messages from the WebSocket, returning them in
    a consistent order.

    Messages not matching the provided filter are ignored.

    Specified keys within the messages can be stabilized. Stabilization replaces
    certain values with predictable placeholders, ensuring consistent
    comparisons across test runs even if the original values change. The same
    original value is always replaced with the same placeholder. The placeholder
    is determined by the key and the order in which unique values for that key
    are encountered.
    """
    async def read_messages(message_count,
                            filter_lambda: Callable[[dict],
                                                    bool] = lambda _: True,
                            keys_to_stabilize: list[str] = [],
                            check_no_other_messages: bool = False,
                            known_values: dict[str, str] | None = None,
                            sort=True):
        messages = []
        for _ in range(message_count):
            # Get the next message matching the filter.
            while True:
                message = await read_JSON_message(websocket)
                if filter_lambda(message):
                    break
            messages.append(message)

        if check_no_other_messages:
            messages = messages + await read_all_messages(
                filter_lambda=filter_lambda)

        if sort:
            messages.sort(key=lambda x: (x["method"], 0) if "method" in x else
                          ("", x["id"]) if "id" in x else ("", 0))
        # Stabilize some values through the messages.
        stabilize_key_values(messages, keys_to_stabilize, known_values)

        if len(messages) > message_count:
            # "Assert equals" to produce a readable overview of all received
            # messages.
            assert messages == [
                {} for _ in range(message_count)
            ], f" Expected {message_count}, but received {len(messages)} messages."

        return messages

    return read_messages


@pytest.fixture
def read_all_messages(websocket):
    async def read_all_messages(
            filter_lambda: Callable[[dict], bool] = lambda _: True):
        messages = []
        """ Walk through all the browsing contexts and evaluate an async script"""
        command_id = await send_JSON_command(websocket, {
            'method': 'browsingContext.getTree',
            'params': {}
        })
        message = await read_JSON_message(websocket)
        while message != AnyExtending({'id': command_id}):
            if filter_lambda(message):
                # Unexpected message. Add to the result list.
                messages.append(message)
            message = await read_JSON_message(websocket)
        else:
            assert message == AnyExtending({
                'type': 'success',
                'id': command_id
            }), "Unexpected command failure"
            for context in message['result']['contexts']:
                command_id = await send_JSON_command(
                    websocket, {
                        "method": "script.evaluate",
                        "params": {
                            "expression": "Promise.resolve()",
                            "target": {
                                "context": context['context']
                            },
                            "awaitPromise": True,
                        }
                    })
                message = await read_JSON_message(websocket)
                while message != AnyExtending({'id': command_id}):
                    # Ignore both success and failure command result.
                    if filter_lambda(message):
                        # Unexpected message. Add to the result list.
                        messages.append(message)
                    message = await read_JSON_message(websocket)
        return messages

    return read_all_messages


@pytest.fixture
def assert_no_more_messages(read_all_messages):
    """Assert that there are no more messages on the websocket."""
    async def assert_no_more_messages():
        assert await read_all_messages() == [], "No more messages are expected"

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
            "method": "goog:cdp.getSession",
            "params": {
                "context": context_id
            }
        })
        return result["session"]

    return get_cdp_session_id


@pytest.fixture
def query_selector(websocket, context_id):
    """ Return an element matching the given selector in the default or provided
    browsing context. """
    async def query_selector(selector: str, context_id=context_id) -> str:
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
    """
    Activates the tab for the default or given browsing context.
    """
    async def get_top_level_context_id(context_id_):
        """ Returns the top-level context id for the given context id."""

        context_to_top_level_map = {}

        # Build a map from each context_id to its top-level context_id.
        # This handles nested contexts (e.g., iframes within iframes).
        def populate_map(contexts, top_level_id):
            for context_ in contexts:
                context_to_top_level_map[context_["context"]] = top_level_id
                if "children" in context_:
                    populate_map(context_["children"], top_level_id)

        resp = await get_tree(websocket)
        for context in resp["contexts"]:
            # Iterate through top-level contexts.
            populate_map([context], context["context"])

        if context_id_ not in context_to_top_level_map:
            raise Exception(f"Unknown context_id: {context_id_}")

        # If the initial_context_id is found in the map, return its
        # corresponding top-level context_id. Otherwise, return the original.
        return context_to_top_level_map.get(context_id_, context_id_)

    async def activate_main_tab(context_id_=context_id):
        top_level_context_id = await get_top_level_context_id(context_id_)
        await execute_command(
            websocket, {
                "method": "browsingContext.activate",
                "params": {
                    "context": top_level_context_id
                }
            })

    return activate_main_tab


@pytest.fixture
def url_download(local_server_http):
    """Return a URL that triggers a download."""
    def url_download(file_name="file-name.txt", content="download content"):
        return local_server_http.url_200(
            content,
            content_type="application/octet-stream",
            headers={
                "Content-Disposition": f"attachment;  filename=\"{file_name}\""
            })

    return url_download


@pytest.fixture
def url_hang_forever_download(local_server_http):
    """Return a URL that triggers a download which hangs forever."""
    try:
        yield local_server_http.url_hang_forever_download
    finally:
        local_server_http.hang_forever_stop()


@pytest.fixture
def html(local_server_http, local_server_http_another_host):
    """Return a factory for URL with the given content."""
    def html(content="",
             same_origin=True,
             headers: dict[str, str] | None = None):
        if same_origin:
            return local_server_http.url_200(content=content, headers=headers)
        else:
            return local_server_http_another_host.url_200(content=content,
                                                          headers=headers)

    return html


@pytest.fixture
def iframe():
    """Return a factory for <iframe> with the given src."""
    def iframe(src=""):
        # Geolocation is required for Geolocation testing in iframes.
        return f'<iframe allow="geolocation *" src="{src}" />'

    return iframe


@pytest_asyncio.fixture
async def iframe_id(websocket, context_id, html, iframe):
    """Navigate to a page with an iframe of the same origin, and return the
    iframe browser context id."""
    await goto_url(websocket, context_id, html(iframe(html("<h1>FRAME</h1>"))))
    result = await get_tree(websocket, context_id)

    iframe_id = result["contexts"][0]["children"][0]["context"]

    # To avoid issue with the events order in headful mode, navigate to some
    # page: https://crbug.com/1353719
    await goto_url(websocket, iframe_id, html("<h1>FRAME</h1>"))

    return iframe_id


@pytest_asyncio.fixture
async def create_user_context(websocket):
    async def create_user_context(params={}):
        """Create a new user context and return its id."""
        result = await execute_command(websocket, {
            "method": "browser.createUserContext",
            "params": params
        })
        return result['userContext']

    return create_user_context


@pytest_asyncio.fixture
async def user_context_id(create_user_context):
    """Create a new user context and return its id."""
    return await create_user_context()


@pytest.fixture
def unpacked_extension_location(tmp_path):
    """
    Return a factory for path to the unpacked extension with the given files.
    Args:
        files_dict: a dictionary with file name keys and file contents values.
    Returns:
        The absolute path (as string) to the folder containing the extension
        files.
    """
    def extension(files_dict):
        for name, content in files_dict.items():
            file = tmp_path / name
            file.write_text(content)
        return str(tmp_path)

    return extension
