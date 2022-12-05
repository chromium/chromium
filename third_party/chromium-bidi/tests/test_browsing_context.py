# Copyright 2021 Google LLC.
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

from _helpers import *


@pytest.mark.asyncio
async def test_browsingContext_noInitialLoadEvents(websocket):
    # Due to the nature, the test does not always fail, even if the
    # implementation does not guarantee the initial context to be fully loaded.
    # The test asserts there was no initial "browsingContext.load" emitted
    # during the following steps:
    # 1. Subscribe for the "browsingContext.load" event.
    # 2. Get the currently open context.
    # 3. Navigate to some url.
    # 4. Verify the new page is loaded.

    url = "data:text/html,<h2>test</h2>"

    await send_JSON_command(
        websocket, {
            "id": 1,
            "method": "session.subscribe",
            "params": {
                "events": ["browsingContext.load"]
            }
        })

    resp = await read_JSON_message(websocket)
    assert resp["id"] == 1

    await send_JSON_command(websocket, {
        "id": 2,
        "method": "browsingContext.getTree",
        "params": {}
    })

    resp = await read_JSON_message(websocket)
    assert resp[
        "id"] == 2, "The message should be result of command `browsingContext.getTree` with `id`: 2"
    context_id = resp["result"]["contexts"][0]["context"]

    await send_JSON_command(
        websocket, {
            "id": 3,
            "method": "browsingContext.navigate",
            "params": {
                "url": url,
                "wait": "none",
                "context": context_id
            }
        })

    resp = await read_JSON_message(websocket)
    assert resp["id"] == 3
    navigation = resp["result"]["navigation"]

    # Wait for the navigated page to be loaded.
    resp = await read_JSON_message(websocket)
    recursive_compare(
        {
            'method': 'browsingContext.load',
            'params': {
                'context': context_id,
                'navigation': navigation,
                'url': url
            }
        }, resp)


@pytest.mark.asyncio
async def test_browsingContext_getTree_contextReturned(websocket, context_id):
    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {}
    })

    assert result == {
        "contexts": [{
            "context": context_id,
            "children": [],
            "parent": None,
            "url": "about:blank"
        }]
    }


@pytest.mark.asyncio
async def test_browsingContext_getTreeWithRoot_contextReturned(
        websocket, context_id):
    result = await execute_command(websocket, {
        "method": "browsingContext.create",
        "params": {
            "type": "tab"
        }
    })
    new_context_id = result["context"]

    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {}
    })

    assert len(result['contexts']) == 2

    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {
            "root": new_context_id
        }
    })

    assert result == {
        "contexts": [{
            "context": new_context_id,
            "parent": None,
            "url": "about:blank",
            "children": []
        }]
    }


@pytest.mark.asyncio
async def test_navigateToPageWithHash_contextInfoUpdated(
        websocket, context_id):
    url = "data:text/html,<h2>test</h2>"
    url_with_hash_1 = url + "#1"

    # Initial navigation.
    await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_with_hash_1,
                "wait": "complete",
                "context": context_id
            }
        })

    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {}
    })

    assert result == {
        "contexts": [{
            "context": context_id,
            "children": [],
            "parent": None,
            "url": url_with_hash_1
        }]
    }


@pytest.mark.asyncio
async def test_browsingContext_getTreeWithNestedSameOriginContexts_contextsReturned(
        websocket, context_id):
    nested_iframe = 'data:text/html,<h1>CHILD_PAGE</h1>'
    page_with_nested_iframe = f'data:text/html,<h1>MAIN_PAGE</h1>' \
                              f'<iframe src="{nested_iframe}" />'
    await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": page_with_nested_iframe,
                "wait": "complete",
                "context": context_id
            }
        })

    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {}
    })

    recursive_compare(
        {
            "contexts": [{
                "context":
                context_id,
                "children": [{
                    "context": any_string,
                    "url": nested_iframe,
                    "children": []
                }],
                "parent":
                None,
                "url":
                page_with_nested_iframe
            }]
        }, result)


# TODO(sadym): make offline.
@pytest.mark.asyncio
async def test_browsingContext_getTreeWithNestedCrossOriginContexts_contextsReturned(
        websocket, context_id):
    nested_iframe = 'https://example.com/'
    page_with_nested_iframe = f'data:text/html,<h1>MAIN_PAGE</h1>' \
                              f'<iframe src="{nested_iframe}" />'
    await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": page_with_nested_iframe,
                "wait": "complete",
                "context": context_id
            }
        })

    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {}
    })

    recursive_compare(
        {
            "contexts": [{
                "context":
                context_id,
                "children": [{
                    "context": any_string,
                    "url": nested_iframe,
                    "children": []
                }],
                "parent":
                None,
                "url":
                page_with_nested_iframe
            }]
        }, result)


