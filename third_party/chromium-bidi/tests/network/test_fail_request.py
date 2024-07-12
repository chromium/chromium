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
async def test_fail_request_non_existent_request(websocket):
    with pytest.raises(
            Exception,
            match=str({
                "error": "no such request",
                "message": "Network request with ID '_UNKNOWN_' doesn't exist"
            })):
        await execute_command(
            websocket, {
                "method": "network.failRequest",
                "params": {
                    "request": '_UNKNOWN_',
                },
            })


@pytest.mark.asyncio
async def test_fail_request_non_blocked_request(websocket, context_id,
                                                assert_no_events_in_queue,
                                                url_hang_forever):
    await subscribe(websocket, [
        "network.beforeRequestSent", "network.responseCompleted",
        "network.fetchError"
    ])

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_hang_forever,
                "context": context_id,
                "wait": "complete",
            }
        })

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
        await execute_command(websocket, {
            "method": "network.failRequest",
            "params": {
                "request": network_id,
            },
        })


@pytest.mark.asyncio
async def test_fail_request_twice(websocket, context_id, url_example):
    await subscribe(websocket, ["network.beforeRequestSent"], [context_id])

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

    result = await execute_command(websocket, {
        "method": "network.failRequest",
        "params": {
            "request": network_id,
        },
    })
    assert result == {}

    with pytest.raises(
            Exception,
            match=str({
                "error": "no such request",
                "message": f"No blocked request found for network id '{network_id}'"
            })):
        await execute_command(websocket, {
            "method": "network.failRequest",
            "params": {
                "request": network_id
            },
        })


@pytest.mark.asyncio
async def test_fail_request_with_auth_required_phase(websocket, context_id,
                                                     url_auth_required,
                                                     url_base):

    await goto_url(websocket, context_id, url_base)

    network_id = await create_blocked_request(websocket, context_id,
                                              url_auth_required,
                                              "authRequired")

    with pytest.raises(
            Exception,
            match=str({
                "error": "invalid argument",
                "message": f"Request '{network_id}' in 'authRequired' phase cannot be failed"
            })):
        await execute_command(websocket, {
            "method": "network.failRequest",
            "params": {
                "request": network_id
            },
        })


@pytest.mark.asyncio
async def test_fail_request_completes(websocket, context_id, url_example):
    await subscribe(websocket, ["network.beforeRequestSent"], [context_id])

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

    await subscribe(websocket, ["network.fetchError"])

    result = await execute_command(websocket, {
        "method": "network.failRequest",
        "params": {
            "request": network_id,
        },
    })
    assert result == {}

    fetch_error_response = await wait_for_event(websocket,
                                                "network.fetchError")

    assert fetch_error_response == {
        "method": "network.fetchError",
        "params": {
            "context": context_id,
            "errorText": "net::ERR_FAILED",
            "isBlocked": False,
            "navigation": ANY_STR,
            "redirectCount": 0,
            "request": AnyExtending(
                {
                    "headers": ANY_LIST,
                    "method": "GET",
                    "request": network_id,
                    "url": url_example,
                }, ),
            "timestamp": ANY_TIMESTAMP,
        },
        "type": "event",
    }


