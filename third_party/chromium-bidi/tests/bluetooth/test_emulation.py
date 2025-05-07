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

from . import (FAKE_DEVICE_ADDRESS, FAKE_DEVICE_NAME, disable_simulation,
               request_device, setup_device)


@pytest_asyncio.fixture(autouse=True)
async def teardown(websocket, context_id):
    yield
    await disable_simulation(websocket, context_id)


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
                                             state_2):
    await execute_command(
        websocket, {
            'method': 'bluetooth.simulateAdapter',
            'params': {
                'context': context_id,
                'state': state_1,
            }
        })

    await execute_command(
        websocket, {
            'method': 'bluetooth.simulateAdapter',
            'params': {
                'context': context_id,
                'state': state_2,
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
                                                    html):
    await subscribe(websocket, ['bluetooth'])
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
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args': ['--enable-features=WebBluetooth']
    }
}],
                         indirect=True)
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


@pytest.mark.asyncio
async def test_bluetooth_disable_simulation_twice(websocket, context_id):
    await execute_command(
        websocket, {
            'method': 'bluetooth.disableSimulation',
            'params': {
                'context': context_id,
            }
        })
    await execute_command(
        websocket, {
            'method': 'bluetooth.disableSimulation',
            'params': {
                'context': context_id,
            }
        })


@pytest.mark.asyncio
async def test_bluetooth_disable_simulation(websocket, context_id):
    await setup_device(websocket, context_id)
    await execute_command(
        websocket, {
            'method': 'bluetooth.disableSimulation',
            'params': {
                'context': context_id,
            }
        })
    # Creating a fake BT device while simulation disabled would fail.
    with pytest.raises(Exception,
                       match=str({
                           'error': 'unknown error',
                           'message': 'BluetoothEmulation not enabled'
                       })):
        await execute_command(
            websocket, {
                'method': 'bluetooth.simulatePreconnectedPeripheral',
                'params': {
                    'context': context_id,
                    'address': FAKE_DEVICE_ADDRESS,
                    'name': FAKE_DEVICE_NAME,
                    'manufacturerData': [{
                        'key': 17,
                        'data': 'AP8BAX8=',
                    }],
                    'knownServiceUuids':
                        ['12345678-1234-5678-9abc-def123456789', ],
                }
            })


@pytest.mark.asyncio
async def test_bluetooth_disable_simulation_in_another_context(
        websocket, context_id, another_context_id):
    pytest.xfail(
        "Bluetooth simulation doesn not support multiple contexts yet")

    await setup_device(websocket, context_id)
    await setup_device(websocket, another_context_id)
    await execute_command(
        websocket, {
            'method': 'bluetooth.disableSimulation',
            'params': {
                'context': another_context_id,
            }
        })
    # Simulation commands should still work after simulation is disabled in another
    # context.
    await execute_command(
        websocket, {
            'method': 'bluetooth.simulatePreconnectedPeripheral',
            'params': {
                'context': context_id,
                'address': FAKE_DEVICE_ADDRESS,
                'name': FAKE_DEVICE_NAME,
                'manufacturerData': [{
                    'key': 17,
                    'data': 'AP8BAX8=',
                }],
                'knownServiceUuids':
                    ['12345678-1234-5678-9abc-def123456789', ],
            }
        })


@pytest.mark.asyncio
async def test_bluetooth_simulate_same_address_device_twice(
        websocket, context_id):
    device_address = await setup_device(websocket, context_id)
    with pytest.raises(
            Exception,
            match=str({
                'error': 'invalid argument',
                'message': f'Bluetooth device with address {device_address} already exists'
            })):
        await execute_command(
            websocket, {
                'method': 'bluetooth.simulatePreconnectedPeripheral',
                'params': {
                    'context': context_id,
                    'address': device_address,
                    'name': FAKE_DEVICE_NAME,
                    'manufacturerData': [],
                    'knownServiceUuids': [],
                }
            })


@pytest.mark.asyncio
async def test_bluetooth_simulate_device_after_reenable_simulation(
        websocket, context_id):
    await setup_device(websocket, context_id)
    await execute_command(
        websocket, {
            'method': 'bluetooth.disableSimulation',
            'params': {
                'context': context_id,
            }
        })
    await setup_device(websocket, context_id)
