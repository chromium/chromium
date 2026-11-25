# Copyright 2026 Google LLC.
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

"""E2E tests for running several BiDi Mapper instances in one Mapper tab.

Uses the `create_mapper` and `mapper_tab_cdp` fixtures from `conftest.py` to
start additional Mapper instances inside the primary "BiDi-CDP Mapper" tab.
"""

import asyncio
import json

import pytest
from anys import ANY_STR
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


async def wait_for_command_and_event(
    websocket, command_id: int, event_method: str, timeout: int = 20
) -> tuple[dict, dict]:
    """Waits for both a command result and an event on the same connection,
    regardless of the order in which they arrive.

    The event is matched by method prefix. Other messages read in the meantime
    are dropped.
    """
    command_result = None
    event_response = None

    async def _wait():
        nonlocal command_result, event_response
        while command_result is None or event_response is None:
            resp = await read_JSON_message(websocket)
            if resp.get("id") == command_id:
                if "result" in resp:
                    command_result = resp["result"]
                else:
                    raise Exception(
                        {"error": resp["error"], "message": resp["message"]}
                    )
            elif resp.get("type") == "event" and resp.get("method", "").startswith(
                event_method
            ):
                event_response = resp

    await asyncio.wait_for(_wait(), timeout)
    assert command_result is not None
    assert event_response is not None
    return command_result, event_response


@pytest.mark.asyncio
async def test_multiple_mappers_browsing_context_get_tree(
    websocket, create_mapper, context_id
):
    """All mappers see the same browsing context tree, and a context created by
    one Mapper is reported to the others.
    """
    mapper_1 = await create_mapper()
    mapper_2 = await create_mapper()

    # Initially, all mappers (including the primary one) see only the default
    # tab.
    tree_primary = await get_tree(websocket)
    tree_1 = await get_tree(mapper_1)
    tree_2 = await get_tree(mapper_2)

    assert [c["context"] for c in tree_primary["contexts"]] == [context_id]
    assert [c["context"] for c in tree_1["contexts"]] == [context_id]
    assert [c["context"] for c in tree_2["contexts"]] == [context_id]

    # `mapper_1` creates a tab. The primary Mapper and `mapper_2` are notified.
    await subscribe(websocket, ["browsingContext.contextCreated"])
    await subscribe(mapper_2, ["browsingContext.contextCreated"])

    create_res = await execute_command(
        mapper_1,
        {"method": "browsingContext.create", "params": {"type": "tab"}},
    )
    second_context_id = create_res["context"]

    created_primary, created_2 = await asyncio.gather(
        wait_for_event(websocket, "browsingContext.contextCreated"),
        wait_for_event(mapper_2, "browsingContext.contextCreated"),
    )
    assert created_primary["params"]["context"] == second_context_id
    assert created_2["params"]["context"] == second_context_id

    # Now all mappers see both tabs.
    tree_primary_after = await get_tree(websocket)
    tree_1_after = await get_tree(mapper_1)
    tree_2_after = await get_tree(mapper_2)

    expected_contexts = {context_id, second_context_id}
    assert {c["context"] for c in tree_primary_after["contexts"]} == expected_contexts
    assert {c["context"] for c in tree_1_after["contexts"]} == expected_contexts
    assert {c["context"] for c in tree_2_after["contexts"]} == expected_contexts


