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
from typing import Literal

import pytest
from anys import ANY_DICT, ANY_LIST, ANY_STR
from test_helpers import (ANY_TIMESTAMP, ANY_UUID, execute_command,
                          send_JSON_command, subscribe, wait_for_event)


@pytest.mark.asyncio
async def test_continue_request_non_existent_request(websocket):
    with pytest.raises(
            Exception,
            match=str({
                "error": "no such request",
                "message": "No blocked request found for network id '_UNKNOWN_'"
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
        websocket, context_id, example_url):

    network_id = await create_dummy_blocked_request(websocket,
                                                    context_id,
                                                    url=example_url,
                                                    phases=["responseStarted"])

    with pytest.raises(
            Exception,
            match=str({
                "error": "invalid argument",
                "message": f"Blocked request for network id '{network_id}' is not in 'BeforeRequestSent' phase"
            })):
        await execute_command(
            websocket, {
                "method": "network.continueRequest",
                "params": {
                    "request": network_id,
                    "url": example_url,
                },
            })


async def test_continue_request_invalid_phase_auth_required(
        websocket, context_id, auth_required_url):

    network_id = await create_dummy_blocked_request(websocket,
                                                    context_id,
                                                    url=auth_required_url,
                                                    phases=["authRequired"])

    with pytest.raises(
            Exception,
            match=str({
                "error": "invalid argument",
                "message": f"Blocked request for network id '{network_id}' is not in 'BeforeRequestSent' phase"
            })):
        await execute_command(
            websocket, {
                "method": "network.continueRequest",
                "params": {
                    "request": network_id,
                    "url": auth_required_url,
                },
            })


@pytest.mark.asyncio
async def test_continue_request_invalid_url(websocket, context_id,
                                            example_url):
    invalid_url = '%invalid%'

    network_id = await create_dummy_blocked_request(
        websocket, context_id, url=example_url, phases=["beforeRequestSent"])

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
                                                    hang_url):
    await subscribe(websocket, [
        "network.beforeRequestSent", "cdp.Network.responseReceived",
        "cdp.Network.loadingFailed"
    ])

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "context": context_id,
                "url": hang_url,
                "wait": "complete",
            }
        })

    before_request_sent_event = await wait_for_event(
        websocket, "network.beforeRequestSent")

    # Assert these events never happen, otherwise the test is ineffective.
    await assert_no_events_in_queue(
        ["cdp.Network.responseReceived", "cdp.Network.loadingFailed"],
        timeout=1.0)

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
                    "url": hang_url,
                },
            })


@pytest.mark.asyncio
async def test_continue_request_completes_use_cdp_events(
        websocket, context_id, example_url):
    await subscribe(websocket, ["cdp.Fetch.requestPaused"])

    await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": ["beforeRequestSent"],
                "urlPatterns": [{
                    "type": "string",
                    "pattern": example_url,
                }, ],
            },
        })
    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": example_url,
                "context": context_id,
            }
        })
    event_response = await wait_for_event(websocket, "cdp.Fetch.requestPaused")
    network_id = event_response["params"]["params"]["networkId"]

    await subscribe(websocket, ["network.responseCompleted"])

    await execute_command(
        websocket, {
            "method": "network.continueRequest",
            "params": {
                "request": network_id,
                "url": example_url,
            },
        })

    event_response = await wait_for_event(websocket,
                                          "network.responseCompleted")
    assert event_response == {
        "method": "network.responseCompleted",
        "params": {
            "context": context_id,
            "isBlocked": False,
            "navigation": ANY_STR,
            "redirectCount": 0,
            "request": ANY_DICT,
            "response": ANY_DICT,
            "timestamp": ANY_TIMESTAMP,
        },
        "type": "event",
    }


@pytest.mark.asyncio
async def test_continue_request_completes_use_bidi_events(
        websocket, context_id, example_url):
    await subscribe(websocket, ["network.beforeRequestSent"], [context_id])

    await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": ["beforeRequestSent"],
                "urlPatterns": [{
                    "type": "string",
                    "pattern": example_url,
                }, ],
            },
        })

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": example_url,
                "context": context_id,
            }
        })

    event_response = await wait_for_event(websocket,
                                          "network.beforeRequestSent")
    assert event_response == {
        "method": "network.beforeRequestSent",
        "params": {
            "context": context_id,
            "initiator": {
                "type": "other",
            },
            "isBlocked": True,
            "navigation": ANY_STR,
            "redirectCount": 0,
            "request": {
                "request": ANY_STR,
                "url": example_url,
                "method": "GET",
                "headers": ANY_LIST,
                "cookies": [],
                "headersSize": -1,
                "bodySize": 0,
                "timings": ANY_DICT,
            },
            "timestamp": ANY_TIMESTAMP,
        },
        "type": "event",
    }
    network_id = event_response["params"]["request"]["request"]

    await subscribe(websocket, ["network.responseCompleted"])

    await execute_command(
        websocket, {
            "method": "network.continueRequest",
            "params": {
                "request": network_id,
                "url": example_url,
            },
        })

    event_response = await wait_for_event(websocket,
                                          "network.responseCompleted")
    assert event_response == {
        "method": "network.responseCompleted",
        "params": {
            "context": context_id,
            "isBlocked": False,
            "navigation": ANY_STR,
            "redirectCount": 0,
            "request": ANY_DICT,
            "response": ANY_DICT,
            "timestamp": ANY_TIMESTAMP,
        },
        "type": "event",
    }