@pytest.mark.asyncio
async def test_fail_request_completes_new_request_still_blocks(
        websocket, context_id, url_example, url_base):

    await goto_url(websocket, context_id, url_base)

    await subscribe(websocket,
                    ["network.beforeRequestSent", "network.fetchError"],
                    [context_id])

    network_id_1 = await create_blocked_request(websocket,
                                                context_id,
                                                url=url_example,
                                                phase="beforeRequestSent")

    result = await execute_command(websocket, {
        "method": "network.failRequest",
        "params": {
            "request": network_id_1,
        },
    })
    assert result == {}

    fetch_error_response = await wait_for_event(websocket,
                                                "network.fetchError")

    assert fetch_error_response == {
        "method": "network.fetchError",
        "params": {
            "context": context_id,
            "errorText": "net::ERR_FAILED",
            "isBlocked": False,
            "navigation": None,
            "redirectCount": 0,
            "request": AnyExtending(
                {
                    "headers": ANY_LIST,
                    "method": "GET",
                    "request": network_id_1,
                    "url": url_example,
                }, ),
            "timestamp": ANY_TIMESTAMP,
        },
        "type": "event",
    }

    await create_request_via_fetch(websocket, context_id, url_example)

    event_response2 = await wait_for_event(websocket,
                                           "network.beforeRequestSent")
    assert event_response2 == AnyExtending({
        "method": "network.beforeRequestSent",
        "params": {
            "context": context_id,
            "initiator": {
                "type": "script",
                "stackTrace": ANY_DICT
            },
            "intercepts": ANY_LIST,
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
    network_id_2 = event_response2["params"]["request"]["request"]

    # The second request should have a different ID.
    assert network_id_1 != network_id_2


@pytest.mark.asyncio
async def test_fail_request_multiple_contexts(websocket, context_id,
                                              another_context_id, url_example):
    await subscribe(websocket, ["network.beforeRequestSent"])

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

    # Navigation in first context.
    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_example,
                "context": context_id,
                "wait": "complete",
            }
        })

    event_response1 = await wait_for_event(websocket,
                                           "network.beforeRequestSent")
    assert event_response1 == AnyExtending({
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
    network_id_1 = event_response1["params"]["request"]["request"]

    # Navigation in second context.
    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_example,
                "context": another_context_id,
                "wait": "complete",
            }
        })

    event_response2 = await wait_for_event(websocket,
                                           "network.beforeRequestSent")
    assert event_response2 == AnyExtending({
        "method": "network.beforeRequestSent",
        "params": {
            "context": another_context_id,
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
    network_id_2 = event_response2["params"]["request"]["request"]

    assert network_id_1 != network_id_2

    await subscribe(websocket, ["network.fetchError"])

    # Fail the first request.
    result = await execute_command(websocket, {
        "method": "network.failRequest",
        "params": {
            "request": network_id_1,
        },
    })
    assert result == {}

    fetch_error_response = await wait_for_event(websocket,
                                                "network.fetchError")

    assert fetch_error_response == {
        "method": "network.fetchError",
        "params": {
            "context": context_id,
            "errorText": "net::ERR_FAILED",
            "isBlocked": False,
            "navigation": ANY_STR,
            "redirectCount": 0,
            "request": AnyExtending(
                {
                    "headers": ANY_LIST,
                    "method": "GET",
                    "request": network_id_1,
                    "url": url_example,
                }, ),
            "timestamp": ANY_TIMESTAMP,
        },
        "type": "event",
    }

    # Fail the second request.
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
                    "url": url_example,
                }, ),
            "timestamp": ANY_TIMESTAMP,
        },
        "type": "event",
    }


@pytest.mark.asyncio
@pytest.mark.skip(reason='TODO: #1890')
async def test_fail_request_remove_intercept_inflight_request(
        websocket, context_id, url_example):

    await goto_url(websocket, context_id, url_example)

    await subscribe(websocket, ["network.beforeRequestSent"], [context_id])

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
    network_id = event_response["params"]["request"]["request"]

    result = await execute_command(
        websocket, {
            "method": "network.removeIntercept",
            "params": {
                "intercept": intercept_id,
            },
        })
    assert result == {}

    await subscribe(websocket, ["network.fetchError"])

    result = await execute_command(websocket, {
        "method": "network.failRequest",
        "params": {
            "request": network_id,
        },
    })
    assert result == {}

    fetch_error_response = await wait_for_event(websocket,
                                                "network.fetchError")

    assert fetch_error_response == {
        "method": "network.fetchError",
        "params": {
            "context": context_id,
            "errorText": "net::ERR_FAILED",
            "isBlocked": False,
            "navigation": ANY_STR,
            "redirectCount": 0,
            "request": AnyExtending(
                {
                    "headers": ANY_LIST,
                    "method": "GET",
                    "request": network_id,
                    "url": url_example,
                }, ),
            "timestamp": ANY_TIMESTAMP,
        },
        "type": "event",
    }
