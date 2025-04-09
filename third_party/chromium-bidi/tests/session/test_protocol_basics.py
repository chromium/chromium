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
from test_helpers import (AnyExtending, execute_command, json,
                          read_JSON_message, send_JSON_command)

# Tests for "handle an incoming message" error handling, when the message
# can't be decoded as known command.
# https://w3c.github.io/webdriver-bidi/#handle-an-incoming-message


@pytest.mark.asyncio
async def test_invalid_json(websocket):
    message = 'this is not json'
    await websocket.send(message)
    resp = await read_JSON_message(websocket)
    assert resp == {
        "type": "error",
        "error": "invalid argument",
        "message": ANY_STR
    }
    assert "unable to parse BiDi command" in resp['message']


@pytest.mark.asyncio
async def test_empty_object(websocket):
    command = {}
    await websocket.send(json.dumps(command))
    resp = await read_JSON_message(websocket)
    assert resp == {
        "type": "error",
        "error": "invalid argument",
        "message": ANY_STR
    }


@pytest.mark.asyncio
async def test_session_status(websocket):
    command = {
        "type": "success",
        "id": 5,
        "method": "session.status",
        "params": {}
    }
    await send_JSON_command(websocket, command)
    resp = await read_JSON_message(websocket)
    assert resp == AnyExtending({
        "id": 5,
        "type": "success",
        "result": {
            "ready": False,
            "message": "already connected"
        }
    })


@pytest.mark.asyncio
async def test_channel_non_empty_static_command(websocket):
    command_id = await send_JSON_command(websocket, {
        "method": "session.status",
        "params": {},
        "goog:channel": "SOME_CHANNEL"
    })
    resp = await read_JSON_message(websocket)

    if "build" in resp["result"]:
        # Heuristic to detect chromedriver.
        pytest.xfail(reason="TODO: http://b/343683918")

    assert resp == AnyExtending({
        "id": command_id,
        "goog:channel": "SOME_CHANNEL",
        "type": "success",
        "result": {
            "ready": False,
            "message": "already connected"
        }
    })


@pytest.mark.asyncio
async def test_channel_non_empty_not_static_command(websocket):
    command_id = await send_JSON_command(
        websocket, {
            "id": 2,
            "method": "browsingContext.getTree",
            "params": {},
            "goog:channel": "SOME_CHANNEL"
        })
    resp = await read_JSON_message(websocket)
    assert resp == AnyExtending({
        "id": command_id,
        "goog:channel": "SOME_CHANNEL",
        "type": "success",
        "result": {
            "contexts": [{}]
        }
    })


@pytest.mark.asyncio
async def test_channel_empty(websocket):
    await send_JSON_command(websocket, {
        "id": 7000,
        "method": "session.status",
        "params": {},
        "goog:channel": ""
    })
    resp = await read_JSON_message(websocket)
    assert resp == AnyExtending({
        "id": 7000,
        "type": "success",
        "result": {
            "ready": False,
            "message": "already connected"
        }
    })


@pytest.mark.asyncio
async def test_command_unknown(websocket):
    unknown_command = "some.unknown.command"
    with pytest.raises(Exception,
                       match=str({
                           'error': 'unknown command',
                           'message': f"Unknown command '{unknown_command}'."
                       })):
        await execute_command(websocket, {
            "method": unknown_command,
            "params": {}
        })
