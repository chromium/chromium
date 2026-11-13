#  Copyright 2026 Google LLC.
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

from . import create_gatt_connection, disable_simulation, setup_granted_device


async def simulate_mtu_change(websocket, context_id: str, address: str, mtu: int):
    return await execute_command(
        websocket,
        {
            "method": "bluetooth.simulateGattMtuChange",
            "params": {
                "context": context_id,
                "address": address,
                "mtu": mtu,
            },
        },
    )


@pytest_asyncio.fixture(autouse=True)
async def teardown(websocket, context_id):
    yield
    await disable_simulation(websocket, context_id)


@pytest.mark.asyncio
async def test_bluetooth_simulateGattMtuChange(websocket, context_id, html):
    device_address = await setup_granted_device(websocket, context_id, html)
    await create_gatt_connection(websocket, context_id)

    assert await simulate_mtu_change(websocket, context_id, device_address, 100) == {}


@pytest.mark.asyncio
@pytest.mark.parametrize("mtu", [22, 65536])
async def test_bluetooth_simulateGattMtuChangeInvalidMtu(
    websocket, context_id, html, mtu
):
    device_address = await setup_granted_device(websocket, context_id, html)
    await create_gatt_connection(websocket, context_id)

    with pytest.raises(
        Exception,
        match=str(
            {
                "error": "unknown error",
                "message": f"MTU {mtu} is outside the valid ATT_MTU range",
            }
        ),
    ):
        await simulate_mtu_change(websocket, context_id, device_address, mtu)
