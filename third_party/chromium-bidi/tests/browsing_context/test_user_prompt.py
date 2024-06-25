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
from test_helpers import send_JSON_command, subscribe, wait_for_event


@pytest.mark.asyncio
async def test_browsingContext_userPromptOpened_event(websocket, context_id):

    await subscribe(websocket, ["browsingContext.userPromptOpened"])

    message = 'Prompt Opened'

    await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": f"""alert('{message}')""",
                "awaitPromise": True,
                "target": {
                    "context": context_id,
                }
            }
        })

    response = await wait_for_event(websocket,
                                    "browsingContext.userPromptOpened")
    assert response == {
        'type': 'event',
        "method": "browsingContext.userPromptOpened",
        "params": {
            "context": context_id,
            "type": 'alert',
            'handler': 'ignore',
            "message": message,
        }
    }


@pytest.mark.asyncio
async def test_browsingContext_userPromptClosed_event(websocket, context_id):

    await subscribe(websocket, [
        "browsingContext.userPromptOpened", "browsingContext.userPromptClosed"
    ])

    message = 'Prompt Opened'

    await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": f"""alert('{message}')""",
                "awaitPromise": True,
                "target": {
                    "context": context_id,
                }
            }
        })

    await wait_for_event(websocket, "browsingContext.userPromptOpened")

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.handleUserPrompt",
            "params": {
                "context": context_id,
            }
        })

    response = await wait_for_event(websocket,
                                    "browsingContext.userPromptClosed")

    assert response == {
        'type': 'event',
        "method": "browsingContext.userPromptClosed",
        "params": {
            "context": context_id,
            "accepted": True,
            "type": "alert"
        }
    }


@pytest.mark.asyncio
async def test_browsingContext_userPromptOpened_event_default_value(
        websocket, context_id):

    await subscribe(websocket, ["browsingContext.userPromptOpened"])

    message = 'Prompt Opened'
    default = 'Prompt Default'

    await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": f"""prompt('{message}', '{default}')""",
                "awaitPromise": True,
                "target": {
                    "context": context_id,
                }
            }
        })

    response = await wait_for_event(websocket,
                                    "browsingContext.userPromptOpened")
    assert response == {
        'type': 'event',
        "method": "browsingContext.userPromptOpened",
        "params": {
            "context": context_id,
            "type": 'prompt',
            'handler': 'ignore',
            "message": message,
            "defaultValue": default,
        }
    }
