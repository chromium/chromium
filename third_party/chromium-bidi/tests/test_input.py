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
from anys import ANY_STR
from test_helpers import execute_command, goto_url

SCRIPT = """
<div style="height: 2000px; width: 10px"></div>
<script>
    var allEvents = [];
    for (const name of [
        "mousedown",
        "mousemove",
        "mouseup",
        "keydown",
        "keypress",
        "keyup",
        "wheel"
    ]) {
        window.addEventListener(name, (event) => {
            switch (name) {
                case "mousedown":
                case "mousemove":
                case "mouseup":
                    allEvents.push({
                        event: name,
                        button: event.button,
                        buttons: event.buttons,
                        clientX: event.clientX,
                        clientY: event.clientY,
                    });
                    break;
                case "keydown":
                case "keypress":
                case "keyup":
                    allEvents.push({
                        event: name,
                        key: event.key,
                        code: event.code,
                        charCode: event.charCode,
                        keyCode: event.keyCode,
                    });
                    break;
                case "wheel":
                    allEvents.push({
                        event: name,
                        deltaX: event.deltaX,
                        deltaY: event.deltaY,
                        deltaZ: event.deltaZ,
                    });
                    break;
            }
        });
    }
</script>
"""


@pytest.mark.asyncio
async def test_input_performActionsEmitsKeyboardEvents(websocket, context_id,
                                                       html):
    await goto_url(websocket, context_id, html(SCRIPT))

    await execute_command(
        websocket, {
            "method": "input.performActions",
            "params": {
                "context": context_id,
                "actions": [{
                    "type": "key",
                    "id": "main",
                    "actions": [{
                        "type": "keyDown",
                        "value": "a",
                    }, {
                        "type": "keyUp",
                        "value": "a",
                    }]
                }]
            }
        })

    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "allEvents",
                "awaitPromise": False,
                "resultOwnership": "root",
                "target": {
                    "context": context_id
                }
            }
        })

    assert {
        'type': 'success',
        'result': {
            'type': 'array',
            'value': [{
                'type': 'object',
                'value': [['event', {
                    'type': 'string',
                    'value': 'keydown'
                }], ['key', {
                    'type': 'string',
                    'value': 'a'
                }], ['code', {
                    'type': 'string',
                    'value': 'KeyA'
                }], ['charCode', {
                    'type': 'number',
                    'value': 0
                }], ['keyCode', {
                    'type': 'number',
                    'value': 65
                }]]
            }, {
                'type': 'object',
                'value': [['event', {
                    'type': 'string',
                    'value': 'keypress'
                }], ['key', {
                    'type': 'string',
                    'value': 'a'
                }], ['code', {
                    'type': 'string',
                    'value': 'KeyA'
                }], ['charCode', {
                    'type': 'number',
                    'value': 97
                }], ['keyCode', {
                    'type': 'number',
                    'value': 97
                }]]
            }, {
                'type': 'object',
                'value': [['event', {
                    'type': 'string',
                    'value': 'keyup'
                }], ['key', {
                    'type': 'string',
                    'value': 'a'
                }], ['code', {
                    'type': 'string',
                    'value': 'KeyA'
                }], ['charCode', {
                    'type': 'number',
                    'value': 0
                }], ['keyCode', {
                    'type': 'number',
                    'value': 65
                }]]
            }],
            'handle': ANY_STR
        },
        'realm': ANY_STR
    } == result


