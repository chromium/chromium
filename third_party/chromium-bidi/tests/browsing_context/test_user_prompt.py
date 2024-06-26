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
@pytest.mark.parametrize("prompt_type", ["alert", "confirm", "prompt"])
@pytest.mark.parametrize('capabilities', [{}, {
    'unhandledPromptBehavior': {
        'default': 'dismiss'
    }
}, {
    'unhandledPromptBehavior': {
        'alert': 'dismiss',
        'default': 'ignore'
    }
}, {
    'unhandledPromptBehavior': {
        'confirm': 'dismiss',
        'default': 'ignore'
    }
}, {
    'unhandledPromptBehavior': {
        'prompt': 'dismiss',
        'default': 'ignore'
    }
}, {
    'unhandledPromptBehavior': {
        'default': 'accept'
    }
}, {
    'unhandledPromptBehavior': {
        'alert': 'accept',
        'default': 'ignore'
    }
}, {
    'unhandledPromptBehavior': {
        'confirm': 'accept',
        'default': 'ignore'
    }
}, {
    'unhandledPromptBehavior': {
        'prompt': 'accept',
        'default': 'ignore'
    }
}, {
    'unhandledPromptBehavior': {
        'default': 'ignore'
    }
}, {
    'unhandledPromptBehavior': {
        'alert': 'ignore',
        'default': 'accept'
    }
}, {
    'unhandledPromptBehavior': {
        'confirm': 'ignore',
        'default': 'accept'
    }
}, {
    'unhandledPromptBehavior': {
        'prompt': 'ignore',
        'default': 'accept'
    }
}],
                         indirect=True)
async def test_browsingContext_userPromptOpened_userPromptClosed(
        websocket, context_id, prompt_type, capabilities):
    await subscribe(websocket, [
        "browsingContext.userPromptOpened", "browsingContext.userPromptClosed"
    ])

    message = 'Prompt Opened'

    await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": f"""{prompt_type}('{message}')""",
                "awaitPromise": True,
                "target": {
                    "context": context_id,
                }
            }
        })

    response = await wait_for_event(websocket,
                                    "browsingContext.userPromptOpened")

    expected_handler = 'dismiss'
    if 'unhandledPromptBehavior' in capabilities:
        if prompt_type in capabilities['unhandledPromptBehavior']:
            expected_handler = capabilities['unhandledPromptBehavior'][
                prompt_type]
        elif 'default' in capabilities['unhandledPromptBehavior']:
            expected_handler = capabilities['unhandledPromptBehavior'][
                'default']

    assert response == {
        'type': 'event',
        "method": "browsingContext.userPromptOpened",
        "params": {
            "context": context_id,
            "type": prompt_type,
            'handler': expected_handler,
            "message": message,
            **({
                "defaultValue": ""
            } if prompt_type == "prompt" else {}),
        }
    }

    if expected_handler == 'ignore':
        # Dismiss the prompt manually.
        await send_JSON_command(
            websocket, {
                "method": "browsingContext.handleUserPrompt",
                "params": {
                    "context": context_id,
                    "accept": False
                }
            })

    response = await wait_for_event(websocket,
                                    "browsingContext.userPromptClosed")

    assert response == {
        'type': 'event',
        "method": "browsingContext.userPromptClosed",
        "params": {
            "context": context_id,
            "accepted": expected_handler == 'accept',
            "type": prompt_type
        }
    }


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'unhandledPromptBehavior': {
        'default': 'ignore'
    }
}],
                         indirect=True)
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
