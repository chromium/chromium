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
                          send_JSON_command, subscribe, wait_for_event,
                          wait_for_filtered_event)


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


# This test is flaky, as it take some time before the `script.realmDestroyed`
# event. Increase the timeout to avoid flakiness.
@pytest.mark.timeout(30)
@pytest.mark.asyncio
async def test_realm_dedicated_worker(websocket, context_id, html):
    worker_url = 'data:application/javascript,while(true){}'
    url = html(f"<script>window.w = new Worker('{worker_url}');</script>")

    await subscribe(websocket,
                    ["script.realmDestroyed", "script.realmCreated"])

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "context": context_id,
                "url": url,
                "wait": "complete",
            }
        })

    # Wait for worker to be created.
    worker_realm_created_event = await wait_for_filtered_event(
        websocket, lambda e: e['method'] == 'script.realmCreated' and e[
            'params']['type'] == 'dedicated-worker')

    # Assert the event.
    assert worker_realm_created_event == {
        'type': 'event',
        'method': 'script.realmCreated',
        'params': {
            'realm': ANY_STR,
            'origin': worker_url,
            'owners': [ANY_STR],
            'type': 'dedicated-worker'
        }
    }

    worker_realm = worker_realm_created_event['params']['realm']

    # Then demolish it!
    await send_JSON_command(
        websocket, {
            'method': 'script.evaluate',
            'params': {
                'target': {
                    'context': context_id
                },
                'expression': 'window.w.terminate()',
                'awaitPromise': True
            }
        })

    # Wait for confirmation that worker was destroyed
    event = await wait_for_event(websocket, 'script.realmDestroyed')
    assert event['params'] == {'realm': worker_realm}