@pytest.mark.asyncio
async def test_continue_request_twice(websocket, context_id, example_url):
    await subscribe(websocket, ["network.beforeRequestSent"], [context_id])

    await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": ["beforeRequestSent"],
                "urlPatterns": [{
                    "type": "string",
                    "pattern": example_url,
                }, ],
            },
        })

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": example_url,
                "context": context_id,
            }
        })

    event_response = await wait_for_event(websocket,
                                          "network.beforeRequestSent")
    assert event_response == {
        "method": "network.beforeRequestSent",
        "params": {
            "context": context_id,
            "initiator": {
                "type": "other",
            },
            "isBlocked": True,
            "navigation": ANY_STR,
            "redirectCount": 0,
            "request": {
                "request": ANY_STR,
                "url": example_url,
                "method": "GET",
                "headers": ANY_LIST,
                "cookies": [],
                "headersSize": -1,
                "bodySize": 0,
                "timings": ANY_DICT,
            },
            "timestamp": ANY_TIMESTAMP,
        },
        "type": "event",
    }
    network_id = event_response["params"]["request"]["request"]

    await subscribe(websocket, ["network.responseCompleted"])

    await execute_command(
        websocket, {
            "method": "network.continueRequest",
            "params": {
                "request": network_id,
                "url": example_url,
            },
        })

    event_response = await wait_for_event(websocket,
                                          "network.responseCompleted")

    with pytest.raises(
            Exception,
            match=str({
                "error": "no such request",
                "message": f"No blocked request found for network id '{network_id}'"
            })):
        await execute_command(
            websocket, {
                "method": "network.continueRequest",
                "params": {
                    "request": network_id,
                    "url": example_url,
                },
            })


async def create_dummy_blocked_request(
    websocket, context_id: str, *, url: str,
    phases: list[Literal["beforeRequestSent", "responseStarted",
                         "authRequired"]]):
    """Creates a dummy blocked network request and returns its network id."""

    await subscribe(websocket, ["cdp.Fetch.requestPaused"])

    await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": phases,
                "urlPatterns": [{
                    "type": "string",
                    "pattern": url,
                }, ],
            },
        })
    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url,
                "context": context_id,
            }
        })
    event_response = await wait_for_event(websocket, "cdp.Fetch.requestPaused")
    network_id = event_response["params"]["params"]["networkId"]

    return network_id


@pytest.mark.asyncio
async def test_continue_request_remove_intercept_inflight_request(
        websocket, context_id, example_url):
    await subscribe(websocket, ["network.beforeRequestSent"], [context_id])

    result = await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": ["beforeRequestSent"],
                "urlPatterns": [{
                    "type": "string",
                    "pattern": example_url,
                }, ],
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
            }
        })

    event_response = await wait_for_event(websocket,
                                          "network.beforeRequestSent")
    assert event_response == {
        "method": "network.beforeRequestSent",
        "params": {
            "context": context_id,
            "initiator": {
                "type": "other",
            },
            "isBlocked": True,
            "navigation": ANY_STR,
            "redirectCount": 0,
            "request": {
                "request": ANY_STR,
                "url": example_url,
                "method": "GET",
                "headers": ANY_LIST,
                "cookies": [],
                "headersSize": -1,
                "bodySize": 0,
                "timings": ANY_DICT,
            },
            "timestamp": ANY_TIMESTAMP,
        },
        "type": "event",
    }
    network_id = event_response["params"]["request"]["request"]

    result = await execute_command(
        websocket, {
            "method": "network.removeIntercept",
            "params": {
                "intercept": intercept_id,
            },
        })
    assert result == {}

    await subscribe(websocket, ["network.responseCompleted"])

    await execute_command(
        websocket, {
            "method": "network.continueRequest",
            "params": {
                "request": network_id,
                "url": example_url,
            },
        })

    event_response = await wait_for_event(websocket,
                                          "network.responseCompleted")
    assert event_response == {
        "method": "network.responseCompleted",
        "params": {
            "context": context_id,
            "isBlocked": False,
            "navigation": ANY_STR,
            "redirectCount": 0,
            "request": ANY_DICT,
            "response": ANY_DICT,
            "timestamp": ANY_TIMESTAMP,
        },
        "type": "event",
    }


# TODO: Replace cdp.Network.loadingFailed with network.fetchError BiDi event when it is implemented?
