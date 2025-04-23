# Copyright 2025 Google LLC.
# Copyright (c) Microsoft Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import pytest
from permissions import set_permission
from test_helpers import execute_command, get_origin, goto_url

SOME_LATITUDE = 1.234
SOME_LONGITUDE = 5.678
SOME_ACCURACY = 9.1011
SOME_ALTITUDE = 12.1314
SOME_ALTITUDE_ACCURACY = 15.1617
SOME_HEADING = 18.192
SOME_SPEED = 21.2223

ANOTHER_LATITUDE = 10.1112
ANOTHER_LONGITUDE = 13.1415
ANOTHER_ACCURACY = 16.1718

SOME_COORDINATES = {
    'latitude': SOME_LATITUDE,
    'longitude': SOME_LONGITUDE,
    'accuracy': SOME_ACCURACY,
    'altitude': SOME_ALTITUDE,
    'altitudeAccuracy': SOME_ALTITUDE_ACCURACY,
    'heading': SOME_HEADING,
    'speed': SOME_SPEED
}

ANOTHER_COORDINATES = {
    'latitude': ANOTHER_LATITUDE,
    'longitude': ANOTHER_LONGITUDE,
    'accuracy': ANOTHER_ACCURACY,
}


async def get_geolocation(websocket, context_id):
    """
    Returns a geolocation, or error if the geolocation is not available in 0.2s.
    """
    resp = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": """
                        new Promise(
                            resolve => window.navigator.geolocation.getCurrentPosition(
                                position => resolve(position.coords.toJSON()),
                                error => resolve({code: error.code, message: error.message}),
                                {timeout: 200}
                        ))
                    """,
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
            }
        })

    return resp["result"] if "result" in resp else resp


@pytest.mark.asyncio
async def test_geolocation_set_and_clear(websocket, context_id, url_example,
                                         snapshot):
    await goto_url(websocket, context_id, url_example)

    await set_permission(websocket, get_origin(url_example),
                         {'name': 'geolocation'}, 'granted')

    initial_geolocation = await get_geolocation(websocket, context_id)

    await execute_command(
        websocket, {
            'method': 'emulation.setGeolocationOverride',
            'params': {
                'contexts': [context_id],
                'coordinates': SOME_COORDINATES
            }
        })

    emulated_geolocation = await get_geolocation(websocket, context_id)

    assert initial_geolocation != emulated_geolocation, "Geolocation should have changed"
    assert emulated_geolocation == snapshot(
    ), "New geolocation should match snapshot"

    await execute_command(
        websocket, {
            'method': 'emulation.setGeolocationOverride',
            'params': {
                'contexts': [context_id],
                'coordinates': ANOTHER_COORDINATES
            }
        })
    emulated_geolocation = await get_geolocation(websocket, context_id)
    assert emulated_geolocation == snapshot(
    ), "New geolocation should match snapshot"

    # Clear geolocation override.
    await execute_command(
        websocket, {
            'method': 'emulation.setGeolocationOverride',
            'params': {
                'contexts': [context_id],
                'coordinates': None
            }
        })

    # Assert the geolocation has returned to the original state.
    assert initial_geolocation == await get_geolocation(websocket, context_id)


@pytest.mark.asyncio
async def test_geolocation_per_user_context(websocket, url_example,
                                            url_example_another_origin,
                                            user_context_id, create_context,
                                            snapshot):
    # `url_example_another_origin` is required as `local_server_http` can
    # be dead-locked in case of concurrent requests.

    await set_permission(websocket, get_origin(url_example),
                         {'name': 'geolocation'}, 'granted', "default")
    await set_permission(websocket, get_origin(url_example_another_origin),
                         {'name': 'geolocation'}, 'granted', user_context_id)

    # Set different geolocation overrides for different user contexts.
    await execute_command(
        websocket, {
            'method': 'emulation.setGeolocationOverride',
            'params': {
                'userContexts': ["default"],
                'coordinates': SOME_COORDINATES
            }
        })
    await execute_command(
        websocket, {
            'method': 'emulation.setGeolocationOverride',
            'params': {
                'userContexts': [user_context_id],
                'coordinates': ANOTHER_COORDINATES
            }
        })

    # Assert the overrides applied for the right contexts.
    browsing_context_id_1 = await create_context()
    await goto_url(websocket, browsing_context_id_1, url_example)
    emulated_geolocation_1 = await get_geolocation(websocket,
                                                   browsing_context_id_1)
    assert emulated_geolocation_1 == snapshot()

    browsing_context_id_2 = await create_context(user_context_id)
    await goto_url(websocket, browsing_context_id_2,
                   url_example_another_origin)
    emulated_geolocation_2 = await get_geolocation(websocket,
                                                   browsing_context_id_2)
    assert emulated_geolocation_2 == snapshot()
