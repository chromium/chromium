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
from uuid import uuid4

import pytest
from test_helpers import execute_command, goto_url


@pytest.fixture
def get_url(url_example):

    def get_url():
        return f"{url_example}?{uuid4()}"

    return get_url


@pytest.fixture
def get_navigator_online(websocket):

    async def get_navigator_online(context_id):
        resp = await execute_command(
            websocket, {
                "method": "script.evaluate",
                "params": {
                    "expression": "navigator.onLine",
                    "target": {
                        "context": context_id
                    },
                    "awaitPromise": True,
                }
            })

        return resp["result"]["value"] if "result" in resp else resp

    return get_navigator_online


@pytest.fixture
def get_can_navigate(websocket, get_url):

    async def can_navigate(context_id):
        try:
            await execute_command(
                websocket, {
                    "method": "browsingContext.navigate",
                    "params": {
                        "url": get_url(),
                        "context": context_id,
                        "wait": "complete",
                    }
                })
            return True
        except Exception as e:
            if str(e) == str({
                    "error": "unknown error",
                    "message": "net::ERR_INTERNET_DISCONNECTED"
            }):
                return False
            raise e

    return can_navigate


@pytest.fixture
def get_can_fetch(websocket, url_echo):

    async def get_can_fetch(context_id):
        resp = await execute_command(
            websocket, {
                'method': 'script.evaluate',
                'params': {
                    'expression': f"fetch('{url_echo}')",
                    'awaitPromise': True,
                    'target': {
                        'context': context_id
                    }
                }
            })
        return resp['type'] == 'success'

    return get_can_fetch


@pytest.fixture
def assert_status(get_navigator_online, get_can_fetch):

    async def assert_status(context_id, expected_status):
        assert expected_status == await get_navigator_online(
            context_id), f"navigator.onLine should be {expected_status}"
        assert expected_status == await get_can_fetch(
            context_id
        ), f"fetch should {'not ' if expected_status else ''}be blocked"

    return assert_status


@pytest.fixture
def prepare_contexts(websocket, get_url):
    """
    Chrome considers the `about:blank` page an offline, so the initial page has
    to be navigated.
    """

    async def prepare_contexts(*context_ids):
        for context_id in context_ids:
            await goto_url(websocket, context_id, get_url())

    return prepare_contexts


@pytest.mark.asyncio
async def test_network_conditions_offline_set_and_clear_per_context(
        websocket, context_id, another_context_id, prepare_contexts,
        assert_status):
    await prepare_contexts(context_id, another_context_id)

    await assert_status(context_id, True)
    await assert_status(another_context_id, True)

    resp = await execute_command(
        websocket, {
            "method": "emulation.setNetworkConditions",
            "params": {
                "networkConditions": {
                    "type": "offline"
                },
                "contexts": [context_id]
            }
        })
    assert resp == {}
    await assert_status(context_id, False)
    await assert_status(another_context_id, True)

    resp = await execute_command(
        websocket, {
            "method": "emulation.setNetworkConditions",
            "params": {
                "networkConditions": None,
                "contexts": [context_id]
            }
        })
    assert resp == {}
    await assert_status(context_id, True)
    await assert_status(another_context_id, True)


