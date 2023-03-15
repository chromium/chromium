#  Copyright 2023 Google LLC.
#  Copyright (c) Microsoft Corporation.
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
import pytest
from anys import ANY_DICT, ANY_LIST, ANY_NUMBER, ANY_STR
from test_helpers import (ANY_TIMESTAMP, execute_command, read_JSON_message,
                          send_JSON_command, subscribe)


@pytest.mark.asyncio
async def test_network_before_request_sent_event_emitted(
        websocket, context_id):
    await subscribe(websocket, "network.beforeRequestSent", context_id)

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": "http://example.com",
                "wait": "complete",
                "context": context_id
            }
        })

    resp = await read_JSON_message(websocket)

    assert resp == {
        "method": "network.beforeRequestSent",
        "params": {
            "context": context_id,
            "navigation": ANY_STR,
            "redirectCount": 0,
            "request": {
                "request": ANY_STR,
                "url": "http://example.com/",
                "method": "GET",
                "headers": ANY_LIST,
                "cookies": [],
                "headersSize": -1,
                "bodySize": 0,
                "timings": ANY_DICT
            },
            "initiator": {
                "type": "other"
            },
            "timestamp": ANY_TIMESTAMP
        }
    }


@pytest.mark.asyncio
async def test_network_before_request_sent_event_with_cookies_emitted(
        websocket, context_id):
    await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": "http://example.com",
                "wait": "complete",
                "context": context_id
            }
        })

    await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "document.cookie = 'foo=bar'",
                "target": {
                    "context": context_id,
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })

    await subscribe(websocket, "network.beforeRequestSent", context_id)

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": "http://example.com/qwe",
                "wait": "complete",
                "context": context_id
            }
        })

    resp = await read_JSON_message(websocket)
    assert resp == {
        "method": "network.beforeRequestSent",
        "params": {
            "context": context_id,
            "navigation": ANY_STR,
            "redirectCount": 0,
            "request": {
                "request": ANY_STR,
                "url": "http://example.com/qwe",
                "method": "GET",
                "headers": ANY_LIST,
                "cookies": [{
                    "domain": "example.com",
                    "expires": -1,
                    "httpOnly": False,
                    "name": "foo",
                    "path": "/",
                    "sameSite": "none",
                    "secure": False,
                    "size": 6,
                    "value": "bar",
                }, ],
                "headersSize": -1,
                "bodySize": 0,
                "timings": ANY_DICT
            },
            "initiator": {
                "type": "other"
            },
            "timestamp": ANY_TIMESTAMP
        }
    }


@pytest.mark.asyncio
async def test_network_network_response_completed_event_emitted(
        websocket, context_id):
    await subscribe(websocket, "network.responseCompleted", context_id)

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": "http://example.com",
                "wait": "complete",
                "context": context_id
            }
        })

    resp = await read_JSON_message(websocket)

    assert resp == {
        "method": "network.responseCompleted",
        "params": {
            "context": context_id,
            "navigation": ANY_STR,
            "redirectCount": 0,
            "request": {
                "request": ANY_STR,
                "url": "http://example.com/",
                "method": "GET",
                "headers": ANY_LIST,
                "cookies": [],
                "headersSize": -1,
                "bodySize": 0,
                "timings": ANY_DICT
            },
            "timestamp": ANY_TIMESTAMP,
            "response": {
                "url": "http://example.com/",
                "protocol": "http/1.1",
                "status": 200,
                "statusText": "OK",
                "fromCache": False,
                "headers": ANY_LIST,
                "mimeType": "text/html",
                "bytesReceived": ANY_NUMBER,
                "headersSize": ANY_NUMBER,
                "bodySize": -1,
                "content": {
                    "size": -1
                }
            }
        }
    }
