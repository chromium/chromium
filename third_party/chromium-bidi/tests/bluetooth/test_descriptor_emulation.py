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

from . import (BATTERY_SERVICE_UUID,
               CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID,
               CLIENT_CHARACTERISTIC_CONFIGURATION_DESCRIPTOR_UUID,
               FAKE_DEVICE_ADDRESS, HEART_RATE_SERVICE_UUID,
               MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID, add_characteristic,
               create_gatt_connection, disable_simulation, setup_device,
               setup_granted_device, simulate_descriptor, simulate_service)


async def get_descriptors(websocket, context_id: str, service_uuid: str,
                          characteristic_uuid: str) -> list[str]:
    response = await execute_command(
        websocket, {
            'method': 'script.evaluate',
            'params': {
                'expression': f'''
                    (async () => {{
                        try {{
                            const service = await device.gatt.getPrimaryService('{service_uuid}');
                            const characteristic = await service.getCharacteristic('{characteristic_uuid}');
                            const descriptors = await characteristic.getDescriptors();
                            return descriptors.map(c => c.uuid);
                        }} catch (e) {{
                            if (e.name === 'NotFoundError') {{
                                return [];
                            }}
                            throw e;
                        }}
                    }})();
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
    await add_characteristic(websocket, context_id, device_address,
                             HEART_RATE_SERVICE_UUID,
                             MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID,
                             {'read': True})

    await simulate_descriptor(websocket, context_id, device_address,
                              HEART_RATE_SERVICE_UUID,
                              MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID,
                              CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID,
                              'add')
    assert await get_descriptors(
        websocket, context_id, HEART_RATE_SERVICE_UUID,
        MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID) == [
            CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID
        ]

    await simulate_descriptor(
        websocket, context_id, device_address, HEART_RATE_SERVICE_UUID,
        MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID,
        CLIENT_CHARACTERISTIC_CONFIGURATION_DESCRIPTOR_UUID, 'add')
    assert sorted(await get_descriptors(
        websocket, context_id, HEART_RATE_SERVICE_UUID,
        MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID)) == sorted([
            CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID,
            CLIENT_CHARACTERISTIC_CONFIGURATION_DESCRIPTOR_UUID
        ])

    await simulate_descriptor(
        websocket, context_id, device_address, HEART_RATE_SERVICE_UUID,
        MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID,
        CLIENT_CHARACTERISTIC_CONFIGURATION_DESCRIPTOR_UUID, 'remove')
    assert await get_descriptors(
        websocket, context_id, HEART_RATE_SERVICE_UUID,
        MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID) == [
            CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID
        ]

    await simulate_descriptor(websocket, context_id, device_address,
                              HEART_RATE_SERVICE_UUID,
                              MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID,
                              CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID,
                              'remove')
    assert await get_descriptors(
        websocket, context_id, HEART_RATE_SERVICE_UUID,
        MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID) == []


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args': ['--enable-features=WebBluetooth']
    }
}],
                         indirect=True)
async def test_bluetooth_add_same_descriptor_uuid_twice(websocket, context_id):
    device_address = await setup_device(websocket, context_id)
    await simulate_service(websocket, context_id, device_address,
                           HEART_RATE_SERVICE_UUID, 'add')
    await add_characteristic(websocket, context_id, device_address,
                             HEART_RATE_SERVICE_UUID,
                             MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID,
                             {'read': True})
    await simulate_descriptor(websocket, context_id, device_address,
                              HEART_RATE_SERVICE_UUID,
                              MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID,
                              CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID,
                              'add')
    with pytest.raises(
            Exception,
            match=str({
                'error': 'invalid argument',
                'message': f'Descriptor with UUID {CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID} already exists'
            })):
        await simulate_descriptor(
            websocket, context_id, device_address, HEART_RATE_SERVICE_UUID,
            MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID,
            CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID, 'add')


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args': ['--enable-features=WebBluetooth']
    }
}],
                         indirect=True)
async def test_bluetooth_remove_unknown_descriptor_uuid(websocket, context_id):
    device_address = await setup_device(websocket, context_id)
    await simulate_service(websocket, context_id, device_address,
                           HEART_RATE_SERVICE_UUID, 'add')
    await add_characteristic(websocket, context_id, device_address,
                             HEART_RATE_SERVICE_UUID,
                             MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID,
                             {'read': True})

    with pytest.raises(
            Exception,
            match=str({
                'error': 'invalid argument',
                'message': (
                    f'Descriptor with UUID {CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID} '
                    f'does not exist for characteristic {MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID} '
                    f'on service {HEART_RATE_SERVICE_UUID} on device {device_address}'
                )
            })):
        await simulate_descriptor(
            websocket, context_id, device_address, HEART_RATE_SERVICE_UUID,
            MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID,
            CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID, 'remove')


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args': ['--enable-features=WebBluetooth']
    }
}],
                         indirect=True)
async def test_bluetooth_add_descriptor_to_unknown_device(
        websocket, context_id):
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
        await simulate_descriptor(
            websocket, context_id, FAKE_DEVICE_ADDRESS,
            HEART_RATE_SERVICE_UUID, MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID,
            CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID, 'add')


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args': ['--enable-features=WebBluetooth']
    }
}],
                         indirect=True)
async def test_bluetooth_add_descriptor_to_unknown_service(
        websocket, context_id):
    device_address = await setup_device(websocket, context_id)
    with pytest.raises(Exception,
                       match=str({
                           'error': 'invalid argument',
                           'message':
                               (f'Service with UUID {HEART_RATE_SERVICE_UUID} '
                                f'on device {device_address} does not exist')
                       })):
        await simulate_descriptor(
            websocket, context_id, device_address, HEART_RATE_SERVICE_UUID,
            MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID,
            CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID, 'add')


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args': ['--enable-features=WebBluetooth']
    }
}],
                         indirect=True)
async def test_bluetooth_add_descriptor_to_unknown_characteristic(
        websocket, context_id):
    device_address = await setup_device(websocket, context_id)
    await simulate_service(websocket, context_id, device_address,
                           HEART_RATE_SERVICE_UUID, 'add')
    with pytest.raises(
            Exception,
            match=str({
                'error': 'invalid argument',
                'message': (
                    f'Characteristic with UUID {MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID} '
                    f'does not exist for service {HEART_RATE_SERVICE_UUID} on device {device_address}'
                )
            })):
        await simulate_descriptor(
            websocket, context_id, device_address, HEART_RATE_SERVICE_UUID,
            MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID,
            CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID, 'add')