# TODO(sadym): make offline.
@pytest.mark.asyncio
async def test_browsingContext_afterNavigation_getTreeWithNestedCrossOriginContexts_contextsReturned(
        websocket, context_id):
    nested_iframe = 'https://example.com/'
    another_nested_iframe = 'https://example.org/'
    page_with_nested_iframe = f'data:text/html,<h1>MAIN_PAGE</h1>' \
                              f'<iframe src="{nested_iframe}" />'
    another_page_with_nested_iframe = f'data:text/html,<h1>ANOTHER_MAIN_PAGE</h1>' \
                                      f'<iframe src="{another_nested_iframe}" />'

    await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": page_with_nested_iframe,
                "wait": "complete",
                "context": context_id
            }
        })

    await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": another_page_with_nested_iframe,
                "wait": "complete",
                "context": context_id
            }
        })

    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {}
    })

    recursive_compare(
        {
            "contexts": [{
                "context":
                context_id,
                "children": [{
                    "context": any_string,
                    "url": another_nested_iframe,
                    "children": []
                }],
                "parent":
                None,
                "url":
                another_page_with_nested_iframe
            }]
        }, result)


@pytest.mark.asyncio
async def test_browsingContext_afterNavigation_getTreeWithNestedContexts_contextsReturned(
        websocket, context_id):
    nested_iframe = 'data:text/html,<h2>IFRAME</h2>'
    another_nested_iframe = 'data:text/html,<h2>ANOTHER_IFRAME</h2>'
    page_with_nested_iframe = f'data:text/html,<h1>MAIN_PAGE</h1>' \
                              f'<iframe src="{nested_iframe}" />'
    another_page_with_nested_iframe = f'data:text/html,<h1>ANOTHER_MAIN_PAGE</h1>' \
                                      f'<iframe src="{another_nested_iframe}" />'

    await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": page_with_nested_iframe,
                "wait": "complete",
                "context": context_id
            }
        })

    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {}
    })

    recursive_compare(
        {
            "contexts": [{
                "context":
                context_id,
                "children": [{
                    "context": any_string,
                    "url": nested_iframe,
                    "children": []
                }],
                "parent":
                None,
                "url":
                page_with_nested_iframe
            }]
        }, result)

    await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": another_page_with_nested_iframe,
                "wait": "complete",
                "context": context_id
            }
        })

    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {}
    })

    recursive_compare(
        {
            "contexts": [{
                "context":
                context_id,
                "children": [{
                    "context": any_string,
                    "url": another_nested_iframe,
                    "children": []
                }],
                "parent":
                None,
                "url":
                another_page_with_nested_iframe
            }]
        }, result)


@pytest.mark.asyncio
async def test_browsingContext_create_eventContextCreatedEmitted(
        websocket, context_id):
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
    messages = [
        await read_JSON_message(websocket), await read_JSON_message(websocket),
        await read_JSON_message(websocket)
    ]

    messages.sort(key=lambda x: x["method"] if "method" in x else "")
    [context_created_event, dom_content_loaded_event, load_event] = messages

    # Read the `browsingContext.create` command result. It should be sent after
    # all the loading events.
    command_result = await read_JSON_message(websocket)

    new_context_id = command_result['result']['context']

    # Assert command done.
    assert command_result == {
        "id": 9,
        "result": {
            'context': new_context_id,
            'parent': None,
            'children': [],
            'url': 'about:blank'
        }
    }

    # Assert "browsingContext.contextCreated" event emitted.
    recursive_compare(
        {
            "method": "browsingContext.contextCreated",
            "params": {
                "context": new_context_id,
                "url": "about:blank",
                "children": None,
                "parent": None
            }
        }, context_created_event)

    # Assert "browsingContext.domContentLoaded" event emitted.
    recursive_compare(
        {
            "method": "browsingContext.domContentLoaded",
            "params": {
                "context": new_context_id,
                "navigation": any_string,
                "url": "about:blank"
            }
        }, dom_content_loaded_event)

    # Assert "browsingContext.load" event emitted.
    recursive_compare(
        {
            "method": "browsingContext.load",
            "params": {
                "context": new_context_id,
                "navigation": any_string,
                "url": "about:blank"
            }
        }, load_event)