@pytest.mark.asyncio
async def test_multiple_mappers_default_transports(
    websocket, mapper_tab_cdp, create_mapper, context_id, html
):
    """Checks the default transports of `runMapperInstance` and the cleanup:

    1. The default `WindowBidiTransport`, used by the primary Mapper, cannot be
       used by a second Mapper.
    2. If the Mapper fails to start, its browser session is detached and its
       custom CDP transport is closed.
    3. A Mapper without a custom CDP transport shares `window.cdp` with the
       primary Mapper, and both work.
    4. A closed Mapper detaches its browser session: its preload scripts are
       not applied to new tabs, and the primary Mapper keeps working.
    """
    control_cdp = mapper_tab_cdp["control_cdp"]
    mapper_target_id = mapper_tab_cdp["mapper_target_id"]
    mapper_session_id = mapper_tab_cdp["mapper_session_id"]

    # 1. Re-binding the singleton default WindowBidiTransport must fail rather
    # than overwriting the primary ChromeDriver Mapper's message handler.
    dup_res = await control_cdp.send_command(
        "Runtime.evaluate",
        {
            "expression": f"window.runMapperInstance({json.dumps(mapper_target_id)})",
            "awaitPromise": True,
        },
        session_id=mapper_session_id,
    )
    assert "exceptionDetails" in dup_res
    assert "Default WindowBidiTransport is already in use" in dup_res["result"].get(
        "description", ""
    )

    # 2. If `BidiServer.createAndStart` rejects after `createBrowserSession`
    # succeeds on a custom CDP transport, `runMapperInstance` must detach that
    # browser session and close the custom CDP transport.
    failed_init_res = await control_cdp.send_command(
        "Runtime.evaluate",
        {
            "expression": f"""
                (async () => {{
                    let onCdp = null;
                    let cdpClosed = false;
                    let detachedSessionId = null;
                    // Fake CDP transport, no browser involved:
                    // - `Target.attachToBrowserTarget` succeeds with session S1;
                    // - `Target.detachFromTarget` is recorded;
                    // - all other commands fail, so the Mapper fails to start
                    //   after its browser session was created.
                    const failingCdpTransport = {{
                        setOnMessage(handler) {{ onCdp = handler; }},
                        sendMessage(msg) {{
                            const {{id, method, params}} = JSON.parse(msg);
                            if (method === 'Target.attachToBrowserTarget') {{
                                // Reply asynchronously like a real transport.
                                // Like Chromium, send the attach event before
                                // the command result.
                                queueMicrotask(() => {{
                                    onCdp?.(JSON.stringify({{
                                        method: 'Target.attachedToTarget',
                                        params: {{sessionId: 'S1'}},
                                    }}));
                                    onCdp?.(JSON.stringify({{
                                        id,
                                        result: {{sessionId: 'S1'}},
                                    }}));
                                }});
                            }} else if (method === 'Target.detachFromTarget') {{
                                detachedSessionId = params.sessionId;
                            }} else {{
                                queueMicrotask(() => onCdp?.(JSON.stringify({{
                                    id,
                                    error: {{code: -32000, message: 'init failed'}},
                                }})));
                            }}
                        }},
                        close() {{ cdpClosed = true; }},
                    }};
                    const dummyBidiTransport = {{
                        setOnMessage() {{}},
                        sendMessage() {{}},
                        close() {{}},
                    }};
                    let errorMessage = null;
                    try {{
                        await window.runMapperInstance(
                            {json.dumps(mapper_target_id)},
                            dummyBidiTransport,
                            failingCdpTransport,
                        );
                    }} catch (err) {{
                        errorMessage = err?.message ?? String(err);
                    }}
                    return {{errorMessage, cdpClosed, detachedSessionId}};
                }})()
            """,
            "awaitPromise": True,
            "returnByValue": True,
        },
        session_id=mapper_session_id,
    )
    assert "exceptionDetails" not in failed_init_res, failed_init_res
    failed_init_val = failed_init_res["result"]["value"]
    assert failed_init_val["cdpClosed"] is True
    assert failed_init_val["detachedSessionId"] == "S1"
    assert (
        failed_init_val["errorMessage"] is not None
        and "init failed" in failed_init_val["errorMessage"]
    )

    # 3. Omitting `cdpTransport` reuses `windowCdpConnection` without clobbering
    # the primary ChromeDriver `websocket` session.
    secondary_mapper = await create_mapper(custom_cdp_transport=False)
    custom_cdp_mapper = await create_mapper(custom_cdp_transport=True)

    # `secondary_mapper` creates a tab: both it and the primary Mapper report
    # it, so both work over the shared `window.cdp`.
    await subscribe(websocket, ["browsingContext.contextCreated"])
    await subscribe(secondary_mapper, ["browsingContext.contextCreated"])

    create_cmd_id = await send_JSON_command(
        secondary_mapper,
        {"method": "browsingContext.create", "params": {"type": "tab"}},
    )

    created_primary, (create_res, created_secondary) = await asyncio.gather(
        wait_for_event(websocket, "browsingContext.contextCreated"),
        wait_for_command_and_event(
            secondary_mapper, create_cmd_id, "browsingContext.contextCreated"
        ),
    )
    second_context_id = create_res["context"]
    assert created_primary["params"]["context"] == second_context_id
    assert created_secondary["params"]["context"] == second_context_id

    # 4. Register preload scripts on both secondary mappers before closing them;
    # if `close()` fails to detach their Browser CDP sessions, they would still
    # auto-attach to `third_context_id` and inject these globals on navigation.
    await execute_command(
        secondary_mapper,
        {
            "method": "script.addPreloadScript",
            "params": {
                "functionDeclaration": "() => { window.__closedSharedMapper = true; }"
            },
        },
    )
    await execute_command(
        custom_cdp_mapper,
        {
            "method": "script.addPreloadScript",
            "params": {
                "functionDeclaration": "() => { window.__closedCustomMapper = true; }"
            },
        },
    )

    # Closing secondary mappers detaches their Browser CDP sessions (even when
    # the underlying browser WebSocket stays open) and closes custom CDP
    # transports while leaving the primary `websocket` Mapper live.
    # `close_ws=False` keeps the custom CDP WebSocket open, so the preload
    # script check below fails if the Mapper does not detach its session.
    await secondary_mapper.close()
    await custom_cdp_mapper.close(close_ws=False)

    # The primary Mapper still works: it creates a new tab and navigates it.
    create_cmd_id = await send_JSON_command(
        websocket,
        {"method": "browsingContext.create", "params": {"type": "tab"}},
    )
    third_res, created_after_close = await wait_for_command_and_event(
        websocket, create_cmd_id, "browsingContext.contextCreated"
    )
    third_context_id = third_res["context"]
    assert created_after_close["params"]["context"] == third_context_id

    await goto_url(websocket, third_context_id, html("<h1>After close</h1>"))
    # The preload scripts of the closed mappers did not run in the new tab.
    eval_after_close = await execute_command(
        websocket,
        {
            "method": "script.evaluate",
            "params": {
                "expression": "[window.__closedSharedMapper, window.__closedCustomMapper]",
                "target": {"context": third_context_id},
                "awaitPromise": False,
            },
        },
    )
    assert eval_after_close["result"] == {
        "type": "array",
        "value": [{"type": "undefined"}, {"type": "undefined"}],
    }

    tree_primary = await get_tree(websocket)
    assert {c["context"] for c in tree_primary["contexts"]} == {
        context_id,
        second_context_id,
        third_context_id,
    }


