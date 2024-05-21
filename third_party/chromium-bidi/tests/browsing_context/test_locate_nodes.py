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


@pytest.mark.parametrize('locator', [
    {
        'type': 'innerText',
        'value': 'foobarBARbaz'
    },
    {
        'type': 'css',
        'value': 'div'
    },
    {
        'type': 'xpath',
        'value': '//div'
    },
    {
        'type': 'accessibility',
        'value': {
            'role': 'button',
            'name': 'test'
        }
    },
])
@pytest.mark.parametrize('start_node_expression',
                         [None, 'document', 'document.body'])
@pytest.mark.asyncio
async def test_locate_nodes_locator_found(websocket, context_id, html, locator,
                                          start_node_expression):
    await goto_url(
        websocket, context_id,
        html(
            '<div data-class="one" aria-label="test" role="button">foobarBARbaz</div><div data-class="two" aria-label="test" role="button">foobarBAR<span>baz</span></div>'
        ))

    start_nodes = None
    if start_node_expression:
        resp = await execute_command(
            websocket, {
                'method': 'script.evaluate',
                'params': {
                    'expression': start_node_expression,
                    'target': {
                        'context': context_id
                    },
                    'awaitPromise': False
                }
            })
        start_nodes = [{'sharedId': resp['result']['sharedId']}]

    resp = await execute_command(
        websocket, {
            'method': 'browsingContext.locateNodes',
            'params': {
                'context': context_id,
                'locator': locator,
                **({
                    'startNodes': start_nodes
                } if start_nodes else {})
            }
        })

    assert resp == {
        'nodes': [
            {
                'sharedId': ANY_SHARED_ID,
                'type': 'node',
                'value': {
                    'attributes': {
                        'data-class': 'one',
                        'aria-label': 'test',
                        'role': 'button',
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
                        'aria-label': 'test',
                        'role': 'button',
                    },
                    'childNodeCount': 2,
                    'localName': 'div',
                    'namespaceURI': 'http://www.w3.org/1999/xhtml',
                    'nodeType': 1,
                    'shadowRoot': None,
                },
            },
        ]
    }


@pytest.mark.parametrize('locator', [
    {
        'type': 'css',
        'value': 'a*b'
    },
    {
        'type': 'xpath',
        'value': ''
    },
])
@pytest.mark.asyncio
async def test_locate_nodes_locator_invalid(websocket, context_id, html,
                                            locator):
    with pytest.raises(Exception,
                       match=re.escape(
                           str({
                               'error': 'invalid selector',
                               'message': 'Not valid selector ' +
                                          locator['value']
                           }))):
        await execute_command(
            websocket, {
                'method': 'browsingContext.locateNodes',
                'params': {
                    'context': context_id,
                    'locator': locator
                }
            })
