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
import os
import pytest
import websockets

@pytest.fixture
async def websocket():
    port = os.getenv('PORT', 8080)
    url = f'ws://localhost:{port}'
    async with websockets.connect(url) as connection:
        yield connection

@pytest.fixture(autouse=True)
async def before_each_test(websocket):
    # Read initial event `browsingContext.contextCreated`
    resp = await read_JSON_message(websocket)
    assert resp['method'] == 'browsingContext.contextCreated'

# Compares 2 objects recursively ignoring values of specific attributes.
def recursiveCompare(expected, actual, ignoreAttributes):
    assert type(expected) == type(actual)
    if type(expected) is list:
        assert len(expected) == len(actual)
        for index, val in enumerate(expected):
            recursiveCompare(expected[index], actual[index], ignoreAttributes)
        return

    if type(expected) is dict:
        assert expected.keys() == actual.keys()
        for index, val in enumerate(expected):
            if val not in ignoreAttributes:
                recursiveCompare(expected[val], actual[val], ignoreAttributes)
        return

    assert expected == actual

# Returns the only open contextID.
# Throws an exception the context is not unique.
async def get_open_context_id(websocket):
    # Send "browsingContext.getTree" command.
    command = {"id": 9999, "method": "browsingContext.getTree", "params": {}}
    await send_JSON_command(websocket, command)
    # Get open context ID.
    resp = await read_JSON_message(websocket)
    assert resp['id'] == 9999
    [context] = resp['result']['contexts']
    contextID = context['context']
    return contextID

async def send_JSON_command(websocket, command):
    await websocket.send(json.dumps(command))

async def read_JSON_message(websocket):
    return json.loads(await websocket.recv())

# Open given URL in the given context.
async def goto_url(websocket, contextID, url):
    # Send "browsingContext.navigate" command.
    command = {
        "id": 9998,
        "method": "browsingContext.navigate",
        "params": {
            "url": url,
            "context": contextID}}
    await send_JSON_command(websocket, command)

    # Assert "DEBUG.Page.load" event emitted.
    resp = await read_JSON_message(websocket)
    assert resp["method"] == "DEBUG.Page.load"

    # Assert "browsingContext.navigate" command done.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == 9998
    return contextID

# Tests for "handle an incoming message" error handling, when the message
# can't be decoded as known command.
# https://w3c.github.io/webdriver-bidi/#handle-an-incoming-message

@pytest.mark.asyncio
async def test_binary(websocket):
    # session.status is used in this test, but any simple command without side
    # effects would work. It is first sent as text, which should work, and then
    # sent again as binary, which should get an error response instead.
    command = {"id": 1, "method": "session.status", "params": {}}

    text_msg = json.dumps(command)
    await websocket.send(text_msg)
    resp = await read_JSON_message(websocket)
    assert resp['id'] == 1

    binary_msg = 'text_msg'.encode('utf-8')
    await websocket.send(binary_msg)
    resp = await read_JSON_message(websocket)
    assert resp == {
        "error": "invalid argument",
        "message": "not supported type (binary)"}

@pytest.mark.asyncio
async def test_invalid_json(websocket):
    message = 'this is not json'
    await websocket.send(message)
    resp = await read_JSON_message(websocket)
    assert resp == {
        "error": "invalid argument",
        "message": "Cannot parse data as JSON"}

@pytest.mark.asyncio
async def test_empty_object(websocket):
    command = {}
    await send_JSON_command(websocket, command)
    resp = await read_JSON_message(websocket)
    assert resp == {
        "error": "invalid argument",
        "message": "Expected unsigned integer but got undefined"}

@pytest.mark.asyncio
async def test_session_status(websocket):
    command = {"id": 5, "method": "session.status", "params": {}}
    await send_JSON_command(websocket, command)
    resp = await read_JSON_message(websocket)
    assert resp == {"id": 5, "result": {"ready": True, "message": "ready"}}

