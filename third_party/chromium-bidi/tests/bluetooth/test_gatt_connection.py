#  Copyright 2025 Google LLC.
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
from test_helpers import (execute_command, send_JSON_command, subscribe,
                          wait_for_event)

from . import disable_simulation, setup_granted_device


@pytest_asyncio.fixture(autouse=True)
async def teardown(websocket, context_id):
    yield
    await disable_simulation(websocket, context_id)


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args': ['--enable-features=WebBluetooth']
    }
}],
                         indirect=True)
@pytest.mark.parametrize('code', [0x0, 0x1, 0x2])
async def test_bluetooth_simulateGattConnectionResponse(
        websocket, context_id, html, code):
    await subscribe(websocket, ['bluetooth.gattConnectionAttempted'])
    await setup_granted_device(websocket, context_id, html)

    await send_JSON_command(
        websocket, {
            'method': 'script.evaluate',
            'params': {
                'expression': 'device.gatt.connect()',
                'awaitPromise': True,
                'target': {
                    'context': context_id,
                },
                'userActivation': True
            }
        })

    event = await wait_for_event(websocket,
                                 'bluetooth.gattConnectionAttempted')
    await execute_command(
        websocket, {
            'method': 'bluetooth.simulateGattConnectionResponse',
            'params': {
                'context': context_id,
                'accept': True,
                'address': event['params']['address'],
                'code': code
            }
        })