@pytest.mark.asyncio
async def test_multiple_mappers_isolated_event_subscriptions(
    create_mapper, context_id, html
):
    """Subscriptions are per Mapper: each Mapper receives only the events it
    subscribed to.
    """
    mapper_1 = await create_mapper()
    mapper_2 = await create_mapper()

    # Mapper 1 subscribes only to `browsingContext.load`.
    # Mapper 2 subscribes only to `log.entryAdded`.
    await subscribe(mapper_1, ["browsingContext.load"])
    await subscribe(mapper_2, ["log.entryAdded"])
    # Drop the events received during setup.
    mapper_1.received_events.clear()
    mapper_2.received_events.clear()

    # The page emits both events: `log.entryAdded` from the inline script, and
    # `browsingContext.load` when it finishes loading.
    url = html("<script>console.log('hello from page');</script><h1>Test</h1>")
    nav_cmd_id = await send_JSON_command(
        mapper_1,
        {
            "method": "browsingContext.navigate",
            "params": {"url": url, "context": context_id, "wait": "complete"},
        },
    )

    (_, load_event), log_event = await asyncio.gather(
        wait_for_command_and_event(mapper_1, nav_cmd_id, "browsingContext.load"),
        wait_for_event(mapper_2, "log.entryAdded"),
    )
    assert load_event == AnyExtending(
        {
            "type": "event",
            "method": "browsingContext.load",
            "params": {"context": context_id, "url": url},
        }
    )
    assert log_event == AnyExtending(
        {
            "type": "event",
            "method": "log.entryAdded",
            "params": {
                "text": "hello from page",
                "source": {"context": context_id},
                "timestamp": ANY_TIMESTAMP,
            },
        }
    )

    # Flush both mappers (including a renderer round-trip on `mapper_2` after
    # `navigate(wait="complete")` finished) so any leaked event would already
    # be recorded in `received_events`.
    await get_tree(mapper_1)
    await execute_command(
        mapper_2,
        {
            "method": "script.evaluate",
            "params": {
                "expression": "1",
                "target": {"context": context_id},
                "awaitPromise": False,
            },
        },
    )

    # `received_events` also has the events dropped by the wait helpers above,
    # so this checks that no other event was received.
    assert [e["method"] for e in mapper_1.received_events] == ["browsingContext.load"]
    assert [e["method"] for e in mapper_2.received_events] == ["log.entryAdded"]


