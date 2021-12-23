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
async def test_script_evaluateThrowingPrimitive_exceptionReturned(websocket,
      context_id):
    exception_details = await execute_command(websocket, {
        "id": 45,
        "method": "script.evaluate",
        "params": {
            "expression": "(()=>{const a=()=>{throw 1;}; const b=()=>{a();};\nconst c=()=>{b();};c();})()",
            "target": {"context": context_id}}}, "exceptionDetails")

    recursiveCompare({
        "text": "Uncaught",
        "columnNumber": 19,
        "lineNumber": 0,
        "exception": {
            "type": "number",
            "value": 1},
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
                "columnNumber": 25}]}
    }, exception_details, ["objectId"])


@pytest.mark.asyncio
async def test_script_evaluateDontWaitPromise_promiseReturned(websocket,
      context_id):
    result = await execute_command(websocket, {
        "method": "script.evaluate",
        "params": {
            "expression": "Promise.resolve('SOME_RESULT')",
            "awaitPromise": False,
            "target": {"context": context_id}}})

    # Compare ignoring `objectId`.
    recursiveCompare({
        "type": "promise",
        "objectId": "__any_value__"
    }, result, ["objectId"])


@pytest.mark.asyncio
async def test_script_evaluateThenableAndWaitPromise_thenableReturned(
      websocket, context_id):
    result = await execute_command(websocket, {
        "method": "script.evaluate",
        "params": {
            "expression": "{then: (r)=>{r('SOME_RESULT');}}",
            "awaitPromise": True,
            "target": {"context": context_id}}})

    # Compare ignoring `objectId`.
    recursiveCompare({
        "objectId": "6648989764296027.141631980526024.9376085393313653",
        "type": "object",
        "value": [[
            "then", {
                "objectId": "02303443180265008.08175681580349026.8281562053772789",
                "type": "function"}]]
    }, result, ["objectId"])


@pytest.mark.asyncio
async def test_script_evaluateWaitPromise_resultReturned(websocket, context_id):
    result = await execute_command(websocket, {
        "method": "script.evaluate",
        "params": {
            "expression": "Promise.resolve('SOME_RESULT')",
            "awaitPromise": True,
            "target": {"context": context_id}}})

    # Compare ignoring `objectId`.
    recursiveCompare({
        "type": "string",
        "value": "SOME_RESULT"
    }, result, ["objectId"])


@pytest.mark.asyncio
async def test_script_evaluateChangingObject_resultObjectDidNotChange(
      websocket, context_id):
    result = await execute_command(websocket, {
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
            "target": {"context": context_id}}})

    # Verify the object wasn't changed.
    recursiveCompare({
        "type": "object",
        "value": [[
            "i", {
                "type": "number",
                "value": 0
            }]],
        "objectId": "__any_value__"
    }, result, ["objectId"])


@pytest.mark.asyncio
async def test_script_evaluateInteractsWithDom_resultReceived(websocket,
      context_id):
    result = await execute_command(websocket, {
        "id": 32,
        "method": "script.evaluate",
        "params": {
            "expression": "'!!@@##, ' + window.location.href",
            "target": {"context": context_id}}})

    assert result == {
        "type": "string",
        "value": "!!@@##, about:blank"}


@pytest.mark.asyncio
async def test_script_callFunctionWithArgs_resultReturn(websocket, context_id):
    result = await execute_command(websocket, {
        "method": "script.callFunction",
        "params": {
            "functionDeclaration": "(...args)=>{return Promise.resolve(args);}",
            "args": [{
                "type": "string",
                "value": "ARGUMENT_STRING_VALUE"
            }, {
                "type": "number",
                "value": 42
            }],
            "target": {"context": context_id}}})

    recursiveCompare({
        "type": "array",
        "value": [{
            "type": 'string',
            "value": 'ARGUMENT_STRING_VALUE'
        }, {
            "type": 'number',
            "value": 42}],
        "objectId": "__any_value__"
    }, result, ["objectId"])


@pytest.mark.asyncio
async def test_script_callFunctionWithThenableArgsAndAwaitParam_thenableReturn(
      websocket, context_id):
    result = await execute_command(websocket, {
        "method": "script.callFunction",
        "params": {
            "functionDeclaration": "(...args)=>({then: (r)=>{r(args);}})",
            "args": [{
                "type": "string",
                "value": "ARGUMENT_STRING_VALUE"
            }, {
                "type": "number",
                "value": 42
            }],
            "awaitPromise": True,
            "target": {"context": context_id}}})

    recursiveCompare({
        "objectId": "0017540866018586065.8588900581845549.5556004446288361",
        "type": "object",
        "value": [[
            "then", {
                "objectId": "27880276111744884.36134691065868907.6435355839204784",
                "type": "function"}]]
    }, result, ["objectId"])


