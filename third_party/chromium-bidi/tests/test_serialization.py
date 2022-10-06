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
async def assertSerialization(websocket, context_id, js_str_object,
      expected_serialized_object):
    result = await execute_command(websocket, {
        "method": "script.evaluate",
        "params": {
            "expression": f"({js_str_object})",
            "target": {"context": context_id},
            "awaitPromise": False,
            "resultOwnership": "root"
        }})

    # Compare ignoring `handle`.
    recursive_compare(expected_serialized_object, result["result"])


# Testing serialization.
async def assertDeserializationAndSerialization(websocket, context_id,
      serialized_object,
      expected_serialized_object=None):
    if expected_serialized_object is None:
        expected_serialized_object = serialized_object

    result = await execute_command(websocket, {
        "method": "script.callFunction",
        "params": {
            "functionDeclaration": "(arg)=>{return arg}",
            "this": {
                "type": "undefined"},
            "arguments": [serialized_object],
            "awaitPromise": False,
            "target": {"context": context_id},
            "resultOwnership": "root"}})
    # Compare ignoring `handle`.
    recursive_compare(expected_serialized_object, result["result"])


@pytest.mark.asyncio
async def test_deserialization_serialization_undefined(websocket, context_id):
    await assertDeserializationAndSerialization(websocket, context_id,
                                                {"type": "undefined"})


@pytest.mark.asyncio
async def test_deserialization_serialization_null(websocket, context_id):
    await assertDeserializationAndSerialization(websocket, context_id,
                                                {"type": "null"})


@pytest.mark.asyncio
async def test_deserialization_serialization_string(websocket, context_id):
    await assertDeserializationAndSerialization(websocket, context_id, {
        "type": "string",
        "value": "someStr"})
    await assertDeserializationAndSerialization(websocket, context_id, {
        "type": "string",
        "value": ""})


@pytest.mark.asyncio
async def test_deserialization_serialization_number(websocket, context_id):
    await assertDeserializationAndSerialization(websocket, context_id, {
        "type": "number",
        "value": 123})
    await assertDeserializationAndSerialization(websocket, context_id, {
        "type": "number",
        "value": 0.56})


@pytest.mark.asyncio
async def test_deserialization_serialization_specialNumber(websocket,
      context_id):
    await assertDeserializationAndSerialization(websocket, context_id, {
        "type": "number",
        "value": "Infinity"})
    await assertDeserializationAndSerialization(websocket, context_id,
                                                {
                                                    "type": "number",
                                                    "value": "+Infinity"
                                                }, {
                                                    "type": "number",
                                                    "value": "Infinity"})
    await assertDeserializationAndSerialization(websocket, context_id, {
        "type": "number",
        "value": "-Infinity"})
    await assertDeserializationAndSerialization(websocket, context_id, {
        "type": "number",
        "value": "-0"})
    await assertDeserializationAndSerialization(websocket, context_id, {
        "type": "number",
        "value": "NaN"})


@pytest.mark.asyncio
async def test_deserialization_serialization_bool(websocket, context_id):
    await assertDeserializationAndSerialization(websocket, context_id, {
        "type": "boolean",
        "value": True})
    await assertDeserializationAndSerialization(websocket, context_id, {
        "type": "boolean",
        "value": False})


@pytest.mark.asyncio
async def test_serialization_function(websocket, context_id):
    await assertSerialization(websocket, context_id,
                              "function(){}", {
                                  "type": "function",
                                  "handle": any_string
                              })


@pytest.mark.asyncio
async def test_serialization_promise(websocket, context_id):
    await assertSerialization(websocket, context_id,
                              "Promise.resolve(1)", {
                                  "type": "promise",
                                  "handle": any_string
                              })


@pytest.mark.asyncio
async def test_serialization_weakMap(websocket, context_id):
    await assertSerialization(websocket, context_id,
                              "new WeakMap()", {
                                  "type": "weakmap",
                                  "handle": any_string
                              })


@pytest.mark.asyncio
async def test_serialization_weakSet(websocket, context_id):
    await assertSerialization(websocket, context_id,
                              "new WeakSet()", {
                                  "type": "weakset",
                                  "handle": any_string
                              })


@pytest.mark.asyncio
# Not specified yet.
async def test_serialization_proxy(websocket, context_id):
    await assertSerialization(websocket, context_id,
                              "new Proxy({}, {})", {
                                  "type": "proxy",
                                  "handle": any_string
                              })


@pytest.mark.asyncio
async def test_serialization_typedarray(websocket, context_id):
    await assertSerialization(websocket, context_id,
                              "new Int32Array()", {
                                  "type": "typedarray",
                                  "handle": any_string
                              })


@pytest.mark.asyncio
async def test_serialization_object(websocket, context_id):
    await assertSerialization(websocket, context_id,
                              "{'foo': {'bar': 'baz'}, 'qux': 'quux'}", {
                                  "type": "object",
                                  "handle": any_string,
                                  "value": [[
                                      "foo", {
                                          "type": "object"}], [
                                      "qux", {
                                          "type": "string",
                                          "value": "quux"}]]})


