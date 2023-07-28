# Copyright 2023 Google LLC.
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
from unittest.mock import ANY

import pytest
from anys import ANY_STR
from test_helpers import execute_command, goto_url


@pytest.mark.asyncio
async def test_script_callFunctionWithArgs_resultReturn(websocket, context_id):
    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "(...arguments)=>{return Promise.resolve(arguments);}",
                "arguments": [{
                    "type": "string",
                    "value": "ARGUMENT_STRING_VALUE"
                }, {
                    "type": "number",
                    "value": 42
                }],
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })

    assert {
        "type": "success",
        "realm": ANY_STR,
        "type": "success",
        "result": {
            "type": "array",
            "value": [{
                "type": 'string',
                "value": 'ARGUMENT_STRING_VALUE'
            }, {
                "type": 'number',
                "value": 42
            }],
            "handle": ANY_STR
        }
    } == result


# Uncomment after behaviour is clarified:
# https://github.com/w3c/webdriver-bidi/issues/201
# @pytest.mark.asyncio
# async def test_script_callFunctionWithThenableArgsAndAwaitParam_thenableReturn(
#       websocket, context_id):
#     result = await execute_command(websocket, {
#         "method": "script.callFunction",
#         "params": {
#             "functionDeclaration": "(...arguments)=>({then: (r)=>{r(arguments);}})",
#             "arguments": [{
#                 "type": "string",
#                 "value": "ARGUMENT_STRING_VALUE"
#             }, {
#                 "type": "number",
#                 "value": 42
#             }],
#             "awaitPromise": True,
#             "target": {"context": context_id}}})
#
#     recursiveCompare({
#         "handle": ANY_STR,
#         "type": "object",
#         "value": [[
#             "then", {
#                 "type": "function"}]]
#     }, result, ["handle"])


@pytest.mark.asyncio
async def test_script_callFunctionWithArgsAndDoNotAwaitPromise_promiseReturn(
        websocket, context_id):
    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "(...arguments)=>{return Promise.resolve(arguments);}",
                "arguments": [{
                    "type": "string",
                    "value": "ARGUMENT_STRING_VALUE"
                }, {
                    "type": "number",
                    "value": 42
                }],
                "awaitPromise": False,
                "resultOwnership": "root",
                "target": {
                    "context": context_id
                }
            }
        })

    assert {
        "type": "success",
        "realm": ANY_STR,
        "type": "success",
        "result": {
            "type": "promise",
            "handle": ANY_STR
        }
    } == result


@pytest.mark.asyncio
async def test_script_callFunctionWithRemoteValueArgument_resultReturn(
        websocket, context_id):
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "({SOME_PROPERTY:'SOME_VALUE'})",
                "awaitPromise": True,
                "resultOwnership": "root",
                "target": {
                    "context": context_id
                }
            }
        })

    handle = result["result"]["handle"]

    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "(obj)=>{return obj.SOME_PROPERTY;}",
                "arguments": [{
                    "handle": handle
                }],
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })

    assert {
        "type": "success",
        "realm": ANY_STR,
        "type": "success",
        "result": {
            "type": "string",
            "value": "SOME_VALUE"
        }
    } == result


@pytest.mark.asyncio
async def test_script_callFunctionWithAsyncArrowFunctionAndAwaitPromise_resultReturned(
        websocket, context_id):
    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "async ()=>{return 'SOME_VALUE'}",
                "this": {
                    "type": "number",
                    "value": 1
                },
                "awaitPromise": True,
                "resultOwnership": "root",
                "target": {
                    "context": context_id
                }
            }
        })

    assert {
        "type": "success",
        "realm": ANY_STR,
        "type": "success",
        "result": {
            "type": "string",
            "value": "SOME_VALUE"
        }
    } == result


@pytest.mark.asyncio
async def test_script_callFunctionWithAsyncArrowFunctionAndAwaitPromiseFalse_promiseReturned(
        websocket, context_id):
    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "async ()=>{return 'SOME_VALUE'}",
                "this": {
                    "type": "number",
                    "value": 1
                },
                "awaitPromise": False,
                "resultOwnership": "root",
                "target": {
                    "context": context_id
                }
            }
        })

    assert {
        "type": "success",
        "realm": ANY_STR,
        "type": "success",
        "result": {
            "type": "promise",
            "handle": ANY_STR
        }
    } == result