@pytest.mark.asyncio
async def test_network_conditions_offline_set_and_clear_per_user_context(
        websocket, user_context_id, create_context, get_can_navigate):
    """
    Assertion navigator.onLine requires navigation, which is not possible
    while offline, so check only possibility of navigation.
    """
    browsing_context_id_1 = await create_context()
    browsing_context_id_2 = await create_context(user_context_id)
    assert await get_can_navigate(browsing_context_id_1)
    assert await get_can_navigate(browsing_context_id_2)

    await execute_command(
        websocket, {
            "method": "emulation.setNetworkConditions",
            "params": {
                "networkConditions": {
                    "type": "offline"
                },
                "userContexts": [user_context_id]
            }
        })

    assert await get_can_navigate(browsing_context_id_1)
    assert not await get_can_navigate(browsing_context_id_2)

    browsing_context_id_3 = await create_context(user_context_id)
    assert not await get_can_navigate(browsing_context_id_3)

    await execute_command(
        websocket, {
            "method": "emulation.setNetworkConditions",
            "params": {
                "networkConditions": {
                    "type": "offline"
                },
                "userContexts": ["default"]
            }
        })
    assert not await get_can_navigate(browsing_context_id_1)
    assert not await get_can_navigate(browsing_context_id_2)
    assert not await get_can_navigate(browsing_context_id_3)

    browsing_context_id_4 = await create_context()
    assert not await get_can_navigate(browsing_context_id_4)

    await execute_command(
        websocket, {
            "method": "emulation.setNetworkConditions",
            "params": {
                "networkConditions": None,
                "userContexts": [user_context_id]
            }
        })
    assert not await get_can_navigate(browsing_context_id_1)
    assert await get_can_navigate(browsing_context_id_2)
    assert await get_can_navigate(browsing_context_id_3)
    assert not await get_can_navigate(browsing_context_id_4)

    await execute_command(
        websocket, {
            "method": "emulation.setNetworkConditions",
            "params": {
                "networkConditions": None,
                "userContexts": ["default"]
            }
        })
    assert await get_can_navigate(browsing_context_id_1)
    assert await get_can_navigate(browsing_context_id_2)
    assert await get_can_navigate(browsing_context_id_3)
    assert await get_can_navigate(browsing_context_id_4)


@pytest.mark.asyncio
async def test_network_conditions_offline_set_and_clear_globally(
        websocket, user_context_id, create_context, get_can_navigate):
    """
    Assertion navigator.onLine requires navigation, which is not possible
    while offline, so check only possibility of navigation.
    """
    browsing_context_id_1 = await create_context()
    browsing_context_id_2 = await create_context(user_context_id)
    assert await get_can_navigate(browsing_context_id_1)
    assert await get_can_navigate(browsing_context_id_2)

    await execute_command(
        websocket, {
            "method": "emulation.setNetworkConditions",
            "params": {
                "networkConditions": {
                    "type": "offline"
                },
            }
        })

    assert not await get_can_navigate(browsing_context_id_1)
    assert not await get_can_navigate(browsing_context_id_2)

    browsing_context_id_3 = await create_context()
    assert not await get_can_navigate(browsing_context_id_3)
    browsing_context_id_4 = await create_context(user_context_id)
    assert not await get_can_navigate(browsing_context_id_4)

    await execute_command(
        websocket, {
            "method": "emulation.setNetworkConditions",
            "params": {
                "networkConditions": None,
            }
        })
    assert await get_can_navigate(browsing_context_id_1)
    assert await get_can_navigate(browsing_context_id_2)
    assert await get_can_navigate(browsing_context_id_3)
    assert await get_can_navigate(browsing_context_id_4)

    browsing_context_id_5 = await create_context()
    assert await get_can_navigate(browsing_context_id_5)
    browsing_context_id_6 = await create_context(user_context_id)
    assert await get_can_navigate(browsing_context_id_6)


@pytest.mark.asyncio
async def test_network_conditions_offline_set_and_clear_with_navigation(
        websocket, context_id, prepare_contexts, get_can_navigate):
    """
    Navigating away from the page while online puts page to an exotic
    `offline` state, so this should be tested separately from the other
    observable effects, like fetch etc.
    """
    await prepare_contexts(context_id)

    assert await get_can_navigate(context_id)

    await execute_command(
        websocket, {
            "method": "emulation.setNetworkConditions",
            "params": {
                "networkConditions": {
                    "type": "offline"
                },
                "contexts": [context_id]
            }
        })
    assert not await get_can_navigate(context_id)

    await execute_command(
        websocket, {
            "method": "emulation.setNetworkConditions",
            "params": {
                "networkConditions": None,
                "contexts": [context_id]
            }
        })
    assert await get_can_navigate(context_id)
