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

import pytest

from _helpers import *


@pytest.mark.asyncio
async def test_script_evaluateThrowingPrimitive_exceptionReturned(
        websocket, context_id):
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression":
                "(()=>{const a=()=>{throw 1;}; const b=()=>{a();};\nconst c=()=>{b();};c();})()",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })

    recursive_compare(
        {
            "type": "exception",
            "realm": any_string,
            "exceptionDetails": {
                "text": "1",
                "columnNumber": 19,
                "lineNumber": 0,
                "exception": {
                    "type": "number",
                    "value": 1
                },
                "stackTrace": {
                    "callFrames": [{
                        "url": "",
                        "functionName": "a",
                        "lineNumber": 0,
                        "columnNumber": 19
                    }, {
                        "url": "",
                        "functionName": "b",
                        "lineNumber": 0,
                        "columnNumber": 43
                    }, {
                        "url": "",
                        "functionName": "c",
                        "lineNumber": 1,
                        "columnNumber": 13
                    }, {
                        "url": "",
                        "functionName": "",
                        "lineNumber": 1,
                        "columnNumber": 19
                    }, {
                        "url": "",
                        "functionName": "",
                        "lineNumber": 1,
                        "columnNumber": 25
                    }]
                }
            }
        }, result)


@pytest.mark.asyncio
async def test_script_evaluateThrowingError_exceptionReturned(
        websocket, context_id):
    exception_details = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression":
                "(()=>{const a=()=>{throw new Error('foo');}; const b=()=>{a();};\nconst c=()=>{b();};c();})()",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })

    recursive_compare(
        {
            "type": "exception",
            "realm": any_string,
            "exceptionDetails": {
                "text": "Error: foo",
                "columnNumber": 19,
                "lineNumber": 0,
                "exception": {
                    "type": "error",
                    "handle": any_string
                },
                "stackTrace": {
                    "callFrames": [{
                        "url": "",
                        "functionName": "a",
                        "lineNumber": 0,
                        "columnNumber": 25
                    }, {
                        "url": "",
                        "functionName": "b",
                        "lineNumber": 0,
                        "columnNumber": 58
                    }, {
                        "url": "",
                        "functionName": "c",
                        "lineNumber": 1,
                        "columnNumber": 13
                    }, {
                        "url": "",
                        "functionName": "",
                        "lineNumber": 1,
                        "columnNumber": 19
                    }, {
                        "url": "",
                        "functionName": "",
                        "lineNumber": 1,
                        "columnNumber": 25
                    }]
                }
            }
        }, exception_details)


@pytest.mark.asyncio
async def test_script_evaluateDontWaitPromise_promiseReturned(
        websocket, context_id):
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "Promise.resolve('SOME_RESULT')",
                "awaitPromise": False,
                "resultOwnership": "root",
                "target": {
                    "context": context_id
                }
            }
        })

    # Compare ignoring `handle`.
    recursive_compare(
        {
            "type": "success",
            "realm": any_string,
            "result": {
                "type": "promise",
                "handle": any_string
            }
        }, result)


# Uncomment after behaviour is clarified:
# https://github.com/w3c/webdriver-bidi/issues/201
# @pytest.mark.asyncio
# async def test_script_evaluateThenableAndWaitPromise_thenableAwaitedAndResultReturned(
#       websocket, context_id):
#     result = await execute_command(websocket, {
#         "method": "script.evaluate",
#         "params": {
#             "expression": "{then: (r)=>{r('SOME_RESULT');}}",
#             "awaitPromise": True,
#             "target": {"context": context_id}}})
#
#     # Compare ignoring `handle`.
#     recursiveCompare({
#         "handle": "6648989764296027.141631980526024.9376085393313653",
#         "type": "object",
#         "value": [[
#             "then", {
#                 "handle": "02303443180265008.08175681580349026.8281562053772789",
#                 "type": "function"}]]
#     }, result, ["handle"])


@pytest.mark.asyncio
async def test_script_evaluateWaitPromise_resultReturned(
        websocket, context_id):
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "Promise.resolve('SOME_RESULT')",
                "awaitPromise": True,
                "resultOwnership": "root",
                "target": {
                    "context": context_id
                }
            }
        })

    # Compare ignoring `handle`.
    recursive_compare(
        {
            "type": "success",
            "realm": any_string,
            "result": {
                "type": "string",
                "value": "SOME_RESULT"
            }
        }, result)


@pytest.mark.asyncio
async def test_script_evaluateChangingObject_resultObjectDidNotChange(
        websocket, context_id):
    result = await execute_command(
        websocket,
        {
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
                "resultOwnership": "root",
                "target": {
                    "context": context_id
                }
            }
        })

    # Verify the object wasn't changed.
    recursive_compare(
        {
            "type": "success",
            "realm": any_string,
            "result": {
                "type": "object",
                "value": [["i", {
                    "type": "number",
                    "value": 0
                }]],
                "handle": any_string
            }
        }, result)


