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

import pytest
from test_helpers import execute_command

TEST_CASES = pytest.mark.parametrize(
    "eval, max_object_depth, expected_result",
    [
        ("({'foo': {'bar': 'baz'}})", 0, {
            "type": "object"
        }),
        ("({'foo': {'bar': 'baz'}})", 1, {
            "type": "object",
            "value": [["foo", {
                "type": "object",
            }]],
        }),
        ("({'foo': {'bar': 'baz'}})", 2, {
            "type": "object",
            "value": [[
                "foo", {
                    "type": "object",
                    "value": [["bar", {
                        "type": "string",
                        "value": "baz"
                    }]]
                }
            ]],
        }),
        ("({'foo': {'bar': 'baz'}})", None, {
            "type": "object",
            "value": [[
                "foo", {
                    "type": "object",
                    "value": [["bar", {
                        "type": "string",
                        "value": "baz"
                    }]]
                }
            ]],
        }),
    ],
    ids=[
        "maxObjectDepth: 0",
        "maxObjectDepth: 1",
        "maxObjectDepth: 2",
        "maxObjectDepth: null",
    ],
)


@pytest.mark.asyncio
@TEST_CASES
async def test_serializationOptions_maxObjectDepth_evaluate(
        websocket, context_id, eval, max_object_depth, expected_result):
    response = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": eval,
                "target": {
                    "context": context_id
                },
                "awaitPromise": False,
                "serializationOptions": {
                    "maxObjectDepth": max_object_depth
                }
            }
        })
    assert response["result"] == expected_result


@pytest.mark.asyncio
@TEST_CASES
async def test_serializationOptions_maxObjectDepth_callFunction(
        websocket, context_id, eval, max_object_depth, expected_result):
    response = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": f"()=>{eval}",
                "this": {
                    "type": "undefined"
                },
                "awaitPromise": False,
                "target": {
                    "context": context_id
                },
                "serializationOptions": {
                    "maxObjectDepth": max_object_depth
                }
            }
        })
    assert response["result"] == expected_result
