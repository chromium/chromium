#  Copyright 2024 Google LLC.
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
import asyncio
import re

import pytest
from test_helpers import (
    ANY_SHARED_ID,
    AnyExtending,
    execute_command,
    goto_url,
    read_JSON_message,
    send_JSON_command,
)


@pytest.mark.parametrize(
    "locator",
    [
        {"type": "innerText", "value": "foobarBARbaz"},
        {"type": "css", "value": "div"},
        {"type": "xpath", "value": "//div"},
        {"type": "accessibility", "value": {"role": "button", "name": "test"}},
    ],
)
@pytest.mark.parametrize("start_node_expression", [None, "document", "document.body"])
@pytest.mark.asyncio
async def test_locate_nodes_locator_found(
    websocket, context_id, html, locator, start_node_expression
):
    await goto_url(
        websocket,
        context_id,
        html(
            '<div data-class="one" aria-label="test" role="button">foobarBARbaz</div><div data-class="two" aria-label="test" role="button">foobarBAR<span>baz</span></div>'
        ),
    )

    start_nodes = None
    if start_node_expression:
        resp = await execute_command(
            websocket,
            {
                "method": "script.evaluate",
                "params": {
                    "expression": start_node_expression,
                    "target": {"context": context_id},
                    "awaitPromise": False,
                },
            },
        )
        start_nodes = [{"sharedId": resp["result"]["sharedId"]}]

    resp = await execute_command(
        websocket,
        {
            "method": "browsingContext.locateNodes",
            "params": {
                "context": context_id,
                "locator": locator,
                **({"startNodes": start_nodes} if start_nodes else {}),
            },
        },
    )

    assert resp == {
        "nodes": [
            {
                "sharedId": ANY_SHARED_ID,
                "type": "node",
                "value": {
                    "attributes": {
                        "data-class": "one",
                        "aria-label": "test",
                        "role": "button",
                    },
                    "childNodeCount": 1,
                    "localName": "div",
                    "namespaceURI": "http://www.w3.org/1999/xhtml",
                    "nodeType": 1,
                    "shadowRoot": None,
                },
            },
            {
                "sharedId": ANY_SHARED_ID,
                "type": "node",
                "value": {
                    "attributes": {
                        "data-class": "two",
                        "aria-label": "test",
                        "role": "button",
                    },
                    "childNodeCount": 2,
                    "localName": "div",
                    "namespaceURI": "http://www.w3.org/1999/xhtml",
                    "nodeType": 1,
                    "shadowRoot": None,
                },
            },
        ]
    }


@pytest.mark.parametrize(
    "locator",
    [
        {"type": "css", "value": "a*b"},
        {"type": "xpath", "value": ""},
    ],
)
@pytest.mark.asyncio
async def test_locate_nodes_locator_invalid(websocket, context_id, html, locator):
    with pytest.raises(
        Exception,
        match=re.escape(
            str(
                {
                    "error": "invalid selector",
                    "message": "Not valid selector " + locator["value"],
                }
            )
        ),
    ):
        await execute_command(
            websocket,
            {
                "method": "browsingContext.locateNodes",
                "params": {"context": context_id, "locator": locator},
            },
        )


async def assert_context_locator_unknown_context(
    websocket, context_id, locator_context
):
    with pytest.raises(
        Exception,
        match=re.escape(
            str(
                {
                    "error": "no such frame",
                    "message": f"Context {locator_context} not found",
                }
            )
        ),
    ):
        await execute_command(
            websocket,
            {
                "method": "browsingContext.locateNodes",
                "params": {
                    "context": context_id,
                    "locator": {
                        "type": "context",
                        "value": {"context": locator_context},
                    },
                },
            },
        )


@pytest.mark.asyncio
async def test_locate_nodes_context_locator_empty(websocket, context_id):
    await assert_context_locator_unknown_context(websocket, context_id, "")


@pytest.mark.asyncio
async def test_locate_nodes_context_locator_unknown(websocket, context_id):
    await assert_context_locator_unknown_context(
        websocket, context_id, "non_existing_context"
    )


@pytest.mark.parametrize(
    "locator",
    [
        {"type": "css", "value": "html"},
        {"type": "xpath", "value": "html"},
    ],
)
@pytest.mark.parametrize(
    "start_node_expression,should_find",
    [(None, True), ("document", True), ("document.body", False)],
)
@pytest.mark.asyncio
async def test_locate_nodes_select_html(
    websocket, context_id, html, locator, start_node_expression, should_find
):
    await goto_url(
        websocket,
        context_id,
        html(
            '<div data-class="one" aria-label="test" role="button">foobarBARbaz</div><div data-class="two" aria-label="test" role="button">foobarBAR<span>baz</span></div>'
        ),
    )

    start_nodes = None
    if start_node_expression:
        resp = await execute_command(
            websocket,
            {
                "method": "script.evaluate",
                "params": {
                    "expression": start_node_expression,
                    "target": {"context": context_id},
                    "awaitPromise": False,
                },
            },
        )
        start_nodes = [{"sharedId": resp["result"]["sharedId"]}]

    resp = await execute_command(
        websocket,
        {
            "method": "browsingContext.locateNodes",
            "params": {
                "context": context_id,
                "locator": locator,
                **({"startNodes": start_nodes} if start_nodes else {}),
            },
        },
    )

    if should_find:
        assert resp == {
            "nodes": [
                AnyExtending(
                    {
                        "type": "node",
                        "value": {
                            "localName": "html",
                        },
                    }
                )
            ]
        }
    else:
        assert resp == {"nodes": []}