@pytest.mark.asyncio
async def test_script_callFunctionWithArgsAndDoNotAwaitPromise_promiseReturn(
      websocket, context_id):
    result = await execute_command(websocket, {
        "method": "script.callFunction",
        "params": {
            "functionDeclaration": "(...args)=>{return Promise.resolve(args);}",
            "args": [{
                "type": "string",
                "value": "ARGUMENT_STRING_VALUE"
            }, {
                "type": "number",
                "value": 42
            }],
            "awaitPromise": False,
            "target": {"context": context_id}}})

    recursiveCompare({
        "type": "promise",
        "objectId": "__any_value__"
    }, result, ["objectId"])


@pytest.mark.asyncio
async def test_script_callFunctionWithRemoteValueArgument_resultReturn(
      websocket, context_id):
    result = await execute_command(websocket, {
        "method": "script.evaluate",
        "params": {
            "expression": "{SOME_PROPERTY:'SOME_VALUE'}",
            "awaitPromise": True,
            "target": {"context": context_id}}})

    object_id = result["objectId"]

    result = await execute_command(websocket, {
        "method": "script.callFunction",
        "params": {
            "functionDeclaration": "(obj)=>{return obj.SOME_PROPERTY;}",
            "args": [{
                "objectId": object_id
            }],
            "target": {"context": context_id}}})

    assert result == {
        "type": "string",
        "value": "SOME_VALUE"}


@pytest.mark.asyncio
async def test_script_callFunctionWithAsyncArrowFunctionAndAwaitPromise_resultReturned(
      websocket, context_id):
    result = await execute_command(websocket, {
        "method": "script.callFunction",
        "params": {
            "functionDeclaration": "async ()=>{return 'SOME_VALUE'}",
            "this": {
                "type": "number",
                "value": 1
            },
            "awaitPromise": True,
            "target": {"context": context_id}}})

    assert result == {
        "type": "string",
        "value": "SOME_VALUE"}


@pytest.mark.asyncio
async def test_script_callFunctionWithAsyncArrowFunctionAndAwaitPromiseFalse_promiseReturned(
      websocket, context_id):
    result = await execute_command(websocket, {
        "method": "script.callFunction",
        "params": {
            "functionDeclaration": "async ()=>{return 'SOME_VALUE'}",
            "this": {
                "type": "number",
                "value": 1
            },
            "awaitPromise": False,
            "target": {"context": context_id}}})

    recursiveCompare({
        "type": "promise",
        "objectId": "__any_value__"
    }, result, ["objectId"])


@pytest.mark.asyncio
async def test_script_callFunctionWithAsyncClassicFunctionAndAwaitPromise_resultReturned(
      websocket, context_id):
    result = await execute_command(websocket, {
        "method": "script.callFunction",
        "params": {
            "functionDeclaration": "async function(){return 'SOME_VALUE'}",
            "this": {
                "type": "number",
                "value": 1
            },
            "awaitPromise": True,
            "target": {"context": context_id}}})

    assert result == {
        "type": "string",
        "value": "SOME_VALUE"}


@pytest.mark.asyncio
async def test_script_callFunctionWithAsyncClassicFunctionAndAwaitPromiseFalse_promiseReturned(
      websocket, context_id):
    result = await execute_command(websocket, {
        "method": "script.callFunction",
        "params": {
            "functionDeclaration": "async function(){return 'SOME_VALUE'}",
            "this": {
                "type": "number",
                "value": 1
            },
            "awaitPromise": False,
            "target": {"context": context_id}}})

    recursiveCompare({
        "type": "promise",
        "objectId": "__any_value__"
    }, result, ["objectId"])


@pytest.mark.asyncio
async def test_script_callFunctionWithArrowFunctionAndThisParameter_thisIsIgnoredAndWindowUsedInstead(
      websocket, context_id):
    result = await execute_command(websocket, {
        "method": "script.callFunction",
        "params": {
            "functionDeclaration": "()=>{return this.constructor.name}",
            "this": {
                "type": "number",
                "value": 1
            },
            "target": {"context": context_id}}})

    assert result == {
        "type": "string",
        "value": "Window"}


@pytest.mark.asyncio
async def test_script_callFunctionWithClassicFunctionAndThisParameter_thisIsUsed(
      websocket, context_id):
    result = await execute_command(websocket, {
        "method": "script.callFunction",
        "params": {
            "functionDeclaration": "function(){return this.constructor.name}",
            "this": {
                "type": "number",
                "value": 1
            },
            "target": {"context": context_id}}})

    assert result == {
        "type": "string",
        "value": "Number"}


@pytest.mark.asyncio
async def test_script_evaluateWithNode_resultReceived(websocket, context_id):
    # 1. Get element.
    # 2. Evaluate script on it.
    await goto_url(websocket, context_id,
                   "data:text/html,<h2>test</h2>")

    # 1. Get element.`
    result = await execute_command(websocket, {
        "method": "PROTO.browsingContext.findElement",
        "params": {
            "selector": "body > h2",
            "context": context_id}})

    object_id = result["objectId"]

    # 2. Evaluate script on it.
    result = await execute_command(websocket, {
        "method": "script.callFunction",
        "params": {
            "functionDeclaration": "(element) => {return '!!@@##, ' + element.innerHTML}",
            "args": [{
                "objectId": object_id}],
            "target": {"context": context_id}}})

    assert result == {
        "type": "string",
        "value": "!!@@##, test"}
