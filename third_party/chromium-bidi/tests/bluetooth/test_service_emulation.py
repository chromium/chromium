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
from test_helpers import execute_command

from . import (BATTERY_SERVICE_UUID, FAKE_DEVICE_ADDRESS,
               HEART_RATE_SERVICE_UUID, create_gatt_connection,
               disable_simulation, setup_device, setup_granted_device,
               simulate_service)


async def get_services(websocket, context_id: str) -> list[str]:
    response = await execute_command(
        websocket, {
            'method': 'script.evaluate',
            'params': {
                'expression': '''
                    (async () => {
                        try {
                            const services = await device.gatt.getPrimaryServices();
                            return services.map(s => s.uuid);
                        } catch (e) {
                            if (e.name === 'NotFoundError') {
                                return [];
                            }
                            throw e;
                        }
                    })();
                ''',
                'awaitPromise': True,
                'target': {
                    'context': context_id,
                },
                'userActivation': True
            }
        })
    return [item['value'] for item in response['result']['value']]


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
async def test_bluetooth_simulateService(websocket, context_id, html):
    device_address = await setup_granted_device(
        websocket, context_id, html,
        [HEART_RATE_SERVICE_UUID, BATTERY_SERVICE_UUID])
    await create_gatt_connection(websocket, context_id)

    await simulate_service(websocket, context_id, device_address,
                           HEART_RATE_SERVICE_UUID, 'add')
    assert await get_services(websocket,
                              context_id) == [HEART_RATE_SERVICE_UUID]

    await simulate_service(websocket, context_id, device_address,
                           BATTERY_SERVICE_UUID, 'add')
    assert sorted(await get_services(websocket, context_id)) == sorted(
        [HEART_RATE_SERVICE_UUID, BATTERY_SERVICE_UUID])

    await simulate_service(websocket, context_id, device_address,
                           BATTERY_SERVICE_UUID, 'remove')
    assert await get_services(websocket,
                              context_id) == [HEART_RATE_SERVICE_UUID]

    await simulate_service(websocket, context_id, device_address,
                           HEART_RATE_SERVICE_UUID, 'remove')
    assert await get_services(websocket, context_id) == []


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args': ['--enable-features=WebBluetooth']
    }
}],
                         indirect=True)
async def test_bluetooth_add_same_service_uuid_twice(websocket, context_id):
    device_address = await setup_device(websocket, context_id)
    await simulate_service(websocket, context_id, device_address,
                           HEART_RATE_SERVICE_UUID, 'add')
    with pytest.raises(
            Exception,
            match=str({
                'error': 'invalid argument',
                'message': f'Service with UUID {HEART_RATE_SERVICE_UUID} already exists'
            })):
        await simulate_service(websocket, context_id, device_address,
                               HEART_RATE_SERVICE_UUID, 'add')


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args': ['--enable-features=WebBluetooth']
    }
}],
                         indirect=True)
async def test_bluetooth_remove_unknown_service_uuid(websocket, context_id):
    device_address = await setup_device(websocket, context_id)
    with pytest.raises(
            Exception,
            match=str({
                'error': 'invalid argument',
                'message': f'Service with UUID {HEART_RATE_SERVICE_UUID} on device {device_address} does not exist'
            })):
        await simulate_service(websocket, context_id, device_address,
                               HEART_RATE_SERVICE_UUID, 'remove')


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args': ['--enable-features=WebBluetooth']
    }
}],
                         indirect=True)
async def test_bluetooth_add_service_to_unknown_device(websocket, context_id):
    await execute_command(
        websocket, {
            'method': 'bluetooth.simulateAdapter',
            'params': {
                'context': context_id,
                'state': 'powered-on',
            }
        })
    with pytest.raises(
            Exception,
            match=str({
                'error': 'invalid argument',
                'message': f'Bluetooth device with address {FAKE_DEVICE_ADDRESS} does not exist'
            })):
        await simulate_service(websocket, context_id, FAKE_DEVICE_ADDRESS,
                               HEART_RATE_SERVICE_UUID, 'add')
