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
from anys import ANY_DICT
from test_helpers import (AnyExtending, execute_command, goto_url,
                          read_JSON_message, send_JSON_command, subscribe,
                          wait_for_event)


@pytest.mark.asyncio
@pytest.mark.parametrize("prompt_type", ["alert", "confirm", "prompt"])
@pytest.mark.parametrize('capabilities', [{}, {
    'unhandledPromptBehavior': 'dismiss'
}, {
    'unhandledPromptBehavior': 'dismiss and notify'
}, {
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
    'unhandledPromptBehavior': 'accept'
}, {
    'unhandledPromptBehavior': 'accept and notify'
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
    'unhandledPromptBehavior': 'ignore'
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
async def test_browsingContext_userPromptOpened_capabilityRespected(
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
        if isinstance(capabilities['unhandledPromptBehavior'], str):
            expected_handler = capabilities['unhandledPromptBehavior'].replace(
                ' and notify', '')
        elif prompt_type in capabilities['unhandledPromptBehavior']:
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
@pytest.mark.parametrize('capabilities', [{}, {
    'unhandledPromptBehavior': 'dismiss'
}, {
    'unhandledPromptBehavior': 'dismiss and notify'
}, {
    'unhandledPromptBehavior': {
        'default': 'dismiss'
    }
}, {
    'unhandledPromptBehavior': {
        'beforeUnload': 'dismiss',
        'default': 'ignore'
    }
}, {
    'unhandledPromptBehavior': 'accept'
}, {
    'unhandledPromptBehavior': 'accept and notify'
}, {
    'unhandledPromptBehavior': {
        'default': 'accept'
    }
}, {
    'unhandledPromptBehavior': {
        'beforeUnload': 'accept',
        'default': 'ignore'
    }
}, {
    'unhandledPromptBehavior': 'ignore'
}, {
    'unhandledPromptBehavior': {
        'default': 'ignore'
    }
}, {
    'unhandledPromptBehavior': {
        'beforeUnload': 'ignore',
        'default': 'accept'
    }
}],
                         indirect=True)
async def test_browsingContext_beforeUnloadPromptOpened_capabilityRespected(
        websocket, context_id, html, capabilities):
    await subscribe(websocket, ["browsingContext.userPromptOpened"])

    url = html("""
        <script>
            window.addEventListener('beforeunload', event => {
                event.returnValue = 'Leave?';
                event.preventDefault();
            });
        </script>
        """)

    await goto_url(websocket, context_id, url)

    # We need to interact with the page to trigger "beforeunload"
    await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "document.body.click()",
                "target": {
                    "context": context_id
                },
                "awaitPromise": False,
                "userActivation": True
            }
        })

    navigate_command_id = await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": 'about:blank',
                "context": context_id,
                "wait": "complete"
            }
        })

    # Default behavior is to accept the before unload prompt.
    expected_handler = 'accept'
    if 'unhandledPromptBehavior' in capabilities:
        if 'beforeUnload' in capabilities['unhandledPromptBehavior']:
            expected_handler = capabilities['unhandledPromptBehavior'][
                'beforeUnload']

    resp = await read_JSON_message(websocket)
    assert resp == {
        'method': 'browsingContext.userPromptOpened',
        'params': {
            'context': context_id,
            'handler': expected_handler,
            'message': '',
            'type': 'beforeunload',
        },
        'type': 'event',
    }

    if expected_handler == 'accept':
        # Assert navigation succeeded.
        resp = await read_JSON_message(websocket)
        assert resp == {
            'id': navigate_command_id,
            'result': ANY_DICT,
            'type': 'success',
        }
    elif expected_handler == 'dismiss':
        # Assert navigation failed.
        resp = await read_JSON_message(websocket)
        assert resp == AnyExtending({
            'id': navigate_command_id,
            'type': 'error'
        })
    elif expected_handler == 'ignore':
        # Handle prompt.
        await execute_command(
            websocket, {
                "method": "browsingContext.handleUserPrompt",
                "params": {
                    "context": context_id,
                    "accept": True
                }
            })

        # Assert navigation succeeded.
        resp = await read_JSON_message(websocket)
        assert resp == {
            'id': navigate_command_id,
            'result': ANY_DICT,
            'type': 'success',
        }
    else:
        assert False, f"Unexpected handler: {expected_handler}"
