# Copyright 2021 Google LLC.
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

import re
from unittest.mock import ANY

import pytest
from anys import ANY_DICT, ANY_STR, AnyWithEntries
from test_helpers import (ANY_TIMESTAMP, AnyExtending, execute_command,
                          get_next_command_id, read_JSON_message,
                          send_JSON_command, subscribe)


@pytest.mark.asyncio
async def test_subscribeForUnknownContext_exceptionReturned(websocket):
    with pytest.raises(Exception,
                       match=str({
                           'error': 'no such frame',
                           'message': 'Context UNKNOWN_CONTEXT_ID not found'
                       })):
        await execute_command(
            websocket, {
                "method": "session.subscribe",
                "params": {
                    "events": ["browsingContext.load"],
                    "contexts": ["UNKNOWN_CONTEXT_ID"]
                }
            })


@pytest.mark.asyncio
async def test_subscribeWithoutContext_subscribesToEventsInAllContexts(
        websocket, context_id, html):
    result = await execute_command(
        websocket, {
            "method": "session.subscribe",
            "params": {
                "events": ["browsingContext.load"]
            }
        })
    assert result == {}

    # Navigate to some page.
    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": html("<h2>test</h2>"),
                "wait": "complete",
                "context": context_id
            }
        })

    # Wait for `browsingContext.load` event.
    resp = await read_JSON_message(websocket)
    assert resp["method"] == "browsingContext.load"
    assert resp["params"]["context"] == context_id


@pytest.mark.asyncio
async def test_subscribeWithContext_subscribesToEventsInGivenContext(
        websocket, context_id, html):
    result = await execute_command(
        websocket, {
            "method": "session.subscribe",
            "params": {
                "events": ["browsingContext.load"],
                "contexts": [context_id]
            }
        })
    assert result == {}

    # Navigate to some page.
    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": html("<h2>test</h2>"),
                "wait": "complete",
                "context": context_id
            }
        })

    # Wait for `browsingContext.load` event.
    resp = await read_JSON_message(websocket)
    assert resp["method"] == "browsingContext.load"
    assert resp["params"]["context"] == context_id


@pytest.mark.asyncio
async def test_subscribeWithContext_subscribesToEventsInNestedContext(
        websocket, context_id, html, iframe, url_all_origins):
    await subscribe(websocket, ["browsingContext.contextCreated"])

    # Navigate to some page.
    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": html(iframe(url_all_origins)),
                "wait": "complete",
                "context": context_id
            }
        })

    # Wait for `browsingContext.load` event.
    resp = await read_JSON_message(websocket)
    assert {
        "type": "event",
        "method": "browsingContext.contextCreated",
        "params": {
            "context": ANY_STR,
            # The `url` is always `about:blank`, as the navigation has not
            # happened yet. https://github.com/w3c/webdriver-bidi/issues/220.
            "url": "about:blank",
            "children": None,
            "parent": context_id,
            "userContext": "default",
            'clientWindow': ANY_STR,
            "originalOpener": None
        }
    } == resp


@pytest.mark.asyncio
async def test_subscribeToNestedContext_subscribesToTopLevelContext(
        websocket, context_id, iframe_id):
    await subscribe(websocket, ["log.entryAdded"], [iframe_id])

    await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "console.log('SOME_MESSAGE')",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True
            }
        })

    # Assert event received.
    resp = await read_JSON_message(websocket)
    assert {
        "type": "event",
        "method": "log.entryAdded",
        "params": ANY,
    } == resp


@pytest.mark.asyncio
async def test_subscribeToNestedContextAndUnsubscribeFromTopLevelContext_unsubscribedFromTopLevelContext(
        websocket, context_id, iframe_id):
    await subscribe(websocket, ["log.entryAdded"], [iframe_id])
    await execute_command(
        websocket, {
            "method": "session.unsubscribe",
            "params": {
                "events": ["log.entryAdded"],
                "contexts": [context_id]
            }
        })

    # Assert unsubscribed from top level context.
    command_id = await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "console.log('SOME_MESSAGE')",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True
            }
        })

    # Assert evaluate script is ended without any events before.
    resp = await read_JSON_message(websocket)
    assert {"type": "success", "id": command_id, 'result': ANY} == resp

    # Assert unsubscribed from nested context.
    command_id = await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "console.log('SOME_MESSAGE')",
                "target": {
                    "context": iframe_id
                },
                "awaitPromise": True
            }
        })

    # Assert evaluate script is ended without any events before.
    resp = await read_JSON_message(websocket)
    assert {"type": "success", "id": command_id, 'result': ANY} == resp


