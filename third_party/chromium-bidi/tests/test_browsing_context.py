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

from _helpers import *


@pytest.mark.asyncio
async def test_browsingContext_getTree_contextReturned(websocket):
    command = {"id": 8, "method": "browsingContext.getTree", "params": {}}
    await send_JSON_command(websocket, command)

    # Assert "browsingContext.getTree" command done.
    resp = await read_JSON_message(websocket)
    [context] = resp['result']['contexts']
    context_id = context['context']
    assert isinstance(context_id, str)
    assert len(context_id) > 1
    assert resp == {
        "id": 8,
        "result": {
            "contexts": [{
                "context": context_id,
                "children": [],
                "url": "about:blank"}]}}


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_getTreeWithGivenParent_contextReturned(websocket):
    ignore = True
    # TODO sadym: implement


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_getTreeWithNestedContexts_contextReturned(websocket):
    ignore = True
    # TODO sadym: implement


@pytest.mark.asyncio
async def test_browsingContext_create_eventContextCreatedEmittedAndContextCreated(
      websocket):
    initial_context_id = await get_open_context_id(websocket)

    command = {
        "id": 9,
        "method": "browsingContext.create",
        "params": {}}
    await send_JSON_command(websocket, command)

    # Assert "browsingContext.contextCreated" event emitted.
    resp = await read_JSON_message(websocket)
    new_context_id = resp['params']['context']
    # TODO: replace with assertion after event is raised with proper URL.
    recursiveCompare(
        resp,
        {
            "method": "browsingContext.contextCreated",
            "params": {
                "context": new_context_id,
                "children": [],
                "url": "about:blank"}},
        ["url"])

    # Assert command done.
    resp = await read_JSON_message(websocket)
    # TODO: replace with assertion after event is raised with proper URL.
    recursiveCompare(
        resp,
        {
            "id": 9,
            "result": {
                "context": new_context_id,
                "children": [],
                "url": "about:blank"}},
        ["url"])

    # Get all contexts and assert new context is created.
    command = {"id": 10, "method": "browsingContext.getTree", "params": {}}
    await send_JSON_command(websocket, command)

    # Assert "browsingContext.getTree" command done.
    # TODO sadym: make order-agnostic. Maybe order by `context`?
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id": 10,
        "result": {
            "contexts": [{
                "context": new_context_id,
                "url": "about:blank",
                "children": []
            }, {
                "context": initial_context_id,
                "url": "about:blank",
                "children": []}]}}


@pytest.mark.asyncio
# TODO: fix test in headful mode.
async def test_DEBUG_browsingContext_close_browsingContext_contextDestroyedEmitted(
      websocket):
    context_id = await get_open_context_id(websocket)

    # Send command.
    command = {"id": 12, "method": "DEBUG.browsingContext.close",
               "params": {"context": context_id}}
    await send_JSON_command(websocket, command)

    # Assert "browsingContext.contextCreated" event emitted.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "method": "browsingContext.contextDestroyed",
        "params": {
            "context": context_id,
            "url": "about:blank",
            "children": []}}

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {"id": 12, "result": {}}


@pytest.mark.asyncio
async def test_browsingContext_navigateWaitNone_navigated(websocket):
    context_id = await get_open_context_id(websocket)

    # Send command.
    command = {
        "id": 13,
        "method": "browsingContext.navigate",
        "params": {
            "url": "data:text/html,<h2>test</h2>",
            "wait": "none",
            "context": context_id}}
    await send_JSON_command(websocket, command)

    # Assert command done.
    resp = await read_JSON_message(websocket)
    recursiveCompare(
        resp,
        {
            "id": 13,
            "result": {
                "navigation": "F595626837D41F9A72482D53B0C22F25",
                "url": "data:text/html,<h2>test</h2>"}},
        ["navigation"])