@pytest.mark.asyncio
async def test_script_evaluateInteractsWithDom_resultReceived(
        websocket, context_id):
    result = await execute_command(
        websocket, {
            "id": 32,
            "method": "script.evaluate",
            "params": {
                "expression": "'!!@@##, ' + window.location.href",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })

    recursive_compare(
        {
            "type": "success",
            "realm": any_string,
            "result": {
                "type": "string",
                "value": "!!@@##, about:blank"
            }
        }, result)


@pytest.mark.asyncio
async def test_script_callFunctionWithArgs_resultReturn(websocket, context_id):
    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration":
                "(...arguments)=>{return Promise.resolve(arguments);}",
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
                "awaitPromise":
                True,
                "resultOwnership":
                "root"
            }
        })

    recursive_compare(
        {
            "type": "success",
            "realm": any_string,
            "result": {
                "type":
                "array",
                "value": [{
                    "type": 'string',
                    "value": 'ARGUMENT_STRING_VALUE'
                }, {
                    "type": 'number',
                    "value": 42
                }],
                "handle":
                any_string
            }
        }, result)


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
#         "handle": any_string,
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
                "functionDeclaration":
                "(...arguments)=>{return Promise.resolve(arguments);}",
                "arguments": [{
                    "type": "string",
                    "value": "ARGUMENT_STRING_VALUE"
                }, {
                    "type": "number",
                    "value": 42
                }],
                "awaitPromise":
                False,
                "resultOwnership":
                "root",
                "target": {
                    "context": context_id
                }
            }
        })

    recursive_compare(
        {
            "type": "success",
            "realm": any_string,
            "result": {
                "type": "promise",
                "handle": any_string
            }
        }, result)


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

    recursive_compare(
        {
            "type": "success",
            "realm": any_string,
            "result": {
                "type": "string",
                "value": "SOME_VALUE"
            }
        }, result)


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

    recursive_compare(
        {
            "type": "success",
            "realm": any_string,
            "result": {
                "type": "string",
                "value": "SOME_VALUE"
            }
        }, result)


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

    recursive_compare(
        {
            "type": "success",
            "realm": any_string,
            "result": {
                "type": "promise",
                "handle": any_string
            }
        }, result)


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

    recursive_compare(
        {
            "type": "success",
            "realm": any_string,
            "result": {
                "type": "string",
                "value": "SOME_VALUE"
            }
        }, result)


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

    recursive_compare(
        {
            "type": "success",
            "realm": any_string,
            "result": {
                "type": "promise",
                "handle": any_string
            }
        }, result)


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

    recursive_compare(
        {
            "type": "success",
            "realm": any_string,
            "result": {
                "type": "string",
                "value": "Window"
            }
        }, result)


@pytest.mark.asyncio
async def test_script_callFunctionWithClassicFunctionAndThisParameter_thisIsUsed(
        websocket, context_id):
    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration":
                "function(){return this.constructor.name}",
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

    recursive_compare(
        {
            "type": "success",
            "realm": any_string,
            "result": {
                "type": "string",
                "value": "Number"
            }
        }, result)


@pytest.mark.asyncio
# TODO(sadym): fix test. Serialize Number, String etc classes properly.
async def _ignore_test_script_callFunctionWithClassicFunctionAndThisParameter_thisIsUsed(
    websocket, context_id):
    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "function(){return this}",
                "this": {
                    "type": "number",
                    "value": 42
                },
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })

    recursive_compare(
        {
            "type": "success",
            "realm": any_string,
            "result": {
                "type": "number",
                "value": 42
            }
        }, result)


@pytest.mark.asyncio
async def test_script_callFunctionWithNode_resultReceived(
        websocket, context_id):
    # 1. Get element.
    # 2. Evaluate script on it.
    await goto_url(websocket, context_id, "data:text/html,<h2>test</h2>")

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
                "functionDeclaration":
                "(element) => {return '!!@@##, ' + element.innerHTML}",
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

    recursive_compare(
        {
            "type": "success",
            "realm": any_string,
            "result": {
                "type": "string",
                "value": "!!@@##, test"
            }
        }, result)


@pytest.mark.asyncio
async def test_script_evaluate_windowOpen_windowOpened(websocket, context_id):
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "window.open()",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })

    recursive_compare(
        {
            "type": "success",
            "realm": any_string,
            "result": {
                "type": "window",
                "handle": any_string
            }
        }, result)

    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {}
    })

    # Assert 2 contexts are present.
    assert len(result['contexts']) == 2