@pytest.mark.asyncio
async def test_deserialization_serialization_object(websocket, context_id):
    await assertDeserializationAndSerialization(websocket, context_id,
                                                {
                                                    "type": "object",
                                                    "value": [[
                                                        "foo", {
                                                            "type": "object",
                                                            "value": []}
                                                    ], [{
                                                        "type": "string",
                                                        "value": "qux"
                                                    }, {
                                                        "type": "string",
                                                        "value": "quux"}]]},
                                                {
                                                    "type": "object",
                                                    "handle": any_string,
                                                    "value": [[
                                                        "foo", {
                                                            "type": "object"}
                                                    ], [
                                                        "qux", {
                                                            "type": "string",
                                                            "value": "quux"}]]})


@pytest.mark.asyncio
async def test_deserialization_serialization_map(websocket, context_id):
    await assertDeserializationAndSerialization(websocket, context_id,
                                                {
                                                    "type": "map",
                                                    "value": [[
                                                        "foo", {
                                                            "type": "object",
                                                            "value": []}
                                                    ], [{
                                                        "type": "string",
                                                        "value": "qux"
                                                    }, {
                                                        "type": "string",
                                                        "value": "quux"}]]},
                                                {
                                                    "type": "map",
                                                    "handle": any_string,
                                                    "value": [[
                                                        "foo", {
                                                            "type": "object"}
                                                    ], [
                                                        "qux", {
                                                            "type": "string",
                                                            "value": "quux"}]]})


@pytest.mark.asyncio
async def test_deserialization_serialization_array(websocket, context_id):
    await assertDeserializationAndSerialization(websocket, context_id,
                                                {
                                                    "type": "array",
                                                    "value": [{
                                                        "type": "number",
                                                        "value": 1
                                                    }, {
                                                        "type": "string",
                                                        "value": "a"
                                                    }]}, {
                                                    "type": "array",
                                                    "handle": any_string,
                                                    "value": [{
                                                        "type": "number",
                                                        "value": 1
                                                    }, {
                                                        "type": "string",
                                                        "value": "a"
                                                    }]}, )


@pytest.mark.asyncio
async def test_serialization_array(websocket, context_id):
    await assertSerialization(websocket, context_id,
                              "[1, 'a', {foo: 'bar'}, [2,[3,4]]]", {
                                  "type": "array",
                                  "handle": any_string,
                                  "value": [{
                                      "type": "number",
                                      "value": 1
                                  }, {
                                      "type": "string",
                                      "value": "a"
                                  }, {
                                      "type": "object"
                                  }, {
                                      "type": "array"}]})


@pytest.mark.asyncio
async def test_deserialization_serialization_set(websocket, context_id):
    await assertDeserializationAndSerialization(websocket, context_id,
                                                {
                                                    "type": "set",
                                                    "value": [{
                                                        "type": "number",
                                                        "value": 1
                                                    }, {
                                                        "type": "string",
                                                        "value": "a"
                                                    }]}, {
                                                    "type": "set",
                                                    "handle": any_string,
                                                    "value": [{
                                                        "type": "number",
                                                        "value": 1
                                                    }, {
                                                        "type": "string",
                                                        "value": "a"
                                                    }]}, )


@pytest.mark.asyncio
async def test_serialization_set(websocket, context_id):
    await assertSerialization(websocket, context_id,
                              "new Set([1, 'a', {foo: 'bar'}, [2,[3,4]]])", {
                                  "type": "set",
                                  "handle": any_string,
                                  "value": [{
                                      "type": "number",
                                      "value": 1
                                  }, {
                                      "type": "string",
                                      "value": "a"
                                  }, {
                                      "type": "object"
                                  }, {
                                      "type": "array"}]})


@pytest.mark.asyncio
async def test_deserialization_serialization_bigint(websocket, context_id):
    await assertDeserializationAndSerialization(websocket, context_id, {
        "type": "bigint",
        "value": "12345678901234567890"})


@pytest.mark.asyncio
async def test_serialization_symbol(websocket, context_id):
    await assertSerialization(websocket, context_id,
                              "Symbol('foo')", {
                                  "type": "symbol",
                                  "handle": any_string
                              })


@pytest.mark.asyncio
async def test_deserialization_serialization_regExp(websocket, context_id):
    await assertDeserializationAndSerialization(websocket, context_id,
                                                {
                                                    "type": "regexp",
                                                    "value": {
                                                        "pattern": "ab+c",
                                                        "flags": "i"
                                                    }
                                                }, {
                                                    "type": "regexp",
                                                    "handle": any_string,
                                                    "value": {
                                                        "pattern": "ab+c",
                                                        "flags": "i"
                                                    }
                                                })


