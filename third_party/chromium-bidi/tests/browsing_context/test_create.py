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
import pytest
from anys import ANY_DICT, ANY_STR
from test_helpers import (ANY_TIMESTAMP, AnyExtending, execute_command,
                          get_tree, goto_url, read_JSON_message,
                          send_JSON_command, subscribe)


@pytest.mark.asyncio
async def test_browsingContext_create_eventContextCreatedEmitted(
        websocket, read_sorted_messages):
    await subscribe(websocket, [
        "browsingContext.contextCreated", "browsingContext.domContentLoaded",
        "browsingContext.load"
    ])

    await send_JSON_command(websocket, {
        "id": 9,
        "method": "browsingContext.create",
        "params": {
            "type": "tab"
        }
    })

    # Read event messages. The order can vary in headless and headful modes, so
    # sort is needed:
    # * `browsingContext.contextCreated` event.
    # * `browsingContext.domContentLoaded` event.
    # * `browsingContext.load` event.
    [context_created_event, dom_content_loaded_event,
     load_event] = await read_sorted_messages(3)

    # Read the `browsingContext.create` command result. It should be sent after
    # all the loading events.
    command_result = await read_JSON_message(websocket)

    new_context_id = command_result['result']['context']

    # Assert command done.
    assert command_result == {
        "type": "success",
        "id": 9,
        "result": {
            'context': new_context_id
        }
    }

    # Assert "browsingContext.contextCreated" event emitted.
    assert {
        "type": "event",
        "method": "browsingContext.contextCreated",
        "params": {
            "context": new_context_id,
            "url": "about:blank",
            "children": None,
            "parent": None,
            "userContext": "default",
            "originalOpener": None
        }
    } == context_created_event

    # Assert "browsingContext.domContentLoaded" event emitted.
    assert {
        "type": "event",
        "method": "browsingContext.domContentLoaded",
        "params": {
            "context": new_context_id,
            "navigation": ANY_STR,
            "timestamp": ANY_TIMESTAMP,
            "url": "about:blank"
        }
    } == dom_content_loaded_event

    # Assert "browsingContext.load" event emitted.
    assert {
        "type": "event",
        "method": "browsingContext.load",
        "params": {
            "context": new_context_id,
            "navigation": ANY_STR,
            "timestamp": ANY_TIMESTAMP,
            "url": "about:blank"
        }
    } == load_event


@pytest.mark.asyncio
async def test_browsingContext_createWithNestedSameOriginContexts_eventContextCreatedEmitted(
        websocket, context_id, html, iframe):
    nested_iframe = html('<h1>PAGE_WITHOUT_CHILD_IFRAMES</h1>')
    intermediate_page = html(
        f'<h1>PAGE_WITH_1_CHILD_IFRAME</h1>{iframe(nested_iframe)}')
    top_level_page = html(
        f'<h1>PAGE_WITH_2_CHILD_IFRAMES</h1>{iframe(intermediate_page)}')

    await subscribe(websocket, ["browsingContext.contextCreated"])

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": top_level_page,
                "wait": "complete",
                "context": context_id
            }
        })

    events = []
    while len(events) < 2:
        resp = await read_JSON_message(websocket)
        if "method" in resp and resp[
                "method"] == "browsingContext.contextCreated":
            events.append(resp)

    tree = await get_tree(websocket)

    assert {
        "contexts": [{
            "context": ANY_STR,
            "parent": None,
            "url": top_level_page,
            "userContext": "default",
            "originalOpener": None,
            "children": [
                {
                    "context": ANY_STR,
                    # It's not guaranteed the nested page is already loaded.
                    "url": ANY_STR,
                    "userContext": "default",
                    "originalOpener": None,
                    "children": [{
                        "context": ANY_STR,
                        # It's not guaranteed the nested page is already loaded.
                        "url": ANY_STR,
                        "userContext": "default",
                        "children": [],
                        "originalOpener": None
                    }]
                },
            ]
        }]
    } == tree

    intermediate_page_context_id = tree["contexts"][0]["children"][0][
        "context"]
    nested_iframe_context_id = \
        tree["contexts"][0]["children"][0]["children"][0]["context"]
    assert events[0] == AnyExtending({
        'type': 'event',
        "method": "browsingContext.contextCreated",
        "params": {
            'context': intermediate_page_context_id,
            'parent': context_id,
            'children': None,
            'url': 'about:blank',
            'userContext': 'default'
        }
    })

    assert events[1] == AnyExtending({
        'type': 'event',
        "method": "browsingContext.contextCreated",
        "params": {
            'context': nested_iframe_context_id,
            'parent': intermediate_page_context_id,
            'children': None,
            'url': 'about:blank',
            'userContext': 'default'
        }
    })


