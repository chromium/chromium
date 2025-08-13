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

PROMPT_MESSAGE = 'Prompt Opened'

TOP_LEVEL_CONTEXT = 'top-level-context'
SAME_PROCESS_IFRAME = 'same-process-iframe'
OOPiF = 'OOPiF'


@pytest.mark.asyncio
@pytest.mark.parametrize("frame_level",
                         [TOP_LEVEL_CONTEXT, SAME_PROCESS_IFRAME, OOPiF])
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
        websocket, context_id, iframe_id, frame_level, prompt_type, html,
        capabilities):
    if frame_level == TOP_LEVEL_CONTEXT:
        targe_context_id = context_id
    else:
        targe_context_id = iframe_id
        if frame_level == OOPiF:
            # Move iFrame out of process
            await goto_url(websocket, targe_context_id,
                           html("", same_origin=False))

    await subscribe(websocket, [
        "browsingContext.userPromptOpened", "browsingContext.userPromptClosed"
    ])

    await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": f"""{prompt_type}('{PROMPT_MESSAGE}')""",
                "awaitPromise": True,
                "target": {
                    "context": targe_context_id,
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
            "context": targe_context_id,
            "type": prompt_type,
            'handler': expected_handler,
            "message": PROMPT_MESSAGE,
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
                    "context": targe_context_id,
                    "accept": False
                }
            })

    response = await wait_for_event(websocket,
                                    "browsingContext.userPromptClosed")

    assert response == {
        'type': 'event',
        "method": "browsingContext.userPromptClosed",
        "params": {
            "context": targe_context_id,
            "accepted": expected_handler == 'accept',
            "type": prompt_type
        }
    }


@pytest.mark.asyncio
@pytest.mark.parametrize("frame_level",
                         [TOP_LEVEL_CONTEXT, SAME_PROCESS_IFRAME, OOPiF])
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
        websocket, context_id, iframe_id, html, frame_level, capabilities):
    if frame_level == TOP_LEVEL_CONTEXT:
        targe_context_id = context_id
    else:
        targe_context_id = iframe_id

    url = html("""
        <script>
            window.addEventListener('beforeunload', event => {
                event.returnValue = 'Leave?';
                event.preventDefault();
            });
        </script>
        """,
               same_origin=frame_level != OOPiF)

    await goto_url(websocket, targe_context_id, url)

    await subscribe(websocket, ["browsingContext.userPromptOpened"])

    # We need to interact with the page to trigger "beforeunload"
    await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "document.body.click()",
                "target": {
                    "context": targe_context_id
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
                "context": targe_context_id,
                "wait": "complete"
            }
        })

    # Default behavior is to accept the before unload prompt.
    expected_handler = 'accept'
    if 'unhandledPromptBehavior' in capabilities:
        if 'beforeUnload' in capabilities['unhandledPromptBehavior']:
            expected_handler = capabilities['unhandledPromptBehavior'][
                'beforeUnload']
        elif 'default' in capabilities['unhandledPromptBehavior']:
            expected_handler = capabilities['unhandledPromptBehavior'][
                'default']
        if isinstance(capabilities['unhandledPromptBehavior'], str):
            expected_handler = 'accept'

    resp = await read_JSON_message(websocket)
    assert resp == {
        'method': 'browsingContext.userPromptOpened',
        'params': {
            'context': targe_context_id,
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
                    "context": targe_context_id,
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
