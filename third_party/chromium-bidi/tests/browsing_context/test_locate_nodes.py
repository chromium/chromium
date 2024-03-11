#  Copyright 2024 Google LLC.
#  Copyright (c) Microsoft Corporation.
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
import re

import pytest
from test_helpers import ANY_SHARED_ID, execute_command, goto_url


@pytest.mark.asyncio
async def test_locate_nodes_css_locator(websocket, context_id, html):
    await goto_url(
        websocket, context_id,
        html(
            """<div data-class="one">foobarBARbaz</div><div data-class="two">foobarBARbaz</div>"""
        ))
    resp = await execute_command(
        websocket, {
            "method": "browsingContext.locateNodes",
            "params": {
                "context": context_id,
                "locator": {
                    "type": "css",
                    "value": "div"
                }
            }
        })

    assert resp == {
        "nodes": [
            {
                'sharedId': ANY_SHARED_ID,
                'type': 'node',
                'value': {
                    'attributes': {
                        'data-class': 'one',
                    },
                    'childNodeCount': 1,
                    'localName': 'div',
                    'namespaceURI': 'http://www.w3.org/1999/xhtml',
                    'nodeType': 1,
                    'shadowRoot': None,
                },
            },
            {
                'sharedId': ANY_SHARED_ID,
                'type': 'node',
                'value': {
                    'attributes': {
                        'data-class': 'two',
                    },
                    'childNodeCount': 1,
                    'localName': 'div',
                    'namespaceURI': 'http://www.w3.org/1999/xhtml',
                    'nodeType': 1,
                    'shadowRoot': None,
                },
            },
        ]
    }


@pytest.mark.asyncio
async def test_locate_nodes_css_locator_invalid(websocket, context_id, html):
    invalid_css_selector = 'a*b'
    with pytest.raises(Exception,
                       match=re.escape(
                           str({
                               'error': 'invalid selector',
                               'message': 'Not valid selector ' +
                                          invalid_css_selector
                           }))):
        await execute_command(
            websocket, {
                'method': 'browsingContext.locateNodes',
                'params': {
                    'context': context_id,
                    'locator': {
                        'type': 'css',
                        'value': invalid_css_selector
                    }
                }
            })
