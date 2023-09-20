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
from anys import ANY_DICT, ANY_NUMBER, ANY_STR
from test_helpers import (ANY_UUID, AnyExtending, execute_command,
                          send_JSON_command, subscribe, wait_for_event)


@pytest.mark.asyncio
async def test_fail_request_non_existent_request(websocket):
    with pytest.raises(Exception) as exception_info:
        await execute_command(
            websocket, {
                "method": "network.failRequest",
                "params": {
                    "request": '_UNKNOWN_',
                },
            })
    assert {
        "error": "no such request",
        "message": "No blocked request found for network id '_UNKNOWN_'"
    } == exception_info.value.args[0]


@pytest.mark.asyncio
async def test_fail_request_twice(websocket, context_id):
    # TODO: make offline.
    url = "https://www.example.com/"

    await subscribe(websocket, ["cdp.Fetch.requestPaused"])

    result = await execute_command(
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

    assert result == {
        "intercept": ANY_UUID,
    }

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url,
                "context": context_id,
            }
        })

    event_response = await wait_for_event(websocket, "cdp.Fetch.requestPaused")
    assert event_response == {
        "method": "cdp.Fetch.requestPaused",
        "params": {
            "event": "Fetch.requestPaused",
            "params": {
                "frameId": context_id,
                "networkId": ANY_STR,
                "request": AnyExtending({
                    "headers": ANY_DICT,
                    "url": url,
                }),
                "requestId": ANY_STR,
                "resourceType": "Document",
            },
            "session": ANY_STR,
        },
        "type": "event",
    }
    network_id = event_response["params"]["params"]["networkId"]

    result = await execute_command(websocket, {
        "method": "network.failRequest",
        "params": {
            "request": network_id,
        },
    })
    assert result == {}

    with pytest.raises(Exception) as exception_info:
        await execute_command(websocket, {
            "method": "network.failRequest",
            "params": {
                "request": network_id
            },
        })
    assert {
        "error": "no such request",
        "message": f"No blocked request found for network id '{network_id}'"
    } == exception_info.value.args[0]


@pytest.mark.asyncio
async def test_fail_request_completes(websocket, context_id):
    # TODO: make offline.
    url = "https://www.example.com/"

    await subscribe(websocket, ["cdp.Fetch.requestPaused"])

    result = await execute_command(
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

    assert result == {
        "intercept": ANY_UUID,
    }

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url,
                "context": context_id,
            }
        })

    event_response = await wait_for_event(websocket, "cdp.Fetch.requestPaused")
    assert event_response == {
        "method": "cdp.Fetch.requestPaused",
        "params": {
            "event": "Fetch.requestPaused",
            "params": {
                "frameId": context_id,
                "networkId": ANY_STR,
                "request": AnyExtending({
                    "headers": ANY_DICT,
                    "url": url,
                }),
                "requestId": ANY_STR,
                "resourceType": "Document",
            },
            "session": ANY_STR,
        },
        "type": "event",
    }
    network_id = event_response["params"]["params"]["networkId"]

    await subscribe(websocket, ["cdp.Network.loadingFailed"])

    result = await execute_command(websocket, {
        "method": "network.failRequest",
        "params": {
            "request": network_id,
        },
    })
    assert result == {}

    loading_failed_response = await wait_for_event(
        websocket, "cdp.Network.loadingFailed")
    assert loading_failed_response == {
        "method": "cdp.Network.loadingFailed",
        "params": {
            "event": "Network.loadingFailed",
            "params": {
                "canceled": False,
                "errorText": "net::ERR_FAILED",
                "requestId": network_id,
                "timestamp": ANY_NUMBER,
                "type": "Document",
            },
            "session": ANY_STR,
        },
        "type": "event",
    }


