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
    result = await execute_command(websocket,
                                   {"method": "browsingContext.getTree",
                                    "params": {}})

    # Assert "browsingContext.getTree" command done.
    [context] = result['contexts']
    context_id = context['context']
    assert isinstance(context_id, str)
    assert len(context_id) > 1
    assert result == {
        "contexts": [{
            "context": context_id,
            "children": None,
            "parent": None,
            "url": "about:blank"}]}


@pytest.mark.asyncio
async def test_browsingContext_getTreeWithRoot_contextReturned(websocket,
      context_id):
    result = await execute_command(websocket, {
        "method": "browsingContext.create",
        "params": {"type": "tab"}})
    new_context_id = result["context"]

    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {}})

    assert len(result['contexts']) == 2

    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {
            "root": new_context_id}})

    assert result == {
        "contexts": [{
            "context": new_context_id,
            "parent": None,
            "url": "about:blank",
            "children": None
        }]}

    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {
            "root": context_id}})

    assert result == {
        "contexts": [{
            "context": context_id,
            "parent": None,
            "url": "about:blank",
            "children": None
        }]}


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_getTreeWithNestedContexts_contextReturned():
    ignore = True
    # TODO sadym: implement


# noinspection PyUnusedLocal
@pytest.mark.asyncio
async def test_browsingContext_create_eventContextCreatedEmitted(
      websocket, context_id):
    await subscribe(websocket, ["browsingContext.contextCreated"])

    await send_JSON_command(websocket, {
        "id": 9,
        "method": "browsingContext.create",
        "params": {"type": "tab"}})

    resp = await read_JSON_message(websocket)

    if "params" in resp:
        event = resp
        command_result = await read_JSON_message(websocket)
    else:
        event = await read_JSON_message(websocket)
        command_result = resp

    new_context_id = event['params']['context']

    # Assert "browsingContext.contextCreated" event emitted.
    assert event == {
        "method": "browsingContext.contextCreated",
        "params": {
            'context': new_context_id,
            'parent': None,
            'children': [],
            'url': 'about:blank'}}

    # Assert command done.
    assert command_result == {
        "id": 9,
        "result": {
            'context': new_context_id,
            'parent': None,
            'children': [],
            'url': 'about:blank'}}


@pytest.mark.asyncio
async def test_browsingContext_close_browsingContext_closed(
      websocket, context_id):
    await subscribe(websocket, ["browsingContext.contextDestroyed"])

    # Send command.
    command = {"id": 12, "method": "browsingContext.close",
               "params": {"context": context_id}}
    await send_JSON_command(websocket, command)

    # Assert "browsingContext.contextCreated" event emitted.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "method": "browsingContext.contextDestroyed",
        "params": {
            "context": context_id,
            "parent": None,
            "url": "about:blank",
            "children": []}}

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {"id": 12, "result": {}}

    result = await execute_command(websocket,
                                   {"method": "browsingContext.getTree",
                                    "params": {}})

    # Assert context is closed.
    assert result == {'contexts': []}


@pytest.mark.asyncio
async def test_browsingContext_navigateWaitNone_navigated(websocket,
      context_id):
    await subscribe(websocket, ["browsingContext.domContentLoaded",
                                "browsingContext.load"])
    # Send command.
    await send_JSON_command(websocket, {
        "id": 13,
        "method": "browsingContext.navigate",
        "params": {
            "url": "data:text/html,<h2>test</h2>",
            "wait": "none",
            "context": context_id}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    navigation_id = resp["result"]["navigation"]
    assert resp == {
        "id": 13,
        "result": {
            "navigation": navigation_id,
            "url": "data:text/html,<h2>test</h2>"}}

    # Wait for `browsingContext.load` event.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "method": "browsingContext.load",
        "params": {
            "context": context_id,
            "navigation": navigation_id}}

    # Wait for `browsingContext.domContentLoaded` event.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "method": "browsingContext.domContentLoaded",
        "params": {
            "context": context_id,
            "navigation": navigation_id}}


@pytest.mark.asyncio
async def test_browsingContext_navigateWaitInteractive_navigated(websocket,
      context_id):
    await subscribe(websocket, ["browsingContext.domContentLoaded",
                                "browsingContext.load"])

    # Send command.
    command = {
        "id": 14,
        "method": "browsingContext.navigate",
        "params": {
            "url": "data:text/html,<h2>test</h2>",
            "wait": "interactive",
            "context": context_id}}
    await send_JSON_command(websocket, command)

    # Wait for `browsingContext.load` event.
    resp = await read_JSON_message(websocket)
    navigation_id = resp["params"]["navigation"]
    assert resp == {
        "method": "browsingContext.load",
        "params": {
            "context": context_id,
            "navigation": navigation_id}}

    # Wait for `browsingContext.domContentLoaded` event.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "method": "browsingContext.domContentLoaded",
        "params": {
            "context": context_id,
            "navigation": navigation_id}}

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id": 14,
        "result": {
            "navigation": navigation_id,
            "url": "data:text/html,<h2>test</h2>"}}


