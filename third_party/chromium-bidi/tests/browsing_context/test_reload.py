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
from anys import ANY_DICT, ANY_STR
from test_helpers import (ANY_TIMESTAMP, AnyExtending, goto_url,
                          read_JSON_message, send_JSON_command, subscribe)


@pytest.mark.asyncio
async def test_browsingContext_reload_waitNone(websocket, context_id, html):
    url = html()

    await subscribe(
        websocket,
        ["browsingContext.domContentLoaded", "browsingContext.load"])

    await goto_url(websocket, context_id, url, "complete")

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.reload",
            "params": {
                "context": context_id,
                "wait": "none",
            }
        })

    # Assert command done.
    response = await read_JSON_message(websocket)
    assert response["result"] == {}

    # Wait for `browsingContext.domContentLoaded` event.
    response = await read_JSON_message(websocket)
    assert response == {
        'type': 'event',
        "method": "browsingContext.domContentLoaded",
        "params": {
            "context": context_id,
            "navigation": ANY_STR,
            "timestamp": ANY_TIMESTAMP,
            "url": url,
        }
    }

    # Wait for `browsingContext.load` event.
    response = await read_JSON_message(websocket)
    assert response == {
        'type': 'event',
        "method": "browsingContext.load",
        "params": {
            "context": context_id,
            "navigation": ANY_STR,
            "timestamp": ANY_TIMESTAMP,
            "url": url,
        }
    }


@pytest.mark.asyncio
async def test_browsingContext_reload_waitInteractive(websocket, context_id,
                                                      html):
    url = html()

    await subscribe(
        websocket,
        ["browsingContext.domContentLoaded", "browsingContext.load"])

    await goto_url(websocket, context_id, url, "complete")

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.reload",
            "params": {
                "context": context_id,
                "wait": "interactive",
            }
        })
    # Wait for `browsingContext.domContentLoaded` event.
    response = await read_JSON_message(websocket)
    assert response == {
        'type': 'event',
        "method": "browsingContext.domContentLoaded",
        "params": {
            "context": context_id,
            "navigation": ANY_STR,
            "timestamp": ANY_TIMESTAMP,
            "url": url,
        }
    }

    # Assert command done.
    response = await read_JSON_message(websocket)
    assert response["result"] == {}

    # Wait for `browsingContext.load` event.
    response = await read_JSON_message(websocket)
    assert response == {
        'type': 'event',
        "method": "browsingContext.load",
        "params": {
            "context": context_id,
            "navigation": ANY_STR,
            "timestamp": ANY_TIMESTAMP,
            "url": url,
        }
    }


@pytest.mark.asyncio
async def test_browsingContext_reload_waitComplete(websocket, context_id,
                                                   html):
    url = html()

    await subscribe(
        websocket,
        ["browsingContext.domContentLoaded", "browsingContext.load"])

    await goto_url(websocket, context_id, url, "complete")

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.reload",
            "params": {
                "context": context_id,
                "wait": "complete",
            }
        })

    # Wait for `browsingContext.domContentLoaded` event.
    response = await read_JSON_message(websocket)
    assert response == {
        'type': 'event',
        "method": "browsingContext.domContentLoaded",
        "params": {
            "context": context_id,
            "navigation": ANY_STR,
            "timestamp": ANY_TIMESTAMP,
            "url": url,
        }
    }

    # Wait for `browsingContext.load` event.
    response = await read_JSON_message(websocket)
    assert response == {
        'type': 'event',
        "method": "browsingContext.load",
        "params": {
            "context": context_id,
            "navigation": ANY_STR,
            "timestamp": ANY_TIMESTAMP,
            "url": url,
        }
    }

    # Assert command done.
    response = await read_JSON_message(websocket)
    assert response["result"] == {}


@pytest.mark.asyncio
@pytest.mark.parametrize("ignoreCache", [True, False])
async def test_browsingContext_reload_ignoreCache(websocket, context_id,
                                                  ignoreCache):
    if not ignoreCache:
        pytest.xfail(reason="TODO: Fix flakiness with ignoreCache=False")

    url = "https://example.com/"

    await subscribe(websocket, [
        "network.beforeRequestSent",
        "network.responseCompleted",
    ])

    await goto_url(websocket, context_id, url)

    id = await send_JSON_command(
        websocket, {
            "method": "browsingContext.reload",
            "params": {
                "context": context_id,
                "ignoreCache": ignoreCache,
                "wait": "complete",
            }
        })

    response = await read_JSON_message(websocket)
    assert response == {
        'type': 'event',
        "method": "network.beforeRequestSent",
        "params": {
            "context": context_id,
            "initiator": ANY_DICT,
            "navigation": ANY_STR,
            "redirectCount": 0,
            "request": ANY_DICT,
            "timestamp": ANY_TIMESTAMP,
        },
    }

    response = await read_JSON_message(websocket)
    assert response == {
        'type': 'event',
        "method": "network.responseCompleted",
        "params": {
            "context": context_id,
            "navigation": ANY_STR,
            "redirectCount": 0,
            "request": ANY_DICT,
            "response": AnyExtending({"status": 200 if ignoreCache else 304}),
            "timestamp": ANY_TIMESTAMP,
        },
    }

    # Assert command done.
    response = await read_JSON_message(websocket)
    assert response == {
        "id": id,
        "type": "success",
        "result": {},
    }
