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
from anys import ANY_STR
from test_helpers import (ANY_SHARED_ID, ANY_UUID, execute_command, goto_url,
                          read_JSON_message, send_JSON_command,
                          stabilize_key_values, subscribe)


def _strip_handle(obj):
    result = copy.deepcopy(obj)
    result.pop("handle", None)
    return result


# Testing serialization.
async def assert_serialization(websocket, context_id, js_str_object,
                               expected_serialized_object):
    await subscribe(websocket, ["log.entryAdded"])

    command_id = await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": f"(()=>{{"
                              f"const value = {js_str_object};"
                              f"console.log(value);"
                              f"return value;"
                              f"}})()",
                "target": {
                    "context": context_id
                },
                "awaitPromise": False,
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
    assert expected_serialized_object_without_handle == resp["params"]["args"][
        0]

    # Assert result serialized properly.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == command_id
    assert expected_serialized_object == resp["result"]["result"]

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
    assert expected_serialized_object == resp["exceptionDetails"]["exception"]


async def assert_callFunction_deserialization_serialization(
        websocket,
        context_id,
        serialized_object,
        expected_serialized_object=None):
    if expected_serialized_object is None:
        expected_serialized_object = serialized_object

    await subscribe(websocket, ["log.entryAdded"])

    command_id = await send_JSON_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "(arg)=>{console.log(arg); return arg;}",
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
    assert expected_serialized_object_without_handle == resp["params"]["args"][
        0]

    resp = await read_JSON_message(websocket)
    assert resp["id"] == command_id
    assert expected_serialized_object == resp["result"]["result"]

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
    assert expected_serialized_object == resp["exceptionDetails"]["exception"]


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
@pytest.mark.parametrize(
    "js_string, expected_serialized",
    [("function(){}", {
        "type": "function",
        "handle": ANY_STR
    }), ("async function(){}", {
        "type": "function",
        "handle": ANY_STR
    }),
     ("(function*() { yield 'a'; })", {
         "type": "function",
         "handle": ANY_STR,
     }), ("Promise.resolve(1)", {
         "type": "promise",
         "handle": ANY_STR
     }), ("new WeakMap()", {
         "type": "weakmap",
         "handle": ANY_STR
     }), ("new WeakSet()", {
         "type": "weakset",
         "handle": ANY_STR
     }),
     ("(new Set([1, 2]))[Symbol.iterator]()", {
         "type": "object",
         "handle": ANY_STR,
         'value': [],
     }),
     ("(new Map([[1, 2]]))[Symbol.iterator]()", {
         "type": "object",
         "handle": ANY_STR,
         'value': [],
     }),
     ("('abc'[Symbol.iterator]())", {
         "type": "object",
         "value": [],
         "handle": ANY_STR
     }),
     ("([1, 2][Symbol.iterator]())", {
         "type": "object",
         "value": [],
         "handle": ANY_STR
     }), ("new Proxy({}, {})", {
         "type": "proxy",
         "handle": ANY_STR
     }),
     ("(function*() { yield 'a'; })()", {
         "type": "generator",
         "handle": ANY_STR,
     }), ("new Int32Array()", {
         "type": "typedarray",
         "handle": ANY_STR
     }),
     ("{'foo': {'bar': 'baz'}, 'qux': 'quux'}", {
         "type": "object",
         "handle": ANY_STR,
         "value": [[
             "foo", {
                 "type": "object",
                 "value": [["bar", {
                     "type": "string",
                     "value": "baz"
                 }]]
             }
         ], ["qux", {
             "type": "string",
             "value": "quux"
         }]]
     }),
     ("[1, 'a', {foo: 'bar'}, [2,[3,4]]]", {
         "type": "array",
         "handle": ANY_STR,
         "value": [{
             "type": "number",
             "value": 1
         }, {
             "type": "string",
             "value": "a"
         }, {
             "type": "object",
             "value": [["foo", {
                 "type": "string",
                 "value": "bar"
             }]]
         }, {
             "type": "array",
             "value": [{
                 "type": "number",
                 "value": 2,
             }, {
                 "type": "array",
                 "value": [{
                     "type": 'number',
                     "value": 3,
                 }, {
                     "type": "number",
                     "value": 4,
                 }]
             }]
         }]
     }),
     ("new Set([1, 'a', {foo: 'bar'}, [2,[3,4]]])", {
         "type": "set",
         "handle": ANY_STR,
         "value": [{
             "type": "number",
             "value": 1
         }, {
             "type": "string",
             "value": "a"
         }, {
             "type": "object",
             "value": [["foo", {
                 "type": "string",
                 "value": "bar"
             }]]
         }, {
             "type": "array",
             "value": [{
                 "type": "number",
                 "value": 2,
             }, {
                 "type": "array",
                 "value": [{
                     "type": 'number',
                     "value": 3,
                 }, {
                     "type": "number",
                     "value": 4,
                 }]
             }]
         }]
     }), ("Symbol('foo')", {
         "type": "symbol",
         "handle": ANY_STR
     }),
     ("this.window", {
         "type": "window",
         "value": {
             "context": ANY_STR
         },
         "handle": ANY_STR
     }), ("new Error('Woops!')", {
         "type": "error",
         "handle": ANY_STR
     }),
     ("new URL('https://example.com')", {
         "type": "object",
         "handle": ANY_STR
     }),
     ("(()=>{"
      "     const foo={a: []};"
      "     const bar=[1,2];"
      "     const result={1: foo, 2: foo, 3: bar, 4: bar};"
      "     result.self=result;"
      "     return result;"
      " })()", {
          "type": "object",
          "handle": ANY_STR,
          "internalId": ANY_UUID,
          "value": [[
              '1', {
                  "type": "object",
                  "value": [["a", {
                      "type": "array",
                      "value": []
                  }]],
                  "internalId": ANY_UUID
              }
          ], ['2', {
              "type": "object",
              "internalId": ANY_UUID
          }],
                    [
                        '3', {
                            "type": "array",
                            "value": [{
                                "type": "number",
                                "value": 1
                            }, {
                                "type": "number",
                                "value": 2
                            }],
                            "internalId": ANY_UUID
                        }
                    ], ['4', {
                        "type": "array",
                        "internalId": ANY_UUID
                    }], ["self", {
                        "type": "object",
                        "internalId": ANY_UUID
                    }]],
      })])
async def test_serialization_function(websocket, context_id, js_string,
                                      expected_serialized):
    await assert_serialization(websocket, context_id, js_string,
                               expected_serialized)


@pytest.mark.asyncio
async def test_serialization_internal_id(websocket, context_id,
                                         test_chromedriver_mode):
    """
    Test that internalId is mapped properly. Required, as generic
    `test_serialization_function` does not check it.
    """
    if test_chromedriver_mode:
        pytest.xfail(reason="TODO: #3294")

    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "(()=>{"
                              " const foo={a: [], document};"
                              " const bar=[1,2, document];"
                              " const result={1: foo, 2: foo, 3: bar, 4: bar};"
                              " result.self=result;"
                              " return result;"
                              "})()",
                "target": {
                    "context": context_id
                },
                "awaitPromise": False,
                "resultOwnership": "root"
            }
        })

    stabilize_key_values(result, ["internalId", "sharedId"])

    assert result["result"] == {
        "type": "object",
        "handle": ANY_STR,
        "internalId": "stable_4",
        "value": [[
            '1', {
                "type": "object",
                "value": [["a", {
                    "type": "array",
                    "value": []
                }],
                          [
                              'document', {
                                  'internalId': 'stable_1',
                                  'sharedId': 'stable_0',
                                  'type': 'node',
                                  'value': {
                                      'childNodeCount': 1,
                                      'nodeType': 9,
                                  }
                              }
                          ]],
                "internalId": "stable_2",
            }
        ], ['2', {
            "type": "object",
            "internalId": "stable_2",
        }],
                  [
                      '3', {
                          "type": "array",
                          "value": [{
                              "type": "number",
                              "value": 1
                          }, {
                              "type": "number",
                              "value": 2
                          }, {
                              'internalId': 'stable_1',
                              'type': 'node',
                          }],
                          "internalId": "stable_3",
                      }
                  ], ['4', {
                      "type": "array",
                      "internalId": "stable_3",
                  }], ["self", {
                      "type": "object",
                      "internalId": "stable_4",
                  }]],
    }


