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

from . import (CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID,
               CLIENT_CHARACTERISTIC_CONFIGURATION_DESCRIPTOR_UUID,
               DATE_TIME_CHARACTERISTIC_UUID, FAKE_DEVICE_ADDRESS,
               HEART_RATE_SERVICE_UUID,
               MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID, add_characteristic,
               create_gatt_connection, disable_simulation,
               remove_characteristic, setup_device, setup_granted_device,
               simulate_descriptor, simulate_service)

CHARACTERISTIC_EVENT_GENERATED = 'bluetooth.characteristicEventGenerated'


async def setup_characteristic(websocket, context_id: str, html,
                               service_uuid: str, characteristic_uuid: str,
                               characteristic_properties):
    device_address = await setup_granted_device(websocket, context_id, html,
                                                [service_uuid])
    await create_gatt_connection(websocket, context_id)
    await simulate_service(websocket, context_id, device_address, service_uuid,
                           'add')
    await add_characteristic(websocket, context_id, device_address,
                             service_uuid, characteristic_uuid,
                             characteristic_properties)
    return device_address


async def get_characteristics(websocket, context_id: str,
                              service_uuid: str) -> list[str]:
    response = await execute_command(
        websocket, {
            'method': 'script.evaluate',
            'params': {
                'expression': f'''
                    (async () => {{
                        try {{
                            const service = await device.gatt.getPrimaryService('{service_uuid}');
                            const characteristics = await service.getCharacteristics();
                            return characteristics.map(c => c.uuid)
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


async def get_characteristic_properties(websocket, context_id: str,
                                        service_uuid: str,
                                        characteristic_uuid: str) -> list[str]:
    response = await execute_command(
        websocket, {
            'method': 'script.evaluate',
            'params': {
                'expression': f'''
                    (async () => {{
                        const toPropertyStrings = (properties) => {{
                            let propertyStrings = [];
                            if (properties.broadcast) {{
                                propertyStrings.push('broadcast');
                            }}
                            if (properties.read) {{
                                propertyStrings.push('read');
                            }}
                            if (properties.writeWithoutResponse) {{
                                propertyStrings.push('writeWithoutResponse');
                            }}
                            if (properties.write) {{
                                propertyStrings.push('write');
                            }}
                            if (properties.notify) {{
                                propertyStrings.push('notify');
                            }}
                            if (properties.indicate) {{
                                propertyStrings.push('indicate');
                            }}
                            if (properties.authenticatedSignedWrites) {{
                                propertyStrings.push('authenticatedSignedWrites');
                            }}
                            return propertyStrings;
                        }};
                        try {{
                            const service = await device.gatt.getPrimaryService('{service_uuid}');
                            const characteristic = await service.getCharacteristic('{characteristic_uuid}');
                            return toPropertyStrings(characteristic.properties);
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
@pytest.mark.parametrize('property', [
    'broadcast', 'read', 'writeWithoutResponse', 'write', 'notify', 'indicate',
    'authenticatedSignedWrites'
])
async def test_bluetooth_simulateCharacteristic(websocket, context_id, html,
                                                property):
    device_address = await setup_granted_device(websocket, context_id, html,
                                                [HEART_RATE_SERVICE_UUID])
    await create_gatt_connection(websocket, context_id)
    await simulate_service(websocket, context_id, device_address,
                           HEART_RATE_SERVICE_UUID, 'add')

    await add_characteristic(websocket, context_id, device_address,
                             HEART_RATE_SERVICE_UUID,
                             DATE_TIME_CHARACTERISTIC_UUID, {property: True})
    assert await get_characteristics(
        websocket, context_id,
        HEART_RATE_SERVICE_UUID) == [DATE_TIME_CHARACTERISTIC_UUID]
    assert await get_characteristic_properties(
        websocket, context_id, HEART_RATE_SERVICE_UUID,
        DATE_TIME_CHARACTERISTIC_UUID) == [property]

    await add_characteristic(websocket, context_id, device_address,
                             HEART_RATE_SERVICE_UUID,
                             MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID,
                             {property: True})
    assert sorted(await get_characteristics(
        websocket, context_id, HEART_RATE_SERVICE_UUID)) == sorted([
            DATE_TIME_CHARACTERISTIC_UUID,
            MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID
        ])
    assert await get_characteristic_properties(
        websocket, context_id, HEART_RATE_SERVICE_UUID,
        MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID) == [property]

    await remove_characteristic(websocket, context_id, device_address,
                                HEART_RATE_SERVICE_UUID,
                                MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID)
    assert await get_characteristics(
        websocket, context_id,
        HEART_RATE_SERVICE_UUID) == [DATE_TIME_CHARACTERISTIC_UUID]

    await remove_characteristic(websocket, context_id, device_address,
                                HEART_RATE_SERVICE_UUID,
                                DATE_TIME_CHARACTERISTIC_UUID)
    assert await get_characteristics(websocket, context_id,
                                     HEART_RATE_SERVICE_UUID) == []


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args': ['--enable-features=WebBluetooth']
    }
}],
                         indirect=True)
async def test_bluetooth_add_same_characteristic_uuid_twice(
        websocket, context_id):
    device_address = await setup_device(websocket, context_id)
    await simulate_service(websocket, context_id, device_address,
                           HEART_RATE_SERVICE_UUID, 'add')
    await add_characteristic(websocket, context_id, device_address,
                             HEART_RATE_SERVICE_UUID,
                             DATE_TIME_CHARACTERISTIC_UUID, {'read': True})
    with pytest.raises(
            Exception,
            match=str({
                'error': 'invalid argument',
                'message': f'Characteristic with UUID {DATE_TIME_CHARACTERISTIC_UUID} already exists'
            })):
        await add_characteristic(websocket, context_id, device_address,
                                 HEART_RATE_SERVICE_UUID,
                                 DATE_TIME_CHARACTERISTIC_UUID, {'read': True})


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args': ['--enable-features=WebBluetooth']
    }
}],
                         indirect=True)
