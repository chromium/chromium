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
from anys import ANY_DICT, ANY_STR
from test_helpers import (ANY_TIMESTAMP, execute_command, send_JSON_command,
                          subscribe, wait_for_event)


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
@pytest.mark.parametrize(
    "phase, url",
    [
        ("responseStarted", "https://www.example.com/"),  # TODO: make offline.
        pytest.param(
            "authRequired",
            "http://httpstat.us/401",
            marks=pytest.mark.skip(reason='TODO: Use our own test server.')),
    ],
    ids=["responseStarted", "authRequired"])
async def test_continue_request_invalid_phase(websocket, context_id, phase,
                                              url):

    network_id = await create_dummy_blocked_request(websocket,
                                                    context_id,
                                                    url=url,
                                                    phases=[phase])

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
                    "url": url,
                },
            })


@pytest.mark.asyncio
async def test_continue_request_invalid_url(websocket, context_id):
    # TODO: make offline.
    url = "https://www.example.com/"
    invalid_url = '%invalid%'

    network_id = await create_dummy_blocked_request(
        websocket, context_id, url=url, phases=["beforeRequestSent"])

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
@pytest.mark.skip(
    reason="TODO: replace url with a server whose requests hang forever.")
async def test_continue_request_non_blocked_request(websocket, context_id,
                                                    assert_no_events_in_queue):
    url = "http://127.0.0.1:5000/hang"

    await subscribe(websocket, [
        "network.beforeRequestSent", "cdp.Network.responseReceived",
        "cdp.Network.loadingFailed"
    ])

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "context": context_id,
                "url": url,
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
                    "url": url,
                },
            })


@pytest.mark.asyncio
async def test_continue_request_completes(websocket, context_id):
    # TODO: make offline.
    url = "https://www.example.com/"

    await subscribe(websocket, ["cdp.Fetch.requestPaused"])

    await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": ["beforeRequestSent"],
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

    await subscribe(websocket, ["network.responseCompleted"])

    await execute_command(
        websocket, {
            "method": "network.continueRequest",
            "params": {
                "request": network_id,
                "url": url,
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
async def test_continue_request_twice(websocket, context_id):
    # TODO: make offline.
    url = "https://www.example.com/"

    await subscribe(websocket, ["cdp.Fetch.requestPaused"])

    await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": ["beforeRequestSent"],
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

    await subscribe(websocket, ["network.responseCompleted"])

    await execute_command(
        websocket, {
            "method": "network.continueRequest",
            "params": {
                "request": network_id,
                "url": url,
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
                    "url": url,
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


# TODO: AuthRequired + exception test
# TODO: assertion with isBlocked: true
