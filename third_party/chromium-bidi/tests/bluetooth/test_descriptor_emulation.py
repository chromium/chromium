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

from . import (BATTERY_SERVICE_UUID,
               CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID,
               CLIENT_CHARACTERISTIC_CONFIGURATION_DESCRIPTOR_UUID,
               FAKE_DEVICE_ADDRESS, HEART_RATE_SERVICE_UUID,
               MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID, add_characteristic,
               create_gatt_connection, disable_simulation, setup_device,
               setup_granted_device, simulate_descriptor, simulate_service)

DESCRIPTOR_EVENT_GENERATED = 'bluetooth.descriptorEventGenerated'


async def setup_descriptor(websocket, context_id: str, html, service_uuid: str,
                           characteristic_uuid: str, characteristic_properties,
                           descriptor_uuid: str):
    device_address = await setup_granted_device(websocket, context_id, html,
                                                [service_uuid])
    await create_gatt_connection(websocket, context_id)
    await simulate_service(websocket, context_id, device_address, service_uuid,
                           'add')
    await add_characteristic(websocket, context_id, device_address,
                             service_uuid, characteristic_uuid,
                             characteristic_properties)
    await simulate_descriptor(websocket, context_id, device_address,
                              service_uuid, characteristic_uuid,
                              descriptor_uuid, 'add')
    return device_address


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


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args': ['--enable-features=WebBluetooth']
    }
}],
                         indirect=True)
async def test_bluetooth_descriptor_write_event(websocket, context_id, html):
    await setup_descriptor(websocket, context_id, html,
                           HEART_RATE_SERVICE_UUID,
                           MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID,
                           {'write': True},
                           CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID)
    await subscribe(websocket, [DESCRIPTOR_EVENT_GENERATED])
    expected_data = [42, 27]
    await send_JSON_command(
        websocket, {
            'method': 'script.evaluate',
            'params': {
                'expression': f'''
                    (async () => {{
                        const service = await device.gatt.getPrimaryService('{HEART_RATE_SERVICE_UUID}');
                        const characteristic = await service.getCharacteristic('{MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID}');
                        const descriptor = await characteristic.getDescriptor('{CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID}');
                        await descriptor.writeValue(new Uint8Array({expected_data}));
                    }})();
                ''',
                'awaitPromise': False,
                'target': {
                    'context': context_id,
                },
                'userActivation': True
            }
        })
    event = await wait_for_event(websocket, DESCRIPTOR_EVENT_GENERATED)
    assert event['params'] == {
        'context': context_id,
        'address': FAKE_DEVICE_ADDRESS,
        'serviceUuid': HEART_RATE_SERVICE_UUID,
        'characteristicUuid': MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID,
        'descriptorUuid': CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID,
        'type': 'write',
        'data': expected_data,
    }

    await execute_command(
        websocket, {
            'method': 'bluetooth.simulateDescriptorResponse',
            'params': {
                'context': context_id,
                'address': FAKE_DEVICE_ADDRESS,
                'serviceUuid': HEART_RATE_SERVICE_UUID,
                'characteristicUuid': MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID,
                'descriptorUuid': CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID,
                'type': 'write',
                'code': 0x0
            }
        })


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args': ['--enable-features=WebBluetooth']
    }
}],
                         indirect=True)
async def test_bluetooth_descriptor_read_event(websocket, context_id, html):
    await setup_descriptor(websocket, context_id, html,
                           HEART_RATE_SERVICE_UUID,
                           MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID,
                           {'read': True},
                           CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID)
    await subscribe(websocket, [DESCRIPTOR_EVENT_GENERATED])
    await send_JSON_command(
        websocket, {
            'method': 'script.evaluate',
            'params': {
                'expression': f'''
                    let readResult;
                    (async () => {{
                        const service = await device.gatt.getPrimaryService('{HEART_RATE_SERVICE_UUID}');
                        const characteristic = await service.getCharacteristic('{MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID}');
                        const descriptor = await characteristic.getDescriptor('{CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID}');
                        readResult = await descriptor.readValue();
                    }})();
                ''',
                'awaitPromise': False,
                'target': {
                    'context': context_id,
                },
                'userActivation': True
            }
        })
    event = await wait_for_event(websocket, DESCRIPTOR_EVENT_GENERATED)
    assert event['params'] == {
        'context': context_id,
        'address': FAKE_DEVICE_ADDRESS,
        'serviceUuid': HEART_RATE_SERVICE_UUID,
        'characteristicUuid': MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID,
        'descriptorUuid': CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID,
        'type': 'read',
    }

    expected_data = [1, 2]
    await execute_command(
        websocket, {
            'method': 'bluetooth.simulateDescriptorResponse',
            'params': {
                'context': context_id,
                'address': FAKE_DEVICE_ADDRESS,
                'serviceUuid': HEART_RATE_SERVICE_UUID,
                'characteristicUuid': MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID,
                'descriptorUuid': CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID,
                'type': 'read',
                'code': 0x0,
                'data': expected_data
            }
        })
    response = await execute_command(
        websocket, {
            'method': 'script.evaluate',
            'params': {
                'expression': 'String.fromCharCode(...new Uint8Array(readResult.buffer))',
                'awaitPromise': False,
                'target': {
                    'context': context_id,
                },
                'userActivation': True
            }
        })
    assert [ord(c) for c in response['result']['value']] == expected_data