@pytest.mark.asyncio
@pytest.mark.parametrize("serialized, excepted_re_serialized", [
    ({
        "type": "object",
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
        "type": "object",
        "handle": ANY_STR,
        "value": [["foo", {
            "type": "object",
            "value": []
        }], ["qux", {
            "type": "string",
            "value": "quux"
        }]]
    }),
    ({
        "type": "map",
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
        "type": "map",
        "handle": ANY_STR,
        "value": [["foo", {
            "type": "object",
            "value": []
        }], ["qux", {
            "type": "string",
            "value": "quux"
        }]]
    }),
    ({
        "type": "array",
        "value": [{
            "type": "number",
            "value": 1
        }, {
            "type": "string",
            "value": "a"
        }]
    }, {
        "type": "array",
        "handle": ANY_STR,
        "value": [{
            "type": "number",
            "value": 1
        }, {
            "type": "string",
            "value": "a"
        }]
    }),
    ({
        "type": "set",
        "value": [{
            "type": "number",
            "value": 1.23
        }, {
            "type": "string",
            "value": "a"
        }]
    }, {
        "type": "set",
        "handle": ANY_STR,
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
        "handle": ANY_STR,
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
async def test_serialization_iframe(websocket, context_id, html, iframe):
    iframe_url = html("<h1>FRAME</h1>")
    await goto_url(websocket, context_id, html(iframe(iframe_url)))

    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "script": "document.querySelector('iframe')",
                "expression": "document.getElementsByTagName('iframe')[0]",
                "awaitPromise": False,
                "target": {
                    "context": context_id
                }
            }
        })

    assert result["result"] == {
        'sharedId': ANY_SHARED_ID,
        'type': 'node',
        'value': {
            'attributes': {
                'src': iframe_url
            },
            'childNodeCount': 1,
            'localName': 'iframe',
            'namespaceURI': 'http://www.w3.org/1999/xhtml',
            'nodeType': 1,
            'shadowRoot': None,
        },
    }


@pytest.mark.parametrize("serialization_options, expected_node_value", [
    ({
        "maxDomDepth": 0
    }, {
        "type": "node",
        "sharedId": ANY_SHARED_ID,
        "value": {
            "nodeType": 1,
            "childNodeCount": 2,
            "shadowRoot": None,
            "localName": "div",
            "namespaceURI": "http://www.w3.org/1999/xhtml",
            "attributes": {
                "some_attr_name": "some_attr_value"
            }
        }
    }),
    ({
        "maxDomDepth": 1
    }, {
        "type": "node",
        "sharedId": ANY_SHARED_ID,
        "value": {
            "nodeType": 1,
            "childNodeCount": 2,
            "shadowRoot": None,
            "localName": "div",
            "namespaceURI": "http://www.w3.org/1999/xhtml",
            "attributes": {
                "some_attr_name": "some_attr_value"
            },
            "children": [{
                "type": "node",
                "sharedId": ANY_SHARED_ID,
                "value": {
                    "childNodeCount": 0,
                    "nodeType": 3,
                    "nodeValue": "some text",
                }
            }, {
                "type": "node",
                "sharedId": ANY_SHARED_ID,
                "value": {
                    "attributes": {},
                    "childNodeCount": 1,
                    "localName": 'h2',
                    "namespaceURI": 'http://www.w3.org/1999/xhtml',
                    "nodeType": 1,
                    "shadowRoot": None,
                }
            }]
        }
    }),
    ({
        "maxDomDepth": 99
    }, {
        "type": "node",
        "sharedId": ANY_SHARED_ID,
        "value": {
            "nodeType": 1,
            "childNodeCount": 2,
            "shadowRoot": None,
            "localName": "div",
            "namespaceURI": "http://www.w3.org/1999/xhtml",
            "attributes": {
                "some_attr_name": "some_attr_value"
            },
            "children": [{
                "type": "node",
                "sharedId": ANY_SHARED_ID,
                "value": {
                    "childNodeCount": 0,
                    "nodeType": 3,
                    "nodeValue": "some text",
                    'children': []
                }
            }, {
                "type": "node",
                "sharedId": ANY_SHARED_ID,
                "value": {
                    "attributes": {},
                    "childNodeCount": 1,
                    "localName": 'h2',
                    "namespaceURI": 'http://www.w3.org/1999/xhtml',
                    "nodeType": 1,
                    "shadowRoot": None,
                    'children': [{
                        'type': 'node',
                        "sharedId": ANY_SHARED_ID,
                        'value': {
                            'childNodeCount': 0,
                            'children': [],
                            'nodeType': 3,
                            'nodeValue': 'some another text',
                        }
                    }]
                },
            }]
        }
    }),
])
@pytest.mark.asyncio
async def test_serialization_node(websocket, context_id, html,
                                  serialization_options, expected_node_value):
    await goto_url(
        websocket, context_id,
        html(
            "<div some_attr_name='some_attr_value'>some text<h2>some another text</h2></div>"
        ))

    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "document.querySelector('body > div');",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "serializationOptions": serialization_options
            }
        })

    assert expected_node_value == result["result"]