async def test_bluetooth_remove_unknown_characteristic_uuid(
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
                    f'does not exist for service {HEART_RATE_SERVICE_UUID} '
                    f'on device {device_address}')
            })):
        await remove_characteristic(websocket, context_id, device_address,
                                    HEART_RATE_SERVICE_UUID,
                                    MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID)


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args': ['--enable-features=WebBluetooth']
    }
}],
                         indirect=True)
async def test_bluetooth_add_characteristic_to_unknown_device(
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
        await add_characteristic(websocket, context_id, FAKE_DEVICE_ADDRESS,
                                 HEART_RATE_SERVICE_UUID,
                                 DATE_TIME_CHARACTERISTIC_UUID, {'read': True})


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args': ['--enable-features=WebBluetooth']
    }
}],
                         indirect=True)
async def test_bluetooth_add_characteristic_to_unknown_service(
        websocket, context_id):
    device_address = await setup_device(websocket, context_id)
    with pytest.raises(Exception,
                       match=str({
                           'error': 'invalid argument',
                           'message':
                               (f'Service with UUID {HEART_RATE_SERVICE_UUID} '
                                f'on device {device_address} does not exist')
                       })):
        await add_characteristic(websocket, context_id, device_address,
                                 HEART_RATE_SERVICE_UUID,
                                 MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID,
                                 {'read': True})


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args': ['--enable-features=WebBluetooth']
    }
}],
                         indirect=True)
async def test_bluetooth_remove_characteristic_uuid_with_properties(
        websocket, context_id):
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
                    'Parameter "characteristicProperties" should not be provided '
                    'for removing a Bluetooth characteristic')
            })):
        await execute_command(
            websocket, {
                'method': 'bluetooth.simulateCharacteristic',
                'params': {
                    'context': context_id,
                    'address': device_address,
                    'serviceUuid': HEART_RATE_SERVICE_UUID,
                    'characteristicUuid': MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID,
                    'characteristicProperties': {
                        'read': True
                    },
                    'type': 'remove'
                }
            })


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args': ['--enable-features=WebBluetooth']
    }
}],
                         indirect=True)
