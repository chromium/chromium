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
                          create_request_via_fetch, execute_command, goto_url,
                          send_JSON_command, subscribe, wait_for_event)

from . import create_blocked_request


@pytest.mark.asyncio
async def test_continue_request_non_existent_request(websocket):
    with pytest.raises(
            Exception,
            match=str({
                "error": "no such request",
                "message": "Network request with ID '_UNKNOWN_' doesn't exist"
            })):
        await execute_command(
            websocket, {
                "method": "network.continueRequest",
                "params": {
                    "request": '_UNKNOWN_',
                },
            })


@pytest.mark.asyncio
async def test_continue_request_invalid_phase_response_started(
        websocket, context_id, url_example):

    network_id = await create_blocked_request(websocket,
                                              context_id,
                                              url=url_example,
                                              phase="responseStarted")

    with pytest.raises(
            Exception,
            match=str({
                "error": "invalid argument",
                "message": f"Blocked request for network id '{network_id}' is in 'responseStarted' phase"
            })):
        await execute_command(
            websocket, {
                "method": "network.continueRequest",
                "params": {
                    "request": network_id,
                    "url": url_example,
                },
            })


@pytest.mark.asyncio
async def test_continue_request_invalid_phase_auth_required(
        websocket, context_id, url_base, url_auth_required):

    await goto_url(websocket, context_id, url_base)

    network_id = await create_blocked_request(websocket,
                                              context_id,
                                              url=url_auth_required,
                                              phase="authRequired")

    with pytest.raises(
            Exception,
            match=str({
                "error": "invalid argument",
                "message": f"Blocked request for network id '{network_id}' is in 'authRequired' phase"
            })):
        await execute_command(
            websocket, {
                "method": "network.continueRequest",
                "params": {
                    "request": network_id,
                    "url": url_auth_required,
                },
            })


@pytest.mark.asyncio
async def test_continue_request_invalid_url(websocket, context_id,
                                            url_example):
    invalid_url = '%invalid%'

    network_id = await create_blocked_request(websocket,
                                              context_id,
                                              url=url_example,
                                              phase="beforeRequestSent")

    with pytest.raises(
            Exception,
            match=str({
                "error": "invalid argument",
                "message": f"Invalid URL '{invalid_url}': TypeError: Failed to construct 'URL': Invalid URL",
            })):
        await execute_command(
            websocket, {
                "method": "network.continueRequest",
                "params": {
                    "request": network_id,
                    "url": invalid_url,
                },
            })


@pytest.mark.asyncio
async def test_continue_request_non_blocked_request(websocket, context_id,
                                                    assert_no_events_in_queue,
                                                    url_hang_forever,
                                                    url_base):
    await goto_url(websocket, context_id, url_base)

    await subscribe(websocket, [
        "network.beforeRequestSent", "network.responseCompleted",
        "network.fetchError"
    ])

    await create_request_via_fetch(websocket, context_id, url_hang_forever)

    before_request_sent_event = await wait_for_event(
        websocket, "network.beforeRequestSent")

    # Assert these events never happen, otherwise the test is ineffective.
    await assert_no_events_in_queue(
        ["network.responseCompleted", "network.fetchError"], timeout=1.0)

    assert not before_request_sent_event["params"]["isBlocked"]

    network_id = before_request_sent_event["params"]["request"]["request"]

    with pytest.raises(
            Exception,
            match=str({
                "error": "no such request",
                "message": f"No blocked request found for network id '{network_id}'",
            })):
        await execute_command(
            websocket, {
                "method": "network.continueRequest",
                "params": {
                    "request": network_id,
                    "url": url_hang_forever,
                },
            })


