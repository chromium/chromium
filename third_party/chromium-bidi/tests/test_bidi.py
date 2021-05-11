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
    # Send "PROTO.browsingContext.navigate" command.
    command = {
        "id": 9998,
        "method": "PROTO.browsingContext.navigate",
        "params": {
            "url": url,
            "context": contextID}}
    await send_JSON_command(websocket, command)

    # Assert "DEBUG.Page.load" event emitted.
    resp = await read_JSON_message(websocket)
    assert resp["method"] == "DEBUG.Page.load"

    # Assert "PROTO.browsingContext.navigate" command done.
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
                    "parent": None,
                    "url": "about:blank"}]}}

@pytest.mark.asyncio
async def test_createContext_eventContextCreatedEmittedAndContextCreated(websocket):
    # Send command.
    command = {
        "id": 9,
        "method": "PROTO.browsingContext.createContext",
        "params": {
            "url": "data:text/html,<h2>test</h2>"}}
    await send_JSON_command(websocket, command)

    # Assert "browsingContext.contextCreated" event emitted.
    resp = await read_JSON_message(websocket)
    contextID = resp['params']['context']
    assert resp == {
        "method": "browsingContext.contextCreated",
        "params": {
            "context":contextID,
            "parent": None,
            "url": "data:text/html,<h2>test</h2>"}}

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id": 9,
        "result": {
            "context": contextID,
            "parent": None,
            "url": "data:text/html,<h2>test</h2>"}}

@pytest.mark.asyncio
async def test_PageClose_browsingContextContextDestroyedEmitted(websocket):
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
async def test_navigate_eventPageLoadEmittedAndNavigated(websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    command = {
        "id": 15,
        "method": "PROTO.browsingContext.navigate",
        "params": {
            "url": "data:text/html,<h2>test</h2>",
            "waitUntil": ["load", "domcontentloaded", "networkidle0", "networkidle2"],
            "context": contextID}}
    await send_JSON_command(websocket, command)

    # Assert "DEBUG.Page.load" event emitted.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "method": "DEBUG.Page.load",
        "params": {
            "context": contextID}}

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {"id": 15, "result": {}}

@pytest.mark.asyncio
async def test_navigateWithShortTimeout_timeoutOccuredAndEventPageLoadEmitted(websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    command = {
        "id": 16,
        "method": "PROTO.browsingContext.navigate",
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
async def test_waitForSelector_success(websocket):
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
async def test_waitForSelector_success_slow(websocket):
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
        "method": "PROTO.page.evaluate",
        "params": {
            "function": "document.documentElement.innerHTML='<h2 />'",
            "context": contextID}})

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
async def test_waitForHiddenSelector_success(websocket):
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
async def test_waitForSelectorWithMinimumTimeout_failedWithTimeout(websocket):
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
async def test_waitForSelectorWithMissingElement_failedWithTimeout_slow(websocket):
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
async def test_clickElement_clickProcessed(websocket):
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
async def test_selectElement_success(websocket):
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
async def test_selectElementMissingElement_missingElement(websocket):
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
async def test_pageEvaluateWithElement_resultReceived(websocket):
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
        "method": "PROTO.page.evaluate",
        "params": {
            "function": "element => '!!@@##, ' + element.innerHTML",
    # TODO: send properly serialised element according to
    # https://w3c.github.io/webdriver-bidi/#data-types-remote-value.
            "args": [{
                "objectId": objectID}],
            "context": contextID}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id":31,
        "result":{
            "type":"string",
            "value":"!!@@##, test"}}

