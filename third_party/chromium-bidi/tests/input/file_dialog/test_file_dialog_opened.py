# Copyright 2025 Google LLC.
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
from test_helpers import (AnyExtending, execute_command, goto_url,
                          send_JSON_command, subscribe, wait_for_event)

KEYS_TO_STABILIZE = ['sharedId', 'context', 'realm', 'id']


@pytest.mark.asyncio
@pytest.mark.parametrize('multiple', [True, False])
async def test_file_dialog_show_file_event(websocket, context_id, url_example,
                                           multiple):
    await goto_url(websocket, context_id, url_example)

    await subscribe(websocket, ["input.fileDialogOpened"])

    await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": f"window.showOpenFilePicker({{multiple: {'true' if multiple else 'false'}}})",
                "target": {
                    "context": context_id,
                },
                "awaitPromise": False,
                "userActivation": True
            }
        })

    response = await wait_for_event(websocket, 'input.fileDialogOpened')
    assert response == {
        'method': 'input.fileDialogOpened',
        'params': {
            'context': context_id,
            'multiple': multiple,
        },
        'type': 'event',
    }


@pytest.mark.asyncio
async def test_file_dialog_show_directory_event(websocket, context_id,
                                                url_example):
    await goto_url(websocket, context_id, url_example)

    await subscribe(websocket, ["input.fileDialogOpened"])

    await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "window.showDirectoryPicker()",
                "target": {
                    "context": context_id,
                },
                "awaitPromise": False,
                "userActivation": True
            }
        })

    response = await wait_for_event(websocket, 'input.fileDialogOpened')
    assert response == {
        'method': 'input.fileDialogOpened',
        'params': {
            'context': context_id,
            'multiple': False,
        },
        'type': 'event',
    }


@pytest.mark.asyncio
@pytest.mark.parametrize('multiple', [True, False])
async def test_file_dialog_input_click_event(websocket, context_id, html,
                                             read_messages, multiple, snapshot,
                                             test_chromedriver_mode):
    if test_chromedriver_mode:
        pytest.xfail(reason="TODO: #3294")

    await goto_url(
        websocket, context_id,
        html(f"<input id=input type=file {'multiple' if multiple else ''} />"))

    await subscribe(websocket, ["input.fileDialogOpened"])

    await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "input.click(); input",
                "target": {
                    "context": context_id,
                },
                "awaitPromise": False,
                "userActivation": True
            }
        })

    messages = await read_messages(2, keys_to_stabilize=KEYS_TO_STABILIZE)
    assert messages == snapshot


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [
    {
        'unhandledPromptBehavior': 'dismiss'
    },
    {
        'unhandledPromptBehavior': 'dismiss and notify'
    },
    {
        'unhandledPromptBehavior': {
            'default': 'dismiss'
        }
    },
    {
        'unhandledPromptBehavior': {
            'file': 'dismiss',
            'default': 'ignore'
        }
    },
    {
        'unhandledPromptBehavior': 'accept'
    },
    {
        'unhandledPromptBehavior': 'accept and notify'
    },
    {
        'unhandledPromptBehavior': {
            'default': 'accept'
        }
    },
    {
        'unhandledPromptBehavior': {
            'file': 'accept',
            'default': 'ignore'
        }
    },
],
                         indirect=True)
