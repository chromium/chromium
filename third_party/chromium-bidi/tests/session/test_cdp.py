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

import asyncio
import json
from unittest.mock import ANY

import pytest
from anys import ANY_INT
from test_helpers import (
    ANY_TIMESTAMP,
    AnyExtending,
    execute_command,
    get_tree,
    goto_url,
    read_JSON_message,
    send_JSON_command,
    subscribe,
    wait_for_event,
    wait_for_filtered_event,
)


@pytest.mark.asyncio
async def test_cdp_sendCommand_resultReturned(websocket):
    command_result = await execute_command(
        websocket,
        {
            "method": "goog:cdp.sendCommand",
            "params": {"method": "Target.getTargets", "params": {}},
        },
    )

    assert {"result": {"targetInfos": ANY}} == command_result


@pytest.mark.asyncio
async def test_cdp_subscribe_toSpecificEvent(websocket, context_id, get_cdp_session_id):
    await subscribe(websocket, ["goog:cdp.Runtime.consoleAPICalled"])

    session_id = await get_cdp_session_id(context_id)

    await send_JSON_command(
        websocket,
        {
            "method": "goog:cdp.sendCommand",
            "params": {
                "method": "Runtime.evaluate",
                "params": {
                    "expression": "console.log(1)",
                },
                "session": session_id,
            },
        },
    )
    resp = await read_JSON_message(websocket)

    assert resp == AnyExtending(
        {
            "type": "event",
            "method": "goog:cdp.Runtime.consoleAPICalled",
            "params": {
                "event": "Runtime.consoleAPICalled",
                "params": {
                    "type": "log",
                    "args": [{"type": "number", "value": 1, "description": "1"}],
                    "executionContextId": ANY_INT,
                    "timestamp": ANY_TIMESTAMP,
                    "stackTrace": ANY,
                },
                "session": session_id,
            },
        }
    )


@pytest.mark.asyncio
async def test_cdp_subscribe_to_all_cdp_events(
    websocket, get_cdp_session_id, context_id
):
    await subscribe(websocket, ["goog:cdp"])

    session_id = await get_cdp_session_id(context_id)

    await send_JSON_command(
        websocket,
        {
            "method": "goog:cdp.sendCommand",
            "params": {
                "method": "Runtime.evaluate",
                "params": {
                    "expression": "console.log(1)",
                },
                "session": session_id,
            },
        },
    )

    resp = await wait_for_event(websocket, "goog:cdp.Runtime.consoleAPICalled")

    assert resp == AnyExtending(
        {
            "type": "event",
            "method": "goog:cdp.Runtime.consoleAPICalled",
            "params": {
                "event": "Runtime.consoleAPICalled",
                "params": {
                    "type": "log",
                    "args": [{"type": "number", "value": 1, "description": "1"}],
                    "executionContextId": ANY_INT,
                    "timestamp": ANY_TIMESTAMP,
                    "stackTrace": ANY,
                },
                "session": session_id,
            },
        }
    )


@pytest.mark.asyncio
async def test_cdp_wait_for_event(websocket, get_cdp_session_id, context_id):
    await subscribe(websocket, ["goog:cdp.Runtime.consoleAPICalled"])

    session_id = await get_cdp_session_id(context_id)

    await send_JSON_command(
        websocket,
        {
            "method": "goog:cdp.sendCommand",
            "params": {
                "method": "Runtime.evaluate",
                "params": {
                    "expression": "console.log(1)",
                },
                "session": session_id,
            },
        },
    )

    event_response = await wait_for_event(
        websocket, "goog:cdp.Runtime.consoleAPICalled"
    )
    assert event_response == AnyExtending(
        {
            "type": "event",
            "method": "goog:cdp.Runtime.consoleAPICalled",
            "params": {
                "event": "Runtime.consoleAPICalled",
                "params": {
                    "type": "log",
                    "args": [{"type": "number", "value": 1, "description": "1"}],
                    "executionContextId": ANY_INT,
                    "timestamp": ANY_TIMESTAMP,
                    "stackTrace": ANY,
                },
                "session": session_id,
            },
        }
    )