@pytest.mark.asyncio
async def test_multiple_mappers_actuate_and_observe(create_mapper, context_id, html):
    """One Mapper navigates the page while the other observes it and then
    interacts with it. Internal sandboxes of one Mapper are not exposed to the
    other.
    """
    mapper_1 = await create_mapper()
    mapper_2 = await create_mapper()

    await subscribe(
        mapper_1, ["browsingContext.navigationCommitted", "script.realmCreated"]
    )
    await subscribe(mapper_2, ["browsingContext.navigationCommitted"])
    # Drop the events received during setup.
    mapper_1.received_events.clear()

    url = html("<h1 id='title'>Agent Target</h1>")
    # Actuate navigation from Mapper 1 while Mapper 2 observes.
    nav_cmd_id = await send_JSON_command(
        mapper_1,
        {
            "method": "browsingContext.navigate",
            "params": {"url": url, "context": context_id, "wait": "complete"},
        },
    )

    (_, nav_1), nav_2 = await asyncio.gather(
        wait_for_command_and_event(
            mapper_1, nav_cmd_id, "browsingContext.navigationCommitted"
        ),
        wait_for_event(mapper_2, "browsingContext.navigationCommitted"),
    )
    assert nav_1["params"]["context"] == context_id
    assert nav_1["params"]["url"] == url
    assert nav_2["params"]["context"] == context_id
    assert nav_2["params"]["url"] == url

    # Mapper 2 locates nodes and captures screenshot on the navigated page.
    nodes_result = await execute_command(
        mapper_2,
        {
            "method": "browsingContext.locateNodes",
            "params": {
                "context": context_id,
                "locator": {"type": "css", "value": "#title"},
            },
        },
    )
    assert len(nodes_result["nodes"]) == 1

    screenshot_result = await execute_command(
        mapper_2,
        {
            "method": "browsingContext.captureScreenshot",
            "params": {"context": context_id},
        },
    )
    assert screenshot_result == {"data": ANY_STR}

    # Internal helper sandboxes created by `captureScreenshot` on `mapper_2`
    # must stay hidden from sibling mappers (`script.getRealms` and
    # `script.realmCreated`). Flush `mapper_1` with a renderer round-trip first
    # so `Runtime.executionContextCreated` for the isolated world has arrived.
    await execute_command(
        mapper_1,
        {
            "method": "script.evaluate",
            "params": {
                "expression": "1",
                "target": {"context": context_id},
                "awaitPromise": False,
            },
        },
    )
    # `mapper_1` sees only the page's default realm, and no sandbox realm was
    # reported to it.
    realms_1 = await execute_command(
        mapper_1,
        {"method": "script.getRealms", "params": {"context": context_id}},
    )
    assert len(realms_1["realms"]) == 1
    assert "sandbox" not in realms_1["realms"][0]
    assert all(
        "sandbox" not in e["params"]
        for e in mapper_1.received_events
        if e["method"] == "script.realmCreated"
    )


