# Copyright 2026 Google LLC.
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
import json

import pytest
import pytest_asyncio
from test_helpers import execute_command

SOME_CLIENT_HINTS = {
    "brands": [{
        "brand": "TestBrand",
        "version": "99"
    }],
    "fullVersionList": [{
        "brand": "TestBrand",
        "version": "99.0.0.0"
    }],
    "mobile": True,
    "platform": "TestPlatform",
    "platformVersion": "1.0.0",
    "architecture": "TestArch",
    "model": "TestModel",
    "bitness": "64",
    "wow64": False,
    "formFactors": ["Tablet", "Mobile"]
}

ANOTHER_CLIENT_HINTS = {
    "brands": [{
        "brand": "Brand1",
        "version": "1"
    }],
    "fullVersionList": [{
        "brand": "Brand1",
        "version": "1.1.1.1"
    }],
    "mobile": False,
    "platform": "Platform1",
    "platformVersion": "2.0.0",
    "architecture": "Arch1",
    "model": "Model1",
    "bitness": "32",
    "wow64": True,
    "formFactors": ["Desktop"]
}


@pytest_asyncio.fixture
async def initial_client_hints(context_id, get_navigator_client_hints):
    """
    Returns the initial client hints from the browser before any override.
    """
    return await get_navigator_client_hints(context_id)


@pytest_asyncio.fixture
async def get_navigator_client_hints(websocket, local_server_good_ssl):
    """
    Returns client hints from navigator.userAgentData.
    """

    async def _get_navigator_client_hints(context_id):
        # Ensure the context is navigated to a secure context for userAgentData to be available
        url_secure = local_server_good_ssl.url_200()
        await execute_command(
            websocket, {
                "method": "browsingContext.navigate",
                "params": {
                    "context": context_id,
                    "url": url_secure,
                    "wait": "complete"
                }
            })

        result = await execute_command(
            websocket, {
                "method": "script.evaluate",
                "params": {
                    "expression": """
                    (async () => {
                        if (!navigator.userAgentData) return JSON.stringify({ error: 'navigator.userAgentData is undefined' });
                        const highEntropy = await navigator.userAgentData.getHighEntropyValues([
                            'architecture',
                            'bitness',
                            'formFactors',
                            'fullVersionList',
                            'model',
                            'platformVersion',
                            'wow64']);
                        return JSON.stringify({
                            brands: navigator.userAgentData.brands,
                            mobile: navigator.userAgentData.mobile,
                            platform: navigator.userAgentData.platform,
                            architecture: highEntropy.architecture,
                            bitness: highEntropy.bitness,
                            formFactors: highEntropy.formFactors,
                            fullVersionList: highEntropy.fullVersionList,
                            model: highEntropy.model,
                            platformVersion: highEntropy.platformVersion,
                            wow64: highEntropy.wow64
                        });
                    })()
                    """,
                    "target": {
                        "context": context_id
                    },
                    "awaitPromise": True
                }
            })

        navigator_data = json.loads(result['result']['value'])
        if 'error' in navigator_data:
            raise Exception(f"userAgentData error: {navigator_data['error']}")
        return navigator_data

    return _get_navigator_client_hints


@pytest_asyncio.fixture
async def get_network_client_hints(websocket, local_server_good_ssl):
    """
    Returns client hints from network headers.
    """

    async def _get_network_client_hints(context_id):
        url_echo = local_server_good_ssl.url_echo()

        # Navigate to echo page to see headers
        await execute_command(
            websocket, {
                "method": "browsingContext.navigate",
                "params": {
                    "context": context_id,
                    "url": url_echo,
                    "wait": "complete"
                }
            })

        # Extract headers from the echo response
        result = await execute_command(
            websocket, {
                "method": "script.evaluate",
                "params": {
                    "expression": "document.body.innerText",
                    "target": {
                        "context": context_id
                    },
                    "awaitPromise": True
                }
            })

        response_data = json.loads(result['result']['value'])
        headers = {k.lower(): v for k, v in response_data['headers'].items()}

        client_hints = {}

        # Parse Sec-CH-UA
        # Example: "TestBrand";v="99"
        if 'sec-ch-ua' in headers:
            brands = []
            parts = headers['sec-ch-ua'].split(',')
            for part in parts:
                part = part.strip()
                if ';v=' in part:
                    brand, version = part.split(';v=')
                    brand = brand.strip().strip('"')
                    version = version.strip().strip('"')
                    brands.append({'brand': brand, 'version': version})
            client_hints['brands'] = brands

        # Parse Sec-CH-UA-Mobile
        if 'sec-ch-ua-mobile' in headers:
            client_hints['mobile'] = headers['sec-ch-ua-mobile'] == '?1'

        # Parse Sec-CH-UA-Platform
        if 'sec-ch-ua-platform' in headers:
            client_hints['platform'] = headers['sec-ch-ua-platform'].strip('"')

        return client_hints

    return _get_network_client_hints


