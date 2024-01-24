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
from anys import ANY_STR
from test_helpers import execute_command, get_tree, goto_url


@pytest.mark.asyncio
async def test_browsingContext_getTree_contextReturned(websocket, context_id):
    result = await get_tree(websocket)

    assert result == {
        "contexts": [{
            "context": context_id,
            "children": [],
            "parent": None,
            "url": "about:blank",
            "userContext": "default"
        }]
    }


@pytest.mark.asyncio
async def test_browsingContext_getTreeWithRoot_contextReturned(websocket):
    result = await execute_command(websocket, {
        "method": "browsingContext.create",
        "params": {
            "type": "tab"
        }
    })
    new_context_id = result["context"]

    result = await get_tree(websocket)

    assert len(result['contexts']) == 2

    result = await get_tree(websocket, new_context_id)

    assert result == {
        "contexts": [{
            "context": new_context_id,
            "parent": None,
            "url": "about:blank",
            "children": [],
            "userContext": "default"
        }]
    }


@pytest.mark.asyncio
async def test_browsingContext_afterNavigation_getTreeWithNestedCrossOriginContexts_contextsReturned(
        websocket, context_id, html, iframe, example_url, another_example_url):
    page_with_nested_iframe = html(iframe(example_url))
    another_page_with_nested_iframe = html(iframe(another_example_url))

    await goto_url(websocket, context_id, page_with_nested_iframe, "complete")
    await goto_url(websocket, context_id, another_page_with_nested_iframe,
                   "complete")

    result = await get_tree(websocket)

    assert {
        "contexts": [{
            "context": context_id,
            "children": [{
                "context": ANY_STR,
                "url": another_example_url,
                "children": [],
                "userContext": "default"
            }],
            "parent": None,
            "url": another_page_with_nested_iframe,
            "userContext": "default"
        }]
    } == result


@pytest.mark.asyncio
async def test_browsingContext_afterNavigation_getTreeWithNestedContexts_contextsReturned(
        websocket, context_id, html, iframe):
    nested_iframe = html('<h2>IFRAME</h2>')
    another_nested_iframe = html('<h2>ANOTHER_IFRAME</h2>')
    page_with_nested_iframe = html('<h1>MAIN_PAGE</h1>' +
                                   iframe(nested_iframe))
    another_page_with_nested_iframe = html('<h1>ANOTHER_MAIN_PAGE</h1>' +
                                           iframe(another_nested_iframe))

    await goto_url(websocket, context_id, page_with_nested_iframe, "complete")

    result = await get_tree(websocket)

    assert {
        "contexts": [{
            "context": context_id,
            "children": [{
                "context": ANY_STR,
                "url": nested_iframe,
                "children": [],
                "userContext": "default"
            }],
            "parent": None,
            "url": page_with_nested_iframe,
            "userContext": "default"
        }]
    } == result

    await goto_url(websocket, context_id, another_page_with_nested_iframe,
                   "complete")

    result = await get_tree(websocket)
    assert {
        "contexts": [{
            "context": context_id,
            "children": [{
                "context": ANY_STR,
                "url": another_nested_iframe,
                "children": [],
                "userContext": "default"
            }],
            "parent": None,
            "url": another_page_with_nested_iframe,
            "userContext": "default"
        }]
    } == result