async def test_file_dialog_unhandled_prompt_behavior_input_cancel(
        websocket, context_id, html, read_messages, snapshot):
    await goto_url(websocket, context_id, html("<input id=input type=file />"))
    await subscribe(websocket, ["script.message", 'input.fileDialogOpened'])

    await send_JSON_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": """function(channel) {
                        const input = document.getElementById('input');
                        input.addEventListener('cancel', (event) => {
                            channel(JSON.stringify({
                                name: 'cancel',
                                event
                            }));
                        });
                        input.click();
                    }""",
                "target": {
                    "context": context_id,
                },
                "arguments": [{
                    "type": "channel",
                    "value": {
                        "channel": "channel_name"
                    }
                }],
                "awaitPromise": False,
                "userActivation": True
            }
        })

    messages = await read_messages(3, keys_to_stabilize=KEYS_TO_STABILIZE)
    assert messages == snapshot


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{}, {
    'unhandledPromptBehavior': 'ignore'
}, {
    'unhandledPromptBehavior': {
        'default': 'ignore'
    }
}, {
    'unhandledPromptBehavior': {
        'file': 'ignore',
        'default': 'dismiss'
    }
}],
                         indirect=True)
async def test_file_dialog_unhandled_prompt_behavior_input_ignore(
        websocket, context_id, html, read_messages, snapshot,
        test_headless_mode):
    if test_headless_mode != "false":
        pytest.xfail("Headless browser always cancels the file dialog")
        return

    await goto_url(websocket, context_id, html("<input id=input type=file />"))
    await subscribe(websocket, ["script.message", 'input.fileDialogOpened'])

    await send_JSON_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": """function(channel) {
                        const input = document.getElementById('input');
                        input.addEventListener('cancel', (event) => {
                            channel(JSON.stringify({
                                name: 'cancel',
                                event
                            }));
                        });
                        input.click();
                    }""",
                "target": {
                    "context": context_id,
                },
                "arguments": [{
                    "type": "channel",
                    "value": {
                        "channel": "channel_name"
                    }
                }],
                "awaitPromise": False,
                "userActivation": True
            }
        })

    messages = await read_messages(2,
                                   keys_to_stabilize=KEYS_TO_STABILIZE,
                                   check_no_other_messages=True)
    assert messages == snapshot


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [
    {
        'unhandledPromptBehavior': 'dismiss'
    },
    {
        'unhandledPromptBehavior': 'dismiss and notify'
    },
    {
        'unhandledPromptBehavior': {
            'default': 'dismiss'
        }
    },
    {
        'unhandledPromptBehavior': {
            'file': 'dismiss',
            'default': 'ignore'
        }
    },
    {
        'unhandledPromptBehavior': 'accept'
    },
    {
        'unhandledPromptBehavior': 'accept and notify'
    },
    {
        'unhandledPromptBehavior': {
            'default': 'accept'
        }
    },
    {
        'unhandledPromptBehavior': {
            'file': 'accept',
            'default': 'ignore'
        }
    },
],
                         indirect=True)
async def test_file_dialog_unhandled_prompt_behavior_show_file_cancel(
        websocket, context_id, url_example):
    await goto_url(websocket, context_id, url_example)
    await subscribe(websocket, ["input.fileDialogOpened"])

    resp = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "window.showOpenFilePicker()",
                "target": {
                    "context": context_id,
                },
                "awaitPromise": True,
                "userActivation": True
            }
        })

    assert resp == AnyExtending({
        "type": "exception",
        "exceptionDetails": {
            "text": "AbortError: Failed to execute 'showOpenFilePicker' on 'Window': Intercepted by Page.setInterceptFileChooserDialog()."
        }
    })


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{}, {
    'unhandledPromptBehavior': 'ignore'
}, {
    'unhandledPromptBehavior': {
        'default': 'ignore'
    }
}, {
    'unhandledPromptBehavior': {
        'file': 'ignore',
        'default': 'dismiss'
    }
}],
                         indirect=True)
async def test_file_dialog_unhandled_prompt_behavior_show_file_ignore(
        websocket, context_id, url_example, test_headless_mode):
    """
    The test exploits the fact that the file picker dialog can't be opened
    twice. This is used as an indicator that the dialog was shown.
    """
    if test_headless_mode != "false":
        pytest.xfail("Headless browser always cancels the file dialog")
        return

    await goto_url(websocket, context_id, url_example)

    resp = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "Promise.all([window.showOpenFilePicker(),window.showOpenFilePicker()])",
                "target": {
                    "context": context_id,
                },
                "awaitPromise": True,
                "userActivation": True
            }
        })

    assert resp == AnyExtending({
        "type": "exception",
        "exceptionDetails": {
            "text": "NotAllowedError: Failed to execute 'showOpenFilePicker' on 'Window': File picker already active."
        }
    })
