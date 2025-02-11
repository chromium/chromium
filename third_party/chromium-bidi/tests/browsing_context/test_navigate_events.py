# Copyright 2024 Google LLC.
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

import pytest
from syrupy.filters import props
from test_helpers import (execute_command, goto_url, send_JSON_command,
                          subscribe)

SNAPSHOT_EXCLUDE = props("timestamp", "timings", "headers", "stacktrace",
                         "response", "initiator", "realm")
KEYS_TO_STABILIZE = [
    'context', 'navigation', 'id', 'url', 'request', 'originalOpener'
]


async def set_beforeunload_handler(websocket, context_id, show_popup=False):
    await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": """
                    (channel, show_popup) => {
                        window.addEventListener('beforeunload', () => {
                            channel("beforeunload");
                            if(show_popup) {
                                event.returnValue = "Are you sure you want to leave?";
                                event.preventDefault();
                            }
                        },false);
                        document.body.click();
                    }
                """,
                "arguments": [{
                    "type": "channel",
                    "value": {
                        "channel": "beforeunload_channel",
                        "ownership": "none",
                    }
                }, {
                    "type": "boolean",
                    "value": show_popup
                }],
                "target": {
                    "context": context_id,
                },
                "awaitPromise": False,
                "userActivation": True
            }
        })


@pytest.mark.asyncio
@pytest.mark.parametrize("wait", ["none", "interactive", "complete"])
async def test_navigate_checkEvents(websocket, context_id, url_base,
                                    url_example, read_messages, snapshot,
                                    wait):
    await goto_url(websocket, context_id, url_base)
    await set_beforeunload_handler(websocket, context_id)
    await subscribe(
        websocket,
        ["browsingContext", "script.message", "network.beforeRequestSent"])

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_example,
                "wait": wait,
                "context": context_id
            }
        })

    messages = await read_messages(7,
                                   keys_to_stabilize=KEYS_TO_STABILIZE,
                                   check_no_other_messages=True,
                                   sort=False)
    assert messages == snapshot(exclude=SNAPSHOT_EXCLUDE)


@pytest.mark.asyncio
@pytest.mark.parametrize("wait", ["none", "interactive", "complete"])
async def test_navigate_fragment_checkEvents(websocket, context_id,
                                             url_example, read_messages,
                                             snapshot, wait):
    await goto_url(websocket, context_id, url_example)
    await set_beforeunload_handler(websocket, context_id)
    await subscribe(
        websocket,
        ["browsingContext", "script.message", "network.beforeRequestSent"])

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_example + "#test",
                "wait": wait,
                "context": context_id
            }
        })

    messages = await read_messages(2,
                                   keys_to_stabilize=KEYS_TO_STABILIZE,
                                   check_no_other_messages=True,
                                   sort=False)
    assert messages == snapshot(exclude=SNAPSHOT_EXCLUDE)


@pytest.mark.asyncio
async def test_window_open_url_checkEvents(websocket, context_id, url_example,
                                           read_messages, snapshot):
    await subscribe(websocket, ["browsingContext"])

    await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": f"window.open('{url_example}');",
                "target": {
                    "context": context_id,
                },
                "awaitPromise": False
            }
        })

    messages = await read_messages(5,
                                   filter_lambda=lambda x: 'id' not in x,
                                   keys_to_stabilize=KEYS_TO_STABILIZE,
                                   check_no_other_messages=True,
                                   sort=False)
    assert messages == snapshot(exclude=SNAPSHOT_EXCLUDE)


@pytest.mark.asyncio
@pytest.mark.parametrize("url", ["", "about:blank", "about:blank?test"])
async def test_window_open_aboutBlank_checkEvents(websocket, context_id, url,
                                                  read_messages, snapshot):
    await subscribe(websocket, ["browsingContext"])

    await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": f"window.open('{url}');",
                "target": {
                    "context": context_id,
                },
                "awaitPromise": False
            }
        })

    messages = await read_messages(1,
                                   filter_lambda=lambda x: 'id' not in x,
                                   keys_to_stabilize=KEYS_TO_STABILIZE,
                                   check_no_other_messages=True,
                                   sort=False)
    assert messages == snapshot(exclude=SNAPSHOT_EXCLUDE)


@pytest.mark.asyncio
@pytest.mark.parametrize("wait", ["none", "interactive", "complete"])
async def test_navigate_aboutBlank_checkEvents(websocket, context_id, url_base,
                                               read_messages, snapshot, wait):
    await goto_url(websocket, context_id, url_base)
    await set_beforeunload_handler(websocket, context_id)
    await subscribe(
        websocket,
        ["browsingContext", "script.message", "network.beforeRequestSent"])

    about_blank_url = 'about:blank'

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": about_blank_url,
                "wait": wait,
                "context": context_id
            }
        })

    messages = await read_messages(6,
                                   keys_to_stabilize=KEYS_TO_STABILIZE,
                                   check_no_other_messages=True,
                                   sort=False)
    assert messages == snapshot(exclude=SNAPSHOT_EXCLUDE)


@pytest.mark.asyncio
@pytest.mark.parametrize("wait", ["none", "interactive", "complete"])
async def test_navigate_dataUrl_checkEvents(websocket, context_id, url_base,
                                            read_messages, snapshot, wait):
    await goto_url(websocket, context_id, url_base)
    await set_beforeunload_handler(websocket, context_id)
    await subscribe(
        websocket,
        ["browsingContext", "script.message", "network.beforeRequestSent"])

    data_url = "data:text/html;,<h2>header</h2>"

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": data_url,
                "wait": wait,
                "context": context_id
            }
        })

    messages = await read_messages(7,
                                   keys_to_stabilize=KEYS_TO_STABILIZE,
                                   check_no_other_messages=True,
                                   sort=False)
    assert messages == snapshot(exclude=SNAPSHOT_EXCLUDE)


@pytest.mark.asyncio
async def test_navigate_hang_navigate_again_checkEvents(
        websocket, context_id, url_base, url_hang_forever,
        url_example_another_origin, read_messages, snapshot):
    # Use `url_example_another_origin`, as `url_example` will hang because of
    # `url_hang_forever`.
    await goto_url(websocket, context_id, url_base)
    await set_beforeunload_handler(websocket, context_id)
    await subscribe(
        websocket,
        ["browsingContext", "script.message", "network.beforeRequestSent"])

    messages_log = []
    known_values = {}

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_hang_forever,
                "wait": "complete",
                "context": context_id
            }
        })

    messages_log += await read_messages(3,
                                        keys_to_stabilize=KEYS_TO_STABILIZE,
                                        known_values=known_values,
                                        sort=False)

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_example_another_origin,
                "wait": "complete",
                "context": context_id
            }
        })

    messages_log += await read_messages(9,
                                        keys_to_stabilize=KEYS_TO_STABILIZE,
                                        known_values=known_values,
                                        check_no_other_messages=True,
                                        sort=False)

    assert messages_log == snapshot(exclude=SNAPSHOT_EXCLUDE)


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'unhandledPromptBehavior': {
        'beforeUnload': 'dismiss'
    }
}],
                         indirect=True)
async def test_navigate_beforeunload_cancel(websocket, context_id, url_base,
                                            url_hang_forever,
                                            url_example_another_origin,
                                            read_messages, snapshot):
    # Use `url_example_another_origin`, as `url_example` will hang because of
    # `url_hang_forever`.
    await goto_url(websocket, context_id, url_base)
    await set_beforeunload_handler(websocket, context_id, True)
    await subscribe(
        websocket,
        ["browsingContext", "script.message", "network.beforeRequestSent"])

    known_values = {}

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_hang_forever,
                "wait": "complete",
                "context": context_id
            }
        })

    messages = await read_messages(6,
                                   keys_to_stabilize=KEYS_TO_STABILIZE,
                                   known_values=known_values,
                                   check_no_other_messages=True,
                                   sort=False)

    assert messages == snapshot(exclude=SNAPSHOT_EXCLUDE)


@pytest.mark.asyncio
@pytest.mark.parametrize("wait", ["none", "interactive", "complete"])
async def test_scriptNavigate_checkEvents(websocket, context_id, url_example,
                                          html, read_messages, snapshot, wait):
    await subscribe(websocket, ["browsingContext"])

    initial_url = html(f"<script>window.location='{url_example}';</script>")

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": initial_url,
                "wait": wait,
                "context": context_id
            }
        })

    messages = await read_messages(
        7,
        # Filter out command result, as it can be
        # racy with other events.
        filter_lambda=lambda x: 'id' not in x,
        keys_to_stabilize=KEYS_TO_STABILIZE,
        check_no_other_messages=True,
        sort=False)
    assert messages == snapshot(exclude=SNAPSHOT_EXCLUDE)


@pytest.mark.asyncio
@pytest.mark.parametrize("wait", ["none", "interactive", "complete"])
async def test_scriptNavigate_aboutBlank_checkEvents(websocket, context_id,
                                                     url_base, html,
                                                     read_messages, snapshot,
                                                     wait):
    # Other events can be racy.
    await subscribe(websocket, [
        "browsingContext.navigationStarted",
        "browsingContext.navigationAborted"
    ])

    about_blank_url = 'about:blank'
    initial_url = html(
        f"<script>window.location='{about_blank_url}';</script>")

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": initial_url,
                "wait": wait,
                "context": context_id
            }
        })

    messages = await read_messages(
        3,
        # Filter out command result, as it can be
        # racy with other events.
        filter_lambda=lambda x: 'id' not in x,
        keys_to_stabilize=KEYS_TO_STABILIZE,
        check_no_other_messages=True,
        sort=False)
    assert messages == snapshot(exclude=SNAPSHOT_EXCLUDE)


@pytest.mark.asyncio
@pytest.mark.parametrize("wait", ["none", "interactive", "complete"])
async def test_scriptNavigate_dataUrl_checkEvents(websocket, context_id,
                                                  url_base, html,
                                                  read_messages, snapshot,
                                                  wait):
    await subscribe(websocket, ["browsingContext"])

    data_url = "data:text/html;,<h2>header</h2>"
    initial_url = html(f"<script>window.location='{data_url}';</script>")

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": initial_url,
                "wait": wait,
                "context": context_id
            }
        })

    messages = await read_messages(
        4,
        # Filter out command result, as it can be
        # racy with other events.
        filter_lambda=lambda x: 'id' not in x,
        keys_to_stabilize=KEYS_TO_STABILIZE,
        check_no_other_messages=True,
        sort=False)
    assert messages == snapshot(exclude=SNAPSHOT_EXCLUDE)


@pytest.mark.asyncio
@pytest.mark.parametrize("wait", ["none", "interactive", "complete"])
async def test_scriptNavigate_fragment_checkEvents(websocket, context_id,
                                                   url_base, html,
                                                   read_messages, snapshot,
                                                   wait):
    await subscribe(websocket, ["browsingContext"])

    initial_url = html("<script>window.location='#test';</script>")

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": initial_url,
                "wait": wait,
                "context": context_id
            }
        })

    messages = await read_messages(
        5,
        # Filter out command result, as it can be
        # racy with other events.
        filter_lambda=lambda x: 'id' not in x,
        keys_to_stabilize=KEYS_TO_STABILIZE,
        check_no_other_messages=True,
        sort=False)
    assert messages == snapshot(exclude=SNAPSHOT_EXCLUDE)


@pytest.mark.asyncio
@pytest.mark.parametrize("wait", ["none", "interactive", "complete"])
async def test_scriptNavigate_fragment_nested_checkEvents(
        websocket, iframe_id, html, url_base, read_messages, snapshot, wait):
    await subscribe(websocket, ["browsingContext"])

    initial_url = html("<script>window.location='#test';</script>")

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": initial_url,
                "wait": wait,
                "context": iframe_id
            }
        })

    messages = await read_messages(
        5,
        # Filter out command result, as it can be
        # racy with other events.
        filter_lambda=lambda x: 'id' not in x,
        keys_to_stabilize=KEYS_TO_STABILIZE,
        check_no_other_messages=True,
        sort=False)
    assert messages == snapshot(exclude=SNAPSHOT_EXCLUDE)


@pytest.mark.asyncio
async def test_reload_checkEvents(websocket, context_id, url_example, html,
                                  read_messages, snapshot):
    await goto_url(websocket, context_id, url_example)

    await set_beforeunload_handler(websocket, context_id)
    await subscribe(
        websocket,
        ["browsingContext", "script.message", "network.beforeRequestSent"])

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.reload",
            "params": {
                "wait": "complete",
                "context": context_id
            }
        })

    messages = await read_messages(7,
                                   keys_to_stabilize=KEYS_TO_STABILIZE,
                                   check_no_other_messages=True,
                                   sort=False)
    assert messages == snapshot(exclude=SNAPSHOT_EXCLUDE)


@pytest.mark.asyncio
async def test_reload_aboutBlank_checkEvents(websocket, context_id, html,
                                             url_base, read_messages,
                                             snapshot):
    url = 'about:blank'
    await goto_url(websocket, context_id, url)

    await set_beforeunload_handler(websocket, context_id)
    await subscribe(
        websocket,
        ["browsingContext", "script.message", "network.beforeRequestSent"])

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.reload",
            "params": {
                "wait": "complete",
                "context": context_id
            }
        })

    messages = await read_messages(6,
                                   keys_to_stabilize=KEYS_TO_STABILIZE,
                                   check_no_other_messages=True,
                                   sort=False)
    assert messages == snapshot(exclude=SNAPSHOT_EXCLUDE)


@pytest.mark.asyncio
async def test_reload_dataUrl_checkEvents(websocket, context_id, html,
                                          url_base, read_messages, snapshot):
    data_url = "data:text/html;,<h2>header</h2>"
    await goto_url(websocket, context_id, data_url)

    await set_beforeunload_handler(websocket, context_id)
    await subscribe(
        websocket,
        ["browsingContext", "script.message", "network.beforeRequestSent"])

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.reload",
            "params": {
                "wait": "complete",
                "context": context_id
            }
        })

    messages = await read_messages(7,
                                   keys_to_stabilize=KEYS_TO_STABILIZE,
                                   check_no_other_messages=True,
                                   sort=False)
    assert messages == snapshot(exclude=SNAPSHOT_EXCLUDE)
