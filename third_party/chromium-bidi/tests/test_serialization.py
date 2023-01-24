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

import copy

import pytest
from test_helpers import *


def _strip_handle(obj):
    result = copy.deepcopy(obj)
    result.pop("handle", None)
    return result


# Testing serialization.
async def assert_serialization(websocket, context_id, js_str_object,
                               expected_serialized_object):
    await subscribe(websocket, "log.entryAdded")

    command_id = await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression":
                f"(()=>{{"
                f"console.log({js_str_object});"
                f"return {js_str_object}"
                f"}})()",
                "target": {
                    "context": context_id
                },
                "awaitPromise":
                False,
                "resultOwnership":
                "root"
            }
        })

    # Assert log event serialized properly.
    # As log is serialized with "resultOwnership": "none", "handle" should be
    # removed from the expectation.
    expected_serialized_object_without_handle = _strip_handle(
        expected_serialized_object)
    resp = await read_JSON_message(websocket)
    assert resp["method"] == "log.entryAdded"
    recursive_compare(expected_serialized_object_without_handle,
                      resp["params"]["args"][0])

    # Assert result serialized properly.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == command_id
    recursive_compare(expected_serialized_object, resp["result"]["result"])

    resp = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": f"throw {js_str_object}",
                "target": {
                    "context": context_id
                },
                "awaitPromise": False,
                "resultOwnership": "root"
            }
        })

    # Assert exception serialized properly.
    recursive_compare(expected_serialized_object,
                      resp["exceptionDetails"]["exception"])


async def assert_callFunction_deserialization_serialization(
        websocket,
        context_id,
        serialized_object,
        expected_serialized_object=None):
    if expected_serialized_object is None:
        expected_serialized_object = serialized_object

    await subscribe(websocket, "log.entryAdded")

    command_id = await send_JSON_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration":
                "(arg)=>{console.log(arg); return arg;}",
                "this": {
                    "type": "undefined"
                },
                "arguments": [serialized_object],
                "awaitPromise": False,
                "target": {
                    "context": context_id
                },
                "resultOwnership": "root"
            }
        })

    # Assert log event serialized properly.
    # As log is serialized with "resultOwnership": "none", "handle" should be
    # removed from the expectation.
    expected_serialized_object_without_handle = _strip_handle(
        expected_serialized_object)
    resp = await read_JSON_message(websocket)
    assert resp["method"] == "log.entryAdded"
    recursive_compare(expected_serialized_object_without_handle,
                      resp["params"]["args"][0])

    resp = await read_JSON_message(websocket)
    assert resp["id"] == command_id
    recursive_compare(expected_serialized_object, resp["result"]["result"])

    resp = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "(arg)=>{throw arg;}",
                "this": {
                    "type": "undefined"
                },
                "arguments": [serialized_object],
                "awaitPromise": False,
                "target": {
                    "context": context_id
                },
                "resultOwnership": "root"
            }
        })
    recursive_compare(expected_serialized_object,
                      resp["exceptionDetails"]["exception"])


@pytest.mark.asyncio
@pytest.mark.parametrize("serialized", [{
    "type": "undefined"
}, {
    "type": "string",
    "value": "someStr"
}, {
    "type": "string",
    "value": ""
}, {
    "type": "number",
    "value": 123
}, {
    "type": "number",
    "value": 0.56
}, {
    "type": "number",
    "value": "Infinity"
}, {
    "type": "number",
    "value": "-Infinity"
}, {
    "type": "number",
    "value": "-0"
}, {
    "type": "number",
    "value": "NaN"
}, {
    "type": "boolean",
    "value": True
}, {
    "type": "boolean",
    "value": False
}, {
    "type": "bigint",
    "value": "12345678901234567890"
}])
async def test_serialization_deserialization(websocket, context_id,
                                             serialized):
    await assert_callFunction_deserialization_serialization(
        websocket, context_id, serialized)