@pytest.mark.asyncio
async def test_subscribeToTopLevelContextAndUnsubscribeFromNestedContext_unsubscribedFromTopLevelContext(
        websocket, context_id, iframe_id):
    await subscribe(websocket, ["log.entryAdded"], [context_id])
    await execute_command(
        websocket, {
            "method": "session.unsubscribe",
            "params": {
                "events": ["log.entryAdded"],
                "contexts": [iframe_id]
            }
        })

    # Assert unsubscribed from top level context.
    command_id = await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "console.log('SOME_MESSAGE')",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True
            }
        })

    # Assert evaluate script is ended without any events before.
    resp = await read_JSON_message(websocket)
    assert {"type": "success", "id": command_id, 'result': ANY} == resp

    # Assert unsubscribed from nested context.
    command_id = await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "console.log('SOME_MESSAGE')",
                "target": {
                    "context": iframe_id
                },
                "awaitPromise": True
            }
        })

    # Assert evaluate script is ended without any events before.
    resp = await read_JSON_message(websocket)
    assert {"type": "success", "id": command_id, 'result': ANY} == resp


@pytest.mark.asyncio
async def test_subscribeWithContext_doesNotSubscribeToEventsInAnotherContexts(
        websocket, context_id, another_context_id, html):
    # 1. Get 2 contexts.
    # 2. Subscribe to event `browsingContext.load` on the first one.
    # 3. Navigate waiting complete loading in both contexts.
    # 4. Verify `browsingContext.load` emitted only for the first context.

    await subscribe(websocket, ["browsingContext.load"], [context_id])

    # 3.1 Navigate first context.
    command_id_1 = get_next_command_id()
    await send_JSON_command(
        websocket, {
            "id": command_id_1,
            "method": "browsingContext.navigate",
            "params": {
                "url": html("<h2>test</h2>"),
                "wait": "complete",
                "context": context_id
            }
        })
    # 4.1 Verify `browsingContext.load` is emitted for the first context.
    resp = await read_JSON_message(websocket)
    assert resp["method"] == "browsingContext.load"
    assert resp["params"]["context"] == context_id

    # Verify first navigation finished.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == command_id_1

    # 3.2 Navigate second context.
    command_id_2 = get_next_command_id()
    assert command_id_1 != command_id_2
    await send_JSON_command(
        websocket, {
            "id": command_id_2,
            "method": "browsingContext.navigate",
            "params": {
                "url": html("<h2>test</h2>"),
                "wait": "complete",
                "context": another_context_id
            }
        })

    # 4.2 Verify second navigation finished without emitting
    # `browsingContext.load`.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == command_id_2


@pytest.mark.asyncio
async def test_subscribeToOneChannel_eventReceivedWithProperChannel(
        websocket, context_id):
    # Subscribe and unsubscribe in `CHANNEL_1`.
    await execute_command(
        websocket, {
            "method": "session.subscribe",
            "channel": "CHANNEL_1",
            "params": {
                "events": ["log.entryAdded"]
            }
        })
    await execute_command(
        websocket, {
            "method": "session.unsubscribe",
            "channel": "CHANNEL_1",
            "params": {
                "events": ["log.entryAdded"]
            }
        })

    # Subscribe in `CHANNEL_2`.
    await execute_command(
        websocket, {
            "method": "session.subscribe",
            "channel": "CHANNEL_2",
            "params": {
                "events": ["log.entryAdded"]
            }
        })

    await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "console.log('SOME_MESSAGE')",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True
            }
        })

    # Assert event received in `CHANNEL_2`.
    resp = await read_JSON_message(websocket)
    assert {
        "type": "event",
        "method": "log.entryAdded",
        "params": ANY_DICT,
        "channel": "CHANNEL_2"
    } == resp


@pytest.mark.asyncio
async def test_subscribeToMultipleChannels_eventsReceivedInProperOrder(
        websocket, context_id):
    empty_channel = ""
    channel_2 = "999_SECOND_SUBSCRIBED_CHANNEL"
    channel_3 = "000_THIRD_SUBSCRIBED_CHANNEL"
    channel_4 = "555_FOURTH_SUBSCRIBED_CHANNEL"

    await subscribe(websocket, ["log.entryAdded"], None, empty_channel)
    await subscribe(websocket, ["log.entryAdded"], None, channel_2)
    await subscribe(websocket, ["log.entryAdded"], [context_id], channel_3)
    await subscribe(websocket, ["log.entryAdded"], [context_id], channel_4)
    # Re-subscribe with specific BrowsingContext.
    await subscribe(websocket, ["log.entryAdded"], [context_id], channel_3)
    # Re-subscribe.
    await subscribe(websocket, ["log.entryAdded"], None, channel_2)

    await execute_command(
        websocket, {
            "method": "session.subscribe",
            "channel": channel_3,
            "params": {
                "events": ["log.entryAdded"]
            }
        })
    # Subscribe with a context. The initial subscription should still have
    # higher priority.
    await execute_command(
        websocket, {
            "method": "session.subscribe",
            "channel": channel_2,
            "params": {
                "events": ["log.entryAdded"],
                "context": context_id
            }
        })

    await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "channel": "SOME_OTHER_CHANNEL",
            "params": {
                "expression": "console.log('SOME_MESSAGE')",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True
            }
        })

    # Empty string channel is considered as no channel provided.
    resp = await read_JSON_message(websocket)
    assert {
        "type": "event",
        "method": "log.entryAdded",
        "params": ANY_DICT
    } == resp

    resp = await read_JSON_message(websocket)
    assert {
        "type": "event",
        "method": "log.entryAdded",
        "channel": channel_2,
        "params": ANY_DICT
    } == resp

    resp = await read_JSON_message(websocket)
    assert {
        "type": "event",
        "method": "log.entryAdded",
        "channel": channel_3,
        "params": ANY_DICT
    } == resp

    resp = await read_JSON_message(websocket)
    assert {
        "type": "event",
        "method": "log.entryAdded",
        "channel": channel_4,
        "params": ANY_DICT
    } == resp