@pytest.mark.asyncio
async def test_multiple_mappers_script_evaluate_and_sandboxes(
    create_mapper, context_id, html
):
    """Both mappers can evaluate scripts in a sandbox with the same name in the
    same context.
    """
    mapper_1 = await create_mapper()
    mapper_2 = await create_mapper()

    await goto_url(mapper_1, context_id, html("<h1>Sandbox Test</h1>"))

    # Both mappers can evaluate scripts in a sandbox on the same context.
    # Note: whether same-named sandboxes across mappers share an isolated world
    # or are isolated per mapper is not specified here; this only checks that
    # `script.evaluate` with a `sandbox` succeeds on both mappers.
    res_1 = await execute_command(
        mapper_1,
        {
            "method": "script.evaluate",
            "params": {
                "expression": "window.tag = 'mapper1'; window.tag",
                "target": {"context": context_id, "sandbox": "agent"},
                "awaitPromise": False,
            },
        },
    )
    res_2 = await execute_command(
        mapper_2,
        {
            "method": "script.evaluate",
            "params": {
                "expression": "window.tag = 'mapper2'; window.tag",
                "target": {"context": context_id, "sandbox": "agent"},
                "awaitPromise": False,
            },
        },
    )
    assert res_1["result"] == {"type": "string", "value": "mapper1"}
    assert res_2["result"] == {"type": "string", "value": "mapper2"}


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "capabilities", [{"unhandledPromptBehavior": "ignore"}], indirect=True
)
async def test_multiple_mappers_user_prompt_handling(create_mapper, context_id):
    """With `unhandledPromptBehavior: ignore`, no Mapper closes prompts
    automatically. A prompt opened via one Mapper is reported to both mappers
    and can be closed via the other one.
    """
    mapper_1 = await create_mapper()
    mapper_2 = await create_mapper()

    await subscribe(
        mapper_1,
        ["browsingContext.userPromptOpened", "browsingContext.userPromptClosed"],
    )
    await subscribe(
        mapper_2,
        ["browsingContext.userPromptOpened", "browsingContext.userPromptClosed"],
    )

    # Open an alert via Mapper 1. Do not wait for the result: `alert()` blocks
    # until the prompt is closed.
    await send_JSON_command(
        mapper_1,
        {
            "method": "script.evaluate",
            "params": {
                "expression": "alert('agent prompt')",
                "target": {"context": context_id},
                "awaitPromise": False,
            },
        },
    )

    opened_1, opened_2 = await asyncio.gather(
        wait_for_event(mapper_1, "browsingContext.userPromptOpened"),
        wait_for_event(mapper_2, "browsingContext.userPromptOpened"),
    )
    assert opened_1["params"]["message"] == "agent prompt"
    assert opened_2["params"]["message"] == "agent prompt"

    # Handle the prompt from Mapper 2 and verify both mappers observe closure.
    handle_cmd_id = await send_JSON_command(
        mapper_2,
        {
            "method": "browsingContext.handleUserPrompt",
            "params": {"context": context_id, "accept": True},
        },
    )

    closed_1, (_, closed_2) = await asyncio.gather(
        wait_for_event(mapper_1, "browsingContext.userPromptClosed"),
        wait_for_command_and_event(
            mapper_2, handle_cmd_id, "browsingContext.userPromptClosed"
        ),
    )

    assert closed_1["params"] == AnyExtending(
        {"context": context_id, "accepted": True, "type": "alert"}
    )
    assert closed_2["params"] == AnyExtending(
        {"context": context_id, "accepted": True, "type": "alert"}
    )