@pytest.mark.asyncio
async def test_browsingContext_createWithNestedSameOriginContexts_eventContextCreatedEmitted(
        websocket, context_id):
    nested_iframe = 'data:text/html,<h1>PAGE_WITHOUT_CHILD_IFRAMES</h1>'
    intermediate_page = 'data:text/html,<h1>PAGE_WITH_1_CHILD_IFRAME</h1>' \
                        '<iframe src="' + \
                        nested_iframe.replace('"', '&quot;') + \
                        '" />'
    top_level_page = 'data:text/html,<h1>PAGE_WITH_2_CHILD_IFRAMES</h1>' \
                     '<iframe src="' + \
                     intermediate_page.replace('"', '&quot;') + \
                     '" />'

    await subscribe(websocket, ["browsingContext.contextCreated"])

    command = {
        "method": "browsingContext.navigate",
        "params": {
            "url": top_level_page,
            "wait": "complete",
            "context": context_id
        }
    }
    await send_JSON_command(websocket, command)

    events = []
    while len(events) < 2:
        resp = await read_JSON_message(websocket)
        if "method" in resp and \
              resp["method"] == "browsingContext.contextCreated":
            events.append(resp)

    tree = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {}
    })

    recursive_compare(
        {
            "contexts": [{
                "context":
                any_string,
                "parent":
                None,
                "url":
                top_level_page,
                "children": [
                    {
                        "context":
                        any_string,
                        # It's not guaranteed the nested page is already loaded.
                        "url":
                        any_string,
                        "children": [{
                            "context": any_string,
                            # It's not guaranteed the nested page is already loaded.
                            "url": any_string,
                            "children": []
                        }]
                    },
                ]
            }]
        },
        tree)

    intermediate_page_context_id = tree["contexts"][0]["children"][0][
        "context"]
    nested_iframe_context_id = \
        tree["contexts"][0]["children"][0]["children"][0]["context"]
    assert events[0] == {
        "method": "browsingContext.contextCreated",
        "params": {
            'context': intermediate_page_context_id,
            'parent': context_id,
            'children': None,
            'url': 'about:blank'
        }
    }

    assert events[1] == {
        "method": "browsingContext.contextCreated",
        "params": {
            'context': nested_iframe_context_id,
            'parent': intermediate_page_context_id,
            'children': None,
            'url': 'about:blank'
        }
    }


@pytest.mark.asyncio
async def test_browsingContext_close_browsingContext_closed(
        websocket, context_id):
    await subscribe(websocket, ["browsingContext.contextDestroyed"])

    # Send command.
    command = {
        "id": 12,
        "method": "browsingContext.close",
        "params": {
            "context": context_id
        }
    }
    await send_JSON_command(websocket, command)

    # Assert "browsingContext.contextCreated" event emitted.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "method": "browsingContext.contextDestroyed",
        "params": {
            "context": context_id,
            "parent": None,
            "url": "about:blank",
            "children": None
        }
    }

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {"id": 12, "result": {}}

    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {}
    })

    # Assert context is closed.
    assert result == {'contexts': []}


@pytest.mark.asyncio
async def test_browsingContext_navigateWaitNone_navigated(
        websocket, context_id):
    await subscribe(
        websocket,
        ["browsingContext.domContentLoaded", "browsingContext.load"])
    # Send command.
    await send_JSON_command(
        websocket, {
            "id": 13,
            "method": "browsingContext.navigate",
            "params": {
                "url": "data:text/html,<h2>test</h2>",
                "wait": "none",
                "context": context_id
            }
        })

    # Assert command done.
    resp = await read_JSON_message(websocket)
    navigation_id = resp["result"]["navigation"]
    assert resp == {
        "id": 13,
        "result": {
            "navigation": navigation_id,
            "url": "data:text/html,<h2>test</h2>"
        }
    }

    # Wait for `browsingContext.load` event.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "method": "browsingContext.load",
        "params": {
            "context": context_id,
            "navigation": navigation_id,
            "url": "data:text/html,<h2>test</h2>"
        }
    }

    # Wait for `browsingContext.domContentLoaded` event.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "method": "browsingContext.domContentLoaded",
        "params": {
            "context": context_id,
            "navigation": navigation_id,
            "url": "data:text/html,<h2>test</h2>"
        }
    }


@pytest.mark.asyncio
async def test_browsingContext_navigateWaitInteractive_navigated(
        websocket, context_id):
    await subscribe(
        websocket,
        ["browsingContext.domContentLoaded", "browsingContext.load"])

    # Send command.
    command = {
        "id": 14,
        "method": "browsingContext.navigate",
        "params": {
            "url": "data:text/html,<h2>test</h2>",
            "wait": "interactive",
            "context": context_id
        }
    }
    await send_JSON_command(websocket, command)

    # Wait for `browsingContext.load` event.
    resp = await read_JSON_message(websocket)
    navigation_id = resp["params"]["navigation"]
    assert resp == {
        "method": "browsingContext.load",
        "params": {
            "context": context_id,
            "navigation": navigation_id,
            "url": "data:text/html,<h2>test</h2>"
        }
    }

    # Wait for `browsingContext.domContentLoaded` event.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "method": "browsingContext.domContentLoaded",
        "params": {
            "context": context_id,
            "navigation": navigation_id,
            "url": "data:text/html,<h2>test</h2>",
        }
    }

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id": 14,
        "result": {
            "navigation": navigation_id,
            "url": "data:text/html,<h2>test</h2>"
        }
    }


