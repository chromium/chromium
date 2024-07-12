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
import pytest
from anys import ANY_DICT, ANY_LIST, ANY_NUMBER, ANY_STR
from test_helpers import (ANY_TIMESTAMP, ANY_UUID, AnyExtending,
                          execute_command, send_JSON_command, subscribe,
                          wait_for_event)


@pytest.mark.asyncio
async def test_remove_intercept_no_such_intercept(websocket):
    with pytest.raises(
            Exception,
            match=str({
                "error": "no such intercept",
                "message": "Intercept '00000000-0000-0000-0000-000000000000' does not exist."
            })):
        await execute_command(
            websocket, {
                "method": "network.removeIntercept",
                "params": {
                    "intercept": "00000000-0000-0000-0000-000000000000",
                },
            })


@pytest.mark.asyncio
async def test_remove_intercept_twice(websocket):
    result = await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": ["beforeRequestSent"],
                "urlPatterns": [{
                    "type": "string",
                    "pattern": "https://www.example.com/*"
                }],
            },
        })
    intercept_id = result["intercept"]

    result = await execute_command(
        websocket, {
            "method": "network.removeIntercept",
            "params": {
                "intercept": intercept_id,
            },
        })
    assert result == {}

    # Check that the intercept is gone.
    with pytest.raises(
            Exception,
            match=str({
                "error": "no such intercept",
                "message": f"Intercept '{intercept_id}' does not exist."
            })):
        await execute_command(
            websocket, {
                "method": "network.removeIntercept",
                "params": {
                    "intercept": intercept_id,
                },
            })


@pytest.mark.asyncio
@pytest.mark.parametrize("url_patterns", [
    [
        {
            "type": "string",
            "pattern": "https://www.example.com/",
        },
    ],
    [
        {
            "type": "pattern",
            "protocol": "https",
            "hostname": "www.example.com",
            "pathname": "/",
        },
    ],
    [
        {
            "type": "string",
            "pattern": "https://www.example.com/",
        },
        {
            "type": "pattern",
            "protocol": "https",
            "hostname": "www.example.com",
            "pathname": "/",
        },
    ],
],
                         ids=[
                             "string",
                             "pattern",
                             "string and pattern",
                         ])
@pytest.mark.asyncio
async def test_remove_intercept_unblocks(websocket, context_id,
                                         another_context_id, url_patterns):
    # TODO: make offline
    example_url = "https://www.example.com/"
    await subscribe(websocket, ["network.beforeRequestSent"], [context_id])
    await subscribe(websocket, ["network"], [another_context_id])

    result = await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": ["beforeRequestSent"],
                "urlPatterns": url_patterns,
            },
        })

    assert result == {
        "intercept": ANY_UUID,
    }
    intercept_id = result["intercept"]

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": example_url,
                "context": context_id,
                "wait": "complete",
            }
        })

    event_response = await wait_for_event(websocket,
                                          "network.beforeRequestSent")
    assert event_response == AnyExtending({
        "method": "network.beforeRequestSent",
        "params": {
            "context": context_id,
            "initiator": {
                "type": "other",
            },
            "intercepts": [intercept_id],
            "isBlocked": True,
            "navigation": ANY_STR,
            "redirectCount": 0,
            "request": {
                "request": ANY_STR,
                "url": example_url,
                "method": "GET",
                "headers": ANY_LIST,
                "cookies": [],
                "headersSize": ANY_NUMBER,
                "bodySize": 0,
                "timings": ANY_DICT
            },
            "timestamp": ANY_TIMESTAMP,
        },
        "type": "event",
    })

    result = await execute_command(
        websocket, {
            "method": "network.removeIntercept",
            "params": {
                "intercept": intercept_id,
            },
        })
    assert result == {}

    # Try again, with the intercept removed, in another context.
    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": example_url,
                "wait": "complete",
                "context": another_context_id,
            }
        })

    # Network events should complete.
    event_response = await wait_for_event(websocket,
                                          "network.responseCompleted")
    assert event_response == AnyExtending({
        'type': 'event',
        "method": "network.responseCompleted",
        "params": {
            "isBlocked": False,
            "context": another_context_id,
            "navigation": ANY_STR,
            "redirectCount": 0,
            "request": {
                "request": ANY_STR,
                "url": example_url,
                "method": "GET",
                "headers": ANY_LIST,
                "cookies": [],
                "headersSize": ANY_NUMBER,
                "bodySize": 0,
                "timings": ANY_DICT
            },
            "response": ANY_DICT,
            "timestamp": ANY_TIMESTAMP,
        }
    })