@pytest.mark.asyncio
async def test_multiple_mappers_auto_dismiss_user_prompt(
    websocket, create_mapper, context_id
):
    """With the default `unhandledPromptBehavior` (`dismiss` for alerts), every
    Mapper tries to dismiss the same alert. Only one
    `Page.handleJavaScriptDialog` succeeds; the others fail with "No dialog is
    showing". These failures must not cause unhandled promise rejections in the
    Mapper tab (checked at `mapper_tab_cdp` teardown).
    """
    # Three mappers race: the primary one, one with a custom CDP transport, and
    # one sharing `window.cdp` with the primary Mapper.
    mapper_1 = await create_mapper()
    mapper_2 = await create_mapper(custom_cdp_transport=False)

    for conn in (websocket, mapper_1, mapper_2):
        await subscribe(
            conn,
            ["browsingContext.userPromptOpened", "browsingContext.userPromptClosed"],
        )

    # `alert()` blocks until the alert is dismissed, so the `script.evaluate`
    # result arrives after the alert is closed.
    eval_cmd_id = await send_JSON_command(
        mapper_1,
        {
            "method": "script.evaluate",
            "params": {
                "expression": "alert('auto prompt')",
                "target": {"context": context_id},
                "awaitPromise": False,
            },
        },
    )

    async def _wait_opened_and_closed(conn) -> tuple[dict, dict]:
        """Reads messages until both prompt events arrive, in any order."""
        opened = None
        closed = None
        while opened is None or closed is None:
            msg = await read_JSON_message(conn)
            if msg.get("method") == "browsingContext.userPromptOpened":
                opened = msg
            elif msg.get("method") == "browsingContext.userPromptClosed":
                closed = msg
        return opened, closed

    async def _wait_mapper_1() -> tuple[dict, dict, dict]:
        """Same as `_wait_opened_and_closed`, but also waits for the
        `script.evaluate` result on `mapper_1`.
        """
        eval_res = None
        opened = None
        closed = None
        while eval_res is None or opened is None or closed is None:
            msg = await read_JSON_message(mapper_1)
            if msg.get("id") == eval_cmd_id:
                assert "result" in msg, msg
                eval_res = msg["result"]
            elif msg.get("method") == "browsingContext.userPromptOpened":
                opened = msg
            elif msg.get("method") == "browsingContext.userPromptClosed":
                closed = msg
        return eval_res, opened, closed

    (
        (opened_p, closed_p),
        (_, opened_1, closed_1),
        (opened_2, closed_2),
    ) = await asyncio.wait_for(
        asyncio.gather(
            _wait_opened_and_closed(websocket),
            _wait_mapper_1(),
            _wait_opened_and_closed(mapper_2),
        ),
        timeout=20,
    )

    # Every Mapper reports the alert with the `dismiss` handler, and its
    # closing as not accepted.
    for opened in (opened_p, opened_1, opened_2):
        assert opened["params"] == AnyExtending(
            {
                "context": context_id,
                "handler": "dismiss",
                "message": "auto prompt",
                "type": "alert",
            }
        )
    for closed in (closed_p, closed_1, closed_2):
        assert closed["params"] == AnyExtending(
            {"context": context_id, "accepted": False, "type": "alert"}
        )

    # Flush all three mappers so any losing `Page.handleJavaScriptDialog`
    # responses (`"No dialog is showing"`) settle before checking for unhandled
    # promise rejections in `mapper_tab_cdp` teardown.
    for conn in (websocket, mapper_1, mapper_2):
        await execute_command(
            conn,
            {
                "method": "script.evaluate",
                "params": {
                    "expression": "1",
                    "target": {"context": context_id},
                    "awaitPromise": False,
                },
            },
        )


