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
from unittest.mock import ANY

import pytest
from anys import ANY_STR
from syrupy.filters import props
from test_helpers import (execute_command, get_tree, send_JSON_command,
                          subscribe, wait_for_filtered_event)


@pytest.mark.asyncio
async def test_script_evaluateThrowingPrimitive_exceptionReturned(
        websocket, context_id):
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "(()=>{const a=()=>{throw 1;}; const b=()=>{a();};\nconst c=()=>{b();};c();})()",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })

    assert {
        "type": "exception",
        "realm": ANY_STR,
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
    } == result


@pytest.mark.asyncio
async def test_script_evaluateThrowingError_exceptionReturned(
        websocket, context_id):
    exception_details = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "(()=>{const a=()=>{throw new Error('foo');}; const b=()=>{a();};\nconst c=()=>{b();};c();})()",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })

    assert {
        "type": "exception",
        "realm": ANY_STR,
        "exceptionDetails": {
            "text": "Error: foo",
            "columnNumber": 19,
            "lineNumber": 0,
            "exception": {
                "type": "error",
                "handle": ANY_STR
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
    } == exception_details


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
    assert {
        "type": "success",
        "realm": ANY_STR,
        "type": "success",
        "result": {
            "type": "promise",
            "handle": ANY_STR
        }
    } == result


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
    assert {
        "type": "success",
        "realm": ANY_STR,
        "type": "success",
        "result": {
            "type": "string",
            "value": "SOME_RESULT"
        }
    } == result


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
    assert {
        "type": "success",
        "realm": ANY_STR,
        "type": "success",
        "result": {
            "type": "object",
            "value": [["i", {
                "type": "number",
                "value": 0
            }]],
            "handle": ANY_STR
        }
    } == result


@pytest.mark.asyncio
async def test_script_evaluateInteractsWithDom_resultReceived(
        websocket, context_id):
    result = await execute_command(
        websocket, {
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

    assert {
        "type": "success",
        "realm": ANY_STR,
        "type": "success",
        "result": {
            "type": "string",
            "value": "!!@@##, about:blank"
        }
    } == result


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

    assert {
        "type": "success",
        "realm": ANY_STR,
        "type": "success",
        "result": {
            "type": "window",
            "value": {
                "context": ANY_STR
            },
            "handle": ANY_STR
        }
    } == result

    result = await get_tree(websocket)

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
    assert {
        "type": "exception",
        "exceptionDetails": ANY,
        "realm": realm
    } == result


@pytest.mark.asyncio
async def test_scriptEvaluate_dedicated_worker(websocket, context_id, html,
                                               snapshot):
    worker_url = 'data:text/javascript,setTimeout(() => {}, 20000)'
    url = html(f"<script>window.w = new Worker('{worker_url}');</script>")

    await subscribe(websocket, ["script.realmCreated"])

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "context": context_id,
                "url": url,
                "wait": "complete",
            }
        })

    # Wait for worker to be created.
    worker_realm_created_event = await wait_for_filtered_event(
        websocket, lambda e: e['method'] == 'script.realmCreated' and e[
            'params']['type'] == 'dedicated-worker')

    assert worker_realm_created_event == {
        'type': 'event',
        'method': 'script.realmCreated',
        'params': {
            'realm': ANY_STR,
            'origin': worker_url,
            'owners': [ANY_STR],
            'type': 'dedicated-worker'
        }
    }
    realm = worker_realm_created_event["params"]["realm"]

    # Set up a listener on the page.
    await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "target": {
                    "context": context_id
                },
                "expression": "window.p = new Promise(resolve => window.w.addEventListener('message', ({data}) => resolve(data), {once: true}))",
                "awaitPromise": False
            }
        })

    # Post a message from the worker.
    await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "target": {
                    "realm": realm
                },
                "expression": "self.postMessage('hello world');",
                "awaitPromise": False
            }
        })

    # Check the promise
    assert await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "target": {
                    "context": context_id
                },
                "expression": "window.p",
                "awaitPromise": True
            }
        }) == snapshot(exclude=props("realm"))