@pytest.mark.asyncio
@pytest.mark.parametrize("jsString, excepted_serialized",
                         [("function(){}", {
                             "type": "function",
                             "handle": any_string
                         }),
                          ("Promise.resolve(1)", {
                              "type": "promise",
                              "handle": any_string
                          }),
                          ("new WeakMap()", {
                              "type": "weakmap",
                              "handle": any_string
                          }),
                          ("new WeakSet()", {
                              "type": "weakset",
                              "handle": any_string
                          }),
                          ("new Proxy({}, {})", {
                              "type": "proxy",
                              "handle": any_string
                          }),
                          ("new Int32Array()", {
                              "type": "typedarray",
                              "handle": any_string
                          }),
                          ("{'foo': {'bar': 'baz'}, 'qux': 'quux'}", {
                              "type":
                              "object",
                              "handle":
                              any_string,
                              "value": [["foo", {
                                  "type": "object"
                              }], ["qux", {
                                  "type": "string",
                                  "value": "quux"
                              }]]
                          }),
                          ("[1, 'a', {foo: 'bar'}, [2,[3,4]]]", {
                              "type":
                              "array",
                              "handle":
                              any_string,
                              "value": [{
                                  "type": "number",
                                  "value": 1
                              }, {
                                  "type": "string",
                                  "value": "a"
                              }, {
                                  "type": "object"
                              }, {
                                  "type": "array"
                              }]
                          }),
                          ("new Set([1, 'a', {foo: 'bar'}, [2,[3,4]]])", {
                              "type":
                              "set",
                              "handle":
                              any_string,
                              "value": [{
                                  "type": "number",
                                  "value": 1
                              }, {
                                  "type": "string",
                                  "value": "a"
                              }, {
                                  "type": "object"
                              }, {
                                  "type": "array"
                              }]
                          }),
                          ("Symbol('foo')", {
                              "type": "symbol",
                              "handle": any_string
                          }),
                          ("this.window", {
                              "type": "window",
                              "handle": any_string
                          }),
                          ("new Error('Woops!')", {
                              "type": "error",
                              "handle": any_string
                          })])
async def test_serialization_function(websocket, context_id, jsString,
                                      excepted_serialized):
    await assert_serialization(websocket, context_id, jsString,
                               excepted_serialized)


@pytest.mark.asyncio
@pytest.mark.parametrize("serialized, excepted_re_serialized", [
    ({
        "type":
        "object",
        "value": [["foo", {
            "type": "object",
            "value": []
        }],
                  [{
                      "type": "string",
                      "value": "qux"
                  }, {
                      "type": "string",
                      "value": "quux"
                  }]]
    }, {
        "type":
        "object",
        "handle":
        any_string,
        "value": [["foo", {
            "type": "object"
        }], ["qux", {
            "type": "string",
            "value": "quux"
        }]]
    }),
    ({
        "type":
        "map",
        "value": [["foo", {
            "type": "object",
            "value": []
        }],
                  [{
                      "type": "string",
                      "value": "qux"
                  }, {
                      "type": "string",
                      "value": "quux"
                  }]]
    }, {
        "type":
        "map",
        "handle":
        any_string,
        "value": [["foo", {
            "type": "object"
        }], ["qux", {
            "type": "string",
            "value": "quux"
        }]]
    }),
    ({
        "type":
        "array",
        "value": [{
            "type": "number",
            "value": 1
        }, {
            "type": "string",
            "value": "a"
        }]
    }, {
        "type":
        "array",
        "handle":
        any_string,
        "value": [{
            "type": "number",
            "value": 1
        }, {
            "type": "string",
            "value": "a"
        }]
    }),
    ({
        "type":
        "set",
        "value": [{
            "type": "number",
            "value": 1.23
        }, {
            "type": "string",
            "value": "a"
        }]
    }, {
        "type":
        "set",
        "handle":
        any_string,
        "value": [{
            "type": "number",
            "value": 1.23
        }, {
            "type": "string",
            "value": "a"
        }]
    }),
    ({
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
])
async def test_serialization_deserialization_complex(websocket, context_id,
                                                     serialized,
                                                     excepted_re_serialized):
    await assert_callFunction_deserialization_serialization(
        websocket, context_id, serialized, excepted_re_serialized)


@pytest.mark.asyncio
async def test_serialization_deserialization_date(websocket, context_id):
    serialized_date = {
        "type": "date",
        "value": "2020-07-19T07:34:56.789+01:00"
    }

    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "(arg)=>{return arg}",
                "this": {
                    "type": "undefined"
                },
                "arguments": [serialized_date],
                "awaitPromise": False,
                "target": {
                    "context": context_id
                }
            }
        })

    assert result["result"] == {
        "type": "date",
        "value": "2020-07-19T06:34:56.789Z"
    }


