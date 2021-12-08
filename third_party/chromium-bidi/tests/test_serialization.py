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


# Testing serialization.
async def assertSerialization(js_str_object, expected_serialized_object,
      websocket):
    context_id = await get_open_context_id(websocket)

    # Send command.
    await send_JSON_command(websocket, {
        "id": 9997,
        "method": "script.evaluate",
        "params": {
            "expression": f"({js_str_object})",
            "target": {"context": context_id}}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == 9997

    # Compare ignoring `objectId`.
    recursiveCompare(expected_serialized_object, resp["result"]["result"],
                     ["objectId"])


@pytest.mark.asyncio
async def test_serialization_undefined(websocket):
    await assertSerialization(
        "undefined",
        {"type": "undefined"},
        websocket)


@pytest.mark.asyncio
async def test_serialization_null(websocket):
    await assertSerialization(
        "null",
        {"type": "null"},
        websocket)


# TODO: test escaping, null bytes string, lone surrogates.
@pytest.mark.asyncio
async def test_serialization_string(websocket):
    await assertSerialization(
        "'someStr'",
        {
            "type": "string",
            "value": "someStr"},
        websocket)


@pytest.mark.asyncio
async def test_serialization_number(websocket):
    await assertSerialization(
        "123",
        {
            "type": "number",
            "value": 123},
        websocket)
    await assertSerialization(
        "0.56",
        {
            "type": "number",
            "value": 0.56},
        websocket)


@pytest.mark.asyncio
async def test_serialization_specialNumber(websocket):
    await assertSerialization(
        "+Infinity",
        {
            "type": "number",
            "value": "+Infinity"},
        websocket)
    await assertSerialization(
        "-Infinity",
        {
            "type": "number",
            "value": "-Infinity"},
        websocket)
    await assertSerialization(
        "-0",
        {
            "type": "number",
            "value": "-0"},
        websocket)
    await assertSerialization(
        "NaN",
        {
            "type": "number",
            "value": "NaN"},
        websocket)


@pytest.mark.asyncio
async def test_serialization_bool(websocket):
    await assertSerialization(
        "true",
        {
            "type": "boolean",
            "value": True},
        websocket)
    await assertSerialization(
        "false",
        {
            "type": "boolean",
            "value": False},
        websocket)


@pytest.mark.asyncio
async def test_serialization_function(websocket):
    await assertSerialization(
        "function(){}",
        {
            "type": "function",
            "objectId": "__any_value__"
        },
        websocket)


@pytest.mark.asyncio
async def test_serialization_object(websocket):
    await assertSerialization(
        "{'foo': {'bar': 'baz'}, 'qux': 'quux'}",
        {
            "type": "object",
            "objectId": "__any_value__",
            "value": [[
                "foo", {
                    "type": "object",
                    "objectId": "__any_value__"}], [
                "qux", {
                    "type": "string",
                    "value": "quux"}]]},
        websocket)


@pytest.mark.asyncio
async def test_serialization_array(websocket):
    await assertSerialization(
        "[1, 'a', {foo: 'bar'}, [2,[3,4]]]",
        {
            "type": "array",
            "objectId": "__any_value__",
            "value": [{
                "type": "number",
                "value": 1
            }, {
                "type": "string",
                "value": "a"
            }, {
                "type": "object",
                "objectId": "__any_value__"
            }, {
                "type": "array",
                "objectId": "__any_value__"}]},
        websocket)


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_serialization_bigint(websocket):
    await assertSerialization(
        "BigInt('12345678901234567890')",
        {
            "type": "bigint",
            "value": "12345678901234567890"},
        websocket)


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_serialization_symbol(websocket):
    await assertSerialization(
        "Symbol('foo')",
        {
            "type": "symbol",
            "PROTO.description": "foo",
            "objectId": "__any_value__"
        },
        websocket)


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_serialization_regExp(websocket):
    await assertSerialization(
        "new RegExp('ab+c')",
        {
            "type": "regexp",
            "value": "/ab+c/",
            "objectId": "__any_value__"
        },
        websocket)


# TODO: check timezone serialization.
@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_serialization_date(websocket):
    await assertSerialization(
        "new Date('2021-02-18T13:53:00+0200')",
        {
            "type": "date",
            "value": "Thu Feb 18 2021 11:53:00 GMT+0000 (Coordinated Universal Time)",
            "objectId": "__any_value__"},
        websocket)


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_serialization_windowProxy(websocket):
    await assertSerialization(
        "this.window",
        {
            "type": "window",
            "objectId": "__any_value__"
        },
        websocket)


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_serialization_error(websocket):
    await assertSerialization(
        "new Error('Woops!')",
        {
            "type": "error",
            "objectId": "__any_value__"
        },
        websocket)


# TODO: add `NodeProperties` after serialization MaxDepth logic specified:
# https://github.com/w3c/webdriver-bidi/issues/86.
@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_serialization_node(websocket):
    context_id = await get_open_context_id(websocket)
    await goto_url(websocket, context_id,
                   "data:text/html,<div some_attr_name='some_attr_value' ><h2>test</h2></div>")

    # Send command.
    await send_JSON_command(websocket, {
        "id": 47,
        "method": "PROTO.browsingContext.waitForSelector",
        "params": {
            "selector": "body > div",
            "context": context_id}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == 47
    object_id = resp["result"]["objectId"]

    await send_JSON_command(websocket, {
        "id": 48,
        "method": "script.evaluate",
        "params": {
            "expression": "element => element",
            # TODO: send properly serialized element according to
            # https://w3c.github.io/webdriver-bidi/#data-types-remote-value.
            "args": [{
                "objectId": object_id}],
            "target": {"context": context_id}}})

    # Assert result.
    resp = await read_JSON_message(websocket)
    recursiveCompare({
        "id": 48,
        "result": {
            "type": "node",
            "objectId": "__any_value__",
            "value": {
                "nodeType": 1,
                "nodeValue": None,
                "localName": "div",
                "namespaceURI": "http://www.w3.org/1999/xhtml",
                "childNodeCount": 1,
                "children": [{
                    "type": "node",
                    "objectId": "__any_value__"}],
                "attributes": [{
                    "name": "some_attr_name",
                    "value": "some_attr_value"}]}}},
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