@pytest.mark.asyncio
async def test_input_performActionsEmitsPointerEvents(websocket, context_id,
                                                      html):
    await goto_url(websocket, context_id, html(SCRIPT))

    await execute_command(
        websocket, {
            "method": "input.performActions",
            "params": {
                "context": context_id,
                "actions": [{
                    "type": "pointer",
                    "id": "main",
                    "actions": [{
                        "type": "pointerDown",
                        "button": 0,
                    }, {
                        "type": "pointerMove",
                        "x": 1,
                        "y": 1,
                    }, {
                        "type": "pointerUp",
                        "button": 0,
                    }]
                }]
            }
        })

    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "allEvents",
                "awaitPromise": False,
                "resultOwnership": "root",
                "target": {
                    "context": context_id
                }
            }
        })

    assert {
        'type': 'success',
        'result': {
            'type': 'array',
            'value': [{
                'type': 'object',
                'value': [['event', {
                    'type': 'string',
                    'value': 'mousedown'
                }], ['button', {
                    'type': 'number',
                    'value': 0
                }], ['buttons', {
                    'type': 'number',
                    'value': 1
                }], ['clientX', {
                    'type': 'number',
                    'value': 0
                }], ['clientY', {
                    'type': 'number',
                    'value': 0
                }]]
            }, {
                'type': 'object',
                'value': [['event', {
                    'type': 'string',
                    'value': 'mousemove'
                }], ['button', {
                    'type': 'number',
                    'value': 0
                }], ['buttons', {
                    'type': 'number',
                    'value': 1
                }], ['clientX', {
                    'type': 'number',
                    'value': 1
                }], ['clientY', {
                    'type': 'number',
                    'value': 1
                }]]
            }, {
                'type': 'object',
                'value': [['event', {
                    'type': 'string',
                    'value': 'mouseup'
                }], ['button', {
                    'type': 'number',
                    'value': 0
                }], ['buttons', {
                    'type': 'number',
                    'value': 0
                }], ['clientX', {
                    'type': 'number',
                    'value': 1
                }], ['clientY', {
                    'type': 'number',
                    'value': 1
                }]]
            }],
            'handle': ANY_STR
        },
        'realm': ANY_STR
    } == result


@pytest.mark.skip(reason="""
        TODO(jrandolf): Investigate mouse wheel flakiness. The deltaY sometimes
        doubles and other times not. It is also not independent of the mouse.
    """)
@pytest.mark.asyncio
async def test_input_performActionsEmitsWheelEvents(websocket, context_id,
                                                    html):
    await goto_url(websocket, context_id, html(SCRIPT))

    await execute_command(
        websocket, {
            "method": "input.performActions",
            "params": {
                "context": context_id,
                "actions": [{
                    "type": "pointer",
                    "id": "main2",
                    "actions": [{
                        "type": "pointerDown",
                        "button": 0,
                    }, {
                        "type": "pointerUp",
                        "button": 0,
                    }]
                }, {
                    "type": "wheel",
                    "id": "main",
                    "actions": [{
                        "type": "scroll",
                        "x": 0,
                        "y": 0,
                        "deltaX": 0,
                        "deltaY": 5,
                    }]
                }]
            }
        })

    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "allEvents",
                "awaitPromise": False,
                "resultOwnership": "root",
                "target": {
                    "context": context_id
                }
            }
        })

    assert {
        'type': 'success',
        'result': {
            'type': 'array',
            'value': [{
                'type': 'object',
                'value': [['event', {
                    'type': 'string',
                    'value': 'mousedown'
                }], ['button', {
                    'type': 'number',
                    'value': 0
                }], ['buttons', {
                    'type': 'number',
                    'value': 1
                }], ['clientX', {
                    'type': 'number',
                    'value': 0
                }], ['clientY', {
                    'type': 'number',
                    'value': 0
                }]]
            }, {
                'type': 'object',
                'value': [['event', {
                    'type': 'string',
                    'value': 'wheel'
                }], ['deltaX', {
                    'type': 'number',
                    'value': 0
                }], ['deltaY', {
                    'type': 'number',
                    'value': 10
                }], ['deltaZ', {
                    'type': 'number',
                    'value': 0
                }]]
            }, {
                'type': 'object',
                'value': [['event', {
                    'type': 'string',
                    'value': 'mouseup'
                }], ['button', {
                    'type': 'number',
                    'value': 0
                }], ['buttons', {
                    'type': 'number',
                    'value': 0
                }], ['clientX', {
                    'type': 'number',
                    'value': 0
                }], ['clientY', {
                    'type': 'number',
                    'value': 0
                }]]
            }],
            'handle': ANY_STR
        },
        'realm': ANY_STR
    } == result
