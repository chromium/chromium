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
from anys import ANY_STR
from test_helpers import execute_command, goto_url


@pytest.mark.asyncio
async def test_preloadScript_remove_nonExistingScript_fails(websocket):
    with pytest.raises(Exception,
                       match=str({
                           'error': 'no such script',
                           'message': "No preload script with BiDi ID '42'"
                       })):
        await execute_command(websocket, {
            "method": "script.removePreloadScript",
            "params": {
                "script": '42',
            }
        })


@pytest.mark.asyncio
async def test_preloadScript_remove_addAndRemoveIsNoop_secondRemoval_fails(
        websocket, context_id, html):
    result = await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "functionDeclaration": "() => { window.foo='bar'; }",
            }
        })
    assert result == {'script': ANY_STR}
    bidi_id = result["script"]

    result = await execute_command(websocket, {
        "method": "script.removePreloadScript",
        "params": {
            "script": bidi_id,
        }
    })
    assert result == {}

    await goto_url(websocket, context_id, html())

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
    assert result["result"] == {"type": "undefined"}

    # Ensure script was removed
    with pytest.raises(
            Exception,
            match=str({
                'error': 'no such script',
                'message': f"No preload script with BiDi ID '{bidi_id}'"
            })):
        await execute_command(
            websocket, {
                "method": "script.removePreloadScript",
                "params": {
                    "script": bidi_id,
                }
            })
