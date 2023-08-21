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
from test_helpers import (ANY_TIMESTAMP, AnyExtending, get_tree, goto_url,
                          read_JSON_message, send_JSON_command, subscribe)


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
            "parent": None
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
    intermediate_page = html('<h1>PAGE_WITH_1_CHILD_IFRAME</h1>' +
                             iframe(nested_iframe.replace('"', '&quot;')))
    top_level_page = html('<h1>PAGE_WITH_2_CHILD_IFRAMES</h1>' +
                          iframe(intermediate_page.replace('"', '&quot;')))

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
            "children": [
                {
                    "context": ANY_STR,
                    # It's not guaranteed the nested page is already loaded.
                    "url": ANY_STR,
                    "children": [{
                        "context": ANY_STR,
                        # It's not guaranteed the nested page is already loaded.
                        "url": ANY_STR,
                        "children": []
                    }]
                },
            ]
        }]
    } == tree

    intermediate_page_context_id = tree["contexts"][0]["children"][0][
        "context"]
    nested_iframe_context_id = \
        tree["contexts"][0]["children"][0]["children"][0]["context"]
    assert events[0] == {
        'type': 'event',
        "method": "browsingContext.contextCreated",
        "params": {
            'context': intermediate_page_context_id,
            'parent': context_id,
            'children': None,
            'url': 'about:blank'
        }
    }

    assert events[1] == {
        'type': 'event',
        "method": "browsingContext.contextCreated",
        "params": {
            'context': nested_iframe_context_id,
            'parent': intermediate_page_context_id,
            'children': None,
            'url': 'about:blank'
        }
    }


@pytest.mark.asyncio
async def test_browsingContext_create_withUserGesture_eventsEmitted(
        websocket, context_id, html):
    blank_url = "https://example.com/"
    LINK_WITH_BLANK_TARGET = html(
        f'''<a href="{blank_url}" target="_blank">new tab</a>''')

    await goto_url(websocket, context_id, LINK_WITH_BLANK_TARGET)

    await subscribe(websocket, [
        "browsingContext.contextCreated",
        "browsingContext.domContentLoaded",
        "browsingContext.load",
    ])

    command_id = await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": """document.querySelector('a').click();""",
                "awaitPromise": True,
                "target": {
                    "context": context_id,
                },
                "userActivation": True
            }
        })

    response = await read_JSON_message(websocket)
    new_context_id = response["params"]["context"]
    assert new_context_id != context_id
    assert response == {
        'type': 'event',
        "method": "browsingContext.contextCreated",
        "params": {
            "context": ANY_STR,
            "url": "about:blank",
            "children": None,
            "parent": None
        }
    }

    response = await read_JSON_message(websocket)
    assert response == AnyExtending({
        "id": command_id,
        "type": "success",
        "result": ANY_DICT
    })

    # TODO: CDP sends Lifecycle events when we send Lifecycle event enable
    # These events correspond to the initial about:blank creation
    # We should try to ignore the from Mapper.
    response = await read_JSON_message(websocket)
    assert response == {
        'type': 'event',
        "method": "browsingContext.domContentLoaded",
        "params": {
            "context": new_context_id,
            "navigation": ANY_STR,
            "timestamp": ANY_TIMESTAMP,
            "url": "about:blank"
        }
    }

    response = await read_JSON_message(websocket)
    assert response == {
        'type': 'event',
        "method": "browsingContext.domContentLoaded",
        "params": {
            "context": new_context_id,
            "navigation": ANY_STR,
            "timestamp": ANY_TIMESTAMP,
            "url": blank_url
        }
    }

    response = await read_JSON_message(websocket)
    assert response == {
        'type': 'event',
        "method": "browsingContext.load",
        "params": {
            "context": new_context_id,
            "navigation": ANY_STR,
            "timestamp": ANY_TIMESTAMP,
            "url": blank_url
        }
    }
