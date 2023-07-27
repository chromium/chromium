# Copyright 2022 Google LLC.
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
from test_helpers import execute_command


async def _evaluate(script, sandbox, context_id, websocket):
    target = {"context": context_id}
    if sandbox is not None:
        target["sandbox"] = sandbox
    return await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": script,
                "awaitPromise": False,
                "target": target
            }
        })


async def _eval_via_call_function(script, sandbox, context_id, websocket):
    target = {"context": context_id}
    if sandbox is not None:
        target["sandbox"] = sandbox

    return await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": f"()=>{{return {script} }}",
                "awaitPromise": False,
                "target": target
            }
        })


@pytest.mark.parametrize("call_delegate", [_evaluate, _eval_via_call_function])
@pytest.mark.asyncio
async def test_sandbox_isolated(websocket, context_id, call_delegate):
    # Create PROPERTY_0 in default execution context.
    await call_delegate("window.PROPERTY_0='VALUE_0'", None, context_id,
                        websocket)

    # Create PROPERTY_ in empty string sandbox.
    await call_delegate("window.PROPERTY_='VALUE_'", None, context_id,
                        websocket)

    # Create SHARED_PROPERTY_NAME with UNIQUE_VALUE_0 in default execution
    # context.
    await call_delegate("window.SHARED_PROPERTY_NAME='UNIQUE_VALUE_0'", None,
                        context_id, websocket)

    # Create PROPERTY_1 in SANDBOX_1
    await call_delegate("window.PROPERTY_1='VALUE_1'", "SANDBOX_1", context_id,
                        websocket)

    # Create SHARED_PROPERTY_NAME with UNIQUE_VALUE_1 in SANDBOX_1
    await call_delegate("window.SHARED_PROPERTY_NAME='UNIQUE_VALUE_1'",
                        "SANDBOX_1", context_id, websocket)

    # Create PROPERTY_2 in SANDBOX_2
    await call_delegate("window.PROPERTY_2='VALUE_2'", "SANDBOX_2", context_id,
                        websocket)

    # Create SHARED_PROPERTY_NAME with UNIQUE_VALUE_2 in SANDBOX_2
    await call_delegate("window.SHARED_PROPERTY_NAME='UNIQUE_VALUE_2'",
                        "SANDBOX_2", context_id, websocket)

    # Get PROPERTY_0, PROPERTY_1, PROPERTY_2 and SHARED_PROPERTY_NAME from
    # default execution context
    result_0 = await call_delegate(
        "["
        "window.PROPERTY_0, "
        "window.PROPERTY_, "
        "window.PROPERTY_1, "
        "window.PROPERTY_2, "
        "window.SHARED_PROPERTY_NAME]", None, context_id, websocket)

    assert result_0["result"] == {
        "type": "array",
        "value": [{
            "type": "string",
            "value": "VALUE_0"
        }, {
            "type": "string",
            "value": "VALUE_"
        }, {
            "type": "undefined"
        }, {
            "type": "undefined"
        }, {
            "type": "string",
            "value": "UNIQUE_VALUE_0"
        }]
    }

    # Get PROPERTY_0, PROPERTY_1, PROPERTY_2 and SHARED_PROPERTY_NAME from
    # empty string sandbox, which redirects to the default realm.
    result_0 = await call_delegate(
        "["
        "window.PROPERTY_0, "
        "window.PROPERTY_, "
        "window.PROPERTY_1, "
        "window.PROPERTY_2, "
        "window.SHARED_PROPERTY_NAME]", "", context_id, websocket)

    assert result_0["result"] == {
        "type": "array",
        "value": [{
            "type": "string",
            "value": "VALUE_0"
        }, {
            "type": "string",
            "value": "VALUE_"
        }, {
            "type": "undefined"
        }, {
            "type": "undefined"
        }, {
            "type": "string",
            "value": "UNIQUE_VALUE_0"
        }]
    }

    # Get PROPERTY_0, PROPERTY_1, PROPERTY_2 and SHARED_PROPERTY_NAME from
    # SANDBOX_1
    result_1 = await call_delegate(
        "["
        "window.PROPERTY_0, "
        "window.PROPERTY_, "
        "window.PROPERTY_1, "
        "window.PROPERTY_2, "
        "window.SHARED_PROPERTY_NAME]", "SANDBOX_1", context_id, websocket)

    assert result_1["result"] == {
        "type": "array",
        "value": [{
            "type": "undefined"
        }, {
            "type": "undefined"
        }, {
            "type": "string",
            "value": "VALUE_1"
        }, {
            "type": "undefined"
        }, {
            "type": "string",
            "value": "UNIQUE_VALUE_1"
        }]
    }

    # Get PROPERTY_0, PROPERTY_1, PROPERTY_2 and SHARED_PROPERTY_NAME from
    # SANDBOX_2
    result_2 = await call_delegate(
        "["
        "window.PROPERTY_0, "
        "window.PROPERTY_, "
        "window.PROPERTY_1, "
        "window.PROPERTY_2, "
        "window.SHARED_PROPERTY_NAME]", "SANDBOX_2", context_id, websocket)
    assert result_2["result"] == {
        "type": "array",
        "value": [{
            "type": "undefined"
        }, {
            "type": "undefined"
        }, {
            "type": "undefined"
        }, {
            "type": "string",
            "value": "VALUE_2"
        }, {
            "type": "string",
            "value": "UNIQUE_VALUE_2"
        }]
    }