@pytest.mark.asyncio
# TODO sadym: find a way to test if the command ended only after the DOM
# actually loaded.
async def test_browsingContext_navigateWaitInteractive_navigated(websocket):
    context_id = await get_open_context_id(websocket)

    # Send command.
    command = {
        "id": 14,
        "method": "browsingContext.navigate",
        "params": {
            "url": "data:text/html,<h2>test</h2>",
            "wait": "interactive",
            "context": context_id}}
    await send_JSON_command(websocket, command)

    # Assert command done.
    resp = await read_JSON_message(websocket)
    recursiveCompare(
        resp,
        {
            "id": 14,
            "result": {
                "navigation": "F595626837D41F9A72482D53B0C22F25",
                "url": "data:text/html,<h2>test</h2>"}},
        ["navigation"])


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_navigateWaitComplete_navigated(websocket):
    ignore = True


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_navigateWithShortTimeout_timeoutOccurredAndEventPageLoadEmitted(
      websocket):
    context_id = await get_open_context_id(websocket)

    # Send command.
    command = {
        "id": 16,
        "method": "browsingContext.navigate",
        "params": {
            "url": "data:text/html,<h2>test</h2>",
            "context": context_id,
            "waitUntil": ["load", "domcontentloaded", "networkidle0",
                          "networkidle2"],
            "timeout": "1"}}

    await send_JSON_command(websocket, command)

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id": 16,
        "error": "unknown error",
        "message": "Navigation timeout of 1 ms exceeded"}

    resp = await read_JSON_message(websocket)
    assert resp == {
        "method": "DEBUG.Page.load",
        "params": {
            "context": context_id}}


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_waitForSelector_success(websocket):
    context_id = await get_open_context_id(websocket)
    await goto_url(websocket, context_id,
                   "data:text/html,<h2>test</h2>")

    # Send command.
    await send_JSON_command(websocket, {
        "id": 17,
        "method": "PROTO.browsingContext.waitForSelector",
        "params": {
            "selector": "body > h2",
            "context": context_id}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    recursiveCompare(
        resp,
        {
            "id": 17,
            "result": {
                "type": "node",
                "objectId": "__any_value__"}},
        ["objectId"])


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_waitForSelector_success_slow(websocket):
    # 1. Wait for element which is not on the page.
    # 2. Assert element not found.
    # 3. Add element to the page.
    # 4. Wait for newly created element.
    # 5. Assert element found.

    context_id = await get_open_context_id(websocket)

    # 1. Wait for element which is not on the page.
    await send_JSON_command(websocket, {
        "id": 18,
        "method": "PROTO.browsingContext.waitForSelector",
        "params": {
            "selector": "body > h2",
            "timeout": 1000,
            "context": context_id}})

    # 2. Assert element not found.
    resp = await read_JSON_message(websocket)
    assert resp == {"id": 18, "error": "unknown error",
                    "message": "waiting for selector `body > h2` failed: timeout 1000ms exceeded"}

    # 3. Add element to the page.
    await send_JSON_command(websocket, {
        "id": 19,
        "method": "script.evaluate",
        "params": {
            "expression": "document.documentElement.innerHTML='<h2 />'",
            "target": {"context": context_id}}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id": 19,
        "result": {
            "type": "string",
            "value": "<h2 />"}}

    # 4. Wait for newly created element.
    await send_JSON_command(websocket, {
        "id": 20,
        "method": "PROTO.browsingContext.waitForSelector",
        "params": {
            "selector": "body > h2",
            "timeout": 1000,
            "context": context_id}})

    # 5. Assert element found.
    resp = await read_JSON_message(websocket)
    recursiveCompare(
        resp,
        {
            "id": 20,
            "result": {
                "type": "node",
                "objectId": "__any_value__"}},
        ["objectId"])


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_waitForHiddenSelector_success(websocket):
    context_id = await get_open_context_id(websocket)
    await goto_url(websocket, context_id,
                   "data:text/html,<h2>test</h2>")

    # Send command.
    await send_JSON_command(websocket, {
        "id": 21,
        "method": "PROTO.browsingContext.waitForSelector",
        "params": {
            "selector": "body > h3",
            "context": context_id,
            "hidden": True}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {"id": 21, "result": {}}


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_waitForSelectorWithMinimumTimeout_failedWithTimeout(
      websocket):
    context_id = await get_open_context_id(websocket)
    await goto_url(websocket, context_id,
                   "data:text/html,<h2>test</h2>")

    # Send command.
    await send_JSON_command(websocket, {
        "id": 22,
        "method": "PROTO.browsingContext.waitForSelector",
        "params": {
            "selector": "body > h2",
            "timeout": 1,
            "context": context_id}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id": 22,
        "error": "unknown error",
        "message": "waiting for selector `body > h2` failed: timeout 1ms exceeded"}


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_waitForSelectorWithMissingElement_failedWithTimeout_slow(
      websocket):
    context_id = await get_open_context_id(websocket)
    await goto_url(websocket, context_id,
                   "data:text/html,<h2>test</h2>")

    # Send command.
    await send_JSON_command(websocket, {
        "id": 23,
        "method": "PROTO.browsingContext.waitForSelector",
        "params": {
            "selector": "body > h3",
            "timeout": 1000,
            "context": context_id}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id": 23,
        "error": "unknown error",
        "message": "waiting for selector `body > h3` failed: timeout 1000ms exceeded"}


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_clickElement_clickProcessed(websocket):
    # 1. Open page with button and click handler. Button click logs message.
    # 2. Get the button.
    # 3. Click the button.
    # 4. Assert console log event emitted.
    # 5. Assert click command done.

    context_id = await get_open_context_id(websocket)

    # 1. Open page with button and click handler. Button click logs message.
    await goto_url(websocket, context_id,
                   "data:text/html,<button onclick=\"console.log('button clicked')\">button</button>")

    # 2. Get the button.
    # Send command.
    await send_JSON_command(websocket, {
        "id": 25,
        "method": "PROTO.browsingContext.waitForSelector",
        "params": {
            "selector": "body > button",
            "context": context_id}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == 25
    object_id = resp["result"]["objectId"]

    # 3. Click the button.
    await send_JSON_command(websocket, {
        "id": 26,
        "method": "PROTO.browsingContext.click",
        "params": {
            "objectId": object_id,
            "context": context_id}})

    # 4. Assert console log event emitted.
    resp = await read_JSON_message(websocket)
    assert resp["method"] == "log.entryAdded"
    assert resp["params"]["text"] == "button clicked"

    # 5. Assert click command done.
    resp = await read_JSON_message(websocket)
    assert resp == {"id": 26, "result": {}}


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_type_textTyped(websocket):
    # 1. Get input element.
    # 2. Type `!!@@## test text` in.
    # 3. Assert input.value is `!!@@## test text`.

    context_id = await get_open_context_id(websocket)
    await goto_url(websocket, context_id,
                   "data:text/html,<input>")

    # 1. Get input element.
    # Send command.
    await send_JSON_command(websocket, {
        "id": 34,
        "method": "PROTO.browsingContext.selectElement",
        "params": {
            "selector": "body > input",
            "context": context_id}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == 34
    object_id = resp["result"]["objectId"]

    # 2. Type `!!@@## test text` in.
    # Send command.
    await send_JSON_command(websocket, {
        "id": 35,
        "method": "PROTO.browsingContext.type",
        "params": {
            "text": "!!@@## test text",
            "objectId": object_id,
            "context": context_id}})

    resp = await read_JSON_message(websocket)
    assert resp == {"id": 35, "result": {}}

    # 3. Assert input.value is `!!@@## test text`.
    # Send command.
    await send_JSON_command(websocket, {
        "id": 36,
        "method": "script.evaluate",
        "params": {
            "expression": "element => element.value",
            # TODO: send properly serialized element according to
            # https://w3c.github.io/webdriver-bidi/#data-types-remote-value.
            "args": [{
                "objectId": object_id}],
            "target": {"context": context_id}}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id": 36,
        "result": {
            "type": "string",
            "value": "!!@@## test text"}}


@pytest.mark.asyncio
async def test_PROTO_browsingContext_findElement_findsElement(websocket):
    context_id = await get_open_context_id(websocket)

    # Send command.
    command = {
        "id": 58,
        "method": "browsingContext.navigate",
        "params": {
            "url": "data:text/html,<div class='container'>test<h2 class='child_1'>child 1</h2><h2 class='child_2'>child 2</h2></div>",
            "wait": "none",
            "context": context_id}}
    await send_JSON_command(websocket, command)

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == 58

    # time.sleep(5)

    # Send command.
    await send_JSON_command(websocket, {
        "id": 59,
        "method": "PROTO.browsingContext.findElement",
        "params": {
            "selector": "body > .container",
            "context": context_id}})

    resp = await read_JSON_message(websocket)
    recursiveCompare({
        "id": 59,
        "result": {
            "result": {
                "objectId": "__SOME_OBJECT_ID_1__",
                "type": "node",
                "value": {
                    "nodeType": 1,
                    "childNodeCount": 3},
                "children": [{
                    "objectId": "__CHILD_OBJECT_1__",
                    "type": "node"
                }, {
                    "objectId": "__CHILD_OBJECT_2__",
                    "type": "node"
                }, {
                    "objectId": "__CHILD_OBJECT_3__",
                    "type": "node"}],
                "attributes": {"class": "container"}}}},
        resp, ["objectId"])


@pytest.mark.asyncio
async def test_PROTO_browsingContext_findElementMissingElement_missingElement(websocket):
    context_id = await get_open_context_id(websocket)
    await goto_url(websocket, context_id,
                   "data:text/html,<h2>test</h2>")

    # Send command.
    await send_JSON_command(websocket, {
        "id": 60,
        "method": "PROTO.browsingContext.findElement",
        "params": {
            "selector": "body > h3",
            "context": context_id}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {"id": 60, "result": {"result": {"type": "null"}}}