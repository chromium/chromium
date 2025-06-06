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
from syrupy.filters import props
from test_helpers import (AnyExtending, execute_command, goto_url,
                          read_JSON_message, send_JSON_command, subscribe,
                          wait_for_event)

SET_FILES_HTML = """
<input id=input type=file>
<script>
    var allEvents = [];
    const input = document.getElementById('input');
    input.addEventListener('change', (event) => {
        allEvents.push({
            type: event.type,
            files: [...event.target.files].map((file) => file.name)
        })
    });
    input.addEventListener('cancel', (event) => {
        allEvents.push({
            type: event.type,
            files: [...event.target.files].map((file) => file.name)
        })
    });
</script>
"""

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
                        clickCount: event.detail,
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

DBL_CLICK_SCRIPT = """
<div style="height: 2000px; width: 10px"></div>
<script>
    var allEvents = [];
    window.addEventListener("dblclick", (event) => {
        allEvents.push({
          event: "dblclick",
          button: event.button,
          buttons: event.buttons,
          clickCount: event.detail,
      });
    });
</script>
"""

DRAG_SCRIPT = """
<div
  style="height: 100px; width: 100px; background-color: red"
  id="drag-target"
  draggable="true"
></div>
<div
  style="height: 100px; width: 100px; background-color: blue"
  id="drop-target"
  ondragenter="event.preventDefault()"
  ondragover="event.preventDefault()"
></div>
<script>
  var allEvents = [];
  for (const name of [
      "mousedown",
      "mousemove",
      "dragstart",
      "dragend",
      "dragover",
      "dragleave",
      "dragenter",
      "drop"
  ]) {
      window.addEventListener(name, (event) => {
          if (event instanceof MouseEvent) {
            allEvents.push({
                event: name,
                button: event.button,
                buttons: event.buttons,
                clientX: event.clientX,
                clientY: event.clientY,
            });
          }
      }, true);
  }
</script>
"""


async def reset_mouse(websocket, context_id):
    """Ensures the mouse is at the origin and events are cleared. This is helpful
    for headful which has problems due to the hardware mouse being within the
    test window"""
    await execute_command(
        websocket, {
            "method": "input.performActions",
            "params": {
                "context": context_id,
                "actions": [{
                    "type": "pointer",
                    "id": "main_mouse",
                    "actions": [{
                        "type": "pointerMove",
                        "x": 0,
                        "y": 0,
                    }]
                }]
            }
        })
    await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "allEvents = []",
                "awaitPromise": False,
                "target": {
                    "context": context_id
                }
            }
        })


def get_events(websocket, context_id):
    return execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "allEvents",
                "awaitPromise": False,
                "target": {
                    "context": context_id
                }
            }
        })