@pytest.mark.asyncio
async def test_browsingContext_create_withUserGesture_eventsEmitted(
        websocket, context_id, html, url_example, read_sorted_messages):
    LINK_WITH_BLANK_TARGET = html(
        f'''<a href="{url_example}" target="_blank">new tab</a>''')

    await goto_url(websocket, context_id, LINK_WITH_BLANK_TARGET)

    await subscribe(websocket, [
        'browsingContext.contextCreated',
        'browsingContext.domContentLoaded',
        'browsingContext.load',
    ])

    command_id = await send_JSON_command(
        websocket, {
            'method': 'script.evaluate',
            'params': {
                'expression': 'document.querySelector("a").click();',
                'awaitPromise': True,
                'target': {
                    'context': context_id,
                },
                'userActivation': True
            }
        })

    # Read event messages. The order can vary, so read all and sort them. Ignore
    # optional "browsingContext.domContentLoaded" event for "about:blank" pages.
    # Expected sorted messages order:
    # 1. Command result.
    # 2. "browsingContext.contextCreated" event.
    # (optional). "browsingContext.domContentLoaded" event for "about:blank".
    #             Omitted in headful mode.
    # 3. "browsingContext.domContentLoaded" event for the example_url.
    # 4. "browsingContext.load" event.
    messages = await read_sorted_messages(
        4, lambda m: 'method' not in m or
        (m['method'] != 'browsingContext.domContentLoaded') or m['params'][
            'url'] != 'about:blank')

    # Get the new context id from the "browsingContext.contextCreated" event.
    new_context_id = messages[1]['params']['context']

    assert messages == [{
        'id': command_id,
        'type': 'success',
        'result': ANY_DICT
    }, {
        'type': 'event',
        'method': 'browsingContext.contextCreated',
        'params': {
            'context': ANY_STR,
            'url': 'about:blank',
            'children': None,
            'parent': None,
            'userContext': 'default',
            'originalOpener': ANY_STR,
        }
    }, {
        'type': 'event',
        'method': 'browsingContext.domContentLoaded',
        'params': {
            'context': new_context_id,
            'navigation': ANY_STR,
            'timestamp': ANY_TIMESTAMP,
            'url': url_example
        }
    }, {
        'type': 'event',
        'method': 'browsingContext.load',
        'params': {
            'context': new_context_id,
            'navigation': ANY_STR,
            'timestamp': ANY_TIMESTAMP,
            'url': url_example
        }
    }]


@pytest.mark.asyncio
@pytest.mark.parametrize("type", ["window", "tab"])
async def test_browsingContext_create_withUserContext(websocket, type):
    user_context = await execute_command(websocket, {
        "method": "browser.createUserContext",
        "params": {}
    })

    await subscribe(websocket, [
        "browsingContext.contextCreated", "browsingContext.domContentLoaded",
        "browsingContext.load"
    ])

    result = await execute_command(
        websocket, {
            "method": "browsingContext.create",
            "params": {
                "type": type,
                "userContext": user_context["userContext"]
            }
        })

    tree = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {}
    })

    assert len(tree['contexts']) == 2

    assert tree["contexts"][1] == AnyExtending({
        'context': result['context'],
        'url': 'about:blank',
        'userContext': user_context["userContext"],
        'children': [],
        'parent': None
    })


@pytest.mark.asyncio
@pytest.mark.parametrize("type", ["window", "tab"])
@pytest.mark.parametrize("global_subscription", [True, False])
async def test_browsingContext_subscribe_to_contextCreated_emits_for_existing(
        websocket, type, context_id, another_context_id, global_subscription):
    subscribe_command_id = await send_JSON_command(
        websocket,
        {
            "method": "session.subscribe",
            "params": {
                # Subscribe to a domain and to a specific event twice. The
                # events should not be duplicated.
                "events": [
                    "browsingContext", "browsingContext.contextCreated",
                    "browsingContext.contextCreated"
                ],
                # Missing "contexts" means global subscription.
                **({} if global_subscription else {
                       "contexts": [another_context_id]
                   })
            }
        })

    # In case of global subscription, the events should be emitted for all
    # contexts. Otherwise, only for the subscribed ones.
    if global_subscription:
        resp = await read_JSON_message(websocket)
        assert resp == AnyExtending({
            'method': 'browsingContext.contextCreated',
            'params': {
                'context': context_id,
            },
            'type': 'event',
        })

    resp = await read_JSON_message(websocket)
    assert resp == AnyExtending({
        'method': 'browsingContext.contextCreated',
        'params': {
            'context': another_context_id,
        },
        'type': 'event',
    })

    resp = await read_JSON_message(websocket)
    assert resp == {
        'id': subscribe_command_id,
        'type': 'success',
        'result': {}
    }


@pytest.mark.parametrize("type", ["window", "tab"])
@pytest.mark.parametrize("background", [True, False])
@pytest.mark.asyncio
async def test_browsingContext_create_background(websocket, background,
                                                 test_headless_mode, type):
    resp = await execute_command(
        websocket, {
            "method": "browsingContext.create",
            "params": {
                "type": type,
                "background": background
            }
        })
    new_context_id = resp["context"]

    if test_headless_mode == "old":
        pytest.xfail("Old headless mode does not support visibility checks")

    resp = await execute_command(
        websocket, {
            'method': 'script.evaluate',
            'params': {
                'expression': '[document.visibilityState, document.hasFocus()]',
                'awaitPromise': True,
                'target': {
                    'context': new_context_id,
                },
                'userActivation': True
            }
        })
    visible = resp["result"]["value"][0]["value"]
    has_focus = resp["result"]["value"][1]["value"]

    if type == "window":
        # In case of new window, the new browsing context should be visible,
        # even if created in background.
        assert visible == "visible", "New window should be visible regardless of background flag"
    else:
        if background:
            assert visible == "hidden", "New tab should be hidden when created in background"
        else:
            assert visible == "visible", "New tab should be visible when created in foreground"

    if test_headless_mode != "false" and type == "window":
        pytest.xfail("Window is not always selected in headless mode")

    assert has_focus is not background, "New context should not have focus" if background else "New context should have focus"
