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
import pytest_asyncio
from test_helpers import execute_command, goto_url

TIMEZONES = [
    "Asia/Yekaterinburg", "Europe/Berlin", "America/New_York", "Asia/Tokyo"
]
TIMEZONE_OFFSET = "+10:00"


@pytest_asyncio.fixture
async def default_timezone(websocket, context_id):
    """
    Returns default timezone.
    """
    return await get_timezone(websocket, context_id)


@pytest.fixture
def some_timezone(default_timezone):
    """
    Returns some timezone which is not equal to `default_timezone` nor to
    `another_timezone`.
    """
    for timezone in TIMEZONES:
        if timezone != default_timezone:
            return timezone

    raise Exception(
        f"Unexpectedly could not find timezone different from the default {default_timezone}"
    )


@pytest.fixture
def another_timezone(default_timezone, some_timezone):
    """
    Returns some another timezone which is not equal to `default_timezone` nor
    to `some_timezone`.
    """
    for timezone in TIMEZONES:
        if timezone != default_timezone and timezone != some_timezone:
            return timezone

    raise Exception(
        f"Unexpectedly could not find timezone different from the default {default_timezone} and some {some_timezone}"
    )


async def get_timezone(websocket, context_id):
    """
    Returns browsing context's current timezone.
    """
    resp = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "Intl.DateTimeFormat().resolvedOptions().timeZone",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
            }
        })

    return resp["result"]["value"] if "result" in resp else resp


@pytest.mark.asyncio
async def test_timezone_set_and_clear(websocket, context_id, default_timezone,
                                      some_timezone):
    await execute_command(
        websocket, {
            'method': 'emulation.setTimezoneOverride',
            'params': {
                'contexts': [context_id],
                'timezone': some_timezone
            }
        })

    assert (await get_timezone(websocket, context_id)) == some_timezone

    await execute_command(
        websocket, {
            'method': 'emulation.setTimezoneOverride',
            'params': {
                'contexts': [context_id],
                'timezone': None
            }
        })
    assert (await get_timezone(websocket, context_id)) == default_timezone


@pytest.mark.asyncio
async def test_timezone_per_user_context(websocket, user_context_id,
                                         create_context, some_timezone,
                                         another_timezone):
    # Set different timezone overrides for different user contexts.
    await execute_command(
        websocket, {
            'method': 'emulation.setTimezoneOverride',
            'params': {
                'userContexts': ["default"],
                'timezone': some_timezone
            }
        })
    await execute_command(
        websocket, {
            'method': 'emulation.setTimezoneOverride',
            'params': {
                'userContexts': [user_context_id],
                'timezone': another_timezone
            }
        })

    # Assert the overrides applied for the right contexts.
    browsing_context_id_1 = await create_context()
    emulated_timezone_1 = await get_timezone(websocket, browsing_context_id_1)
    assert emulated_timezone_1 == some_timezone

    browsing_context_id_2 = await create_context(user_context_id)
    emulated_timezone_2 = await get_timezone(websocket, browsing_context_id_2)
    assert emulated_timezone_2 == another_timezone


@pytest.mark.asyncio
async def test_timezone_per_browsing_context(websocket, context_id,
                                             another_context_id,
                                             create_context, some_timezone,
                                             another_timezone):
    # Set different timezone overrides for different user contexts.
    await execute_command(
        websocket, {
            'method': 'emulation.setTimezoneOverride',
            'params': {
                'contexts': [context_id],
                'timezone': some_timezone
            }
        })
    await execute_command(
        websocket, {
            'method': 'emulation.setTimezoneOverride',
            'params': {
                'contexts': [another_context_id],
                'timezone': another_timezone
            }
        })

    assert await get_timezone(websocket, context_id) == some_timezone
    assert await get_timezone(websocket,
                              another_context_id) == another_timezone


@pytest.mark.asyncio
async def test_timezone_iframe(websocket, context_id, iframe_id, html,
                               default_timezone, some_timezone,
                               another_timezone):
    await execute_command(
        websocket, {
            'method': 'emulation.setTimezoneOverride',
            'params': {
                'contexts': [context_id],
                'timezone': some_timezone
            }
        })

    assert await get_timezone(websocket, iframe_id) == some_timezone

    pytest.xfail(
        "TODO: https://github.com/GoogleChromeLabs/chromium-bidi/issues/3532")

    # Move iframe out of process
    await goto_url(websocket, iframe_id,
                   html("<h1>FRAME</h1>", same_origin=False))
    # Assert timezone emulation persisted.
    assert await get_timezone(websocket, iframe_id) == some_timezone

    await execute_command(
        websocket, {
            'method': 'emulation.setTimezoneOverride',
            'params': {
                'contexts': [context_id],
                'timezone': another_timezone
            }
        })
    assert await get_timezone(websocket, iframe_id) == another_timezone

    await execute_command(
        websocket, {
            'method': 'emulation.setTimezoneOverride',
            'params': {
                'contexts': [context_id],
                'timezone': None
            }
        })
    assert await get_timezone(websocket, iframe_id) == default_timezone


@pytest.mark.asyncio
async def test_timezone_invalid(websocket, context_id):
    INVALID_TIMEZONE = "abcd"
    with pytest.raises(Exception,
                       match=str({
                           "error": "invalid argument",
                           "message": f'Invalid timezone "{INVALID_TIMEZONE}"'
                       })):
        await execute_command(
            websocket, {
                'method': 'emulation.setTimezoneOverride',
                'params': {
                    'contexts': [context_id],
                    'timezone': INVALID_TIMEZONE
                }
            })


@pytest.mark.asyncio
async def test_timezone_offset(websocket, context_id, default_timezone):
    await execute_command(
        websocket, {
            'method': 'emulation.setTimezoneOverride',
            'params': {
                'contexts': [context_id],
                'timezone': TIMEZONE_OFFSET
            }
        })

    assert (await get_timezone(websocket, context_id)) == TIMEZONE_OFFSET