@pytest.mark.asyncio
async def test_getTree_contextReturned(websocket):
    command = {"id": 8, "method": "browsingContext.getTree", "params": {}}
    await send_JSON_command(websocket, command)

    # Assert "browsingContext.getTree" command done.
    resp = await read_JSON_message(websocket)
    [context] = resp['result']['contexts']
    contextID = context['context']
    assert isinstance(contextID, str)
    assert len(contextID) > 1
    assert resp == {
        "id": 8,
        "result": {
            "contexts": [{
                    "context": contextID,
                    "children": [],
                    "url": "about:blank"}]}}

@pytest.mark.asyncio
# Not implemented yet.
async def ignore_test_getTreeWithGivenParent_contextReturned(websocket):
    ignore = True
    # TODO sadym: implement

@pytest.mark.asyncio
# Not implemented yet.
async def ignore_test_getTreeWithNestedContexts_contextReturned(websocket):
    ignore = True
    # TODO sadym: implement

@pytest.mark.asyncio
async def test_PROTO_browsingContextCreate_eventContextCreatedEmittedAndContextCreated(websocket):
    initialContextID = await get_open_context_id(websocket)

    command = {
        "id": 9,
        "method": "PROTO.browsingContext.create",
        "params": {}}
    await send_JSON_command(websocket, command)

    # Assert "browsingContext.contextCreated" event emitted.
    resp = await read_JSON_message(websocket)
    newContextID = resp['params']['context']
    assert resp == {
        "method": "browsingContext.contextCreated",
        "params": {
            "context": newContextID,
            "children": [],
            "url": ""}}

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id": 9,
        "result": {
            "context": newContextID,
            "children": [],
            "url": ""}}

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
                "context": newContextID,
                "url": "about:blank",
                "children": []
            }, {
                "context": initialContextID,
                "url": "about:blank",
                "children": [] }]}}

@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_PageClose_browsingContextContextDestroyedEmitted(websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    command = {"id": 12, "method": "DEBUG.Page.close", "params": {"context": contextID}}
    await send_JSON_command(websocket, command)

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {"id": 12, "result": {}}

    # Assert "browsingContext.contextCreated" event emitted.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "method": "browsingContext.contextDestroyed",
        "params": {
            "context": contextID,
            "parent": None,
            "url": "about:blank"}}