@pytest.mark.parametrize("same_origin", [True, False])
@pytest.mark.parametrize(
    "locator_value",
    [
        {"role": "button", "name": "Inner"},
        {"role": "button"},
    ],
)
@pytest.mark.asyncio
async def test_locate_nodes_accessibility_in_iframe(
    websocket, context_id, html, iframe, same_origin, locator_value
):
    # Create the inner iframe URL with its own button.
    iframe_url = html('<button id="inner">Inner</button>', same_origin=same_origin)
    # Navigate the top-level page with an outer button and the child iframe.
    await goto_url(
        websocket,
        context_id,
        html(f'<button id="outer">Outer</button>{iframe(iframe_url)}'),
    )

    # Fetch the browsing context tree to get the child iframe's context ID.
    tree = await execute_command(
        websocket,
        {
            "method": "browsingContext.getTree",
            "params": {"root": context_id},
        },
    )
    # Extract the child frame browsing context ID.
    iframe_context_id = tree["contexts"][0]["children"][0]["context"]

    # Locate nodes by accessibility inside the child iframe context.
    resp = await execute_command(
        websocket,
        {
            "method": "browsingContext.locateNodes",
            "params": {
                "context": iframe_context_id,
                "locator": {"type": "accessibility", "value": locator_value},
            },
        },
    )

    # Verify that only the inner frame's button is matched and returned.
    assert resp == {
        "nodes": [
            {
                "sharedId": ANY_SHARED_ID,
                "type": "node",
                "value": {
                    "attributes": {
                        "id": "inner",
                    },
                    "childNodeCount": 1,
                    "localName": "button",
                    "namespaceURI": "http://www.w3.org/1999/xhtml",
                    "nodeType": 1,
                    "shadowRoot": None,
                },
            },
        ]
    }


@pytest.mark.asyncio
async def test_locate_nodes_accessibility_stale_start_node_after_reload(
    websocket, context_id, html
):
    await goto_url(
        websocket,
        context_id,
        html("<title>Ready</title><button>Go</button>"),
    )

    # Obtain a sharedId for document.body before reloading the page.
    eval_resp = await execute_command(
        websocket,
        {
            "method": "script.evaluate",
            "params": {
                "expression": "document.body",
                "target": {"context": context_id},
                "awaitPromise": False,
            },
        },
    )
    stale_shared_id = eval_resp["result"]["sharedId"]

    # Reload the page so the previous document becomes inactive.
    await execute_command(
        websocket,
        {
            "method": "browsingContext.reload",
            "params": {"context": context_id, "wait": "complete"},
        },
    )

    # Locating accessibility nodes using a startNode from the old document must
    # fail with "no such node" rather than crashing the renderer process.
    with pytest.raises(
        Exception,
        match=re.escape("'error': 'no such node'"),
    ):
        await execute_command(
            websocket,
            {
                "method": "browsingContext.locateNodes",
                "params": {
                    "context": context_id,
                    "locator": {
                        "type": "accessibility",
                        "value": {"role": "button", "name": "Go"},
                    },
                    "startNodes": [{"sharedId": stale_shared_id}],
                },
            },
        )

    # Verify the browsing context remains responsive to subsequent commands.
    title_resp = await execute_command(
        websocket,
        {
            "method": "script.evaluate",
            "params": {
                "expression": "document.title",
                "target": {"context": context_id},
                "awaitPromise": False,
            },
        },
    )
    assert title_resp["result"] == {"type": "string", "value": "Ready"}


@pytest.mark.asyncio
async def test_locate_nodes_accessibility_concurrent_reload(
    websocket, context_id, html
):
    url = html("<title>Ready</title><button>Go</button>")
    await goto_url(websocket, context_id, url)

    # Sweep short delays between locateNodes and reload so the reload straddles
    # the internal Accessibility.getRootAXNode and Accessibility.queryAXTree
    # CDP calls.
    for delay in [0.0, 0.002, 0.004, 0.006, 0.008, 0.01]:
        locate_id = await send_JSON_command(
            websocket,
            {
                "method": "browsingContext.locateNodes",
                "params": {
                    "context": context_id,
                    "locator": {
                        "type": "accessibility",
                        "value": {"role": "button", "name": "Go"},
                    },
                },
            },
        )
        if delay > 0:
            await asyncio.sleep(delay)
        reload_id = await send_JSON_command(
            websocket,
            {
                "method": "browsingContext.reload",
                "params": {"context": context_id, "wait": "complete"},
            },
        )

        # Wait for both the locateNodes and reload responses.
        responses = {}
        while locate_id not in responses or reload_id not in responses:
            msg = await read_JSON_message(websocket)
            if "id" in msg:
                responses[msg["id"]] = msg

        assert responses[reload_id]["type"] == "success"
        assert responses[locate_id]["type"] in ("success", "error")

        # Verify the page continues answering commands after the reload.
        eval_resp = await execute_command(
            websocket,
            {
                "method": "script.evaluate",
                "params": {
                    "expression": "document.title",
                    "target": {"context": context_id},
                    "awaitPromise": False,
                },
            },
        )
        assert eval_resp["result"] == {"type": "string", "value": "Ready"}
