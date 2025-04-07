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

import asyncio
from unittest.mock import ANY

import pytest
from anys import ANY_INT
from test_helpers import (ANY_TIMESTAMP, AnyExtending, execute_command,
                          read_JSON_message, send_JSON_command, subscribe,
                          wait_for_event)


@pytest.mark.asyncio
async def test_cdp_sendCommand_resultReturned(websocket):
    command_result = await execute_command(
        websocket, {
            "method": "goog:cdp.sendCommand",
            "params": {
                "method": "Target.getTargets",
                "params": {}
            }
        })

    assert {"result": {"targetInfos": ANY}} == command_result


@pytest.mark.asyncio
async def test_cdp_subscribe_toSpecificEvent(websocket, context_id,
                                             get_cdp_session_id):
    await subscribe(websocket, ["goog:cdp.Runtime.consoleAPICalled"])

    session_id = await get_cdp_session_id(context_id)

    await send_JSON_command(
        websocket, {
            "method": "goog:cdp.sendCommand",
            "params": {
                "method": "Runtime.evaluate",
                "params": {
                    "expression": "console.log(1)",
                },
                "session": session_id
            }
        })
    resp = await read_JSON_message(websocket)

    assert resp == AnyExtending({
        "type": "event",
        "method": "goog:cdp.Runtime.consoleAPICalled",
        "params": {
            "event": "Runtime.consoleAPICalled",
            "params": {
                "type": "log",
                "args": [{
                    "type": "number",
                    "value": 1,
                    "description": "1"
                }],
                "executionContextId": ANY_INT,
                "timestamp": ANY_TIMESTAMP,
                "stackTrace": ANY
            },
            "session": session_id
        }
    })


@pytest.mark.asyncio
async def test_cdp_subscribe_to_all_cdp_events(websocket, get_cdp_session_id,
                                               context_id):
    await subscribe(websocket, ["goog:cdp"])

    session_id = await get_cdp_session_id(context_id)

    await send_JSON_command(
        websocket, {
            "method": "goog:cdp.sendCommand",
            "params": {
                "method": "Runtime.evaluate",
                "params": {
                    "expression": "console.log(1)",
                },
                "session": session_id
            }
        })

    resp = await wait_for_event(websocket, "goog:cdp.Runtime.consoleAPICalled")

    assert resp == AnyExtending({
        "type": "event",
        "method": "goog:cdp.Runtime.consoleAPICalled",
        "params": {
            "event": "Runtime.consoleAPICalled",
            "params": {
                "type": "log",
                "args": [{
                    "type": "number",
                    "value": 1,
                    "description": "1"
                }],
                "executionContextId": ANY_INT,
                "timestamp": ANY_TIMESTAMP,
                "stackTrace": ANY
            },
            "session": session_id
        }
    })


@pytest.mark.asyncio
async def test_cdp_wait_for_event(websocket, get_cdp_session_id, context_id):
    await subscribe(websocket, ["goog:cdp.Runtime.consoleAPICalled"])

    session_id = await get_cdp_session_id(context_id)

    await send_JSON_command(
        websocket, {
            "method": "goog:cdp.sendCommand",
            "params": {
                "method": "Runtime.evaluate",
                "params": {
                    "expression": "console.log(1)",
                },
                "session": session_id
            }
        })

    event_response = await wait_for_event(websocket,
                                          "goog:cdp.Runtime.consoleAPICalled")
    assert event_response == AnyExtending({
        "type": "event",
        "method": "goog:cdp.Runtime.consoleAPICalled",
        "params": {
            "event": "Runtime.consoleAPICalled",
            "params": {
                "type": "log",
                "args": [{
                    "type": "number",
                    "value": 1,
                    "description": "1"
                }],
                "executionContextId": ANY_INT,
                "timestamp": ANY_TIMESTAMP,
                "stackTrace": ANY
            },
            "session": session_id
        }
    })


@pytest.mark.asyncio
async def test_cdp_no_extraneous_events(websocket, get_cdp_session_id,
                                        create_context, url_base):
    new_context_id = await create_context()
    await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_base,
                "wait": "complete",
                "context": new_context_id
            }
        })

    await subscribe(websocket, ["goog:cdp"], [new_context_id])

    session_id = await get_cdp_session_id(new_context_id)

    id = await send_JSON_command(
        websocket, {
            "method": "goog:cdp.sendCommand",
            "params": {
                "method": "Target.attachToTarget",
                "params": {
                    "targetId": new_context_id,
                    "flatten": True
                },
                "session": session_id
            }
        })

    events = []
    event = await read_JSON_message(websocket)

    session_id = None
    with pytest.raises(asyncio.TimeoutError):
        while True:
            if 'id' in event and event['id'] == id:
                session_id = event['result']['result']['sessionId']
            if 'id' not in event:
                events.append(event)
            event = await asyncio.wait_for(read_JSON_message(websocket),
                                           timeout=1.0)
