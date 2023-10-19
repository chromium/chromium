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
import re

import pytest
from anys import ANY_DICT, ANY_LIST, ANY_STR
from test_helpers import (ANY_TIMESTAMP, ANY_UUID, AnyExtending,
                          execute_command, send_JSON_command, subscribe,
                          wait_for_event)

from . import create_blocked_request


@pytest.mark.asyncio
async def test_continue_response_non_existent_request(websocket):
    with pytest.raises(
            Exception,
            match=str({
                "error": "no such request",
                "message": "No blocked request found for network id '_UNKNOWN_'"
            })):
        await execute_command(
            websocket, {
                "method": "network.continueResponse",
                "params": {
                    "request": '_UNKNOWN_',
                },
            })


@pytest.mark.asyncio
async def test_continue_response_invalid_phase(websocket, context_id,
                                               example_url):
    network_id = await create_blocked_request(websocket,
                                              context_id,
                                              url=example_url,
                                              phases=["beforeRequestSent"])

    with pytest.raises(
            Exception,
            match=str({
                "error": "invalid argument",
                "message": f"Blocked request for network id '{network_id}' is in 'BeforeRequestSent' phase"
            })):
        await execute_command(
            websocket, {
                "method": "network.continueResponse",
                "params": {
                    "request": network_id
                },
            })


@pytest.mark.asyncio
async def test_continue_response_invalid_status_code(websocket, context_id,
                                                     example_url):
    network_id = await create_blocked_request(websocket,
                                              context_id,
                                              url=example_url,
                                              phases=["responseStarted"])

    with pytest.raises(
            Exception,
            match=re.compile(
                str({
                    "error": "invalid argument",
                    "message": 'Number must be greater than or equal to 0 in "statusCode".*'
                }))):
        await execute_command(
            websocket, {
                "method": "network.continueResponse",
                "params": {
                    "request": network_id,
                    "statusCode": -1,
                },
            })


@pytest.mark.asyncio
async def test_continue_response_invalid_reason_phrase(websocket, context_id,
                                                       example_url):
    network_id = await create_blocked_request(websocket,
                                              context_id,
                                              url=example_url,
                                              phases=["responseStarted"])

    with pytest.raises(
            Exception,
            match=str({
                "error": "invalid argument",
                "message": 'Expected string, received array in "reasonPhrase".'
            })):
        await execute_command(
            websocket, {
                "method": "network.continueResponse",
                "params": {
                    "request": network_id,
                    "reasonPhrase": [],
                },
            })


@pytest.mark.asyncio
async def test_continue_response_invalid_headers(websocket, context_id,
                                                 example_url):
    network_id = await create_blocked_request(websocket,
                                              context_id,
                                              url=example_url,
                                              phases=["responseStarted"])

    with pytest.raises(
            Exception,
            match=str({
                "error": "invalid argument",
                "message": 'Expected array, received string in "headers".'
            })):
        await execute_command(
            websocket, {
                "method": "network.continueResponse",
                "params": {
                    "request": network_id,
                    "headers": "",
                },
            })


@pytest.mark.asyncio
async def test_continue_response_non_blocked_request(websocket, context_id,
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
                "method": "network.continueResponse",
                "params": {
                    "request": network_id,
                },
            })


@pytest.mark.parametrize("continueResponseParams", [
    {
        "headers": [{
            "name": "header1",
            "value": {
                "type": "string",
                "value": "value1",
            }
        }]
    },
    {
        "statusCode": 401,
    },
],
                         ids=["headers-only", "statusCode-only"])
@pytest.mark.asyncio
async def test_continue_response_must_specify_both_status_and_headers(
        websocket, context_id, example_url, continueResponseParams):
    await subscribe(websocket, ["network.beforeRequestSent"], [context_id])

    await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": ["responseStarted"],
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
                "wait": "complete",
            }
        })

    event_response = await wait_for_event(websocket,
                                          "network.beforeRequestSent")
    network_id = event_response["params"]["request"]["request"]

    await subscribe(websocket, ["network.responseCompleted"])

    # TODO(https://github.com/w3c/webdriver-bidi/issues/572): Follow up with spec.
    with pytest.raises(
            Exception,
            match=str({
                "error": "unknown error",
                "message": "Cannot override only status or headers, both should be provided"
            })):
        await execute_command(
            websocket, {
                "method": "network.continueResponse",
                "params": {
                    "request": network_id,
                    "reasonPhrase": "Remember to drink water",
                } | continueResponseParams,
            })


@pytest.mark.asyncio
async def test_continue_response_completes_use_cdp_events(
        websocket, context_id, example_url):
    await subscribe(websocket, ["cdp.Fetch.requestPaused"])

    await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": ["responseStarted"],
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
            "method": "network.continueResponse",
            "params": {
                "request": network_id,
                "statusCode": 501,
                "reasonPhrase": "Remember to drink water",
                "headers": [{
                    "name": "header1",
                    "value": {
                        "type": "string",
                        "value": "value1",
                    }
                }],
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
            "response": AnyExtending({
                "headers": [{
                    "name": "header1",
                    "value": {
                        "type": "string",
                        "value": "value1",
                    },
                }, ],
                "url": example_url,
                "status": 200,  # TODO: Update status code to 501?
                "statusText": "Remember to drink water",
            }),
            "timestamp": ANY_TIMESTAMP,
        },
        "type": "event",
    }


@pytest.mark.asyncio
async def test_continue_response_completes_use_bidi_events(
        websocket, context_id, example_url):
    await subscribe(websocket, ["network.beforeRequestSent"], [context_id])

    await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": ["responseStarted"],
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
            "isBlocked": False,
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
            "method": "network.continueResponse",
            "params": {
                "request": network_id,
                "statusCode": 501,
                "reasonPhrase": "Remember to drink water",
                "headers": [{
                    "name": "header1",
                    "value": {
                        "type": "string",
                        "value": "value1",
                    }
                }],
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
            "response": AnyExtending({
                "headers": [{
                    "name": "header1",
                    "value": {
                        "type": "string",
                        "value": "value1",
                    },
                }, ],
                "url": example_url,
                "status": 200,  # TODO: Update status code to 501?
                "statusText": "Remember to drink water",
            }),
            "timestamp": ANY_TIMESTAMP,
        },
        "type": "event",
    }


@pytest.mark.asyncio
async def test_continue_response_twice(websocket, context_id, example_url):
    await subscribe(websocket, ["network.beforeRequestSent"], [context_id])

    await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": ["responseStarted"],
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
            "isBlocked": False,
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
            "method": "network.continueResponse",
            "params": {
                "request": network_id,
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
                "method": "network.continueResponse",
                "params": {
                    "request": network_id,
                },
            })


@pytest.mark.asyncio
async def test_continue_response_remove_intercept_inflight_request(
        websocket, context_id, example_url):
    await subscribe(websocket, ["network.beforeRequestSent"], [context_id])

    result = await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": ["responseStarted"],
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
            "isBlocked": False,
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
            "method": "network.continueResponse",
            "params": {
                "request": network_id,
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