@pytest.mark.asyncio
async def test_browsingContext_navigateSameDocumentNavigation_navigated(
      websocket,
      context_id):
    url = "data:text/html,<h2>test</h2>"
    url_with_hash_1 = url + "#1"
    url_with_hash_2 = url + "#2"

    # Initial navigation.
    await execute_command(websocket, {
        "method": "browsingContext.navigate",
        "params": {
            "url": url,
            "wait": "complete",
            "context": context_id}})

    # Navigate back and forth in the same document with `wait:none`.
    resp = await execute_command(websocket, {
        "method": "browsingContext.navigate",
        "params": {
            "url": url_with_hash_1,
            "wait": "none",
            "context": context_id}})
    recursiveCompare({
        'navigation': '__any_value__',
        'url': url_with_hash_1},
        resp, "navigation")

    resp = await execute_command(websocket, {
        "method": "browsingContext.navigate",
        "params": {
            "url": url_with_hash_2,
            "wait": "none",
            "context": context_id}})
    recursiveCompare({
        'navigation': '__any_value__',
        'url': url_with_hash_2},
        resp, "navigation")

    # Navigate back and forth in the same document with `wait:interactive`.
    resp = await execute_command(websocket, {
        "method": "browsingContext.navigate",
        "params": {
            "url": url_with_hash_1,
            "wait": "interactive",
            "context": context_id}})
    recursiveCompare({
        'navigation': '__any_value__',
        'url': url_with_hash_1},
        resp, "navigation")

    resp = await execute_command(websocket, {
        "method": "browsingContext.navigate",
        "params": {
            "url": url_with_hash_2,
            "wait": "interactive",
            "context": context_id}})
    recursiveCompare({
        'navigation': '__any_value__',
        'url': url_with_hash_2},
        resp, "navigation")

    # Navigate back and forth in the same document with `wait:complete`.
    resp = await execute_command(websocket, {
        "method": "browsingContext.navigate",
        "params": {
            "url": url_with_hash_1,
            "wait": "complete",
            "context": context_id}})
    recursiveCompare({
        'navigation': '__any_value__',
        'url': url_with_hash_1},
        resp, "navigation")

    resp = await execute_command(websocket, {
        "method": "browsingContext.navigate",
        "params": {
            "url": url_with_hash_2,
            "wait": "complete",
            "context": context_id}})
    recursiveCompare({
        'navigation': '__any_value__',
        'url': url_with_hash_2},
        resp, "navigation")


@pytest.mark.asyncio
async def test_PROTO_browsingContext_findElement_findsElement(websocket,
      context_id):
    await goto_url(websocket, context_id,
                   "data:text/html," +
                   "<div class='container'>container text" +
                   "<h2 class='child_1'>child 1 text</h2>" +
                   "<h2 class='child_2'>child 2 text</h2>" +
                   "</div>")

    result = await execute_command(websocket, {
        "method": "PROTO.browsingContext.findElement",
        "params": {
            "selector": "body > .container",
            "context": context_id}})

    recursiveCompare({
        "result": {
            "type": "node",
            "value": {
                "nodeType": 1,
                "nodeValue": "",
                "nodeName": "",
                "localName": "div",
                "namespaceURI": "http://www.w3.org/1999/xhtml",
                "childNodeCount": 3,
                "attributes": {
                    "class": "container"},
                "children": [{
                    "type": "node",
                    "value": {
                        "nodeType": 3,
                        "nodeValue": "container text",
                        "nodeName": "container text"}
                }, {
                    "type": "node",
                    "value": {
                        "nodeType": 1,
                        "nodeValue": "",
                        "nodeName": "",
                        "localName": "h2",
                        "namespaceURI": "http://www.w3.org/1999/xhtml",
                        "childNodeCount": 1,
                        "attributes": {
                            "class": "child_1"}}
                }, {
                    "type": "node",
                    "value": {
                        "nodeType": 1,
                        "nodeValue": "",
                        "nodeName": "",
                        "localName": "h2",
                        "namespaceURI": "http://www.w3.org/1999/xhtml",
                        "childNodeCount": 1,
                        "attributes": {
                            "class": "child_2"}}
                }]
            }, "handle": "__SOME_handle_1__"
        }}, result, ["handle"])


@pytest.mark.asyncio
async def test_PROTO_browsingContext_findElementMissingElement_missingElement(
      websocket, context_id):
    await goto_url(websocket, context_id,
                   "data:text/html,<h2>test</h2>")

    result = await execute_command(websocket, {
        "method": "PROTO.browsingContext.findElement",
        "params": {
            "selector": "body > h3",
            "context": context_id}})

    assert result == {"result": {"type": "null"}}


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_type_textTyped():
    ignore = True
    # TODO sadym: implement


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_navigateWithShortTimeout_timeoutOccurredAndEventPageLoadEmitted():
    ignore = True
    # TODO sadym: implement


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_waitForSelector_success():
    ignore = True
    # TODO sadym: implement


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_waitForSelector_success_slow():
    # 1. Wait for element which is not on the page.
    # 2. Assert element not found.
    # 3. Add element to the page.
    # 4. Wait for newly created element.
    # 5. Assert element found.

    ignore = True
    # TODO sadym: implement


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_waitForHiddenSelector_success():
    ignore = True
    # TODO sadym: implement


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_waitForSelectorWithMinimumTimeout_failedWithTimeout():
    ignore = True
    # TODO sadym: implement


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_waitForSelectorWithMissingElement_failedWithTimeout_slow():
    ignore = True
    # TODO sadym: implement


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContext_clickElement_clickProcessed():
    ignore = True
    # TODO sadym: implement
