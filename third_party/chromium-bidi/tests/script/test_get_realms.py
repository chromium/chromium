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
# limitations under the Licensefrom unittest.mock import ANY
import pytest
from anys import ANY_STR
from test_helpers import execute_command


@pytest.mark.asyncio
async def test_scriptGetRealms(websocket, context_id):
    result = await execute_command(websocket, {
        "method": "script.getRealms",
        "params": {}
    })

    assert {
        "realms": [{
            "realm": ANY_STR,
            "origin": "null",
            "type": "window",
            "context": context_id
        }]
    } == result

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

    assert ["realms"] == list(result.keys())

    # Assert 2 realms are created.
    def realm_key(x):
        return x["realm"]

    assert sorted([{
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
    }],
                  key=realm_key) == sorted(result["realms"], key=realm_key)

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
    assert {"realms": []} == result
