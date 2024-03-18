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

import re

import pytest
from test_helpers import (ANY_SHARED_ID, execute_command, get_tree, goto_url,
                          set_html_content)


@pytest.mark.asyncio
async def test_shared_id_in_same_realm_same_navigable(websocket, context_id,
                                                      html):
    await goto_url(websocket, context_id, html("<div>some text</div>"))

    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "document.querySelector('body > div');",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True
            }
        })

    shared_id = result["result"]["sharedId"]
    assert "UNKNOWN" not in shared_id

    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "(arg)=>arg",
                "this": {
                    "type": "undefined"
                },
                "arguments": [{
                    "sharedId": shared_id
                }],
                "awaitPromise": False,
                "target": {
                    "context": context_id
                }
            }
        })

    assert result["type"] == "success"
    assert result["result"]["sharedId"] == shared_id


@pytest.mark.asyncio
async def test_shared_id_without_navigation(websocket, context_id):
    await set_html_content(websocket, context_id, "<div>some text</div>")

    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "document.querySelector('body > div');",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True
            }
        })

    shared_id = result["result"]["sharedId"]
    assert "UNKNOWN" not in shared_id

    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "(arg)=>arg",
                "this": {
                    "type": "undefined"
                },
                "arguments": [{
                    "sharedId": shared_id
                }],
                "awaitPromise": False,
                "target": {
                    "context": context_id
                }
            }
        })

    assert result["type"] == "success"
    assert result["result"]["sharedId"] == shared_id


@pytest.mark.asyncio
async def test_shared_id_in_different_realm_same_navigable(
        websocket, context_id, html):
    await goto_url(websocket, context_id, html("<div>some text</div>"))

    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "document.querySelector('body > div');",
                "target": {
                    "context": context_id,
                    "sandbox": "SOME_SANDBOX"
                },
                "awaitPromise": True
            }
        })

    realm_1 = result["realm"]
    shared_id = result["result"]["sharedId"]

    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "(arg)=>arg",
                "this": {
                    "type": "undefined"
                },
                "arguments": [{
                    "sharedId": shared_id
                }],
                "awaitPromise": False,
                "target": {
                    "context": context_id,
                    "sandbox": "ANOTHER_SANDBOX"
                }
            }
        })

    assert result["realm"] != realm_1
    assert result["type"] == "success"
    assert result["result"]["sharedId"] == shared_id


@pytest.mark.asyncio
async def test_shared_id_in_different_navigable(websocket, context_id, html):
    await goto_url(websocket, context_id, html("<div>some text</div>"))

    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "document.querySelector('body > div');",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True
            }
        })

    shared_id = result["result"]["sharedId"]

    await goto_url(websocket, context_id, html("some other page"))

    with pytest.raises(
            Exception,
            match=re.compile(
                str({
                    "error": "no such node",
                    "message": 'SharedId ".*" belongs to different document. Current document is .*'
                }))):
        await execute_command(
            websocket, {
                "method": "script.callFunction",
                "params": {
                    "functionDeclaration": "(arg)=>arg",
                    "this": {
                        "type": "undefined"
                    },
                    "arguments": [{
                        "sharedId": shared_id
                    }],
                    "awaitPromise": False,
                    "target": {
                        "context": context_id
                    }
                }
            })


@pytest.mark.asyncio
async def test_shared_id_not_found(websocket, context_id, html):
    await goto_url(websocket, context_id, html("<div>some text</div>"))

    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "document.querySelector('body > div');",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True
            }
        })

    shared_id = result["result"]["sharedId"] + "9999"

    with pytest.raises(Exception,
                       match=re.compile(
                           str({
                               "error": "no such node",
                               "message": 'SharedId ".*" was not found.'
                           }))):
        await execute_command(
            websocket, {
                "method": "script.callFunction",
                "params": {
                    "functionDeclaration": "(arg)=>arg",
                    "this": {
                        "type": "undefined"
                    },
                    "arguments": [{
                        "sharedId": shared_id
                    }],
                    "awaitPromise": False,
                    "target": {
                        "context": context_id
                    }
                }
            })


@pytest.mark.asyncio
async def test_shared_id_format(websocket, context_id, html):
    await goto_url(
        websocket, context_id,
        html(
            "<div some_attr_name='some_attr_value'>some text<h2>some another text</h2></div>"
        ))

    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "document.querySelector('body > div');",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "serializationOptions": {
                    "maxDomDepth": 0
                }
            }
        })
    shared_id = result["result"]["sharedId"]
    assert shared_id == ANY_SHARED_ID


@pytest.mark.asyncio
async def test_shared_id_uses_node_frame(websocket, context_id, html):
    # `iframe_id` does not work in this context, as it is cross-origin.
    await goto_url(websocket, context_id,
                   html('data:text/html,<iframe src="about:blank" />'))

    tree = await get_tree(websocket, context_id)
    iframe_id = tree['contexts'][0]['children'][0]['context']

    # Get top-level element reference from iframe.
    result = await execute_command(
        websocket, {
            'method': 'script.evaluate',
            'params': {
                'expression': 'parent.document.getElementsByTagName("iframe")[0]',
                'target': {
                    'context': iframe_id
                },
                'awaitPromise': True,
                'serializationOptions': {
                    'maxDomDepth': 0
                }
            }
        })

    top_level_shared_id = result['result']['sharedId']
    # Assert top-level element's reference points to top-level browsing context.
    assert top_level_shared_id.startswith(
        f'f.{context_id}.d.'
    ), 'SharedId should point to the top level frame id'

    # Get top-level element reference from top-level frame.
    result = await execute_command(
        websocket, {
            'method': 'script.evaluate',
            'params': {
                'expression': 'document.getElementsByTagName("iframe")[0]',
                'target': {
                    'context': context_id
                },
                'awaitPromise': True,
                'serializationOptions': {
                    'maxDomDepth': 0
                }
            }
        })
    # Assert the top-level element's reference is the same regardless of the
    # frame it was evaluated from.
    assert top_level_shared_id == result['result']['sharedId']
