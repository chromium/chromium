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
from test_helpers import execute_command


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
    assert {
        "type": "success",
        "type": "success",
        "result": {
            "type": "object",
            "value": ANY,
            "handle": ANY_STR
        },
        "realm": default_realm
    } == result

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

    assert {
        "type": "success",
        "type": "success",
        "result": {
            "type": "object",
            "value": ANY
        },
        "realm": ANY_STR
    } == result

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

    assert {} == result

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

    assert {
        "type": "success",
        "type": "success",
        "result": {
            "type": "object",
            "value": ANY
        },
        "realm": ANY_STR
    } == result

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

    assert {} == result

    # Assert the object is disposed.
    with pytest.raises(Exception,
                       match=str({
                           "error": "no such handle",
                           "message": "Handle was not found."
                       })):
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