@pytest.mark.parametrize(
    'write_type', [('writeValueWithoutResponse', 'write-without-response'),
                   ('writeValueWithResponse', 'write-with-response')])
async def test_bluetooth_characteristic_write_event(websocket, context_id,
                                                    html, write_type):
    await setup_characteristic(websocket, context_id, html,
                               HEART_RATE_SERVICE_UUID,
                               DATE_TIME_CHARACTERISTIC_UUID, {'write': True})
    await subscribe(websocket, [CHARACTERISTIC_EVENT_GENERATED])
    expected_data = [1]
    await send_JSON_command(
        websocket, {
            'method': 'script.evaluate',
            'params': {
                'expression': f'''
                    (async () => {{
                        const service = await device.gatt.getPrimaryService('{HEART_RATE_SERVICE_UUID}');
                        const characteristic = await service.getCharacteristic('{DATE_TIME_CHARACTERISTIC_UUID}');
                        await characteristic.{write_type[0]}(new Uint8Array({expected_data}));

                    }})();
                ''',
                'awaitPromise': False,
                'target': {
                    'context': context_id,
                },
                'userActivation': True
            }
        })
    event = await wait_for_event(websocket, CHARACTERISTIC_EVENT_GENERATED)
    assert event['params'] == {
        'context': context_id,
        'address': FAKE_DEVICE_ADDRESS,
        'serviceUuid': HEART_RATE_SERVICE_UUID,
        'characteristicUuid': DATE_TIME_CHARACTERISTIC_UUID,
        'type': write_type[1],
        'data': expected_data,
    }

    await execute_command(
        websocket, {
            'method': 'bluetooth.simulateCharacteristicResponse',
            'params': {
                'context': context_id,
                'address': FAKE_DEVICE_ADDRESS,
                'serviceUuid': HEART_RATE_SERVICE_UUID,
                'characteristicUuid': DATE_TIME_CHARACTERISTIC_UUID,
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
async def test_bluetooth_characteristic_read_event(websocket, context_id,
                                                   html):
    await setup_characteristic(websocket, context_id, html,
                               HEART_RATE_SERVICE_UUID,
                               DATE_TIME_CHARACTERISTIC_UUID, {'read': True})
    await subscribe(websocket, [CHARACTERISTIC_EVENT_GENERATED])
    await send_JSON_command(
        websocket, {
            'method': 'script.evaluate',
            'params': {
                'expression': f'''
                    let readResult;
                    (async () => {{
                        const service = await device.gatt.getPrimaryService('{HEART_RATE_SERVICE_UUID}');
                        const characteristic = await service.getCharacteristic('{DATE_TIME_CHARACTERISTIC_UUID}');
                        readResult = await characteristic.readValue();
                    }})();
                ''',
                'awaitPromise': False,
                'target': {
                    'context': context_id,
                },
                'userActivation': True
            }
        })
    event = await wait_for_event(websocket, CHARACTERISTIC_EVENT_GENERATED)
    assert event['params'] == {
        'context': context_id,
        'address': FAKE_DEVICE_ADDRESS,
        'serviceUuid': HEART_RATE_SERVICE_UUID,
        'characteristicUuid': DATE_TIME_CHARACTERISTIC_UUID,
        'type': 'read',
    }

    expected_data = [1, 2]
    await execute_command(
        websocket, {
            'method': 'bluetooth.simulateCharacteristicResponse',
            'params': {
                'context': context_id,
                'address': FAKE_DEVICE_ADDRESS,
                'serviceUuid': HEART_RATE_SERVICE_UUID,
                'characteristicUuid': DATE_TIME_CHARACTERISTIC_UUID,
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


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args': ['--enable-features=WebBluetooth']
    }
}],
                         indirect=True)
async def test_bluetooth_characteristic_notification_event(
        websocket, context_id, html):
    device_address = await setup_characteristic(websocket, context_id, html,
                                                HEART_RATE_SERVICE_UUID,
                                                DATE_TIME_CHARACTERISTIC_UUID,
                                                {'notify': True})
    # The following two descriptors are needed for testing start and stop
    # notification subscription.
    await simulate_descriptor(websocket, context_id, device_address,
                              HEART_RATE_SERVICE_UUID,
                              DATE_TIME_CHARACTERISTIC_UUID,
                              CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID,
                              'add')
    await simulate_descriptor(
        websocket, context_id, device_address, HEART_RATE_SERVICE_UUID,
        DATE_TIME_CHARACTERISTIC_UUID,
        CLIENT_CHARACTERISTIC_CONFIGURATION_DESCRIPTOR_UUID, 'add')

    # Test start notification.
    await subscribe(websocket, [CHARACTERISTIC_EVENT_GENERATED])
    await send_JSON_command(
        websocket, {
            'method': 'script.evaluate',
            'params': {
                'expression': f'''
                    (async () => {{
                        const service = await device.gatt.getPrimaryService('{HEART_RATE_SERVICE_UUID}');
                        const characteristic = await service.getCharacteristic('{DATE_TIME_CHARACTERISTIC_UUID}');
                        await characteristic.startNotifications();

                    }})();
                ''',
                'awaitPromise': False,
                'target': {
                    'context': context_id,
                },
                'userActivation': True
            }
        })
    event = await wait_for_event(websocket, CHARACTERISTIC_EVENT_GENERATED)
    assert event['params'] == {
        'context': context_id,
        'address': FAKE_DEVICE_ADDRESS,
        'serviceUuid': HEART_RATE_SERVICE_UUID,
        'characteristicUuid': DATE_TIME_CHARACTERISTIC_UUID,
        'type': 'subscribe-to-notifications',
    }

    await execute_command(
        websocket, {
            'method': 'bluetooth.simulateCharacteristicResponse',
            'params': {
                'context': context_id,
                'address': FAKE_DEVICE_ADDRESS,
                'serviceUuid': HEART_RATE_SERVICE_UUID,
                'characteristicUuid': DATE_TIME_CHARACTERISTIC_UUID,
                'type': 'subscribe-to-notifications',
                'code': 0x0
            }
        })

    # Test stop notification.
    await subscribe(websocket, [CHARACTERISTIC_EVENT_GENERATED])
    await send_JSON_command(
        websocket, {
            'method': 'script.evaluate',
            'params': {
                'expression': f'''
                    (async () => {{
                        const service = await device.gatt.getPrimaryService('{HEART_RATE_SERVICE_UUID}');
                        const characteristic = await service.getCharacteristic('{DATE_TIME_CHARACTERISTIC_UUID}');
                        await characteristic.stopNotifications();

                    }})();
                ''',
                'awaitPromise': False,
                'target': {
                    'context': context_id,
                },
                'userActivation': True
            }
        })
    event = await wait_for_event(websocket, CHARACTERISTIC_EVENT_GENERATED)
    assert event['params'] == {
        'context': context_id,
        'address': FAKE_DEVICE_ADDRESS,
        'serviceUuid': HEART_RATE_SERVICE_UUID,
        'characteristicUuid': DATE_TIME_CHARACTERISTIC_UUID,
        'type': 'unsubscribe-from-notifications',
    }

    await execute_command(
        websocket, {
            'method': 'bluetooth.simulateCharacteristicResponse',
            'params': {
                'context': context_id,
                'address': FAKE_DEVICE_ADDRESS,
                'serviceUuid': HEART_RATE_SERVICE_UUID,
                'characteristicUuid': DATE_TIME_CHARACTERISTIC_UUID,
                'type': 'unsubscribe-from-notifications',
                'code': 0x0
            }
        })