@pytest.mark.asyncio
async def test_subscribeWithoutContext_bufferedEventsFromNotClosedContextsAreReturned(
        websocket, context_id, another_context_id):
    await execute_command(
        websocket, {
            "method": "script.evaluate",
            "channel": "SOME_OTHER_CHANNEL",
            "params": {
                "expression": "console.log('SOME_MESSAGE')",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True
            }
        })

    await execute_command(
        websocket, {
            "method": "script.evaluate",
            "channel": "SOME_OTHER_CHANNEL",
            "params": {
                "expression": "console.log('ANOTHER_MESSAGE')",
                "target": {
                    "context": another_context_id
                },
                "awaitPromise": True
            }
        })

    await execute_command(
        websocket, {
            "method": "browsingContext.close",
            "params": {
                "context": another_context_id
            }
        })

    command_id = await send_JSON_command(websocket, {
        "method": "session.subscribe",
        "params": {
            "events": ["log.entryAdded"]
        }
    })

    # Assert only message from not closed context is received.
    resp = await read_JSON_message(websocket)

    assert {
        "type": "event",
        "method": "log.entryAdded",
        "params": {
            "level": "info",
            "source": {
                "realm": ANY,
                "context": context_id
            },
            "text": "SOME_MESSAGE",
            "timestamp": ANY_TIMESTAMP,
            "stackTrace": ANY,
            "type": "console",
            "method": "log",
            "args": [{
                "type": "string",
                "value": "SOME_MESSAGE"
            }]
        }
    } == resp

    # Assert no more events were buffered.
    resp = await read_JSON_message(websocket)
    assert {"type": "success", "id": command_id, 'result': ANY} == resp


@pytest.mark.asyncio
async def test_unsubscribeIsAtomic(websocket, context_id, iframe_id):
    await subscribe(websocket, ["log.entryAdded"], [iframe_id])

    with pytest.raises(
            Exception,
            match=re.compile(
                str({
                    "error": "invalid argument",
                    "message": 'Cannot unsubscribe from network.responseCompleted, .*. No subscription found.'
                }))):
        await execute_command(
            websocket,
            {
                "method": "session.unsubscribe",
                "params": {
                    "events": [
                        "log.entryAdded",
                        # This event is not subscribed.
                        "network.responseCompleted",
                    ],
                    "contexts": [context_id]
                }
            })

    await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "console.log('SOME_MESSAGE')",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True
            }
        })

    # Assert evaluate script ended with an event before, as log.entryAdded was not unsubscribed.
    resp = await read_JSON_message(websocket)
    assert AnyWithEntries({
        'type': 'event',
        'method': 'log.entryAdded',
        'params': AnyWithEntries({'text': 'SOME_MESSAGE'})
    }) == resp


@pytest.mark.asyncio
async def test_unsubscribe_from_detached_target(websocket, context_id,
                                                read_sorted_messages):
    events = [
        'bluetooth', 'browser', 'browsingContext', 'cdp', 'input', 'log',
        'network', 'script', 'session'
    ]

    await subscribe(websocket, events)

    close_command_id = await send_JSON_command(websocket, {
        "method": "browsingContext.close",
        "params": {
            "context": context_id
        }
    })

    unsubscribe_command_id = await send_JSON_command(websocket, {
        "method": "session.unsubscribe",
        "params": {
            "events": events
        }
    })

    # Read only command responses ignoring events previously subscribed.
    [close_command_response, unsubscribe_command_response
     ] = await read_sorted_messages(2, lambda message: "id" in message)
    assert close_command_response == AnyExtending({
        "id": close_command_id,
        "type": "success"
    })
    assert unsubscribe_command_response == AnyExtending({
        "id": unsubscribe_command_id,
        "type": "success"
    })
