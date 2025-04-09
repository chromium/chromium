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
import websockets
from anys import ANY_DICT, ANY_STR
from test_helpers import (AnyExtending, execute_command, get_tree, goto_url,
                          read_JSON_message, send_JSON_command, subscribe)


@pytest.mark.asyncio
async def test_browsingContext_close_last_command(websocket, context_id):
    await subscribe(websocket, ["browsingContext.contextDestroyed"])

    command_id = await send_JSON_command(websocket, {
        "method": "browsingContext.close",
        "params": {
            "context": context_id
        }
    })

    # Assert "browsingContext.contextCreated" event emitted.
    resp = await read_JSON_message(websocket)
    assert resp == AnyExtending({
        'type': 'event',
        "method": "browsingContext.contextDestroyed",
        "params": {
            "context": context_id,
            "parent": None,
            "url": "about:blank",
            "children": [],
            "userContext": "default"
        }
    })

    resp = await read_JSON_message(websocket)
    assert resp == {"type": "success", "id": command_id, "result": {}}

    try:
        result = await get_tree(websocket)
        assert result['contexts'] == []
    except websockets.exceptions.ConnectionClosedError:
        # Chromedriver closes the connection after the last context (not
        # counting the mapper tab) is closed. NodeJS runner does not.
        pass


@pytest.mark.asyncio
async def test_browsingContext_close_not_last_command(websocket, context_id,
                                                      another_context_id):
    await subscribe(websocket, ["browsingContext.contextDestroyed"])

    command_id = await send_JSON_command(
        websocket, {
            "method": "browsingContext.close",
            "params": {
                "context": another_context_id
            }
        })

    # Assert "browsingContext.contextCreated" event emitted.
    resp = await read_JSON_message(websocket)
    assert resp == {
        'type': 'event',
        "method": "browsingContext.contextDestroyed",
        "params": {
            "context": another_context_id,
            "parent": None,
            "url": "about:blank",
            "children": [],
            "clientWindow": ANY_STR,
            'originalOpener': None,
            "userContext": "default"
        }
    }

    resp = await read_JSON_message(websocket)
    assert resp == {"type": "success", "id": command_id, "result": {}}

    result = await get_tree(websocket)

    # Assert only one context is left.
    assert result == AnyExtending({'contexts': [{
        "context": context_id,
    }]})


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'unhandledPromptBehavior': {
        'beforeUnload': 'ignore'
    }
}],
                         indirect=True)
@pytest.mark.parametrize("accept", [True, False])
async def test_browsingContext_close_prompt(websocket, context_id, html,
                                            accept):
    await subscribe(websocket, [
        "browsingContext.userPromptOpened", "browsingContext.contextDestroyed"
    ])

    url = html("""
        <script>
            window.addEventListener('beforeunload', event => {
                event.returnValue = 'Leave?';
                event.preventDefault();
            });
        </script>
        """)

    await goto_url(websocket, context_id, url)

    # We need to interact with the page to trigger "beforeunload"
    await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "document.body.click()",
                "target": {
                    "context": context_id
                },
                "awaitPromise": False,
                "userActivation": True
            }
        })

    close_command_id = await send_JSON_command(
        websocket, {
            "method": "browsingContext.close",
            "params": {
                "context": context_id,
                "promptUnload": True
            }
        })

    # Assert "browsingContext.userPromptOpened" event emitted.
    resp = await read_JSON_message(websocket)
    assert resp == {
        'type': 'event',
        "method": "browsingContext.userPromptOpened",
        "params": {
            "context": context_id,
            "message": "",
            "type": "beforeunload",
            "handler": "ignore",
        }
    }

    handle_command_id = await send_JSON_command(
        websocket, {
            "method": "browsingContext.handleUserPrompt",
            "params": {
                "context": context_id,
                "accept": accept
            }
        })

    if accept:
        # The expected order of the events:
        # 1. Handle command response.
        # 2. ContextDestroyed event.
        # 3. Close command response.
        resp = await read_JSON_message(websocket)
        assert resp == {
            "type": "success",
            "id": handle_command_id,
            "result": {}
        }

        resp = await read_JSON_message(websocket)
        assert resp == {
            'method': 'browsingContext.contextDestroyed',
            'params': {
                'context': context_id,
                'children': [],
                'originalOpener': None,
                'clientWindow': ANY_STR,
                'parent': None,
                # Url-encoded `url`.
                'url': ANY_STR,
                'userContext': 'default',
            },
            'type': 'event',
        }

        resp = await read_JSON_message(websocket)
        assert resp == {
            "type": "success",
            "id": close_command_id,
            "result": {}
        }

    else:
        # Only handle command response is expected.
        # TODO: should the close command response with an error?
        resp = await read_JSON_message(websocket)
        assert resp == {
            "type": "success",
            "id": handle_command_id,
            "result": {}
        }


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'unhandledPromptBehavior': {
        'beforeUnload': 'ignore'
    }
}],
                         indirect=True)
@pytest.mark.parametrize("accept", [True, False])
async def test_browsingContext_navigate_prompt(websocket, context_id, html,
                                               accept):
    await subscribe(websocket, ["browsingContext.userPromptOpened"])

    url = html("""
        <script>
            window.addEventListener('beforeunload', event => {
                event.returnValue = 'Leave?';
                event.preventDefault();
            });
        </script>
        """)

    await goto_url(websocket, context_id, url)

    # We need to interact with the page to trigger "beforeunload"
    await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "document.body.click()",
                "target": {
                    "context": context_id
                },
                "awaitPromise": False,
                "userActivation": True
            }
        })

    navigate_command_id = await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url,
                "context": context_id,
                "wait": "complete"
            }
        })

    # Assert "browsingContext.userPromptOpened" event emitted.
    resp = await read_JSON_message(websocket)
    assert resp == {
        'type': 'event',
        "method": "browsingContext.userPromptOpened",
        "params": {
            "context": context_id,
            "message": "",
            "type": "beforeunload",
            "handler": "ignore",
        }
    }

    handle_command_id = await send_JSON_command(
        websocket, {
            "method": "browsingContext.handleUserPrompt",
            "params": {
                "context": context_id,
                "accept": accept
            }
        })

    if accept:
        resp = await read_JSON_message(websocket)
        assert resp == {
            "type": "success",
            "id": handle_command_id,
            "result": {}
        }

        resp = await read_JSON_message(websocket)
        assert resp == {
            "type": "success",
            "id": navigate_command_id,
            "result": ANY_DICT
        }
    else:
        # Navigation expected to fail.
        resp = await read_JSON_message(websocket)
        assert resp == AnyExtending({
            'error': 'unknown error',
            'id': navigate_command_id,
            'message': 'net::ERR_ABORTED',
            'stacktrace': ANY_STR,
            'type': 'error',
        })
        # Navigation expected to fail.
        resp = await read_JSON_message(websocket)
        assert resp == {
            "type": "success",
            "id": handle_command_id,
            "result": {}
        }
