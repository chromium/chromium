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
from anys import ANY_DICT, ANY_STR
from test_helpers import (AnyExtending, execute_command, read_JSON_message,
                          send_JSON_command, subscribe)


@pytest.mark.asyncio
async def test_addPreloadScript_setGlobalVariable(websocket, context_id, html):
    result = await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "expression": "() => { window.foo='bar'; }",
                "context": context_id,
            }
        })
    assert result == {'script': ANY_STR}

    await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": html("<h2>test</h2>"),
                "wait": "complete",
                "context": context_id
            }
        })

    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "window.foo",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })
    assert result["result"] == {"type": "string", "value": 'bar'}


@pytest.mark.asyncio
async def test_addPreloadScript_logging(websocket, context_id, html):
    await subscribe(websocket, "log.entryAdded")

    result = await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "expression": "() => console.log('my preload script')",
                "context": context_id,
            }
        })
    assert result == {'script': ANY_STR}

    command_id = await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": html("<h2>test</h2>"),
                "wait": "complete",
                "context": context_id
            }
        })

    # Log event should happen before navigation.
    result = await read_JSON_message(websocket)
    assert result == AnyExtending({
        "method": "log.entryAdded",
        "params": {
            "args": [{
                "type": "string",
                "value": "my preload script"
            }]
        }
    })

    # Assert navigation is finished.
    result = await read_JSON_message(websocket)
    assert result == {"id": command_id, "result": ANY_DICT}