@pytest.mark.asyncio
async def test_fail_request_completes_new_request_still_blocks(
        websocket, context_id):
    # TODO: make offline.
    url = "https://www.example.com/"

    await subscribe(websocket, ["cdp.Fetch.requestPaused"])

    result = await execute_command(
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

    assert result == {
        "intercept": ANY_UUID,
    }

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url,
                "context": context_id,
            }
        })

    event_response1 = await wait_for_event(websocket,
                                           "cdp.Fetch.requestPaused")
    assert event_response1 == {
        "method": "cdp.Fetch.requestPaused",
        "params": {
            "event": "Fetch.requestPaused",
            "params": {
                "frameId": context_id,
                "networkId": ANY_STR,
                "request": AnyExtending({
                    "headers": ANY_DICT,
                    "url": url,
                }),
                "requestId": ANY_STR,
                "resourceType": "Document",
            },
            "session": ANY_STR,
        },
        "type": "event",
    }
    network_id_1 = event_response1["params"]["params"]["networkId"]

    await subscribe(websocket, ["cdp.Network.loadingFailed"])

    result = await execute_command(websocket, {
        "method": "network.failRequest",
        "params": {
            "request": network_id_1,
        },
    })
    assert result == {}

    loading_failed_response = await wait_for_event(
        websocket, "cdp.Network.loadingFailed")
    assert loading_failed_response == {
        "method": "cdp.Network.loadingFailed",
        "params": {
            "event": "Network.loadingFailed",
            "params": {
                "canceled": False,
                "errorText": "net::ERR_FAILED",
                "requestId": network_id_1,
                "timestamp": ANY_NUMBER,
                "type": "Document",
            },
            "session": ANY_STR,
        },
        "type": "event",
    }

    # Perform the same navigation again.
    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url,
                "context": context_id,
            }
        })

    event_response2 = await wait_for_event(websocket,
                                           "cdp.Fetch.requestPaused")
    assert event_response2 == {
        "method": "cdp.Fetch.requestPaused",
        "params": {
            "event": "Fetch.requestPaused",
            "params": {
                "frameId": context_id,
                "networkId": ANY_STR,
                "request": AnyExtending({
                    "headers": ANY_DICT,
                    "url": url,
                }),
                "requestId": ANY_STR,
                "resourceType": "Document",
            },
            "session": ANY_STR,
        },
        "type": "event",
    }
    network_id_2 = event_response2["params"]["params"]["networkId"]

    assert event_response1 != event_response2

    # The second request should have a different ID.
    assert network_id_1 != network_id_2


@pytest.mark.asyncio
async def test_fail_request_multiple_contexts(websocket, context_id,
                                              another_context_id):
    # TODO: make offline.
    url = "https://www.example.com/"

    await subscribe(websocket, ["cdp.Fetch.requestPaused"])

    result = await execute_command(
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

    assert result == {
        "intercept": ANY_UUID,
    }

    # Navigation in first context.
    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url,
                "context": context_id,
            }
        })

    event_response1 = await wait_for_event(websocket,
                                           "cdp.Fetch.requestPaused")
    assert event_response1 == {
        "method": "cdp.Fetch.requestPaused",
        "params": {
            "event": "Fetch.requestPaused",
            "params": {
                "frameId": context_id,
                "networkId": ANY_STR,
                "request": AnyExtending({
                    "headers": ANY_DICT,
                    "url": url,
                }),
                "requestId": ANY_STR,
                "resourceType": "Document",
            },
            "session": ANY_STR,
        },
        "type": "event",
    }
    network_id_1 = event_response1["params"]["params"]["networkId"]

    # Navigation in second context.
    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url,
                "context": another_context_id,
            }
        })

    event_response2 = await wait_for_event(websocket,
                                           "cdp.Fetch.requestPaused")
    assert event_response2 == {
        "method": "cdp.Fetch.requestPaused",
        "params": {
            "event": "Fetch.requestPaused",
            "params": {
                "frameId": another_context_id,
                "networkId": ANY_STR,
                "request": AnyExtending({
                    "headers": ANY_DICT,
                    "url": url,
                }),
                "requestId": ANY_STR,
                "resourceType": "Document",
            },
            "session": ANY_STR,
        },
        "type": "event",
    }
    network_id_2 = event_response2["params"]["params"]["networkId"]

    assert network_id_1 != network_id_2

    await subscribe(websocket, ["cdp.Network.loadingFailed"])

    # Fail the first request.
    result = await execute_command(websocket, {
        "method": "network.failRequest",
        "params": {
            "request": network_id_1,
        },
    })
    assert result == {}

    loading_failed_response = await wait_for_event(
        websocket, "cdp.Network.loadingFailed")
    assert loading_failed_response == {
        "method": "cdp.Network.loadingFailed",
        "params": {
            "event": "Network.loadingFailed",
            "params": {
                "canceled": False,
                "errorText": "net::ERR_FAILED",
                "requestId": network_id_1,
                "timestamp": ANY_NUMBER,
                "type": "Document",
            },
            "session": ANY_STR,
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

    loading_failed_response = await wait_for_event(
        websocket, "cdp.Network.loadingFailed")
    assert loading_failed_response == {
        "method": "cdp.Network.loadingFailed",
        "params": {
            "event": "Network.loadingFailed",
            "params": {
                "canceled": False,
                "errorText": "net::ERR_FAILED",
                "requestId": network_id_2,
                "timestamp": ANY_NUMBER,
                "type": "Document",
            },
            "session": ANY_STR,
        },
        "type": "event",
    }


# TODO: AuthRequired + exception test