@pytest.mark.asyncio
async def test_deserialization_serialization_date(websocket, context_id):
    serialized_date = {
        "type": "date",
        "value": "2020-07-19T07:34:56.789+01:00"}

    result = await execute_command(websocket, {
        "method": "script.callFunction",
        "params": {
            "functionDeclaration": "(arg)=>{return arg}",
            "this": {
                "type": "undefined"},
            "arguments": [serialized_date],
            "awaitPromise": False,
            "target": {"context": context_id}}})

    assert result["result"] == {
        "type": "date",
        "value": "2020-07-19T06:34:56.789Z"}


@pytest.mark.asyncio
async def test_serialization_windowProxy(websocket, context_id):
    await assertSerialization(websocket, context_id,
                              "this.window", {
                                  "type": "window",
                                  "handle": any_string
                              })


@pytest.mark.asyncio
async def test_serialization_error(websocket, context_id):
    await assertSerialization(websocket, context_id,
                              "new Error('Woops!')", {
                                  "type": "error",
                                  "handle": any_string
                              })


@pytest.mark.asyncio
async def test_serialization_node(websocket, context_id):
    await goto_url(websocket, context_id,
                   "data:text/html,<div some_attr_name='some_attr_value' "
                   ">some text<h2>some another text</h2></div>")

    result = await execute_command(websocket, {
        "method": "PROTO.browsingContext.findElement",
        "params": {
            "selector": "body > div",
            "context": context_id}})

    recursive_compare({
        "type": "node",
        "handle": any_string,
        "value": {
            "nodeType": 1,
            "nodeValue": "",
            "nodeName": "",
            "localName": "div",
            "namespaceURI": "http://www.w3.org/1999/xhtml",
            "childNodeCount": 2,
            "attributes": {
                "some_attr_name": "some_attr_value"},
            "children": [{
                "type": "node",
                "value": {
                    "nodeType": 3,
                    "nodeValue": "some text",
                    "nodeName": "some text"}
            }, {
                "type": "node",
                "value": {
                    "nodeType": 1,
                    "nodeValue": "",
                    "nodeName": "",
                    "localName": "h2",
                    "namespaceURI": "http://www.w3.org/1999/xhtml",
                    "childNodeCount": 1,
                    "attributes": {}}}]}},
        result["result"])


@pytest.mark.asyncio
async def test_deserialization_nestedObjectInObject(websocket, context_id):
    result = await execute_command(websocket, {
        "method": "script.evaluate",
        "params": {
            "expression": "({a:1})",
            "target": {"context": context_id},
            "awaitPromise": False,
            "resultOwnership": "root"
        }})

    nested_handle = result["result"]["handle"]

    arg = {"type": "object",
           "value": [[
               "nested_object", {
                   "handle": nested_handle}]]}

    result = await execute_command(websocket, {
        "method": "script.callFunction",
        "params": {
            "functionDeclaration": "(arg)=>{return arg}",
            "this": {
                "type": "undefined"},
            "arguments": [arg],
            "awaitPromise": False,
            "target": {"context": context_id}}})

    recursive_compare({
        "result": {
            "type": "object",
            "value": [[
                "nested_object", {
                    "type": "object"}]]},
        "realm": any_string},
        result)


@pytest.mark.asyncio
async def test_deserialization_nestedObjectInArray(websocket, context_id):
    result = await execute_command(websocket, {
        "method": "script.evaluate",
        "params": {
            "expression": "({a:1})",
            "target": {"context": context_id},
            "awaitPromise": False,
            "resultOwnership": "root"
        }})

    nested_handle = result["result"]["handle"]

    arg = {"type": "array",
           "value": [{
               "handle": nested_handle}]}

    result = await execute_command(websocket, {
        "method": "script.callFunction",
        "params": {
            "functionDeclaration": "(arg)=>{return arg}",
            "this": {
                "type": "undefined"},
            "arguments": [arg],
            "awaitPromise": False,
            "target": {"context": context_id}}})

    recursive_compare({
        "result": {
            "type": "array",
            "value": [{
                "type": "object"}]},
        "realm": any_string},
        result)


@pytest.mark.asyncio
async def test_deserialization_handleAndValue(websocket, context_id):
    # When `handle` is present, `type` and `values` are ignored.
    result = await execute_command(websocket, {
        "method": "script.evaluate",
        "params": {
            "expression": "({a:1})",
            "target": {"context": context_id},
            "awaitPromise": False,
            "resultOwnership": "root"
        }})

    nested_handle = result["result"]["handle"]

    arg = {"type": "object",
           "value": [[
               "nested_object", {
                   "handle": nested_handle,
                   "type": "string",
                   "value": "SOME_STRING"}]]}

    result = await execute_command(websocket, {
        "method": "script.callFunction",
        "params": {
            "functionDeclaration": "(arg)=>{return arg.nested_object}",
            "this": {
                "type": "undefined"},
            "arguments": [arg],
            "awaitPromise": False,
            "target": {"context": context_id}}})

    # Assert the `type` and `value` were ignored.
    recursive_compare({
        "result": {
            "type": "object",
            "value": [[
                "a", {
                    "type": "number",
                    "value": 1}]]},
        "realm": any_string},
        result)