@pytest.mark.asyncio
async def test_pageEvaluateWithoutArgs_resultReceived(websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    await send_JSON_command(websocket, {
        "id": 32,
        "method": "PROTO.page.evaluate",
        "params": {
            "function": "'!!@@##, ' + window.location.href",
            "context": contextID}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id":32,
        "result":{
            "type":"string",
            "value":"!!@@##, about:blank"}}

@pytest.mark.asyncio
async def test_pageEvaluateWithScalarArgs_resultReceived(websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    await send_JSON_command(websocket, {
        "id": 45,
        "method": "PROTO.page.evaluate",
        "params": {
    # TODO: send properly serialised scalars according to
    # https://w3c.github.io/webdriver-bidi/#data-types-remote-value.
            "args": [1, 2],
            "function": "(a,b) => a+b",
            "context": contextID}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id":45,
        "result":{
            "type":"number",
            "value":3}}

@pytest.mark.asyncio
async def test_consoleLog_logEntryAddedEventEmmited(websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    await send_JSON_command(websocket, {
        "id": 33,
        "method": "PROTO.page.evaluate",
        "params": {
            "function": "console.log('some log message')",
            "context": contextID}})

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
async def test_browsingContextType_textTyped(websocket):
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
        "method": "PROTO.page.evaluate",
        "params": {
            "function": "element => element.value",
    # TODO: send properly serialised element according to
    # https://w3c.github.io/webdriver-bidi/#data-types-remote-value.
            "args": [{
                "objectId": objectID}],
            "context": contextID}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id":36,
        "result":{
            "type":"string",
            "value":"!!@@## test text"}}

@pytest.mark.asyncio
async def test_consoleInfo_logEntryWithMethodInfoEmmited(websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    await send_JSON_command(websocket, {
        "id": 43,
        "method": "PROTO.page.evaluate",
        "params": {
            "function": "console.info('some log message')",
            "context": contextID}})

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
async def test_consoleError_logEntryWithMethodErrorEmmited(websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    await send_JSON_command(websocket, {
        "id": 44,
        "method": "PROTO.page.evaluate",
        "params": {
            "function": "console.error('some log message')",
            "context": contextID}})

    # Assert method "error".
    resp = await read_JSON_message(websocket)

    assert resp["method"] == "log.entryAdded"
    assert resp["params"]["method"] == "error"

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id":44,
        "result":{"type":"undefined"}}

# Testing serialisation.
async def assertSerialisation(jsStrObject, expectedSerialisedObject, websocket):
    contextID = await get_open_context_id(websocket)

    # Send command.
    await send_JSON_command(websocket, {
        "id": 9997,
        "method": "PROTO.page.evaluate",
        "params": {
            "function": f"({jsStrObject})",
            "context": contextID}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == 9997

    # Compare ignoring `objectId`.
    recursiveCompare(expectedSerialisedObject, resp["result"], ["objectId"])

@pytest.mark.asyncio
async def test_serialisation_undefined(websocket):
    await assertSerialisation(
        "undefined",
        {"type":"undefined"},
        websocket)

@pytest.mark.asyncio
async def test_serialisation_null(websocket):
    await assertSerialisation(
        "null",
        {"type":"null"},
        websocket)

# TODO: test escaping, null bytes string, lone surrogates.
@pytest.mark.asyncio
async def test_serialisation_string(websocket):
    await assertSerialisation(
        "'someStr'",
        {
            "type":"string",
            "value":"someStr"},
        websocket)

@pytest.mark.asyncio
async def test_serialisation_number(websocket):
    await assertSerialisation(
        "123",
        {
            "type":"number",
            "value":123},
        websocket)
    await assertSerialisation(
        "0.56",
        {
            "type":"number",
            "value":0.56},
        websocket)

@pytest.mark.asyncio
async def test_serialisation_specialNumber(websocket):
    await assertSerialisation(
        "+Infinity",
        {
            "type":"number",
            "value":"+Infinity"},
        websocket)
    await assertSerialisation(
        "-Infinity",
        {
            "type":"number",
            "value":"-Infinity"},
        websocket)
    await assertSerialisation(
        "-0",
        {
            "type":"number",
            "value":"-0"},
        websocket)
    await assertSerialisation(
        "NaN",
        {
            "type":"number",
            "value":"NaN"},
        websocket)

@pytest.mark.asyncio
async def test_serialisation_bool(websocket):
    await assertSerialisation(
        "true",
        {
            "type":"boolean",
            "value":True},
        websocket)
    await assertSerialisation(
        "false",
        {
            "type":"boolean",
            "value":False},
        websocket)

@pytest.mark.asyncio
async def test_serialisation_bigint(websocket):
    await assertSerialisation(
        "BigInt('12345678901234567890')",
        {
            "type":"bigint",
            "value":"12345678901234567890"},
        websocket)

@pytest.mark.asyncio
async def test_serialisation_symbol(websocket):
    await assertSerialisation(
        "Symbol('foo')",
        {
            "type":"symbol",
            "PROTO.description":"foo",
            "objectId":"__any_value__"
        },
        websocket)

@pytest.mark.asyncio
async def test_serialisation_function(websocket):
    await assertSerialisation(
        "function(){}",
        {
            "type":"function",
            "objectId":"__any_value__"
        },
        websocket)

@pytest.mark.asyncio
async def test_serialisation_regExp(websocket):
    await assertSerialisation(
        "new RegExp('ab+c')",
        {
            "type":"regexp",
            "value":"/ab+c/",
            "objectId":"__any_value__"
        },
        websocket)

# TODO: check timezone serialisation.
@pytest.mark.asyncio
async def test_serialisation_date(websocket):
    await assertSerialisation(
        "new Date('2021-02-18T13:53:00+0200')",
        {
            "type":"date",
            "value":"Thu Feb 18 2021 11:53:00 GMT+0000 (Coordinated Universal Time)",
            "objectId":"__any_value__"},
        websocket)

@pytest.mark.asyncio
async def test_serialisation_windowProxy(websocket):
    await assertSerialisation(
        "this.window",
        {
            "type":"window",
            "objectId":"__any_value__"
        },
        websocket)

@pytest.mark.asyncio
async def test_serialisation_error(websocket):
    await assertSerialisation(
        "new Error('Woops!')",
        {
            "type":"error",
            "objectId":"__any_value__"
        },
        websocket)

# TODO: implement after serialisation MaxDepth logic specified:
# https://github.com/w3c/webdriver-bidi/issues/86
@pytest.mark.asyncio
async def test_serialisation_array(websocket):
    await assertSerialisation(
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

# TODO: implement after serialisation MaxDepth logic specified:
# https://github.com/w3c/webdriver-bidi/issues/86
@pytest.mark.asyncio
async def test_serialisation_object(websocket):
    await assertSerialisation(
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

# TODO: add `NodeProperties` after serialisation MaxDepth logic specified:
# https://github.com/w3c/webdriver-bidi/issues/86.
@pytest.mark.asyncio
async def test_serialisation_node(websocket):
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
        "method": "PROTO.page.evaluate",
        "params": {
            "function": "element => element",
    # TODO: send properly serialised element according to
    # https://w3c.github.io/webdriver-bidi/#data-types-remote-value.
            "args": [{
                "objectId": objectId}],
            "context": contextID}})

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





# TODO: implement proper serialisation according to
# https://w3c.github.io/webdriver-bidi/#data-types-remote-value.

# @pytest.mark.asyncio
# async def test_serialisation_map(websocket):

# @pytest.mark.asyncio
# async def test_serialisation_set(websocket):

# @pytest.mark.asyncio
# async def test_serialisation_weakMap(websocket):

# @pytest.mark.asyncio
# async def test_serialisation_weakSet(websocket):

# @pytest.mark.asyncio
# async def test_serialisation_iterator(websocket):

# @pytest.mark.asyncio
# async def test_serialisation_generator(websocket):

# @pytest.mark.asyncio
# async def test_serialisation_proxy(websocket):

# @pytest.mark.asyncio
# async def test_serialisation_promise(websocket):

# @pytest.mark.asyncio
# async def test_serialisation_typedArray(websocket):

# @pytest.mark.asyncio
# async def test_serialisation_arrayBuffer(websocket):
