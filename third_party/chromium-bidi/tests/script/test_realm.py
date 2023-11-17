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

    await subscribe(websocket, ["script.realmCreated"])

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
        "type": "event",
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

    await subscribe(websocket, ["script.realmCreated"])

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
        "type": "event",
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
async def test_realm_realmCreated_worker(websocket, context_id, html):
    worker_url = 'data:application/javascript,while(true){}'
    url = html(f"<script>new Worker('{worker_url}');</script>")

    await subscribe(websocket, ["script.realmCreated"])

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "context": context_id,
                "url": url,
                "wait": "complete",
            }
        })

    # Realm created
    assert {
        "type": "event",
        "method": "script.realmCreated",
        "params": {
            "type": "window",
            "origin": "null",
            "realm": ANY_STR,
            "context": context_id,
        }
    } == await read_JSON_message(websocket)

    # Navigation
    assert {
        "navigation": ANY_STR,
        "url": url
    } == (await read_JSON_message(websocket))['result']

    # Worker realm is created from the HTML script.
    assert {
        "type": "event",
        "method": "script.realmCreated",
        "params": {
            "type": "dedicated-worker",
            "origin": worker_url,
            "realm": ANY_STR,
            "context": ANY_STR
        }
    } == await read_JSON_message(websocket)


@pytest.mark.asyncio
async def test_realm_realmDestroyed(websocket, context_id):

    await subscribe(websocket, ["script.realmDestroyed"])

    await send_JSON_command(websocket, {
        "method": "browsingContext.close",
        "params": {
            "context": context_id,
        }
    })

    response = await read_JSON_message(websocket)

    assert {
        "type": "event",
        "method": "script.realmDestroyed",
        "params": {
            "realm": ANY_STR,
        }
    } == response


@pytest.mark.asyncio
async def test_realm_realmDestroyed_sandbox(websocket, context_id):

    await subscribe(websocket, ["script.realmDestroyed"])

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

    await send_JSON_command(websocket, {
        "method": "browsingContext.close",
        "params": {
            "context": context_id,
        }
    })

    response = await read_JSON_message(websocket)

    assert {
        "type": "event",
        "method": "script.realmDestroyed",
        "params": {
            "realm": ANY_STR,
        }
    } == response

    response = await read_JSON_message(websocket)

    assert {
        "type": "event",
        "method": "script.realmDestroyed",
        "params": {
            "realm": ANY_STR,
        }
    } == response


@pytest.mark.asyncio
async def test_realm_realmDestroyed_worker(websocket, context_id, html):
    worker_url = 'data:application/javascript,while(true){}'
    url = html(f"<script>window.w = new Worker('{worker_url}');</script>")

    await subscribe(websocket, ["script.realmDestroyed"])

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "context": context_id,
                "url": url,
                "wait": "complete",
            }
        })

    assert {
        "type": "event",
        "method": "script.realmDestroyed",
        "params": {
            "realm": ANY_STR,
        }
    } == await read_JSON_message(websocket)

    await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "target": {
                    "context": context_id
                },
                "expression": "window.w.terminate()",
                "awaitPromise": True
            }
        })

    # Worker realm is destroyed from the HTML script.
    assert {
        "type": "event",
        "method": "script.realmDestroyed",
        "params": {
            "realm": ANY_STR
        }
    } == await read_JSON_message(websocket)