@pytest.mark.asyncio
async def test_scriptEvaluate_realm(websocket, context_id):
    # Create a sandbox.
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "(document.foo='bar')",
                "target": {
                    "context": context_id,
                    "sandbox": 'some_sandbox'
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })

    recursive_compare(
        {
            "type": "success",
            "result": {
                "type": "string",
                "value": "bar"
            },
            "realm": any_string
        }, result)

    realm = result["realm"]

    # Access sandbox by realm.
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "(document.foo)",
                "target": {
                    "realm": realm
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })

    recursive_compare(
        {
            "type": "success",
            "result": {
                "type": "string",
                "value": "bar"
            },
            "realm": any_string
        }, result)

    # Throw an exception in the sandbox.
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "throw new Error('SOME_ERROR')",
                "target": {
                    "context": context_id,
                    "sandbox": 'some_sandbox'
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })

    # Assert result contains realm.
    recursive_compare(
        {
            "type": "exception",
            "exceptionDetails": any_value,
            "realm": realm
        }, result)


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

    recursive_compare(
        {
            "type": "success",
            "result": {
                "type": "string",
                "value": "bar"
            },
            "realm": any_string
        }, result)

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

    recursive_compare(
        {
            "type": "success",
            "result": {
                "type": "string",
                "value": "bar"
            },
            "realm": any_string
        }, result)

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
    recursive_compare(
        {
            "type": "exception",
            "exceptionDetails": any_value,
            "realm": realm
        }, result)


@pytest.mark.asyncio
async def test_scriptGetRealms(websocket, context_id):
    result = await execute_command(websocket, {
        "method": "script.getRealms",
        "params": {}
    })

    recursive_compare(
        {
            "realms": [{
                "realm": any_string,
                "origin": "null",
                "type": "window",
                "context": context_id
            }]
        }, result)

    old_realm = result["realms"][0]["realm"]

    # Create a sandbox.
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "",
                "target": {
                    "context": context_id,
                    "sandbox": 'some_sandbox'
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })

    assert result["type"] == "success"

    sandbox_realm = result["realm"]

    result = await execute_command(websocket, {
        "method": "script.getRealms",
        "params": {}
    })

    # Assert 2 realms are created.
    recursive_compare(
        {
            "realms":
            compare_sorted("realm", [{
                "realm": old_realm,
                "origin": "null",
                "type": "window",
                "context": context_id
            }, {
                "realm": sandbox_realm,
                "origin": "null",
                "type": "window",
                "sandbox": "some_sandbox",
                "context": context_id
            }])
        }, result)

    # Assert closing browsing context destroys its realms.
    await execute_command(websocket, {
        "method": "browsingContext.close",
        "params": {
            "context": context_id
        }
    })

    result = await execute_command(websocket, {
        "method": "script.getRealms",
        "params": {}
    })

    # Assert no more realms existed.
    recursive_compare({"realms": []}, result)


@pytest.mark.asyncio
async def test_disown_releasesObject(websocket, default_realm, sandbox_realm):
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "({foo:'bar'})",
                "target": {
                    "realm": default_realm,
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })
    recursive_compare(
        {
            "type": "success",
            "result": {
                "type": "object",
                "value": any_value,
                "handle": any_string
            },
            "realm": default_realm
        }, result)

    handle = result["result"]["handle"]

    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "(obj)=>{return obj;}",
                "arguments": [{
                    "handle": handle
                }],
                "target": {
                    "realm": default_realm
                },
                "awaitPromise": True,
                "resultOwnership": "none"
            }
        })

    recursive_compare(
        {
            "type": "success",
            "result": {
                "type": "object",
                "value": any_value
            },
            "realm": any_string
        }, result)

    # Disown in the wrong realm does not have any effect.
    result = await execute_command(
        websocket, {
            "method": "script.disown",
            "params": {
                "handles": [handle],
                "target": {
                    "realm": sandbox_realm
                }
            }
        })

    recursive_compare({}, result)

    # Assert the object is not disposed.
    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "(obj)=>{return obj;}",
                "arguments": [{
                    "handle": handle
                }],
                "target": {
                    "realm": default_realm
                },
                "awaitPromise": True,
                "resultOwnership": "none"
            }
        })

    recursive_compare(
        {
            "type": "success",
            "result": {
                "type": "object",
                "value": any_value
            },
            "realm": any_string
        }, result)

    # Disown the object in proper realm.
    result = await execute_command(
        websocket, {
            "method": "script.disown",
            "params": {
                "handles": [handle],
                "target": {
                    "realm": default_realm
                }
            }
        })

    recursive_compare({}, result)

    # Assert the object is disposed.
    with pytest.raises(Exception) as exception_info:
        await execute_command(
            websocket, {
                "method": "script.callFunction",
                "params": {
                    "functionDeclaration": "(obj)=>{return obj;}",
                    "arguments": [{
                        "handle": handle
                    }],
                    "target": {
                        "realm": default_realm
                    },
                    "awaitPromise": True,
                    "resultOwnership": "none"
                }
            })

    recursive_compare(
        {
            "error": "invalid argument",
            "message": "Handle was not found."
        }, exception_info.value.args[0])