@pytest.mark.asyncio
async def test_remove_intercept_does_not_affect_another_intercept(
        websocket, context_id, another_context_id, url_example,
        url_example_another_origin):
    await subscribe(websocket, ["network.beforeRequestSent"])

    result = await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": ["beforeRequestSent"],
                "urlPatterns": [{
                    "type": "string",
                    "pattern": url_example,
                }, ]
            },
        })
    assert result == {
        "intercept": ANY_UUID,
    }
    intercept_id_1 = result["intercept"]

    result = await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": ["beforeRequestSent"],
                "urlPatterns": [{
                    "type": "string",
                    "pattern": url_example_another_origin,
                }, ]
            },
        })
    intercept_id_2 = result["intercept"]

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_example,
                "context": context_id,
                "wait": "complete",
            }
        })
    event_response = await wait_for_event(websocket,
                                          "network.beforeRequestSent")
    assert event_response == AnyExtending({
        "method": "network.beforeRequestSent",
        "params": {
            "context": context_id,
            "initiator": {
                "type": "other",
            },
            "intercepts": [intercept_id_1],
            "isBlocked": True,
            "navigation": ANY_STR,
            "redirectCount": 0,
            "request": {
                "request": ANY_STR,
                "url": url_example,
                "method": "GET",
                "headers": ANY_LIST,
                "cookies": [],
                "headersSize": ANY_NUMBER,
                "bodySize": 0,
                "timings": ANY_DICT
            },
            "timestamp": ANY_TIMESTAMP,
        },
        "type": "event",
    })

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_example_another_origin,
                "context": another_context_id,
                "wait": "complete",
            }
        })
    event_response_2 = await wait_for_event(websocket,
                                            "network.beforeRequestSent")
    assert event_response_2 == AnyExtending({
        "method": "network.beforeRequestSent",
        "params": {
            "context": another_context_id,
            "initiator": {
                "type": "other",
            },
            "intercepts": [intercept_id_2],
            "isBlocked": True,
            "navigation": ANY_STR,
            "redirectCount": 0,
            "request": {
                "request": ANY_STR,
                "url": url_example_another_origin,
                "method": "GET",
                "headers": ANY_LIST,
                "cookies": [],
                "headersSize": ANY_NUMBER,
                "bodySize": 0,
                "timings": ANY_DICT
            },
            "timestamp": ANY_TIMESTAMP,
        },
        "type": "event",
    })
    network_id_2 = event_response_2["params"]["request"]["request"]

    result = await execute_command(
        websocket, {
            "method": "network.removeIntercept",
            "params": {
                "intercept": intercept_id_1,
            },
        })
    assert result == {}

    await subscribe(websocket, ["network.fetchError"])

    result = await execute_command(websocket, {
        "method": "network.failRequest",
        "params": {
            "request": network_id_2,
        },
    })
    assert result == {}

    fetch_error_response = await wait_for_event(websocket,
                                                "network.fetchError")

    assert fetch_error_response == {
        "method": "network.fetchError",
        "params": {
            "context": another_context_id,
            "errorText": "net::ERR_FAILED",
            "isBlocked": False,
            "navigation": ANY_STR,
            "redirectCount": 0,
            "request": AnyExtending(
                {
                    "headers": ANY_LIST,
                    "method": "GET",
                    "request": network_id_2,
                    "url": url_example_another_origin,
                }, ),
            "timestamp": ANY_TIMESTAMP,
        },
        "type": "event",
    }