@pytest.mark.asyncio
async def test_input_performActionsEmitsKeyboardEvents(websocket, context_id,
                                                       html, activate_main_tab,
                                                       snapshot):
    await goto_url(websocket, context_id, html(SCRIPT))
    await activate_main_tab()
    await reset_mouse(websocket, context_id)

    await execute_command(
        websocket, {
            "method": "input.performActions",
            "params": {
                "context": context_id,
                "actions": [{
                    "type": "key",
                    "id": "main_keyboard",
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

    result = await get_events(websocket, context_id)

    assert result == snapshot(exclude=props("realm"))


@pytest.mark.asyncio
async def test_input_performActionsEmitsDblClicks(websocket, context_id, html,
                                                  activate_main_tab,
                                                  query_selector, snapshot):
    await goto_url(websocket, context_id, html(DBL_CLICK_SCRIPT))
    await activate_main_tab()
    await reset_mouse(websocket, context_id)

    target_element = await query_selector('div')

    await execute_command(
        websocket, {
            "method": "input.performActions",
            "params": {
                "context": context_id,
                "actions": [{
                    "type": "pointer",
                    "id": "__puppeteer_mouse",
                    "actions": [{
                        "type": "pointerMove",
                        "x": 0,
                        "y": 0,
                        "origin": {
                            "type": "element",
                            "element": target_element
                        }
                    }, {
                        "type": "pointerDown",
                        "button": 0
                    }, {
                        "type": "pointerUp",
                        "button": 0
                    }, {
                        "type": "pointerDown",
                        "button": 0
                    }, {
                        "type": "pointerUp",
                        "button": 0
                    }]
                }]
            }
        })

    await execute_command(
        websocket, {
            "method": "input.performActions",
            "params": {
                "context": context_id,
                "actions": [{
                    "type": "pointer",
                    "id": "__puppeteer_mouse",
                    "actions": [{
                        "type": "pointerMove",
                        "x": 0,
                        "y": 0,
                        "origin": {
                            "type": "element",
                            "element": target_element
                        }
                    }, {
                        "type": "pointerDown",
                        "button": 0
                    }, {
                        "type": "pointerUp",
                        "button": 0
                    }, {
                        "type": "pointerDown",
                        "button": 0
                    }, {
                        "type": "pointerUp",
                        "button": 0
                    }]
                }]
            }
        })

    result = await get_events(websocket, context_id)

    assert result == snapshot(exclude=props("realm"))


@pytest.mark.asyncio
async def test_input_performActionsEmitsDragging(websocket, context_id, html,
                                                 query_selector, snapshot,
                                                 activate_main_tab):
    await goto_url(websocket, context_id, html(DRAG_SCRIPT))
    await activate_main_tab()
    await reset_mouse(websocket, context_id)

    drag_target = await query_selector('#drag-target')
    drop_target = await query_selector('#drop-target')

    await execute_command(
        websocket, {
            "method": "input.performActions",
            "params": {
                "context": context_id,
                "actions": [{
                    "type": "pointer",
                    "id": "main_mouse",
                    "actions": [{
                        "type": "pointerMove",
                        "x": 0,
                        "y": 0,
                        "origin": {
                            "type": "element",
                            "element": drag_target
                        }
                    }, {
                        "type": "pointerDown",
                        "button": 0,
                    }, {
                        "type": "pointerMove",
                        "x": 0,
                        "y": 0,
                        "origin": {
                            "type": "element",
                            "element": drop_target
                        }
                    }, {
                        "type": "pointerUp",
                        "button": 0,
                    }, {
                        "type": "pointerMove",
                        "x": 1,
                        "y": 1,
                        "origin": {
                            "type": "element",
                            "element": drop_target
                        }
                    }]
                }]
            }
        })

    result = await get_events(websocket, context_id)

    assert result == snapshot(exclude=props("realm"))


@pytest.mark.asyncio
async def test_input_performActionsCancelsDragging(websocket, context_id, html,
                                                   query_selector, snapshot,
                                                   activate_main_tab):
    await goto_url(websocket, context_id, html(DRAG_SCRIPT))
    await activate_main_tab()
    await reset_mouse(websocket, context_id)

    drag_target = await query_selector('#drag-target')
    drop_target = await query_selector('#drop-target')

    await execute_command(
        websocket, {
            "method": "input.performActions",
            "params": {
                "context": context_id,
                "actions": [{
                    "type": "pointer",
                    "id": "main_mouse",
                    "actions": [{
                        "type": "pointerMove",
                        "x": 0,
                        "y": 0,
                        "origin": {
                            "type": "element",
                            "element": drag_target
                        }
                    }, {
                        "type": "pointerDown",
                        "button": 0,
                    }, {
                        "type": "pointerMove",
                        "x": 0,
                        "y": 0,
                        "origin": {
                            "type": "element",
                            "element": drop_target
                        }
                    }]
                }]
            }
        })

    await execute_command(
        websocket,
        {
            "method": "input.performActions",
            "params": {
                "context": context_id,
                "actions": [{
                    "type": "key",
                    "id": "main_keyboard",
                    "actions": [{
                        "type": "keyDown",
                        # Pressing Escape
                        "value": "\uE00C",
                    }]
                }]
            }
        })

    await execute_command(
        websocket, {
            "method": "input.performActions",
            "params": {
                "context": context_id,
                "actions": [{
                    "type": "pointer",
                    "id": "main_mouse",
                    "actions": [{
                        "type": "pointerMove",
                        "x": 1,
                        "y": 1,
                        "origin": {
                            "type": "element",
                            "element": drop_target
                        }
                    }]
                }]
            }
        })

    result = await get_events(websocket, context_id)

    assert result == snapshot(exclude=props("realm"))


@pytest.mark.asyncio
async def test_input_performActionsDoesNotCancelDraggingWithAlt(
        websocket, context_id, html, query_selector, snapshot,
        activate_main_tab):
    await goto_url(websocket, context_id, html(DRAG_SCRIPT))
    await activate_main_tab()
    await reset_mouse(websocket, context_id)

    drag_target = await query_selector('#drag-target')
    drop_target = await query_selector('#drop-target')

    await execute_command(
        websocket, {
            "method": "input.performActions",
            "params": {
                "context": context_id,
                "actions": [{
                    "type": "pointer",
                    "id": "main_mouse",
                    "actions": [{
                        "type": "pointerMove",
                        "x": 0,
                        "y": 0,
                        "origin": {
                            "type": "element",
                            "element": drag_target
                        }
                    }, {
                        "type": "pointerDown",
                        "button": 0,
                    }, {
                        "type": "pointerMove",
                        "x": 0,
                        "y": 0,
                        "origin": {
                            "type": "element",
                            "element": drop_target
                        }
                    }]
                }]
            }
        })

    await execute_command(
        websocket,
        {
            "method": "input.performActions",
            "params": {
                "context": context_id,
                "actions": [{
                    "type": "key",
                    "id": "main_keyboard",
                    "actions": [
                        {
                            "type": "keyDown",
                            # Pressing Alt
                            "value": "\uE00A",
                        },
                        {
                            "type": "keyDown",
                            # Pressing Escape
                            "value": "\uE00C",
                        }
                    ]
                }]
            }
        })

    await execute_command(
        websocket, {
            "method": "input.performActions",
            "params": {
                "context": context_id,
                "actions": [{
                    "type": "pointer",
                    "id": "main_mouse",
                    "actions": [{
                        "type": "pointerMove",
                        "x": 1,
                        "y": 1,
                        "origin": {
                            "type": "element",
                            "element": drop_target
                        }
                    }]
                }]
            }
        })

    result = await get_events(websocket, context_id)

    assert result == snapshot(exclude=props("realm"))


@pytest.mark.asyncio
async def test_input_performActionsEmitsPointerEvents(websocket, context_id,
                                                      html, activate_main_tab,
                                                      snapshot):
    await goto_url(websocket, context_id, html(SCRIPT))
    await activate_main_tab()
    await reset_mouse(websocket, context_id)

    await execute_command(
        websocket, {
            "method": "input.performActions",
            "params": {
                "context": context_id,
                "actions": [{
                    "type": "pointer",
                    "id": "main_mouse",
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

    result = await get_events(websocket, context_id)

    assert result == snapshot(exclude=props("realm"))


@pytest.mark.asyncio
async def test_input_performActionsEmitsWheelEvents(websocket, context_id,
                                                    html, activate_main_tab,
                                                    snapshot):
    await goto_url(websocket, context_id, html(SCRIPT))
    await activate_main_tab()
    await reset_mouse(websocket, context_id)

    await execute_command(
        websocket, {
            "method": "input.performActions",
            "params": {
                "context": context_id,
                "actions": [{
                    "type": "pointer",
                    "id": "main_mouse",
                    "actions": [{
                        "type": "pointerDown",
                        "button": 0,
                    }, {
                        "type": "pointerUp",
                        "button": 0,
                    }]
                }, {
                    "type": "wheel",
                    "id": "main_wheel",
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

    result = await get_events(websocket, context_id)

    assert result == snapshot(exclude=props("realm"))


@pytest.mark.parametrize("same_origin", [True, False])
@pytest.mark.asyncio
async def test_click_iframe_context(websocket, context_id, html, same_origin,
                                    read_messages):
    # TODO: add test for double-nested iframes.
    await subscribe(websocket, ["log.entryAdded"])

    iframe_url = html(content="""
        <script>
            document.addEventListener('mousedown', function(event) {
                document.body.textContent = `X: ${event.clientX}, Y: ${event.clientY}`;
                console.log("mousedown event", event.clientX, event.clientY)
            });
            console.log("iframe loaded")
        </script>
        """,
                      same_origin=same_origin)

    # Parent page with an iFrame with margins and borders.
    main_page_url = html(content=f"""
        <iframe style="border: 10px solid red; margin-top: 20px; margin-left: 30px" src="{iframe_url}" />
    """)

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": main_page_url,
                "context": context_id,
                "wait": "none"
            }
        })

    # Wait for the iframe to load. It cannot be guaranteed by the "wait"
    # condition.
    [_, frame_loaded_console_event] = await read_messages(2, sort=True)
    assert frame_loaded_console_event == AnyExtending({
        "method": "log.entryAdded",
        "params": {
            'args': [{
                'type': 'string',
                'value': 'iframe loaded',
            }, ],
        }
    })
    iframe_id = frame_loaded_console_event["params"]["source"]["context"]

    (X, Y) = (7, 13)
    # Perform action with iframe as origin.
    await send_JSON_command(
        websocket, {
            "method": "input.performActions",
            "params": {
                "context": iframe_id,
                "actions": [{
                    "type": "pointer",
                    "id": "main_mouse",
                    "actions": [{
                        "origin": "pointer",
                        "type": "pointerMove",
                        "x": X,
                        "y": Y,
                    }, {
                        "type": "pointerDown",
                        "button": 0,
                    }, {
                        "type": "pointerUp",
                        "button": 0,
                    }, {
                        "type": "pointerDown",
                        "button": 1,
                    }, {
                        "type": "pointerUp",
                        "button": 1,
                    }]
                }]
            }
        })

    await wait_for_event(websocket, "log.entryAdded")

    [mousedown_console_event] = await read_messages(1, sort=True)
    assert mousedown_console_event == AnyExtending({
        "method": "log.entryAdded",
        "params": {
            'args': [
                {
                    'value': 'mousedown event',
                },
                {
                    'value': X,
                },
                {
                    'value': Y,
                },
            ],
        }
    })


@pytest.mark.asyncio
async def test_input_performActionsEmitsClickCountsByButton(
        websocket, context_id, html, activate_main_tab, snapshot):
    await goto_url(websocket, context_id, html(SCRIPT))
    await activate_main_tab()
    await reset_mouse(websocket, context_id)

    await execute_command(
        websocket, {
            "method": "input.performActions",
            "params": {
                "context": context_id,
                "actions": [{
                    "type": "pointer",
                    "id": "main_mouse",
                    "actions": [{
                        "type": "pointerDown",
                        "button": 0,
                    }, {
                        "type": "pointerUp",
                        "button": 0,
                    }, {
                        "type": "pointerDown",
                        "button": 0,
                    }, {
                        "type": "pointerUp",
                        "button": 0,
                    }, {
                        "type": "pointerDown",
                        "button": 1,
                    }, {
                        "type": "pointerUp",
                        "button": 1,
                    }]
                }]
            }
        })

    result = await get_events(websocket, context_id)

    assert result == snapshot(exclude=props("realm"))


@pytest.mark.asyncio
async def test_input_setFiles(websocket, context_id, html, snapshot,
                              query_selector):
    await goto_url(websocket, context_id, html(SET_FILES_HTML))

    input = await query_selector('#input')

    await execute_command(
        websocket, {
            "method": "input.setFiles",
            "params": {
                "context": context_id,
                "element": input,
                "files": ["path/to/noop.txt"],
            }
        })

    result = await get_events(websocket, context_id)

    assert result == snapshot(exclude=props("realm"))


@pytest.mark.asyncio
async def test_input_setFiles_twice(websocket, context_id, html, snapshot,
                                    query_selector):
    await goto_url(websocket, context_id, html(SET_FILES_HTML))

    input = await query_selector('#input')

    await execute_command(
        websocket, {
            "method": "input.setFiles",
            "params": {
                "context": context_id,
                "element": input,
                "files": ["path/to/noop.txt"],
            }
        })

    result = await get_events(websocket, context_id)

    assert result == snapshot(exclude=props("realm"))

    await execute_command(
        websocket, {
            "method": "input.setFiles",
            "params": {
                "context": context_id,
                "element": input,
                "files": ["path/to/noop-2.txt"],
            }
        })

    result = await get_events(websocket, context_id)

    assert result == snapshot(exclude=props("realm"))


@pytest.mark.asyncio
async def test_input_setFiles_twice_same(websocket, context_id, html, snapshot,
                                         query_selector):
    await goto_url(websocket, context_id, html(SET_FILES_HTML))

    input = await query_selector('#input')

    await execute_command(
        websocket, {
            "method": "input.setFiles",
            "params": {
                "context": context_id,
                "element": input,
                "files": ["path/to/noop.txt"],
            }
        })

    result = await get_events(websocket, context_id)

    assert result == snapshot(exclude=props("realm"))

    await execute_command(
        websocket, {
            "method": "input.setFiles",
            "params": {
                "context": context_id,
                "element": input,
                "files": ["path/to/noop.txt"],
            }
        })

    result = await get_events(websocket, context_id)

    assert result == snapshot(exclude=props("realm"))


@pytest.mark.asyncio
async def test_input_setFiles_noSuchElement(websocket, context_id, html,
                                            snapshot):
    await goto_url(websocket, context_id, html(SET_FILES_HTML))

    message = None
    try:
        await execute_command(
            websocket, {
                "method": "input.setFiles",
                "params": {
                    "context": context_id,
                    "element": {
                        "sharedId": "invalid"
                    },
                    "files": ["path/to/noop.txt"],
                }
            })
    except Exception as exception:
        message = exception.args[0]['error']

    assert message == snapshot


@pytest.mark.asyncio
async def test_input_setFiles_unableToSetFileInput(websocket, context_id, html,
                                                   snapshot, query_selector):
    await goto_url(websocket, context_id,
                   html(SET_FILES_HTML.replace('<input', '<input disabled')))

    input = await query_selector('#input')

    message = None
    try:
        await execute_command(
            websocket, {
                "method": "input.setFiles",
                "params": {
                    "context": context_id,
                    "element": input,
                    "files": ["path/to/noop.txt"],
                }
            })
    except Exception as exception:
        message = exception.args[0]['error']

    assert message == snapshot


@pytest.mark.asyncio
async def test_input_keyDown_closes_browsing_context(websocket, context_id,
                                                     html):
    url = html("""
        <input onkeydown="window.close()">close</input>
        <script>document.querySelector("input").focus();</script>
        """)

    await subscribe(websocket, ["browsingContext.load"])
    on_load = wait_for_event(websocket, "browsingContext.load")

    # Open new window via script. Required for script to be able to close it.
    resp = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": f"window.open('{url}')",
                "awaitPromise": False,
                "target": {
                    "context": context_id
                }
            }
        })

    # Wait for the new context to load.
    await on_load

    new_context = resp['result']['value']['context']

    command_id = await send_JSON_command(
        websocket, {
            "method": "input.performActions",
            "params": {
                "context": new_context,
                "actions": [{
                    "type": "key",
                    "id": "main_keyboard",
                    "actions": [{
                        "type": "keyDown",
                        "value": "a",
                    }, {
                        "type": "pause",
                        "duration": 250,
                    }, {
                        "type": "keyUp",
                        "value": "a",
                    }]
                }]
            }
        })

    resp = await read_JSON_message(websocket)
    assert resp == {
        'error': 'no such frame',
        'id': command_id,
        'message': ANY_STR,
        'type': 'error',
    }
