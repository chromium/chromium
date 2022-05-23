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

from _helpers import *


# Tests for "handle an incoming message" error handling, when the message
# can't be decoded as known command.
# https://w3c.github.io/webdriver-bidi/#handle-an-incoming-message

@pytest.mark.asyncio
async def test_binary(websocket):
    # session.status is used in this test, but any simple command without side
    # effects would work. It is first sent as text, which should work, and then
    # sent again as binary, which should get an error response instead.
    command = {"id": 1, "method": "session.status", "params": {}}

    text_msg = json.dumps(command)
    await websocket.send(text_msg)
    resp = await read_JSON_message(websocket)
    assert resp['id'] == 1

    binary_msg = 'text_msg'.encode('utf-8')
    await websocket.send(binary_msg)
    resp = await read_JSON_message(websocket)
    assert resp == {
        "error": "invalid argument",
        "message": "not supported type (binary)"}


@pytest.mark.asyncio
async def test_invalid_json(websocket):
    message = 'this is not json'
    await websocket.send(message)
    resp = await read_JSON_message(websocket)
    assert resp == {
        "error": "invalid argument",
        "message": "Cannot parse data as JSON"}


@pytest.mark.asyncio
async def test_empty_object(websocket):
    command = {}
    await websocket.send(json.dumps(command))
    resp = await read_JSON_message(websocket)
    assert resp == {
        "error": "invalid argument",
        "message": "Expected unsigned integer but got undefined"}


@pytest.mark.asyncio
async def test_session_status(websocket):
    command = {"id": 5, "method": "session.status", "params": {}}
    await send_JSON_command(websocket, command)
    resp = await read_JSON_message(websocket)
    assert resp == {"id": 5, "result": {"ready": True, "message": "ready"}}


@pytest.mark.asyncio
async def test_sessionSubscribeWithoutContext_subscribesToEventsInAllContexts(
      websocket, context_id):
    result = await execute_command(websocket, {
        "method": "session.subscribe",
        "params": {
            "events": [
                "browsingContext.load"]}})
    assert result == {}

    # Navigate to some page.
    await send_JSON_command(websocket, {
        "id": get_next_command_id(),
        "method": "browsingContext.navigate",
        "params": {
            "url": "data:text/html,<h2>test</h2>",
            "wait": "complete",
            "context": context_id}})

    # Wait for `browsingContext.load` event.
    resp = await read_JSON_message(websocket)
    assert resp["method"] == "browsingContext.load"
    assert resp["params"]["context"] == context_id


@pytest.mark.asyncio
async def test_sessionSubscribeWithContext_subscribesToEventsInGivenContext(
      websocket, context_id):
    result = await execute_command(websocket, {
        "method": "session.subscribe",
        "params": {
            "events": [
                "browsingContext.load"],
            "contexts": [
                context_id]}})
    assert result == {}

    # Navigate to some page.
    await send_JSON_command(websocket, {
        "id": get_next_command_id(),
        "method": "browsingContext.navigate",
        "params": {
            "url": "data:text/html,<h2>test</h2>",
            "wait": "complete",
            "context": context_id}})

    # Wait for `browsingContext.load` event.
    resp = await read_JSON_message(websocket)
    assert resp["method"] == "browsingContext.load"
    assert resp["params"]["context"] == context_id


@pytest.mark.asyncio
async def test_sessionSubscribeWithContext_doesNotSubscribeToEventsInAnotherContexts(
      websocket, context_id):
    # 1. Get 2 contexts.
    # 2. Subscribe to event `browsingContext.load` on the first one.
    # 3. Navigate waiting complete loading in both contexts.
    # 4. Verify `browsingContext.load` emitted only for the first context.

    first_context_id = context_id

    result = await execute_command(websocket, {
        "method": "browsingContext.create",
        "params": {"type": "tab"}})
    second_context_id = result["context"]

    await subscribe(websocket, ["browsingContext.load"], [first_context_id])

    # 3.1 Navigate first context.
    command_id_1 = get_next_command_id()
    await send_JSON_command(websocket, {
        "id": command_id_1,
        "method": "browsingContext.navigate",
        "params": {
            "url": "data:text/html,<h2>test</h2>",
            "wait": "complete",
            "context": first_context_id}})
    # 4.1 Verify `browsingContext.load` is emitted for the first context.
    resp = await read_JSON_message(websocket)
    assert resp["method"] == "browsingContext.load"
    assert resp["params"]["context"] == first_context_id

    # Verify first navigation finished.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == command_id_1

    # 3.2 Navigate second context.
    command_id_2 = get_next_command_id()
    await send_JSON_command(websocket, {
        "id": command_id_2,
        "method": "browsingContext.navigate",
        "params": {
            "url": "data:text/html,<h2>test</h2>",
            "wait": "complete",
            "context": second_context_id}})

    # 4.2 Verify second navigation finished without emitting
    # `browsingContext.load`.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == command_id_2