@pytest.mark.asyncio
async def test_serialization_shadow_dom(websocket, context_id):
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": """(()=>{
                    const createShadow = (mode)=>{
                        // Create container element.
                        const shadowContainer = document.createElement('div');
                        document.body.appendChild(shadowContainer);
                        // Create a closed shadow DOM.
                        const shadowRoot = shadowContainer.attachShadow({ mode });
                        // Create another element.
                        const shadowElement = document.createElement('div');
                        shadowElement.innerHTML = `element in ${mode} shadow DOM`;
                        // Attach shadow element to the shadow DOM.
                        shadowRoot.appendChild(shadowElement);
                        return shadowContainer;
                    }
                    return [createShadow('open'),createShadow('closed')];
                    })()""",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "serializationOptions": {
                    "maxDomDepth": 99,
                    "includeShadowTree": "open"
                }
            }
        })

    assert {
        "type": "array",
        "value": [{
            "type": "node",
            "sharedId": ANY_SHARED_ID,
            "value": {
                "nodeType": 1,
                "childNodeCount": 0,
                "localName": "div",
                "namespaceURI": "http://www.w3.org/1999/xhtml",
                "attributes": {},
                "children": [],
                "shadowRoot": {
                    "type": "node",
                    "sharedId": ANY_SHARED_ID,
                    "value": {
                        "nodeType": 11,
                        "childNodeCount": 1,
                        "mode": "open",
                        "children": [{
                            "type": "node",
                            "sharedId": ANY_SHARED_ID,
                            "value": {
                                "nodeType": 1,
                                "childNodeCount": 1,
                                "shadowRoot": None,
                                "localName": "div",
                                "namespaceURI": "http://www.w3.org/1999/xhtml",
                                "attributes": {},
                                "children": [{
                                    "type": "node",
                                    "sharedId": ANY_SHARED_ID,
                                    "value": {
                                        "nodeType": 3,
                                        "nodeValue": "element in open shadow DOM",
                                        "childNodeCount": 0,
                                        "children": []
                                    }
                                }]
                            }
                        }]
                    }
                }
            }
        }, {
            "type": "node",
            "sharedId": ANY_SHARED_ID,
            "value": {
                "nodeType": 1,
                "childNodeCount": 0,
                "localName": "div",
                "namespaceURI": "http://www.w3.org/1999/xhtml",
                "attributes": {},
                "children": [],
                "shadowRoot": {
                    "type": "node",
                    "value": {
                        "nodeType": 11,
                        "childNodeCount": 1,
                        "mode": "closed"
                    },
                    "sharedId": ANY_SHARED_ID,
                }
            }
        }]
    } == result["result"]


# Verify node nested in other data structures are serialized with the proper
# `sharedId`.
@pytest.mark.parametrize("eval_delegate, extract_delegate", [
    (lambda s: f"[{s}]", lambda r: r["value"][0]),
    (lambda s: f"new Set([{s}])", lambda r: r["value"][0]),
    (lambda s: f"({{qwe: {s}}})", lambda r: r["value"][0][1]),
    (lambda s: f"new Map([['qwe', {s}]])", lambda r: r["value"][0][1]),
])
@pytest.mark.asyncio
async def test_serialization_nested_node(websocket, context_id, html,
                                         eval_delegate, extract_delegate):
    await goto_url(
        websocket, context_id,
        html(
            "<div some_attr_name='some_attr_value'>some text<h2>some another text</h2></div>"
        ))

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

    assert {
        "type": "node",
        "sharedId": ANY_SHARED_ID,
        "value": {
            "nodeType": 1,
            "localName": "div",
            "namespaceURI": "http://www.w3.org/1999/xhtml",
            "childNodeCount": 2,
            'shadowRoot': None,
            "attributes": {
                "some_attr_name": "some_attr_value"
            },
            'shadowRoot': None,
        },
    } == extract_delegate(result["result"])


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

    assert {
        "type": "success",
        "type": "success",
        "result": {
            "type": "object",
            "value": [[
                "nested_object", {
                    "type": "object",
                    'value': [["a", {
                        "type": "number",
                        "value": 1,
                    }]]
                }
            ]]
        },
        "realm": ANY_STR
    } == result


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

    assert {
        "type": "success",
        "type": "success",
        "result": {
            "type": "array",
            "value": [{
                "type": "object",
                "value": [["a", {
                    "type": "number",
                    "value": 1,
                }]]
            }],
        },
        "realm": ANY_STR
    } == result


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
        "type": "object",
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
    assert {
        "type": "success",
        "type": "success",
        "result": {
            "type": "object",
            "value": [["a", {
                "type": "number",
                "value": 1
            }]]
        },
        "realm": ANY_STR
    } == result


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "test_input,expected",
    [(1, {
        'type': 'number',
        'value': 1
    }), ("2n", {
        'type': 'bigint',
        'value': '2'
    }), ('"3"', {
        'type': 'string',
        'value': '3'
    }),
     ("{'a': 'x', b: 'y'}", {
         'handle': ANY_STR,
         'type': 'object',
         'value': [['a', {
             'type': 'string',
             'value': 'x'
         }], ['b', {
             'type': 'string',
             'value': 'y'
         }]]
     }), ("() => {}", {
         'handle': ANY_STR,
         'type': 'function'
     })])
async def test_channel_complexTypes(test_input, expected, websocket,
                                    context_id):
    await subscribe(websocket, ["script.message"])

    await execute_command(
        websocket,
        {
            "method": "script.callFunction",
            "params": {
                # A small delay is needed to avoid a race condition.
                "functionDeclaration": """(binding) => {
                    setTimeout(() => {"""
                                       f'binding({test_input});\n'
                                       """}, 1);
                }""",
                "arguments": [{
                    "type": "channel",
                    "value": {
                        "channel": "MY_CHANNEL",
                        "ownership": "root",
                    },
                }],
                "target": {
                    "context": context_id
                },
                "awaitPromise": False,
                "resultOwnership": "root"
            }
        })

    resp = await read_JSON_message(websocket)
    assert resp == {
        "type": "event",
        "method": "script.message",
        "params": {
            "channel": "MY_CHANNEL",
            "data": expected,
            "source": {
                "context": context_id,
                "realm": ANY_STR,
            }
        }
    }