@pytest.mark.asyncio
async def test_multiple_mappers_no_cdp_errors(
    websocket,
    mapper_tab_cdp,
    create_mapper,
    html,
    iframe,
):
    """No CDP command fails while several mappers create a tab, attach to an
    out-of-process iframe (OOPIF), close the tab and are closed.

    CDP errors are collected from the debug log of the Mapper tab: all Mapper
    instances log received CDP messages via `window.sendDebugMessage`.
    """
    control_cdp = mapper_tab_cdp["control_cdp"]
    mapper_session_id = mapper_tab_cdp["mapper_session_id"]

    # Hook `window.sendDebugMessage` before starting secondary mappers so CDP
    # traffic during `runMapperInstance` and `session.new` is also checked.
    # The original hook (if any) is still called and restored in `finally`.
    hook_res = await control_cdp.send_command(
        "Runtime.evaluate",
        {
            "expression": """
                window.cdpErrors = [];
                window.__origSendDebugMessage = window.sendDebugMessage;
                window.sendDebugMessage = (json) => {
                    window.__origSendDebugMessage?.(json);
                    const data = JSON.parse(json);
                    if (data.logType === 'cdp:RECV ◂' && data.messages?.[0]?.error) {
                        window.cdpErrors.push(data.messages[0]);
                    }
                };
            """
        },
        session_id=mapper_session_id,
    )
    assert "exceptionDetails" not in hook_res, hook_res

    try:
        # `mapper_1` has its own CDP transport, `mapper_2` shares `window.cdp`
        # with the primary Mapper.
        mapper_1 = await create_mapper()
        mapper_2 = await create_mapper(custom_cdp_transport=False)

        await subscribe(websocket, ["browsingContext"])
        await subscribe(mapper_1, ["browsingContext", "script.realmCreated"])
        await subscribe(mapper_2, ["browsingContext", "script.realmCreated"])

        # `mapper_1` creates a tab. Wait until the other mappers attached to it.
        create_res = await execute_command(
            mapper_1,
            {"method": "browsingContext.create", "params": {"type": "tab"}},
        )
        new_context_id = create_res["context"]
        await asyncio.gather(
            wait_for_filtered_event(
                websocket,
                lambda e: (
                    e["method"] == "browsingContext.contextCreated"
                    and e["params"]["context"] == new_context_id
                ),
            ),
            wait_for_filtered_event(
                mapper_2,
                lambda e: (
                    e["method"] == "browsingContext.contextCreated"
                    and e["params"]["context"] == new_context_id
                ),
            ),
        )

        # `mapper_2` loads a page with a cross-origin iframe, which becomes a
        # separate OOPIF target. Wait until the other mappers attached to it.
        oopif_url = html("<h1>OOPIF</h1>", same_origin=False)
        await goto_url(mapper_2, new_context_id, html(iframe(oopif_url)))
        await asyncio.gather(
            wait_for_filtered_event(
                websocket,
                lambda e: (
                    e["method"] == "browsingContext.contextCreated"
                    and e["params"]["parent"] == new_context_id
                ),
            ),
            wait_for_filtered_event(
                mapper_1,
                lambda e: (
                    e["method"] == "browsingContext.contextCreated"
                    and e["params"]["parent"] == new_context_id
                ),
            ),
        )

        tree_1 = await get_tree(mapper_1, new_context_id)
        tree_2 = await get_tree(mapper_2, new_context_id)
        assert len(tree_1["contexts"][0]["children"]) == 1
        assert len(tree_2["contexts"][0]["children"]) == 1
        child_context_id = tree_1["contexts"][0]["children"][0]["context"]

        # Ensure all mappers (`websocket`, `mapper_1`, and `mapper_2`) have
        # finished unblocking the OOPIF target before `mapper_2` closes the tab
        # so in-flight `#unblock()` CDP commands are not aborted mid-attachment.
        for conn in (websocket, mapper_1, mapper_2):
            await execute_command(
                conn,
                {
                    "method": "script.evaluate",
                    "params": {
                        "expression": "1 + 1",
                        "target": {"context": child_context_id},
                        "awaitPromise": False,
                    },
                },
            )

        # `mapper_2` closes the tab. Wait until the other mappers report it.
        await execute_command(
            mapper_2,
            {"method": "browsingContext.close", "params": {"context": new_context_id}},
        )
        await asyncio.gather(
            wait_for_filtered_event(
                websocket,
                lambda e: (
                    e["method"] == "browsingContext.contextDestroyed"
                    and e["params"]["context"] == new_context_id
                ),
            ),
            wait_for_filtered_event(
                mapper_1,
                lambda e: (
                    e["method"] == "browsingContext.contextDestroyed"
                    and e["params"]["context"] == new_context_id
                ),
            ),
        )

        # Also close both secondary mappers (and flush `windowCdpConnection` via
        # `websocket`) before inspecting `window.cdpErrors` so CDP traffic
        # during mapper teardown is verified error-free.
        await mapper_1.close()
        await mapper_2.close()
        await execute_command(
            websocket,
            {
                "method": "goog:cdp.sendCommand",
                "params": {"method": "Browser.getVersion", "params": {}},
            },
        )

        errors_result = await control_cdp.send_command(
            "Runtime.evaluate",
            {"expression": "window.cdpErrors", "returnByValue": True},
            session_id=mapper_session_id,
        )
        assert errors_result["result"]["value"] == []
    finally:
        await control_cdp.send_command(
            "Runtime.evaluate",
            {
                "expression": """
                    window.sendDebugMessage = window.__origSendDebugMessage;
                    delete window.__origSendDebugMessage;
                """
            },
            session_id=mapper_session_id,
        )


