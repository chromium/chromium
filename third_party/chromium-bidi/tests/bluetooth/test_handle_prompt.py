#  Copyright 2024 Google LLC.
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
import pytest_asyncio
from test_helpers import (AnyExtending, execute_command, goto_url, subscribe,
                          wait_for_event)

from . import (FAKE_DEVICE_ADDRESS, disable_simulation, request_device,
               setup_device)


@pytest_asyncio.fixture(autouse=True)
async def teardown(websocket, context_id):
    yield
    await disable_simulation(websocket, context_id)


@pytest.mark.asyncio
async def test_bluetooth_requestDevicePromptUpdated(websocket, context_id,
                                                    html):
    await subscribe(websocket, ['bluetooth.requestDevicePromptUpdated'])
    await goto_url(websocket, context_id, html())
    await setup_device(websocket, context_id)
    await request_device(websocket, context_id)
    response = await wait_for_event(websocket,
                                    'bluetooth.requestDevicePromptUpdated')
    assert response == AnyExtending({
        'type': 'event',
        'method': 'bluetooth.requestDevicePromptUpdated',
        'params': {
            'context': context_id,
            'devices': [{
                'id': FAKE_DEVICE_ADDRESS
            }],
        }
    })


@pytest.mark.asyncio
@pytest.mark.parametrize('accept', [True, False])
async def test_bluetooth_handleRequestDevicePrompt(websocket, context_id, html,
                                                   accept):
    await subscribe(websocket, ['bluetooth'])
    await goto_url(websocket, context_id, html())
    await setup_device(websocket, context_id)
    await request_device(websocket, context_id)
    event = await wait_for_event(websocket,
                                 'bluetooth.requestDevicePromptUpdated')

    await execute_command(
        websocket, {
            'method': 'bluetooth.handleRequestDevicePrompt',
            'params': {
                'context': context_id,
                'accept': accept,
                'prompt': event['params']['prompt'],
                'device': event['params']['devices'][0]['id']
            }
        })