@pytest.mark.asyncio
async def test_cdp_no_extraneous_events(
    websocket, get_cdp_session_id, create_context, url_base
):
    new_context_id = await create_context()
    await execute_command(
        websocket,
        {
            "method": "browsingContext.navigate",
            "params": {"url": url_base, "wait": "complete", "context": new_context_id},
        },
    )

    await subscribe(websocket, ["goog:cdp"], [new_context_id])

    session_id = await get_cdp_session_id(new_context_id)

    id = await send_JSON_command(
        websocket,
        {
            "method": "goog:cdp.sendCommand",
            "params": {
                "method": "Target.attachToTarget",
                "params": {"targetId": new_context_id, "flatten": True},
                "session": session_id,
            },
        },
    )

    events = []
    event = await read_JSON_message(websocket)

    session_id = None
    with pytest.raises(asyncio.TimeoutError):
        while True:
            if "id" in event and event["id"] == id:
                session_id = event["result"]["result"]["sessionId"]
            if "id" not in event:
                events.append(event)
            event = await asyncio.wait_for(read_JSON_message(websocket), timeout=1.0)


@pytest.mark.asyncio
async def test_cdp_subscribe_browser_session(websocket, context_id):
    await subscribe(websocket, ["goog:cdp.Target.attachedToTarget"])

    await send_JSON_command(
        websocket,
        {
            "method": "goog:cdp.sendCommand",
            "params": {
                "method": "Target.attachToTarget",
                "params": {"targetId": context_id, "flatten": True},
            },
        },
    )

    event_response = await wait_for_event(websocket, "goog:cdp.Target.attachedToTarget")
    assert event_response == AnyExtending(
        {
            "type": "event",
            "method": "goog:cdp.Target.attachedToTarget",
            "params": {
                "event": "Target.attachedToTarget",
                "params": {
                    "sessionId": ANY,
                    "targetInfo": AnyExtending({"targetId": context_id}),
                    "waitingForDebugger": False,
                },
                "session": ANY,
            },
        }
    )


@pytest.mark.asyncio
async def test_cdp_subscribe_custom_session(websocket, context_id):
    attach_result = await execute_command(
        websocket,
        {
            "method": "goog:cdp.sendCommand",
            "params": {
                "method": "Target.attachToTarget",
                "params": {"targetId": context_id, "flatten": True},
            },
        },
    )
    custom_session_id = attach_result["result"]["sessionId"]

    await subscribe(websocket, ["goog:cdp.Runtime.executionContextCreated"])

    await send_JSON_command(
        websocket,
        {
            "method": "goog:cdp.sendCommand",
            "params": {
                "method": "Runtime.enable",
                "params": {},
                "session": custom_session_id,
            },
        },
    )

    event_response = await wait_for_event(
        websocket, "goog:cdp.Runtime.executionContextCreated"
    )
    assert event_response == AnyExtending(
        {
            "type": "event",
            "method": "goog:cdp.Runtime.executionContextCreated",
            "params": {
                "event": "Runtime.executionContextCreated",
                "params": {
                    "context": ANY,
                },
                "session": custom_session_id,
            },
        }
    )