@pytest.mark.asyncio
async def test_navigateWaitNone_navigated(websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    command = {
        "id": 13,
        "method": "browsingContext.navigate",
        "params": {
            "url": "data:text/html,<h2>test</h2>",
            "wait": "none",
            "context": contextID}}
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
# Not implemented yet.
async def _ignore_test_navigateWaitInteractive_navigated(websocket):
    ignore = True

@pytest.mark.asyncio
# TODO sadym: find a way to test if the command ended only after the DOM
# actually loaded.
async def test_navigateWaitInteractive_navigated(websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    command = {
        "id": 14,
        "method": "browsingContext.navigate",
        "params": {
            "url": "data:text/html,<h2>test</h2>",
            "wait": "interactive",
            "context": contextID}}
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
async def _ignore_test_navigateWaitInteractive_navigated(websocket):
    ignore = True

@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_navigateWaitComplete_navigated(websocket):
    ignore = True

@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_navigateWithShortTimeout_timeoutOccuredAndEventPageLoadEmitted(websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    command = {
        "id": 16,
        "method": "browsingContext.navigate",
        "params": {
            "url": "data:text/html,<h2>test</h2>",
            "context": contextID,
            "waitUntil": ["load", "domcontentloaded", "networkidle0", "networkidle2"],
            "timeout":"1"}}

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
            "context": contextID}}

@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_waitForSelector_success(websocket):
    contextID = await get_open_context_id(websocket)
    await goto_url(websocket, contextID,
        "data:text/html,<h2>test</h2>")

    # Send command.
    await send_JSON_command(websocket, {
        "id": 17,
        "method": "PROTO.browsingContext.waitForSelector",
        "params": {
            "selector": "body > h2",
            "context": contextID}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    recursiveCompare(
        resp,
        {
            "id": 17,
            "result": {
                "type": "node",
                "objectId": "__any_value__" }},
        ["objectId"])

@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_waitForSelector_success_slow(websocket):
# 1. Wait for element which is not on the page.
# 2. Assert element not found.
# 3. Add element to the page.
# 4. Wait for newly created element.
# 5. Assert element found.

    contextID = await get_open_context_id(websocket)

# 1. Wait for element which is not on the page.
    await send_JSON_command(websocket, {
        "id": 18,
        "method": "PROTO.browsingContext.waitForSelector",
        "params": {
            "selector": "body > h2",
            "timeout": 1000,
            "context": contextID}})

# 2. Assert element not found.
    resp = await read_JSON_message(websocket)
    assert resp == {"id":18,"error":"unknown error","message":"waiting for selector `body > h2` failed: timeout 1000ms exceeded"}

# 3. Add element to the page.
    await send_JSON_command(websocket, {
        "id": 19,
        "method": "script.evaluate",
        "params": {
            "expression": "document.documentElement.innerHTML='<h2 />'",
            "target": {"context": contextID}}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id":19,
        "result":{
            "type":"string",
            "value":"<h2 />"}}

# 4. Wait for newly created element.
    await send_JSON_command(websocket, {
        "id": 20,
        "method": "PROTO.browsingContext.waitForSelector",
        "params": {
            "selector": "body > h2",
            "timeout": 1000,
            "context": contextID}})

# 5. Assert element found.
    resp = await read_JSON_message(websocket)
    recursiveCompare(
        resp,
        {
            "id": 20,
            "result": {
                "type": "node",
                "objectId": "__any_value__" }},
        ["objectId"])

@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_waitForHiddenSelector_success(websocket):
    contextID = await get_open_context_id(websocket)
    await goto_url(websocket, contextID,
        "data:text/html,<h2>test</h2>")

    # Send command.
    await send_JSON_command(websocket, {
        "id": 21,
        "method": "PROTO.browsingContext.waitForSelector",
        "params": {
            "selector": "body > h3",
            "context": contextID,
            "hidden": True}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {"id": 21, "result": {}}

@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_waitForSelectorWithMinimumTimeout_failedWithTimeout(websocket):
    contextID = await get_open_context_id(websocket)
    await goto_url(websocket, contextID,
        "data:text/html,<h2>test</h2>")

    # Send command.
    await send_JSON_command(websocket, {
        "id": 22,
        "method": "PROTO.browsingContext.waitForSelector",
        "params": {
            "selector": "body > h2",
            "timeout": 1,
            "context": contextID}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id": 22,
        "error": "unknown error",
        "message": "waiting for selector `body > h2` failed: timeout 1ms exceeded"}

@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_waitForSelectorWithMissingElement_failedWithTimeout_slow(websocket):
    contextID = await get_open_context_id(websocket)
    await goto_url(websocket, contextID,
        "data:text/html,<h2>test</h2>")

    # Send command.
    await send_JSON_command(websocket, {
        "id": 23,
        "method": "PROTO.browsingContext.waitForSelector",
        "params": {
            "selector": "body > h3",
            "timeout": 1000,
            "context": contextID}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id": 23,
        "error": "unknown error",
        "message": "waiting for selector `body > h3` failed: timeout 1000ms exceeded"}

@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_clickElement_clickProcessed(websocket):
# 1. Open page with button and click handler. Button click logs message.
# 2. Get the button.
# 3. Click the button.
# 4. Assert console log event emmited.
# 5. Assert click command done.

    contextID = await get_open_context_id(websocket)

# 1. Open page with button and click handler. Button click logs message.
    await goto_url(websocket, contextID,
        "data:text/html,<button onclick=\"console.log('button clicked')\">button</button>")

# 2. Get the button.
    # Send command.
    await send_JSON_command(websocket, {
        "id": 25,
        "method": "PROTO.browsingContext.waitForSelector",
        "params": {
            "selector": "body > button",
            "context": contextID}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == 25
    objectID = resp["result"]["objectId"]

# 3. Click the button.
    await send_JSON_command(websocket, {
        "id": 26,
        "method": "PROTO.browsingContext.click",
        "params": {
            "objectId": objectID,
            "context": contextID}})

# 4. Assert console log event emmited.
    resp = await read_JSON_message(websocket)
    assert resp["method"] == "log.entryAdded"
    assert resp["params"]["text"] == "button clicked"

# 5. Assert click command done.
    resp = await read_JSON_message(websocket)
    assert resp ==  {"id": 26, "result": {}}

@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_selectElement_success(websocket):
    contextID = await get_open_context_id(websocket)
    await goto_url(websocket, contextID,
        "data:text/html,<h2>test</h2>")

    # Send command.
    await send_JSON_command(websocket, {
        "id": 28,
        "method": "PROTO.browsingContext.selectElement",
        "params": {
            "selector": "body > h2",
            "context": contextID}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    recursiveCompare(
        resp,
        {
            "id": 28,
                "result": {
                    "type": "node",
                    "objectId": "__any_value__" }},
        ["objectId"])

@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_selectElementMissingElement_missingElement(websocket):
    contextID = await get_open_context_id(websocket)
    await goto_url(websocket, contextID,
        "data:text/html,<h2>test</h2>")

    # Send command.
    await send_JSON_command(websocket, {
        "id": 29,
        "method": "PROTO.browsingContext.selectElement",
        "params": {
            "selector": "body > h3",
            "context": contextID}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {"id":29, "result": {}}

@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_pageEvaluateWithElement_resultReceived(websocket):
# 1. Get element.
# 2. Evaluate script on it.
    contextID = await get_open_context_id(websocket)
    await goto_url(websocket, contextID,
        "data:text/html,<h2>test</h2>")

# 1. Get element.
    # Send command.
    await send_JSON_command(websocket, {
        "id": 30,
        "method": "PROTO.browsingContext.selectElement",
        "params": {
            "selector": "body > h2",
            "context": contextID}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == 30
    objectID = resp["result"]["objectId"]

# 2. Evaluate script on it.
    # Send command.
    await send_JSON_command(websocket, {
        "id": 31,
        "method": "script.evaluate",
        "params": {
            "expression": "element => '!!@@##, ' + element.innerHTML",
    # TODO: send properly serialized element according to
    # https://w3c.github.io/webdriver-bidi/#data-types-remote-value.
            "args": [{
                "objectId": objectID}],
            "target": {"context": contextID}}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id":31,
        "result":{
            "type":"string",
            "value":"!!@@##, test"}}

@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_pageEvaluateWithoutArgs_resultReceived(websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    await send_JSON_command(websocket, {
        "id": 32,
        "method": "script.evaluate",
        "params": {
            "expression": "'!!@@##, ' + window.location.href",
            "target": {"context": contextID}}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id":32,
        "result":{
            "type":"string",
            "value":"!!@@##, about:blank"}}

@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_pageEvaluateWithScalarArgs_resultReceived(websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    await send_JSON_command(websocket, {
        "id": 45,
        "method": "script.evaluate",
        "params": {
    # TODO: send properly serialized scalars according to
    # https://w3c.github.io/webdriver-bidi/#data-types-remote-value.
            "args": [1, 2],
            "expression": "(a,b) => a+b",
            "target": {"context": contextID}}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id":45,
        "result":{
            "type":"number",
            "value":3}}

@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_consoleLog_logEntryAddedEventEmmited(websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    await send_JSON_command(websocket, {
        "id": 33,
        "method": "script.evaluate",
        "params": {
            "expression": "console.log('some log message')",
            "target": {"context": contextID}}})

    # Assert "log.entryAdded" event emitted.
    resp = await read_JSON_message(websocket)
    recursiveCompare(
        resp,
        {
            "method":"log.entryAdded",
            "params":{
                # BaseLogEntry
                "level":"info",
                "text":"some log message",
                "timestamp":"__any_value__",
                "stackTrace":[{
                    "url":"__any_value__",
                    "functionName":"",
                    "lineNumber":0,
                    "columnNumber":8}],
                # ConsoleLogEntry
                "type": "console",
                "method": "log",
                # TODO: replace `PROTO.context` with `realm`.
                "PROTO.context":contextID,
                "args":[{
                    "type":"string",
                    "value":"some log message"}]}},
        ["timestamp", "url"])

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id":33,
        "result":{"type":"undefined"}}

@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_browsingContextType_textTyped(websocket):
# 1. Get input element.
# 2. Type `!!@@## test text` in.
# 3. Assert input.value is `!!@@## test text`.

    contextID = await get_open_context_id(websocket)
    await goto_url(websocket, contextID,
        "data:text/html,<input>")

# 1. Get input element.
    # Send command.
    await send_JSON_command(websocket, {
        "id": 34,
        "method": "PROTO.browsingContext.selectElement",
        "params": {
            "selector": "body > input",
            "context": contextID}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == 34
    objectID = resp["result"]["objectId"]

# 2. Type `!!@@## test text` in.
    # Send command.
    await send_JSON_command(websocket, {
        "id": 35,
        "method": "PROTO.browsingContext.type",
        "params": {
            "text": "!!@@## test text",
            "objectId": objectID,
            "context": contextID}})

    resp = await read_JSON_message(websocket)
    assert resp ==  {"id": 35, "result": {}}

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
                "objectId": objectID}],
            "target": {"context": contextID}}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id":36,
        "result":{
            "type":"string",
            "value":"!!@@## test text"}}

@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_consoleInfo_logEntryWithMethodInfoEmmited(websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    await send_JSON_command(websocket, {
        "id": 43,
        "method": "script.evaluate",
        "params": {
            "expression": "console.info('some log message')",
            "target": {"context": contextID}}})

    # Assert method "info".
    resp = await read_JSON_message(websocket)

    assert resp["method"] == "log.entryAdded"
    assert resp["params"]["method"] == "info"

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id":43,
        "result":{"type":"undefined"}}

@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_consoleError_logEntryWithMethodErrorEmmited(websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    await send_JSON_command(websocket, {
        "id": 44,
        "method": "script.evaluate",
        "params": {
            "expression": "console.error('some log message')",
            "target": {"context": contextID}}})

    # Assert method "error".
    resp = await read_JSON_message(websocket)

    assert resp["method"] == "log.entryAdded"
    assert resp["params"]["method"] == "error"

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id":44,
        "result":{"type":"undefined"}}

@pytest.mark.asyncio
async def test_scriptEvaluateThrowing1_exceptionReturned(websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    await send_JSON_command(websocket, {
        "id": 45,
        "method": "script.evaluate",
        "params": {
            "expression": "(()=>{const a=()=>{throw 1;}; const b=()=>{a();};\nconst c=()=>{b();};c();})()",
            "target": {"context": contextID}}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == 45

    # Compare ignoring `objectId`.
    recursiveCompare({
        "exceptionDetails":{
            "text":"Uncaught",
            "columnNumber":19,
            "lineNumber":0,
            "exception":{
                "type":"number",
                "value":1},
            "stackTrace":{
                "callFrames":[{
                    "url":"",
                    "functionName":"a",
                    "lineNumber":0,
                    "columnNumber":19
                },{
                    "url":"",
                    "functionName":"b",
                    "lineNumber":0,
                    "columnNumber":43
                },{
                    "url":"",
                    "functionName":"c",
                    "lineNumber":1,
                    "columnNumber":13
                },{
                    "url":"",
                    "functionName":"",
                    "lineNumber":1,
                    "columnNumber":19
                },{
                    "url":"",
                    "functionName":"",
                    "lineNumber":1,
                    "columnNumber":25}]}}},
        resp["result"], ["objectId"])

@pytest.mark.asyncio
async def test_scriptEvaluateDontWaitPromise_promiseReturned(websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    await send_JSON_command(websocket, {
        "id": 46,
        "method": "script.evaluate",
        "params": {
            "expression": "Promise.resolve('SOME_RESULT')",
            "awaitPromise": False,
            "target": {"context": contextID}}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == 46

    # Compare ignoring `objectId`.
    recursiveCompare({
            "type":"promise",
            "objectId": "__any_value__"
        }, resp["result"]["result"], ["objectId"])

@pytest.mark.asyncio
async def test_scriptEvaluateThenableAndWaitPromise_thenableReturned(websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    await send_JSON_command(websocket, {
        "id": 460,
        "method": "script.evaluate",
        "params": {
            "expression": "{then: (r)=>{r('SOME_RESULT');}}",
            "awaitPromise": True,
            "target": {"context": contextID}}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == 460

    # Compare ignoring `objectId`.
    recursiveCompare({
        "id": 460,
        "result": {
            "result": {
            "objectId": "6648989764296027.141631980526024.9376085393313653",
            "type": "object",
            "value": [[
                "then", {
                    "objectId": "02303443180265008.08175681580349026.8281562053772789",
                    "type": "function"}]]}}},
        resp,
        ["objectId"])

@pytest.mark.asyncio
async def test_scriptEvaluateWaitPromise_resultReturned(websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    await send_JSON_command(websocket, {
        "id": 46,
        "method": "script.evaluate",
        "params": {
            "expression": "Promise.resolve('SOME_RESULT')",
            "awaitPromise": True,
            "target": {"context": contextID}}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == 46

    # Compare ignoring `objectId`.
    recursiveCompare({
        "type":"string",
        "value":"SOME_RESULT"
        }, resp["result"]["result"], ["objectId"])

@pytest.mark.asyncio
async def test_scriptEvaluateChangingObject_resultObjectDidNotChange(websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    await send_JSON_command(websocket, {
        "id": 47,
        "method": "script.evaluate",
        "params": {
            # Create an object and schedule its property to be changed by
            # `setTimeout(..., 0)`. This allows to verify if the object was
            # serialized at the moment of the call or later.
            "expression": """(() => {
                const someObject = { i: 0 };
                const changeObjectAfterCurrentJsThread = () => {
                    setTimeout(() => {
                            someObject.i++;
                            changeObjectAfterCurrentJsThread(); },
                        0); };
                changeObjectAfterCurrentJsThread();
                return someObject; })()""",
            "awaitPromise": True,
            "target": {"context": contextID}}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == 47

    # Verify the object wasn't changed.
    recursiveCompare({
        "type": "object",
        "value": [[
            "i", {
            "type": "number",
            "value": 0
            }]],
        "objectId": "__any_value__"
        }, resp["result"]["result"], ["objectId"])

@pytest.mark.asyncio
async def test_PROTO_scriptInvokeWithArgs_invokeResultReturn(websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    await send_JSON_command(websocket, {
        "id": 48,
        "method": "PROTO.script.invoke",
        "params": {
            "functionDeclaration": "(...args)=>{return Promise.resolve(args);}",
            "args": [{
                "type": "string",
                "value": "ARGUMENT_STRING_VALUE"
            },{
                "type": "number",
                "value": 42
            }],
            "target": {"context": contextID}}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == 48

    recursiveCompare({
        "type": "array",
        "value": [{
            'type': 'string',
            'value': 'ARGUMENT_STRING_VALUE'
        }, {
            'type': 'number',
            'value': 42}],
        "objectId": "__any_value__"
        }, resp["result"]["result"], ["objectId"])

@pytest.mark.asyncio
async def test_PROTO_scriptInvokeWithThenableArgsAndAwaitParam_thenableReturn(websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    await send_JSON_command(websocket, {
        "id": 49,
        "method": "PROTO.script.invoke",
        "params": {
            "functionDeclaration": "(...args)=>({then: (r)=>{r(args);}})",
            "args": [{
                "type": "string",
                "value": "ARGUMENT_STRING_VALUE"
            },{
                "type": "number",
                "value": 42
            }],
            "awaitPromise": True,
            "target": {"context": contextID}}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == 49

    recursiveCompare({
        "id": 49,
        "result": {
            "result": {
            "objectId": "0017540866018586065.8588900581845549.5556004446288361",
            "type": "object",
            "value": [[
                "then", {
                    "objectId": "27880276111744884.36134691065868907.6435355839204784",
                    "type": "function" }]]}}},
        resp,
        ["objectId"])

@pytest.mark.asyncio
async def test_PROTO_scriptInvokeWithArgsAndDoNotAwaitPromise_promiseReturn(websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    await send_JSON_command(websocket, {
        "id": 49,
        "method": "PROTO.script.invoke",
        "params": {
            "functionDeclaration": "(...args)=>{return Promise.resolve(args);}",
            "args": [{
                "type": "string",
                "value": "ARGUMENT_STRING_VALUE"
            },{
                "type": "number",
                "value": 42
            }],
            "awaitPromise": False,
            "target": {"context": contextID}}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == 49

    recursiveCompare({
            "type":"promise",
            "objectId": "__any_value__"
        }, resp["result"]["result"], ["objectId"])

@pytest.mark.asyncio
async def test_PROTO_scriptInvokeWithRemoteValueArgument_resultReturn(websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    await send_JSON_command(websocket, {
        "id": 50,
        "method": "script.evaluate",
        "params": {
            "expression": "{SOME_PROPERTY:'SOME_VALUE'}",
            "awaitPromise": True,
            "target": {"context": contextID}}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == 50

    objectId = resp["result"]["result"]["objectId"]

    # Send command.
    await send_JSON_command(websocket, {
        "id": 51,
        "method": "PROTO.script.invoke",
        "params": {
            "functionDeclaration": "(obj)=>{return obj.SOME_PROPERTY;}",
            "args": [{
                "objectId": objectId
            }],
            "target": {"context": contextID}}})

    resp = await read_JSON_message(websocket)
    assert resp == {
        "id": 51,
        "result": {
            "result": {
                "type": "string",
                "value": "SOME_VALUE"}}}

# Testing serialization.
async def assertSerialization(jsStrObject, expectedSerializedObject, websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    await send_JSON_command(websocket, {
        "id": 9997,
        "method": "script.evaluate",
        "params": {
            "expression": f"({jsStrObject})",
            "target": {"context": contextID}}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == 9997

    # Compare ignoring `objectId`.
    recursiveCompare(expectedSerializedObject, resp["result"]["result"], ["objectId"])

@pytest.mark.asyncio
async def test_serialization_undefined(websocket):
    await assertSerialization(
        "undefined",
        {"type":"undefined"},
        websocket)


@pytest.mark.asyncio
async def test_serialization_null(websocket):
    await assertSerialization(
        "null",
        {"type":"null"},
        websocket)

# TODO: test escaping, null bytes string, lone surrogates.
@pytest.mark.asyncio
async def test_serialization_string(websocket):
    await assertSerialization(
        "'someStr'",
        {
            "type":"string",
            "value":"someStr"},
        websocket)

@pytest.mark.asyncio
async def test_serialization_number(websocket):
    await assertSerialization(
        "123",
        {
            "type":"number",
            "value":123},
        websocket)
    await assertSerialization(
        "0.56",
        {
            "type":"number",
            "value":0.56},
        websocket)

@pytest.mark.asyncio
async def test_serialization_specialNumber(websocket):
    await assertSerialization(
        "+Infinity",
        {
            "type":"number",
            "value":"+Infinity"},
        websocket)
    await assertSerialization(
        "-Infinity",
        {
            "type":"number",
            "value":"-Infinity"},
        websocket)
    await assertSerialization(
        "-0",
        {
            "type":"number",
            "value":"-0"},
        websocket)
    await assertSerialization(
        "NaN",
        {
            "type":"number",
            "value":"NaN"},
        websocket)

@pytest.mark.asyncio
async def test_serialization_bool(websocket):
    await assertSerialization(
        "true",
        {
            "type":"boolean",
            "value":True},
        websocket)
    await assertSerialization(
        "false",
        {
            "type":"boolean",
            "value":False},
        websocket)

@pytest.mark.asyncio
async def test_serialization_function(websocket):
    await assertSerialization(
        "function(){}",
        {
            "type":"function",
            "objectId":"__any_value__"
        },
        websocket)

@pytest.mark.asyncio
async def test_serialization_object(websocket):
    await assertSerialization(
        "{'foo': {'bar': 'baz'}, 'qux': 'quux'}",
        {
            "type":"object",
            "objectId":"__any_value__",
            "value":[[
                "foo", {
                    "type":"object",
                    "objectId":"__any_value__"}],[
                "qux", {
                    "type":"string",
                    "value":"quux"}]]},
        websocket)


@pytest.mark.asyncio
async def test_serialization_array(websocket):
    await assertSerialization(
        "[1, 'a', {foo: 'bar'}, [2,[3,4]]]",
        {
            "type":"array",
            "objectId":"__any_value__",
            "value":[{
                "type":"number",
                "value":1
            },{
                "type":"string",
                "value":"a"
            },{
                "type":"object",
                "objectId":"__any_value__"
            },{
                "type":"array",
                "objectId":"__any_value__"}]},
        websocket)

@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_serialization_bigint(websocket):
    await assertSerialization(
        "BigInt('12345678901234567890')",
        {
            "type":"bigint",
            "value":"12345678901234567890"},
        websocket)

@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_serialization_symbol(websocket):
    await assertSerialization(
        "Symbol('foo')",
        {
            "type":"symbol",
            "PROTO.description":"foo",
            "objectId":"__any_value__"
        },
        websocket)

@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_serialization_regExp(websocket):
    await assertSerialization(
        "new RegExp('ab+c')",
        {
            "type":"regexp",
            "value":"/ab+c/",
            "objectId":"__any_value__"
        },
        websocket)

# TODO: check timezone serialization.
@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_serialization_date(websocket):
    await assertSerialization(
        "new Date('2021-02-18T13:53:00+0200')",
        {
            "type":"date",
            "value":"Thu Feb 18 2021 11:53:00 GMT+0000 (Coordinated Universal Time)",
            "objectId":"__any_value__"},
        websocket)

@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_serialization_windowProxy(websocket):
    await assertSerialization(
        "this.window",
        {
            "type":"window",
            "objectId":"__any_value__"
        },
        websocket)

@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_serialization_error(websocket):
    await assertSerialization(
        "new Error('Woops!')",
        {
            "type":"error",
            "objectId":"__any_value__"
        },
        websocket)

# TODO: add `NodeProperties` after serialization MaxDepth logic specified:
# https://github.com/w3c/webdriver-bidi/issues/86.
@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_serialization_node(websocket):
    contextID = await get_open_context_id(websocket)
    await goto_url(websocket, contextID,
        "data:text/html,<div some_attr_name='some_attr_value' ><h2>test</h2></div>")

    # Send command.
    await send_JSON_command(websocket, {
        "id": 47,
        "method": "PROTO.browsingContext.waitForSelector",
        "params": {
            "selector": "body > div",
            "context": contextID}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp["id"]==47
    objectId=resp["result"]["objectId"]

    await send_JSON_command(websocket, {
        "id": 48,
        "method": "script.evaluate",
        "params": {
            "expression": "element => element",
    # TODO: send properly serialized element according to
    # https://w3c.github.io/webdriver-bidi/#data-types-remote-value.
            "args": [{
                "objectId": objectId}],
            "target": {"context":contextID}}})

    # Assert result.
    resp = await read_JSON_message(websocket)
    recursiveCompare({
        "id":48,
        "result":{
            "type":"node",
            "objectId":"__any_value__",
            "value":{
                "nodeType":1,
                "nodeValue":None,
                "localName":"div",
                "namespaceURI":"http://www.w3.org/1999/xhtml",
                "childNodeCount":1,
                "children":[{
                    "type":"node",
                    "objectId":"__any_value__"}],
                "attributes":[{
                    "name":"some_attr_name",
                    "value":"some_attr_value"}]}}},
    resp, ["objectId"])


# TODO: implement proper serialization according to
# https://w3c.github.io/webdriver-bidi/#data-types-remote-value.

# @pytest.mark.asyncio
# async def test_serialization_map(websocket):

# @pytest.mark.asyncio
# async def test_serialization_set(websocket):

# @pytest.mark.asyncio
# async def test_serialization_weakMap(websocket):

# @pytest.mark.asyncio
# async def test_serialization_weakSet(websocket):

# @pytest.mark.asyncio
# async def test_serialization_iterator(websocket):

# @pytest.mark.asyncio
# async def test_serialization_generator(websocket):

# @pytest.mark.asyncio
# async def test_serialization_proxy(websocket):

# @pytest.mark.asyncio
# async def test_serialization_promise(websocket):

# @pytest.mark.asyncio
# async def test_serialization_typedArray(websocket):

# @pytest.mark.asyncio
# async def test_serialization_arrayBuffer(websocket):