@pytest.mark.asyncio
async def test_continue_request_completes(websocket, context_id, url_example):

    await goto_url(websocket, context_id, url_example)

    await subscribe(websocket,
                    ["network.beforeRequestSent", "network.responseCompleted"],
                    [context_id])

    result = await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": ["beforeRequestSent"],
                "urlPatterns": [{
                    "type": "string",
                    "pattern": url_example,
                }, ],
            },
        })

    await create_request_via_fetch(websocket, context_id, url_example)

    event_response = await wait_for_event(websocket,
                                          "network.beforeRequestSent")
    assert event_response == AnyExtending({
        "method": "network.beforeRequestSent",
        "params": {
            "context": context_id,
            "initiator": {
                "type": "script",
                "stackTrace": ANY_DICT
            },
            "intercepts": [result["intercept"]],
            "isBlocked": True,
            "navigation": None,
            "redirectCount": 0,
            "request": {
                "request": ANY_STR,
                "url": url_example,
                "method": "GET",
                "headers": ANY_LIST,
                "cookies": [],
                "headersSize": ANY_NUMBER,
                "bodySize": 0,
                "timings": ANY_DICT,
            },
            "timestamp": ANY_TIMESTAMP,
        },
        "type": "event",
    })
    network_id = event_response["params"]["request"]["request"]

    await execute_command(
        websocket, {
            "method": "network.continueRequest",
            "params": {
                "request": network_id,
                "url": url_example,
            },
        })

    event_response = await wait_for_event(websocket,
                                          "network.responseCompleted")
    assert event_response == {
        "method": "network.responseCompleted",
        "params": {
            "context": context_id,
            "isBlocked": False,
            "navigation": None,
            "redirectCount": 0,
            "request": ANY_DICT,
            "response": ANY_DICT,
            "timestamp": ANY_TIMESTAMP,
        },
        "type": "event",
    }


@pytest.mark.asyncio
async def test_continue_request_twice(websocket, context_id, url_example):
    await subscribe(websocket,
                    ["network.beforeRequestSent", "network.responseCompleted"],
                    [context_id])

    result = await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": ["beforeRequestSent"],
                "urlPatterns": [{
                    "type": "string",
                    "pattern": url_example,
                }, ],
            },
        })

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
            "intercepts": [result["intercept"]],
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
                "timings": ANY_DICT,
            },
            "timestamp": ANY_TIMESTAMP,
        },
        "type": "event",
    })
    network_id = event_response["params"]["request"]["request"]

    await execute_command(
        websocket, {
            "method": "network.continueRequest",
            "params": {
                "request": network_id,
                "url": url_example,
            },
        })

    event_response = await wait_for_event(websocket,
                                          "network.responseCompleted")

    with pytest.raises(
            Exception,
            match=str({
                "error": "no such request",
                "message": f"Network request with ID '{network_id}' doesn't exist"
            })):
        await execute_command(
            websocket, {
                "method": "network.continueRequest",
                "params": {
                    "request": network_id,
                    "url": url_example,
                },
            })


@pytest.mark.asyncio
@pytest.mark.skip(reason="TODO: #1890")
async def test_continue_request_remove_intercept_inflight_request(
        websocket, context_id, url_example):

    await goto_url(websocket, context_id, url_example)

    await subscribe(websocket,
                    ["network.beforeRequestSent", "network.responseCompleted"],
                    [context_id])

    result = await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": ["beforeRequestSent"],
                "urlPatterns": [{
                    "type": "string",
                    "pattern": url_example,
                }, ],
            },
        })

    assert result == {
        "intercept": ANY_UUID,
    }
    intercept_id = result["intercept"]

    await create_request_via_fetch(websocket, context_id, url_example)

    event_response = await wait_for_event(websocket,
                                          "network.beforeRequestSent")
    assert event_response == AnyExtending({
        "method": "network.beforeRequestSent",
        "params": {
            "context": context_id,
            "initiator": {
                "type": "script",
            },
            "intercepts": [intercept_id],
            "isBlocked": True,
            "navigation": None,
            "redirectCount": 0,
            "request": {
                "request": ANY_STR,
                "url": url_example,
                "method": "GET",
                "headers": ANY_LIST,
                "cookies": [],
                "headersSize": ANY_NUMBER,
                "bodySize": 0,
                "timings": ANY_DICT,
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

    network_id = event_response["params"]["request"]["request"]

    await execute_command(
        websocket, {
            "method": "network.continueRequest",
            "params": {
                "request": network_id,
                "url": url_example,
            },
        })

    event_response = await wait_for_event(websocket,
                                          "network.responseCompleted")
    assert event_response == {
        "method": "network.responseCompleted",
        "params": {
            "context": context_id,
            "isBlocked": False,
            "navigation": None,
            "redirectCount": 0,
            "request": ANY_DICT,
            "response": ANY_DICT,
            "timestamp": ANY_TIMESTAMP,
        },
        "type": "event",
    }