@pytest.mark.asyncio
async def test_serialization_node(websocket, context_id):
    await goto_url(
        websocket, context_id,
        "data:text/html,<div some_attr_name='some_attr_value' "
        ">some text<h2>some another text</h2></div>")

    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "document.querySelector('body > div');",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True
            }
        })

    recursive_compare(
        {
            "type": "node",
            "value": {
                "nodeType":
                1,
                "sharedId":
                any_shared_id,
                "localName":
                "div",
                "namespaceURI":
                "http://www.w3.org/1999/xhtml",
                "childNodeCount":
                2,
                "attributes": {
                    "some_attr_name": "some_attr_value"
                },
                "children": [{
                    "type": "node",
                    "value": {
                        "nodeType": 3,
                        "nodeValue": "some text",
                        "sharedId": any_shared_id,
                    }
                }, {
                    "type": "node",
                    "value": {
                        "nodeType": 1,
                        "sharedId": any_shared_id,
                        "localName": "h2",
                        "namespaceURI": "http://www.w3.org/1999/xhtml",
                        "childNodeCount": 1,
                        "attributes": {}
                    }
                }]
            }
        }, result["result"])


# Verify node nested in other data structures are serialized with the proper
# `sharedId`.
@pytest.mark.parametrize("eval_delegate, extract_delegate", [
    (lambda s: f"[{s}]", lambda r: r["value"][0]),
    (lambda s: f"new Set([{s}])", lambda r: r["value"][0]),
    (lambda s: f"({{qwe: {s}}})", lambda r: r["value"][0][1]),
    (lambda s: f"new Map([['qwe', {s}]])", lambda r: r["value"][0][1]),
])
@pytest.mark.asyncio
async def test_serialization_nested_node(websocket, context_id, eval_delegate,
                                         extract_delegate):
    await goto_url(
        websocket, context_id,
        "data:text/html,<div some_attr_name='some_attr_value' "
        ">some text<h2>some another text</h2></div>")

    eval_node = "document.querySelector('body > div')"

    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": eval_delegate(eval_node),
                "target": {
                    "context": context_id
                },
                "awaitPromise": True
            }
        })

    recursive_compare(
        {
            "type": "node",
            "value": {
                "nodeType": 1,
                "localName": "div",
                "namespaceURI": "http://www.w3.org/1999/xhtml",
                "childNodeCount": 2,
                "attributes": {
                    "some_attr_name": "some_attr_value"
                },
                "sharedId": any_shared_id
            }
        }, extract_delegate(result["result"]))


@pytest.mark.asyncio
async def test_deserialization_nestedObjectInObject(websocket, context_id):
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "({a:1})",
                "target": {
                    "context": context_id
                },
                "awaitPromise": False,
                "resultOwnership": "root"
            }
        })

    nested_handle = result["result"]["handle"]

    arg = {
        "type": "object",
        "value": [["nested_object", {
            "handle": nested_handle
        }]]
    }

    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "(arg)=>{return arg}",
                "this": {
                    "type": "undefined"
                },
                "arguments": [arg],
                "awaitPromise": False,
                "target": {
                    "context": context_id
                }
            }
        })

    recursive_compare(
        {
            "type": "success",
            "result": {
                "type": "object",
                "value": [["nested_object", {
                    "type": "object"
                }]]
            },
            "realm": any_string
        }, result)


@pytest.mark.asyncio
async def test_deserialization_nestedObjectInArray(websocket, context_id):
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "({a:1})",
                "target": {
                    "context": context_id
                },
                "awaitPromise": False,
                "resultOwnership": "root"
            }
        })

    nested_handle = result["result"]["handle"]

    arg = {"type": "array", "value": [{"handle": nested_handle}]}

    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "(arg)=>{return arg}",
                "this": {
                    "type": "undefined"
                },
                "arguments": [arg],
                "awaitPromise": False,
                "target": {
                    "context": context_id
                }
            }
        })

    recursive_compare(
        {
            "type": "success",
            "result": {
                "type": "array",
                "value": [{
                    "type": "object"
                }]
            },
            "realm": any_string
        }, result)


@pytest.mark.asyncio
async def test_deserialization_handleAndValue(websocket, context_id):
    # When `handle` is present, `type` and `values` are ignored.
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "({a:1})",
                "target": {
                    "context": context_id
                },
                "awaitPromise": False,
                "resultOwnership": "root"
            }
        })

    nested_handle = result["result"]["handle"]

    arg = {
        "type":
        "object",
        "value": [[
            "nested_object", {
                "handle": nested_handle,
                "type": "string",
                "value": "SOME_STRING"
            }
        ]]
    }

    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "(arg)=>{return arg.nested_object}",
                "this": {
                    "type": "undefined"
                },
                "arguments": [arg],
                "awaitPromise": False,
                "target": {
                    "context": context_id
                }
            }
        })

    # Assert the `type` and `value` were ignored.
    recursive_compare(
        {
            "type": "success",
            "result": {
                "type": "object",
                "value": [["a", {
                    "type": "number",
                    "value": 1
                }]]
            },
            "realm": any_string
        }, result)
