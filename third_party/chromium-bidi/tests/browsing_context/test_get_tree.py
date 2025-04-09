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
            "userContext": "default",
            "originalOpener": None,
            'clientWindow': ANY_STR,
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
            "userContext": "default",
            "originalOpener": None,
            'clientWindow': ANY_STR,
        }]
    }


@pytest.mark.asyncio
async def test_browsingContext_afterNavigation_getTree_contextsReturned(
        websocket, context_id, html, iframe, url_all_origins,
        test_chromedriver_mode):
    if test_chromedriver_mode:
        pytest.xfail(reason="TODO: #3294")

    page_with_nested_iframe = html(iframe(url_all_origins))
    another_page_with_nested_iframe = html(iframe(url_all_origins))

    await goto_url(websocket, context_id, page_with_nested_iframe, "complete")

    result = await get_tree(websocket)
    assert {
        "contexts": [{
            "context": context_id,
            "children": [{
                "context": ANY_STR,
                "url": url_all_origins,
                "children": [],
                "userContext": "default",
                "originalOpener": None,
                'clientWindow': ANY_STR,
            }],
            "parent": None,
            "url": page_with_nested_iframe,
            "userContext": "default",
            "originalOpener": None,
            'clientWindow': ANY_STR,
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
                "url": url_all_origins,
                "children": [],
                "userContext": "default",
                "originalOpener": None,
                'clientWindow': ANY_STR,
            }],
            "parent": None,
            "url": another_page_with_nested_iframe,
            "userContext": "default",
            "originalOpener": None,
            'clientWindow': ANY_STR,
        }]
    } == result
