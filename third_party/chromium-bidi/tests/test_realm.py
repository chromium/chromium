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
from test_helpers import (execute_command, read_JSON_message,
                          send_JSON_command, subscribe)


@pytest.mark.asyncio
async def test_realm_realmCreated(websocket, context_id, html):
    url = html()

    await subscribe(websocket, "script.realmCreated")

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "context": context_id,
                "url": url,
                "wait": "complete",
            }
        })

    response = await read_JSON_message(websocket)

    assert {
        "method": "script.realmCreated",
        "params": {
            "type": "window",
            "origin": "null",
            "realm": ANY_STR,
            "context": context_id,
        }
    } == response


@pytest.mark.asyncio
async def test_realm_realmCreated_sandbox(websocket, context_id):

    await subscribe(websocket, "script.realmCreated")

    await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "target": {
                    "context": context_id,
                    "sandbox": "SOME_SANDBOX"
                },
                "expression": "2 + 2",
                "awaitPromise": True
            }
        })

    response = await read_JSON_message(websocket)

    assert {
        "method": "script.realmCreated",
        "params": {
            "type": "window",
            "origin": "null",
            "realm": ANY_STR,
            "context": context_id,
            "sandbox": "SOME_SANDBOX"
        }
    } == response


@pytest.mark.asyncio
async def test_realm_realmDestroyed(websocket, context_id):

    await subscribe(websocket, "script.realmDestroyed")

    await send_JSON_command(websocket, {
        "method": "browsingContext.close",
        "params": {
            "context": context_id,
        }
    })

    response = await read_JSON_message(websocket)

    assert {
        "method": "script.realmDestroyed",
        "params": {
            "realm": ANY_STR,
        }
    } == response


@pytest.mark.asyncio
async def test_realm_realmDestroyed_sandbox(websocket, context_id):

    await subscribe(websocket, "script.realmDestroyed")

    await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "target": {
                    "context": context_id,
                    "sandbox": "SOME_SANDBOX"
                },
                "expression": "2 + 2",
                "awaitPromise": True
            }
        })

    await send_JSON_command(
        websocket, {
            "id": 22,
            "method": "browsingContext.close",
            "params": {
                "context": context_id,
            }
        })

    response = await read_JSON_message(websocket)

    assert {
        "method": "script.realmDestroyed",
        "params": {
            "realm": ANY_STR,
        }
    } == response

    response = await read_JSON_message(websocket)

    assert {
        "method": "script.realmDestroyed",
        "params": {
            "realm": ANY_STR,
        }
    } == response
