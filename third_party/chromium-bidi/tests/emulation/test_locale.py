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

LOCALES = ["de-DE", "es-ES", "fr-FR", "it-IT"]


@pytest_asyncio.fixture
async def default_locale(websocket, context_id):
    """
    Returns default locale.
    """
    return await get_locale(websocket, context_id)


@pytest.fixture
def some_locale(default_locale):
    """
    Returns some locale which is not equal to `default_locale` nor to
    `another_locale`.
    """
    for locale in LOCALES:
        if locale != default_locale:
            return locale

    raise Exception(
        f"Unexpectedly could not find locale different from the default {default_locale}"
    )


@pytest.fixture
def another_locale(default_locale, some_locale):
    """
    Returns some another locale which is not equal to `default_locale` nor to
    `some_locale`.
    """
    for locale in LOCALES:
        if locale != default_locale and locale != some_locale:
            return locale

    raise Exception(
        f"Unexpectedly could not find locale different from the default {default_locale} and some {some_locale}"
    )


async def get_locale(websocket, context_id):
    """
    Returns browsing context's current locale.
    """
    resp = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "new Intl.DateTimeFormat().resolvedOptions().locale",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
            }
        })

    return resp["result"]["value"] if "result" in resp else resp


@pytest.mark.asyncio
async def test_locale_set_and_clear(websocket, context_id, default_locale,
                                    some_locale):
    await execute_command(
        websocket, {
            'method': 'emulation.setLocaleOverride',
            'params': {
                'contexts': [context_id],
                'locale': some_locale
            }
        })

    assert (await get_locale(websocket, context_id)) == some_locale

    await execute_command(
        websocket, {
            'method': 'emulation.setLocaleOverride',
            'params': {
                'contexts': [context_id],
                'locale': None
            }
        })
    assert (await get_locale(websocket, context_id)) == default_locale


@pytest.mark.asyncio
async def test_locale_per_user_context(websocket, user_context_id,
                                       create_context, some_locale,
                                       another_locale):
    # Set different locale overrides for different user contexts.
    await execute_command(
        websocket, {
            'method': 'emulation.setLocaleOverride',
            'params': {
                'userContexts': ["default"],
                'locale': some_locale
            }
        })
    await execute_command(
        websocket, {
            'method': 'emulation.setLocaleOverride',
            'params': {
                'userContexts': [user_context_id],
                'locale': another_locale
            }
        })

    # Assert the overrides applied for the right contexts.
    browsing_context_id_1 = await create_context()
    emulated_locale_1 = await get_locale(websocket, browsing_context_id_1)
    assert emulated_locale_1 == some_locale

    browsing_context_id_2 = await create_context(user_context_id)
    emulated_locale_2 = await get_locale(websocket, browsing_context_id_2)
    assert emulated_locale_2 == another_locale


@pytest.mark.asyncio
async def test_locale_per_browsing_context(websocket, context_id,
                                           another_context_id, create_context,
                                           some_locale, another_locale):
    # Set different locale overrides for different user contexts.
    await execute_command(
        websocket, {
            'method': 'emulation.setLocaleOverride',
            'params': {
                'contexts': [context_id],
                'locale': some_locale
            }
        })
    await execute_command(
        websocket, {
            'method': 'emulation.setLocaleOverride',
            'params': {
                'contexts': [another_context_id],
                'locale': another_locale
            }
        })

    assert await get_locale(websocket, context_id) == some_locale
    assert await get_locale(websocket, another_context_id) == another_locale


@pytest.mark.asyncio
async def test_locale_iframe(websocket, context_id, iframe_id, html,
                             default_locale, some_locale, another_locale):
    await execute_command(
        websocket, {
            'method': 'emulation.setLocaleOverride',
            'params': {
                'contexts': [context_id],
                'locale': some_locale
            }
        })

    assert await get_locale(websocket, iframe_id) == some_locale

    pytest.xfail(
        "TODO: https://github.com/GoogleChromeLabs/chromium-bidi/issues/3532")

    # Move iframe out of process
    await goto_url(websocket, iframe_id,
                   html("<h1>FRAME</h1>", same_origin=False))
    # Assert locale emulation persisted.
    assert await get_locale(websocket, iframe_id) == some_locale

    await execute_command(
        websocket, {
            'method': 'emulation.setLocaleOverride',
            'params': {
                'contexts': [context_id],
                'locale': another_locale
            }
        })
    assert await get_locale(websocket, iframe_id) == another_locale

    await execute_command(
        websocket, {
            'method': 'emulation.setLocaleOverride',
            'params': {
                'contexts': [context_id],
                'locale': None
            }
        })
    assert await get_locale(websocket, iframe_id) == default_locale


@pytest.mark.asyncio
async def test_locale_invalid(websocket, context_id):
    INVALID_LOCALE = "abcd"
    with pytest.raises(Exception,
                       match=str({
                           "error": "invalid argument",
                           "message": f'Invalid locale "{INVALID_LOCALE}"'
                       })):
        await execute_command(
            websocket, {
                'method': 'emulation.setLocaleOverride',
                'params': {
                    'contexts': [context_id],
                    'locale': INVALID_LOCALE
                }
            })
