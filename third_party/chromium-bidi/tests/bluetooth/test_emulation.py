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
from test_helpers import (AnyExtending, execute_command, goto_url,
                          send_JSON_command, subscribe, wait_for_event)

HTML_SINGLE_PERIPHERAL = """
<div>
    <button id="bluetooth">bluetooth</button>
    <script>
        const options = {filters: [{name:"SomeDevice"}]};
        document.getElementById('bluetooth').addEventListener('click', () => {
          navigator.bluetooth.requestDevice(options);
        });
    </script>
</div>
"""

# Create a fake BT device.
fake_device_address = '09:09:09:09:09:09'


async def setup_device(websocket, context_id):
    # Simulate a Bluetooth adapter.
    await execute_command(
        websocket, {
            'method': 'bluetooth.simulateAdapter',
            'params': {
                'context': context_id,
                'type': 'create',
                'state': 'powered-on',
            }
        })

    # Create a fake BT device.
    fake_device_address = '09:09:09:09:09:09'
    await execute_command(
        websocket, {
            'method': 'bluetooth.simulatePreconnectedPeripheral',
            'params': {
                'context': context_id,
                'address': fake_device_address,
                'name': 'SomeDevice',
                'manufacturerData': [{
                    'key': 17,
                    'data': 'AP8BAX8=',
                }],
                'knownServiceUuids':
                    ['12345678-1234-5678-9abc-def123456789', ],
            }
        })


@pytest.mark.asyncio
@pytest.mark.parametrize("state_1", ["absent", "powered-off", "powered-on"])
@pytest.mark.parametrize("state_2", ["absent", "powered-off", "powered-on"])
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args': ['--enable-features=WebBluetooth']
    }
}],
                         indirect=True)
async def test_simulate_create_adapter_twice(websocket, context_id, state_1,
                                             state_2, test_headless_mode):
    if test_headless_mode == "old":
        pytest.xfail("Old headless mode does not support Bluetooth")

    await execute_command(
        websocket, {
            'method': 'bluetooth.simulateAdapter',
            'params': {
                'context': context_id,
                'state': state_1,
                'type': 'create',
            }
        })

    await execute_command(
        websocket, {
            'method': 'bluetooth.simulateAdapter',
            'params': {
                'context': context_id,
                'state': state_2,
                'type': 'create',
            }
        })


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args': ['--enable-features=WebBluetooth']
    }
}],
                         indirect=True)
async def test_bluetooth_requestDevicePromptUpdated(websocket, context_id,
                                                    html, test_headless_mode):
    if test_headless_mode == "old":
        pytest.xfail("Old headless mode does not support Bluetooth")

    await subscribe(websocket, ['bluetooth'])

    url = html(HTML_SINGLE_PERIPHERAL)
    await goto_url(websocket, context_id, url)

    await setup_device(websocket, context_id)

    await send_JSON_command(
        websocket, {
            'method': 'script.evaluate',
            'params': {
                'expression': 'document.querySelector("#bluetooth").click();',
                'awaitPromise': True,
                'target': {
                    'context': context_id,
                },
                'userActivation': True
            }
        })

    response = await wait_for_event(websocket,
                                    'bluetooth.requestDevicePromptUpdated')
    assert response == AnyExtending({
        'type': 'event',
        'method': 'bluetooth.requestDevicePromptUpdated',
        'params': {
            'context': context_id,
            'devices': [{
                'id': fake_device_address
            }],
        }
    })


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args': ['--enable-features=WebBluetooth']
    }
}],
                         indirect=True)
@pytest.mark.parametrize('accept', [True, False])
async def test_bluetooth_handleRequestDevicePrompt(websocket, context_id, html,
                                                   test_headless_mode, accept):
    if test_headless_mode == "old":
        pytest.xfail("Old headless mode does not support Bluetooth")

    await subscribe(websocket, ['bluetooth'])

    url = html(HTML_SINGLE_PERIPHERAL)
    await goto_url(websocket, context_id, url)

    await setup_device(websocket, context_id)

    await send_JSON_command(
        websocket, {
            'method': 'script.evaluate',
            'params': {
                'expression': 'document.querySelector("#bluetooth").click();',
                'awaitPromise': True,
                'target': {
                    'context': context_id,
                },
                'userActivation': True
            }
        })

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