@pytest.mark.asyncio
async def test_browsingContext_navigateWaitComplete_navigated(
        websocket, context_id):
    await subscribe(
        websocket,
        ["browsingContext.domContentLoaded", "browsingContext.load"])

    # Send command.
    command = {
        "id": 15,
        "method": "browsingContext.navigate",
        "params": {
            "url": "data:text/html,<h2>test</h2>",
            "wait": "complete",
            "context": context_id
        }
    }
    await send_JSON_command(websocket, command)

    # Wait for `browsingContext.load` event.
    resp = await read_JSON_message(websocket)
    navigation_id = resp["params"]["navigation"]
    assert resp == {
        "method": "browsingContext.load",
        "params": {
            "context": context_id,
            "navigation": navigation_id,
            "url": "data:text/html,<h2>test</h2>"
        }
    }

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id": 15,
        "result": {
            "navigation": navigation_id,
            "url": "data:text/html,<h2>test</h2>"
        }
    }

    # Wait for `browsingContext.domContentLoaded` event.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "method": "browsingContext.domContentLoaded",
        "params": {
            "context": context_id,
            "navigation": navigation_id,
            "url": "data:text/html,<h2>test</h2>"
        }
    }


@pytest.mark.asyncio
async def test_browsingContext_navigateSameDocumentNavigation_navigated(
        websocket, context_id):
    url = "data:text/html,<h2>test</h2>"
    url_with_hash_1 = url + "#1"
    url_with_hash_2 = url + "#2"

    # Initial navigation.
    await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url,
                "wait": "complete",
                "context": context_id
            }
        })

    # Navigate back and forth in the same document with `wait:none`.
    resp = await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_with_hash_1,
                "wait": "none",
                "context": context_id
            }
        })
    assert resp == {'navigation': None, 'url': url_with_hash_1}

    resp = await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_with_hash_2,
                "wait": "none",
                "context": context_id
            }
        })
    assert resp == {'navigation': None, 'url': url_with_hash_2}

    # Navigate back and forth in the same document with `wait:interactive`.
    resp = await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_with_hash_1,
                "wait": "interactive",
                "context": context_id
            }
        })
    assert resp == {'navigation': None, 'url': url_with_hash_1}

    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {
            "root": context_id
        }
    })

    recursive_compare(
        {
            "contexts": [{
                "context": context_id,
                "children": [],
                "parent": None,
                "url": url_with_hash_1
            }]
        }, result)

    resp = await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_with_hash_2,
                "wait": "interactive",
                "context": context_id
            }
        })
    assert resp == {'navigation': None, 'url': url_with_hash_2}

    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {
            "root": context_id
        }
    })

    recursive_compare(
        {
            "contexts": [{
                "context": context_id,
                "children": [],
                "parent": None,
                "url": url_with_hash_2
            }]
        }, result)

    # Navigate back and forth in the same document with `wait:complete`.
    resp = await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_with_hash_1,
                "wait": "complete",
                "context": context_id
            }
        })
    assert resp == {'navigation': None, 'url': url_with_hash_1}

    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {
            "root": context_id
        }
    })

    recursive_compare(
        {
            "contexts": [{
                "context": context_id,
                "children": [],
                "parent": None,
                "url": url_with_hash_1
            }]
        }, result)

    resp = await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_with_hash_2,
                "wait": "complete",
                "context": context_id
            }
        })
    assert resp == {'navigation': None, 'url': url_with_hash_2}

    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {
            "root": context_id
        }
    })

    recursive_compare(
        {
            "contexts": [{
                "context": context_id,
                "children": [],
                "parent": None,
                "url": url_with_hash_2
            }]
        }, result)


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_type_textTyped():
    pass
    # TODO sadym: implement


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_navigateWithShortTimeout_timeoutOccurredAndEventPageLoadEmitted(
):
    pass
    # TODO sadym: implement


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_waitForSelector_success():
    pass
    # TODO sadym: implement


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_waitForSelector_success_slow():
    # 1. Wait for element which is not on the page.
    # 2. Assert element not found.
    # 3. Add element to the page.
    # 4. Wait for newly created element.
    # 5. Assert element found.

    pass
    # TODO sadym: implement


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_waitForHiddenSelector_success():
    pass
    # TODO sadym: implement


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_waitForSelectorWithMinimumTimeout_failedWithTimeout(
):
    pass
    # TODO sadym: implement


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_waitForSelectorWithMissingElement_failedWithTimeout_slow(
):
    pass
    # TODO sadym: implement


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_clickElement_clickProcessed():
    pass
    # TODO sadym: implement