@pytest.mark.asyncio
async def test_cdp_no_errors_on_non_page_targets(
    websocket,
    context_id,
    create_context,
    get_cdp_session_id,
    html,
    iframe,
):
    targets = await execute_command(
        websocket,
        {
            "method": "goog:cdp.sendCommand",
            "params": {"method": "Target.getTargets", "params": {}},
        },
    )
    mapper_target_id = next(
        t["targetId"]
        for t in targets["result"]["targetInfos"]
        if t["title"] == "BiDi-CDP Mapper"
    )
    session_id = await get_cdp_session_id(context_id)
    custom_session = await execute_command(
        websocket,
        {
            "method": "goog:cdp.sendCommand",
            "params": {
                "method": "Target.attachToTarget",
                "params": {"targetId": context_id, "flatten": True},
                "session": session_id,
            },
        },
    )
    mapper_session = await execute_command(
        websocket,
        {
            "method": "goog:cdp.sendCommand",
            "params": {
                "method": "Target.attachToTarget",
                "params": {"targetId": mapper_target_id, "flatten": True},
                "session": custom_session["result"]["sessionId"],
            },
        },
    )
    mapper_session_id = mapper_session["result"]["sessionId"]

    await execute_command(
        websocket,
        {
            "method": "goog:cdp.sendCommand",
            "params": {
                "method": "Runtime.evaluate",
                "params": {
                    "expression": """
                        window.cdpErrors = [];
                        const origSendDebugMessage = window.sendDebugMessage;
                        window.sendDebugMessage = (json) => {
                            origSendDebugMessage?.(json);
                            const data = JSON.parse(json);
                            if (data.logType === 'cdp:RECV ◂' && data.messages?.[0]?.error) {
                                window.cdpErrors.push(data.messages[0]);
                            }
                        };
                    """,
                },
                "session": mapper_session_id,
            },
        },
    )

    await subscribe(
        websocket,
        ["script.realmCreated", "bluetooth", "speculation"],
    )
    await execute_command(
        websocket,
        {
            "method": "browsingContext.setViewport",
            "params": {
                "userContexts": ["default"],
                "viewport": {"width": 500, "height": 400},
            },
        },
    )
    await execute_command(
        websocket,
        {
            "method": "emulation.setGeolocationOverride",
            "params": {
                "userContexts": ["default"],
                "coordinates": {"latitude": 10, "longitude": 20},
            },
        },
    )
    await execute_command(
        websocket,
        {
            "method": "emulation.setTouchOverride",
            "params": {"userContexts": ["default"], "maxTouchPoints": 5},
        },
    )
    await execute_command(
        websocket,
        {
            "method": "emulation.setMediaFeaturesOverride",
            "params": {
                "userContexts": ["default"],
                "features": {"prefers-color-scheme": "dark"},
            },
        },
    )
    await execute_command(
        websocket,
        {
            "method": "emulation.setScriptingEnabled",
            "params": {
                "userContexts": ["default"],
                "enabled": False,
            },
        },
    )
    await execute_command(
        websocket,
        {
            "method": "script.addPreloadScript",
            "params": {"functionDeclaration": "() => {}"},
        },
    )

    new_context_id = await create_context()
    oopif_url = html("<h1>OOPIF</h1>", same_origin=False)
    await goto_url(websocket, new_context_id, html(iframe(oopif_url)))

    await execute_command(
        websocket,
        {
            "method": "network.addIntercept",
            "params": {
                "phases": ["beforeRequestSent"],
                "urlPatterns": [{"type": "string", "pattern": "https://example.com/"}],
            },
        },
    )

    worker_url = "data:application/javascript,setInterval(() => {}, 1000)"
    await send_JSON_command(
        websocket,
        {
            "method": "script.evaluate",
            "params": {
                "expression": f"window.w = new Worker('{worker_url}')",
                "target": {"context": new_context_id},
                "awaitPromise": False,
            },
        },
    )

    await wait_for_filtered_event(
        websocket,
        lambda e: (
            e["method"] == "script.realmCreated"
            and e["params"]["type"] == "dedicated-worker"
        ),
    )

    errors_result = await execute_command(
        websocket,
        {
            "method": "goog:cdp.sendCommand",
            "params": {
                "method": "Runtime.evaluate",
                "params": {
                    "expression": "window.cdpErrors",
                    "returnByValue": True,
                },
                "session": mapper_session_id,
            },
        },
    )
    assert errors_result["result"]["result"]["value"] == []