def get_expected_nerwork_client_hints(client_hints):
    return {
        'brands': client_hints['brands'],
        'mobile': client_hints['mobile'],
        'platform': client_hints['platform'],
    }


@pytest.mark.asyncio
async def test_client_hints_override_global(websocket, context_id,
                                            create_context,
                                            get_navigator_client_hints,
                                            get_network_client_hints,
                                            initial_client_hints):
    # Set global override
    await execute_command(
        websocket, {
            'method': 'emulation.setClientHintsOverride',
            'params': {
                'clientHints': SOME_CLIENT_HINTS
            }
        })

    # Verify via fixtures
    navigator_hints = await get_navigator_client_hints(context_id)
    assert navigator_hints == (SOME_CLIENT_HINTS)

    # Verify Network Headers
    network_hints = await get_network_client_hints(context_id)
    # Filter expected hints to only those we expect in headers (Low Entropy)
    expected_network_hints = get_expected_nerwork_client_hints(
        SOME_CLIENT_HINTS)
    assert network_hints == expected_network_hints

    # Verify new context inherits
    new_context_id = await create_context()
    navigator_hints_new = await get_navigator_client_hints(new_context_id)
    assert navigator_hints_new == (SOME_CLIENT_HINTS)

    network_hints_new = await get_network_client_hints(new_context_id)
    assert network_hints_new == expected_network_hints

    # Clear override
    await execute_command(
        websocket, {
            'method': 'emulation.setClientHintsOverride',
            'params': {
                'clientHints': None
            }
        })

    # Check that override is cleared and matches initial hints
    assert (await
            get_navigator_client_hints(context_id)) == initial_client_hints


@pytest.mark.asyncio
async def test_client_hints_override_per_context(websocket, context_id,
                                                 create_context,
                                                 get_navigator_client_hints,
                                                 get_network_client_hints):
    await execute_command(
        websocket, {
            'method': 'emulation.setClientHintsOverride',
            'params': {
                'clientHints': SOME_CLIENT_HINTS,
                'contexts': [context_id]
            }
        })

    new_context_id = await create_context()
    await execute_command(
        websocket, {
            'method': 'emulation.setClientHintsOverride',
            'params': {
                'clientHints': ANOTHER_CLIENT_HINTS,
                'contexts': [new_context_id]
            }
        })

    # Verify initial context
    navigator_hints_1 = await get_navigator_client_hints(context_id)
    assert navigator_hints_1 == (SOME_CLIENT_HINTS)
    # Check headers for initial context
    network_hints_1 = await get_network_client_hints(context_id)
    expected_network_hints1 = get_expected_nerwork_client_hints(
        SOME_CLIENT_HINTS)
    assert network_hints_1 == expected_network_hints1

    # Verify new context
    navigator_hints_2 = await get_navigator_client_hints(new_context_id)
    assert navigator_hints_2 == (ANOTHER_CLIENT_HINTS)
    # Check headers for new context
    network_hints_2 = await get_network_client_hints(new_context_id)
    expected_network_hints_2 = get_expected_nerwork_client_hints(
        ANOTHER_CLIENT_HINTS)
    assert network_hints_2 == expected_network_hints_2


@pytest.mark.asyncio
async def test_client_hints_override_per_user_context(
        websocket, create_context, create_user_context,
        get_navigator_client_hints, get_network_client_hints, context_id,
        initial_client_hints):
    user_context = await create_user_context()
    context_in_user_context = await create_context(user_context_id=user_context
                                                   )

    # Set override for the user context
    await execute_command(
        websocket, {
            'method': 'emulation.setClientHintsOverride',
            'params': {
                'clientHints': SOME_CLIENT_HINTS,
                'userContexts': [user_context]
            }
        })

    # Verify context in user context has the override
    navigator_hints = await get_navigator_client_hints(context_in_user_context)
    assert navigator_hints == SOME_CLIENT_HINTS

    network_hints = await get_network_client_hints(context_in_user_context)
    expected_network_hints = get_expected_nerwork_client_hints(
        SOME_CLIENT_HINTS)
    assert network_hints == expected_network_hints

    # Verify default context (not in user context) does NOT have the override
    navigator_hints_default = await get_navigator_client_hints(context_id)
    assert navigator_hints_default == initial_client_hints

    # Verify new context in user context inherits override
    new_context_in_user_context = await create_context(
        user_context_id=user_context)
    navigator_hints_new = await get_navigator_client_hints(
        new_context_in_user_context)
    assert navigator_hints_new == SOME_CLIENT_HINTS
