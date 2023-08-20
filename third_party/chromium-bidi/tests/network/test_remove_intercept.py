#  Copyright 2023 Google LLC.
#  Copyright (c) Microsoft Corporation.
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
import pytest
from test_helpers import execute_command


@pytest.mark.asyncio
async def test_remove_intercept_no_such_intercept(websocket):
    with pytest.raises(Exception) as exception_info:
        await execute_command(
            websocket, {
                "method": "network.removeIntercept",
                "params": {
                    "intercept": "00000000-0000-0000-0000-000000000000",
                },
            })

    assert {
        "error": "no such intercept",
        "message": "Intercept '00000000-0000-0000-0000-000000000000' does not exist."
    } == exception_info.value.args[0]


@pytest.mark.asyncio
async def test_remove_intercept_twice(websocket):
    result = await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": ["beforeRequestSent"],
                "urlPatterns": [{
                    "type": "string",
                    "pattern": "https://www.example.com/*"
                }],
            },
        })
    intercept_id = result["intercept"]

    result = await execute_command(
        websocket, {
            "method": "network.removeIntercept",
            "params": {
                "intercept": intercept_id,
            },
        })
    assert result == {}

    # Check that the intercept is gone.
    with pytest.raises(Exception) as exception_info:
        await execute_command(
            websocket, {
                "method": "network.removeIntercept",
                "params": {
                    "intercept": intercept_id,
                },
            })

    assert {
        "error": "no such intercept",
        "message": f"Intercept '{intercept_id}' does not exist."
    } == exception_info.value.args[0]