@pytest.mark.asyncio
async def test_restricted_mode_mapper(create_mapper):
    """A Mapper started in restricted mode creates only restricted sessions,
    and accepts no ACL changes before `session.new`.
    """
    mapper = await create_mapper(create_session=False, restricted_mode=True)

    # Commands other than `session.new` and `session.status` fail before the
    # session is created.
    with pytest.raises(Exception) as exception_info:
        await execute_command(
            mapper,
            {
                "method": "goog:session.setAcl",
                "params": {
                    "acl": {"default": {"commands": ["browsingContext.getTree"]}}
                },
            },
        )
    assert exception_info.value.args[0] == {
        "error": "invalid session id",
        "message": "Command 'goog:session.setAcl' requires a session. Call 'session.new' first.",
    }

    with pytest.raises(Exception) as exception_info:
        await execute_command(
            mapper, {"method": "session.new", "params": {"capabilities": {}}}
        )
    assert exception_info.value.args[0] == {
        "error": "session not created",
        "message": 'Only restricted sessions are allowed. Request capability "goog:sessionType": "restricted".',
    }

    session_result = await execute_command(
        mapper,
        {
            "method": "session.new",
            "params": {
                "capabilities": {"alwaysMatch": {"goog:sessionType": "restricted"}}
            },
        },
    )
    assert session_result["capabilities"]["goog:sessionType"] == "restricted"

    with pytest.raises(Exception) as exception_info:
        await execute_command(
            mapper, {"method": "browsingContext.getTree", "params": {}}
        )
    assert exception_info.value.args[0] == {
        "error": "unsupported operation",
        "message": "Command 'browsingContext.getTree' is not allowed by the session ACL.",
    }

    await execute_command(
        mapper,
        {
            "method": "goog:session.setAcl",
            "params": {"acl": {"default": {"commands": ["browsingContext.getTree"]}}},
        },
    )
    tree = await execute_command(
        mapper, {"method": "browsingContext.getTree", "params": {}}
    )
    assert tree == {"contexts": [AnyExtending({"url": "about:blank"})]}