@pytest.mark.asyncio
async def test_cdp_ignores_events_from_non_owning_session(
    websocket,
    context_id,
    iframe_id,
    create_context,
    get_cdp_session_id,
    html,
):
    # Create a second top-level context and move the iframe cross-origin so
    # that `iframe_id` and `second_context_id` run in separate CDP sessions.
    second_context_id = await create_context()
    await goto_url(
        websocket, iframe_id, html("<h1>CHILD</h1>", same_origin=False), "complete"
    )

    iframe_session_id = await get_cdp_session_id(iframe_id)
    second_session_id = await get_cdp_session_id(second_context_id)

    # Attach to the BiDi-CDP Mapper target via a custom page session (so
    # CdpTargetManager does not auto-detach the mapper target) to invoke
    # `window.cdp.onmessage` with synthetic CDP events directly.
    targets = await execute_command(
        websocket,
        {
            "method": "goog:cdp.sendCommand",
            "params": {"method": "Target.getTargets", "params": {}},
        },
    )
    mapper_target_id = next(
        t["targetId"]
        for t in targets["result"]["targetInfos"]
        if t["title"] == "BiDi-CDP Mapper"
    )
    custom_session = await execute_command(
        websocket,
        {
            "method": "goog:cdp.sendCommand",
            "params": {
                "method": "Target.attachToTarget",
                "params": {"targetId": context_id, "flatten": True},
            },
        },
    )
    mapper_session = await execute_command(
        websocket,
        {
            "method": "goog:cdp.sendCommand",
            "params": {
                "method": "Target.attachToTarget",
                "params": {"targetId": mapper_target_id, "flatten": True},
                "session": custom_session["result"]["sessionId"],
            },
        },
    )
    mapper_session_id = mapper_session["result"]["sessionId"]

    # Emit synthetic CDP events from `iframe_session_id` targeting contexts and
    # sessions that `iframe_session_id` does not own.
    synthetic_events = [
        # `context_id` is already tracked (and owned by the parent session);
        # re-attaching it under `iframe_id` would create a parent cycle.
        {
            "method": "Page.frameAttached",
            "params": {
                "frameId": context_id,
                "parentFrameId": iframe_id,
            },
            "sessionId": iframe_session_id,
        },
        # `second_context_id` is owned by `second_session_id`, not `iframe_session_id`.
        {
            "method": "Page.frameAttached",
            "params": {
                "frameId": "synthetic-frame",
                "parentFrameId": second_context_id,
            },
            "sessionId": iframe_session_id,
        },
        # `second_context_id` cannot be detached by `iframe_session_id`.
        {
            "method": "Page.frameSubtreeWillBeDetached",
            "params": {
                "frameId": second_context_id,
            },
            "sessionId": iframe_session_id,
        },
        # `second_session_id` is already active and cannot be replaced.
        {
            "method": "Target.attachedToTarget",
            "params": {
                "sessionId": second_session_id,
                "targetInfo": {
                    "targetId": "synthetic-target",
                    "type": "page",
                    "title": "",
                    "url": "about:blank",
                    "attached": True,
                    "canAccessOpener": False,
                },
                "waitingForDebugger": False,
            },
            "sessionId": iframe_session_id,
        },
        # `second_session_id` was attached by the browser session, not `iframe_session_id`.
        {
            "method": "Target.detachedFromTarget",
            "params": {
                "sessionId": second_session_id,
                "targetId": second_context_id,
            },
            "sessionId": iframe_session_id,
        },
    ]

    for event in synthetic_events:
        await execute_command(
            websocket,
            {
                "method": "goog:cdp.sendCommand",
                "params": {
                    "method": "Runtime.evaluate",
                    "params": {
                        "expression": f"window.cdp.onmessage('{json.dumps(event)}')",
                    },
                    "session": mapper_session_id,
                },
            },
        )

    # Verify the browsing context tree is intact and `second_context_id` still works.
    tree = await get_tree(websocket)
    contexts_by_id = {c["context"]: c for c in tree["contexts"]}
    assert set(contexts_by_id.keys()) == {context_id, second_context_id}
    assert [c["context"] for c in contexts_by_id[context_id]["children"]] == [iframe_id]
    assert contexts_by_id[second_context_id]["children"] == []

    eval_result = await execute_command(
        websocket,
        {
            "method": "script.evaluate",
            "params": {
                "expression": "1 + 1",
                "awaitPromise": False,
                "target": {"context": second_context_id},
            },
        },
    )
    assert eval_result["result"] == {"type": "number", "value": 2}