@pytest.mark.asyncio
async def test_script_callFunctionWithAsyncClassicFunctionAndAwaitPromise_resultReturned(
        websocket, context_id):
    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "async function(){return 'SOME_VALUE'}",
                "this": {
                    "type": "number",
                    "value": 1
                },
                "awaitPromise": True,
                "resultOwnership": "root",
                "target": {
                    "context": context_id
                }
            }
        })

    assert {
        "type": "success",
        "realm": ANY_STR,
        "type": "success",
        "result": {
            "type": "string",
            "value": "SOME_VALUE"
        }
    } == result


@pytest.mark.asyncio
async def test_script_callFunctionWithAsyncClassicFunctionAndAwaitPromiseFalse_promiseReturned(
        websocket, context_id):
    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "async function(){return 'SOME_VALUE'}",
                "this": {
                    "type": "number",
                    "value": 1
                },
                "awaitPromise": False,
                "resultOwnership": "root",
                "target": {
                    "context": context_id
                }
            }
        })

    assert {
        "type": "success",
        "realm": ANY_STR,
        "type": "success",
        "result": {
            "type": "promise",
            "handle": ANY_STR
        }
    } == result


@pytest.mark.asyncio
async def test_script_callFunctionWithArrowFunctionAndThisParameter_thisIsIgnoredAndWindowUsedInstead(
        websocket, context_id):
    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "()=>{return this.constructor.name}",
                "this": {
                    "type": "number",
                    "value": 1
                },
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })

    assert {
        "type": "success",
        "realm": ANY_STR,
        "type": "success",
        "result": {
            "type": "string",
            "value": "Window"
        }
    } == result


@pytest.mark.asyncio
async def test_script_callFunctionWithClassicFunctionAndThisParameter_thisIsUsed(
        websocket, context_id):
    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "function(){return this.constructor.name}",
                "this": {
                    "type": "number",
                    "value": 1
                },
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })

    assert {
        "type": "success",
        "realm": ANY_STR,
        "type": "success",
        "result": {
            "type": "string",
            "value": "Number"
        }
    } == result


@pytest.mark.asyncio
async def test_script_callFunctionWithNode_resultReceived(
        websocket, context_id, html):
    # 1. Get element.
    # 2. Evaluate script on it.
    await goto_url(websocket, context_id, html("<h2>test</h2>"))

    # 1. Get element.
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "document.querySelector('body > h2');",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })

    handle = result["result"]["handle"]

    # 2. Evaluate script on it.
    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "(element) => {return '!!@@##, ' + element.innerHTML}",
                "arguments": [{
                    "handle": handle
                }],
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })

    assert {
        "type": "success",
        "realm": ANY_STR,
        "type": "success",
        "result": {
            "type": "string",
            "value": "!!@@##, test"
        }
    } == result


@pytest.mark.asyncio
async def test_scriptCallFunction_realm(websocket, context_id):
    # Create a sandbox.
    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "()=>{return document.foo='bar';}",
                "arguments": [],
                "target": {
                    "context": context_id,
                    "sandbox": 'some_sandbox'
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })

    assert {
        "type": "success",
        "type": "success",
        "result": {
            "type": "string",
            "value": "bar"
        },
        "realm": ANY_STR
    } == result

    realm = result["realm"]

    # Access sandbox by realm.
    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "()=>{return document.foo;}",
                "arguments": [],
                "target": {
                    "realm": realm
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })

    assert {
        "type": "success",
        "type": "success",
        "result": {
            "type": "string",
            "value": "bar"
        },
        "realm": ANY_STR
    } == result

    # Throw an exception in the sandbox.
    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "()=>{throw new Error('SOME_ERROR');}",
                "arguments": [],
                "target": {
                    "realm": realm
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })

    # Assert result contains realm.
    assert {
        "type": "exception",
        "exceptionDetails": ANY,
        "realm": realm
    } == result


@pytest.mark.asyncio
@pytest.mark.parametrize("user_activation", [True, False])
async def test_script_callFunction_userActivation(websocket, context_id,
                                                  user_activation):
    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": """
                  () => {
                    document.body.appendChild(document.createTextNode('test'));
                    document.execCommand('selectAll');
                    return document.execCommand('copy');
                  }
                """,
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root",
                "userActivation": user_activation
            }
        })

    assert {
        "type": "success",
        "realm": ANY_STR,
        "type": "success",
        "result": {
            "type": "boolean",
            "value": user_activation
        }
    } == result
