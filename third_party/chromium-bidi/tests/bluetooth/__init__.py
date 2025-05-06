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
"""Shared utilities and constants for Bluetooth BiDi tests."""

from test_helpers import (execute_command, goto_url, send_JSON_command,
                          subscribe, wait_for_event)

FAKE_DEVICE_ADDRESS = '09:09:09:09:09:09'
FAKE_DEVICE_NAME = 'SomeDevice'


async def setup_device(websocket, context_id: str) -> None:
    """Simulates a powered-on Bluetooth adapter and a preconnected peripheral."""
    await execute_command(
        websocket, {
            'method': 'bluetooth.simulateAdapter',
            'params': {
                'context': context_id,
                'state': 'powered-on',
            }
        })
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


async def request_device(websocket, context_id: str) -> None:
    """Sends the JavaScript `navigator.bluetooth.requestDevice()` command.

    This function should be called after the test has navigated to the target page.
    Once the command completes successfully, the page's JavaScript context
    will have access to the selected Bluetooth device via the `device` variable.
    """
    await send_JSON_command(
        websocket, {
            'method': 'script.evaluate',
            'params': {
                'expression': '''
                    let device;
                    const options = {acceptAllDevices: true};
                    navigator.bluetooth.requestDevice(options).then(d => device = d)
                ''',
                'awaitPromise': True,
                'target': {
                    'context': context_id,
                },
                'userActivation': True
            }
        })


async def setup_granted_device(websocket, context_id: str, html) -> None:
    """Navigates, sets up simulation, and grants device access to the page.

    Combines navigation, device simulation setup, and device request steps.
    The page will have access to the bluetooth device via the `device`
    JavaScript variable after this function completes.
    """
    await goto_url(websocket, context_id, html())
    await setup_device(websocket, context_id)
    await subscribe(websocket, ['bluetooth.requestDevicePromptUpdated'])
    await request_device(websocket, context_id)
    event = await wait_for_event(websocket,
                                 'bluetooth.requestDevicePromptUpdated')
    await execute_command(
        websocket, {
            'method': 'bluetooth.handleRequestDevicePrompt',
            'params': {
                'context': context_id,
                'accept': True,
                'prompt': event['params']['prompt'],
                'device': event['params']['devices'][0]['id']
            }
        })


async def disable_simulation(websocket, context_id: str) -> None:
    """Disables Bluetooth simulation for the given context."""
    await execute_command(
        websocket, {
            'method': 'bluetooth.disableSimulation',
            'params': {
                'context': context_id,
            }
        })
