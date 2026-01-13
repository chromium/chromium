#  Copyright 2023 Google LLC.
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

from json import JSONDecoder

import pytest
import pytest_asyncio
from test_helpers import execute_command, goto_url

SOME_HEADER_NAME = 'Some-Header-Name'
SOME_HEADER_VALUE = 'SOME HEADER VALUE'
ANOTHER_HEADER_NAME = 'Another-Header-Name'
ANOTHER_HEADER_VALUE = 'ANOTHER HEADER VALUE'
REFERER_HEADER_NAME = "Referer"
REFERER_HEADER_VALUE = "http://google.com/"


@pytest_asyncio.fixture
async def setup(websocket, url_base):

    async def _setup(context_id):
        await execute_command(
            websocket, {
                "method": "browsingContext.navigate",
                "params": {
                    "url": url_base,
                    "wait": "complete",
                    "context": context_id
                }
            })

    return _setup


@pytest_asyncio.fixture
async def get_headers(websocket, get_url_echo):

    async def _get_headers(context_id, same_origin):
        response = await execute_command(
            websocket, {
                'method': 'script.evaluate',
                'params': {
                    'expression': f"fetch('{get_url_echo(same_origin)}').then(r => r.text())",
                    'awaitPromise': True,
                    'target': {
                        'context': context_id
                    }
                }
            })

        response_obj = JSONDecoder().decode(response['result']['value'])
        return response_obj['headers']

    return _get_headers


@pytest_asyncio.fixture
async def assert_headers(get_headers):

    async def _assert_headers(context_id,
                              expected_headers={},
                              same_origin=True):
        """
        :param expected_headers: value of `None` signals the headers should not
                                 be present.
        """
        headers = await get_headers(context_id, same_origin)
        for key, value in expected_headers.items():
            if value is None:
                assert key not in headers, f"'{key}' header should not be present in the request headers"
            else:
                assert key in headers, f"'{key}' header should be present in the request headers"
                assert headers[
                    key] == value, f"'{key}' header should have value of '{value}'"

    return _assert_headers


@pytest.mark.asyncio
async def test_network_set_extra_headers_global(websocket, context_id, setup,
                                                assert_headers):
    await setup(context_id)
    await execute_command(
        websocket, {
            "method": "network.setExtraHeaders",
            "params": {
                "headers": [{
                    'name': SOME_HEADER_NAME,
                    'value': {
                        'type': 'string',
                        'value': SOME_HEADER_VALUE
                    }
                }],
            }
        })
    await assert_headers(
        context_id, expected_headers={SOME_HEADER_NAME: SOME_HEADER_VALUE})

    await execute_command(
        websocket, {
            "method": "network.setExtraHeaders",
            "params": {
                "headers": [{
                    'name': ANOTHER_HEADER_NAME,
                    'value': {
                        'type': 'string',
                        'value': ANOTHER_HEADER_VALUE
                    }
                }],
            }
        })
    await assert_headers(context_id,
                         expected_headers={
                             ANOTHER_HEADER_NAME: ANOTHER_HEADER_VALUE,
                             SOME_HEADER_NAME: None
                         })

    # Clear headers.
    await execute_command(websocket, {
        "method": "network.setExtraHeaders",
        "params": {
            "headers": [],
        }
    })
    await assert_headers(context_id,
                         expected_headers={
                             ANOTHER_HEADER_NAME: None,
                             SOME_HEADER_NAME: None
                         })


@pytest.mark.asyncio
async def test_network_set_extra_headers_per_context(websocket, context_id,
                                                     another_context_id, setup,
                                                     assert_headers):
    await setup(context_id)
    await setup(another_context_id)
    # Set different headers for different browsing contexts.
    await execute_command(
        websocket, {
            "method": "network.setExtraHeaders",
            "params": {
                "contexts": [context_id],
                "headers": [{
                    'name': SOME_HEADER_NAME,
                    'value': {
                        'type': 'string',
                        'value': SOME_HEADER_VALUE
                    }
                }],
            }
        })
    await assert_headers(context_id,
                         expected_headers={
                             SOME_HEADER_NAME: SOME_HEADER_VALUE,
                             ANOTHER_HEADER_NAME: None
                         })
    await assert_headers(another_context_id,
                         expected_headers={SOME_HEADER_NAME: None})

    await execute_command(
        websocket, {
            "method": "network.setExtraHeaders",
            "params": {
                "contexts": [another_context_id],
                "headers": [{
                    'name': ANOTHER_HEADER_NAME,
                    'value': {
                        'type': 'string',
                        'value': ANOTHER_HEADER_VALUE
                    }
                }],
            }
        })
    await assert_headers(context_id,
                         expected_headers={
                             SOME_HEADER_NAME: SOME_HEADER_VALUE,
                             ANOTHER_HEADER_NAME: None
                         })
    await assert_headers(another_context_id,
                         expected_headers={
                             ANOTHER_HEADER_NAME: ANOTHER_HEADER_VALUE,
                             SOME_HEADER_NAME: None
                         })

    # Clear headers for the first context.
    await execute_command(
        websocket, {
            "method": "network.setExtraHeaders",
            "params": {
                "contexts": [context_id],
                "headers": [],
            }
        })
    await assert_headers(context_id,
                         expected_headers={
                             SOME_HEADER_NAME: None,
                             ANOTHER_HEADER_NAME: None
                         })
    await assert_headers(another_context_id,
                         expected_headers={
                             ANOTHER_HEADER_NAME: ANOTHER_HEADER_VALUE,
                             SOME_HEADER_NAME: None
                         })


@pytest.mark.asyncio
async def test_network_set_extra_headers_per_user_context(
        websocket, user_context_id, create_context, setup, assert_headers):
    # Set different headers for different user contexts.
    await execute_command(
        websocket, {
            "method": "network.setExtraHeaders",
            "params": {
                "userContexts": ["default"],
                "headers": [{
                    'name': SOME_HEADER_NAME,
                    'value': {
                        'type': 'string',
                        'value': SOME_HEADER_VALUE
                    }
                }],
            }
        })
    await execute_command(
        websocket, {
            "method": "network.setExtraHeaders",
            "params": {
                "userContexts": [user_context_id],
                "headers": [{
                    'name': ANOTHER_HEADER_NAME,
                    'value': {
                        'type': 'string',
                        'value': ANOTHER_HEADER_VALUE
                    }
                }],
            }
        })

    # Assert the headers applied for the right contexts.
    browsing_context_id_1 = await create_context()
    await setup(browsing_context_id_1)
    await assert_headers(
        browsing_context_id_1,
        expected_headers={SOME_HEADER_NAME: SOME_HEADER_VALUE})

    browsing_context_id_2 = await create_context(user_context_id)
    await setup(browsing_context_id_2)
    await assert_headers(
        browsing_context_id_2,
        expected_headers={ANOTHER_HEADER_NAME: ANOTHER_HEADER_VALUE})

    # Overwrite headers for the first user context.
    await execute_command(
        websocket, {
            "method": "network.setExtraHeaders",
            "params": {
                "userContexts": ["default"],
                "headers": [{
                    'name': ANOTHER_HEADER_NAME,
                    'value': {
                        'type': 'string',
                        'value': ANOTHER_HEADER_VALUE
                    }
                }],
            }
        })

    await assert_headers(browsing_context_id_1,
                         expected_headers={
                             ANOTHER_HEADER_NAME: ANOTHER_HEADER_VALUE,
                             SOME_HEADER_NAME: None
                         })
    await assert_headers(browsing_context_id_2,
                         expected_headers={
                             ANOTHER_HEADER_NAME: ANOTHER_HEADER_VALUE,
                             SOME_HEADER_NAME: None
                         })

    # Clear headers for the first user context.
    await execute_command(
        websocket, {
            "method": "network.setExtraHeaders",
            "params": {
                "userContexts": ["default"],
                "headers": [],
            }
        })
    await assert_headers(browsing_context_id_1,
                         expected_headers={
                             ANOTHER_HEADER_NAME: None,
                             SOME_HEADER_NAME: None
                         })
    await assert_headers(browsing_context_id_2,
                         expected_headers={
                             ANOTHER_HEADER_NAME: ANOTHER_HEADER_VALUE,
                             SOME_HEADER_NAME: None
                         })


@pytest.mark.asyncio
async def test_network_set_extra_headers_iframe(websocket, context_id,
                                                iframe_id, assert_headers,
                                                html, setup):
    await setup(iframe_id)

    await execute_command(
        websocket, {
            "method": "network.setExtraHeaders",
            "params": {
                "contexts": [context_id],
                "headers": [{
                    'name': SOME_HEADER_NAME,
                    'value': {
                        'type': 'string',
                        'value': SOME_HEADER_VALUE
                    }
                }],
            }
        })

    await assert_headers(iframe_id, {SOME_HEADER_NAME: SOME_HEADER_VALUE})

    # Move iframe out of process.
    await goto_url(websocket, iframe_id, html(same_origin=False))
    await assert_headers(iframe_id, {SOME_HEADER_NAME: SOME_HEADER_VALUE},
                         same_origin=False)

    # Overwrite headers.
    await execute_command(
        websocket, {
            "method": "network.setExtraHeaders",
            "params": {
                "contexts": [context_id],
                "headers": [{
                    'name': ANOTHER_HEADER_NAME,
                    'value': {
                        'type': 'string',
                        'value': ANOTHER_HEADER_VALUE
                    }
                }],
            }
        })
    await assert_headers(iframe_id,
                         expected_headers={
                             ANOTHER_HEADER_NAME: ANOTHER_HEADER_VALUE,
                             SOME_HEADER_NAME: None
                         },
                         same_origin=False)

    # Clear headers.
    await execute_command(
        websocket, {
            "method": "network.setExtraHeaders",
            "params": {
                "contexts": [context_id],
                "headers": [],
            }
        })
    await assert_headers(iframe_id,
                         expected_headers={
                             ANOTHER_HEADER_NAME: None,
                             SOME_HEADER_NAME: None
                         },
                         same_origin=False)


@pytest.mark.asyncio
async def test_network_set_extra_headers_invalid_value(websocket, context_id,
                                                       setup, assert_headers):
    with pytest.raises(
            Exception,
            match=str({
                'error': 'unsupported operation',
                'message': 'Only string headers values are supported'
            })):
        await execute_command(
            websocket, {
                "method": "network.setExtraHeaders",
                "params": {
                    "headers": [{
                        'name': SOME_HEADER_NAME,
                        'value': {
                            'type': 'base64',
                            'value': SOME_HEADER_VALUE
                        }
                    }],
                }
            })


@pytest.mark.asyncio
async def test_network_set_extra_headers_referer_with_fetch(
        websocket, context_id, setup, assert_headers):
    await setup(context_id)
    await execute_command(
        websocket, {
            "method": "network.setExtraHeaders",
            "params": {
                "headers": [{
                    'name': SOME_HEADER_NAME,
                    'value': {
                        'type': 'string',
                        'value': SOME_HEADER_VALUE
                    }
                }, {
                    'name': REFERER_HEADER_NAME,
                    'value': {
                        'type': 'string',
                        'value': REFERER_HEADER_VALUE
                    }
                }],
            }
        })

    await assert_headers(context_id,
                         expected_headers={
                             SOME_HEADER_NAME: SOME_HEADER_VALUE,
                             REFERER_HEADER_NAME: REFERER_HEADER_VALUE
                         })


@pytest.mark.asyncio
async def test_network_set_extra_headers_referer_with_navigation(
        websocket, context_id, url_echo):
    await execute_command(
        websocket, {
            "method": "network.setExtraHeaders",
            "params": {
                "headers": [{
                    'name': SOME_HEADER_NAME,
                    'value': {
                        'type': 'string',
                        'value': SOME_HEADER_VALUE
                    }
                }, {
                    'name': REFERER_HEADER_NAME,
                    'value': {
                        'type': 'string',
                        'value': REFERER_HEADER_VALUE
                    }
                }],
            }
        })

    await goto_url(websocket, context_id, url_echo)

    response = await execute_command(
        websocket, {
            'method': 'script.evaluate',
            'params': {
                'expression': "document.body.innerText",
                'awaitPromise': True,
                'target': {
                    'context': context_id
                }
            }
        })

    assert response['result']['value']
    request_data = JSONDecoder().decode(response['result']['value'])
    headers = request_data['headers']
    assert headers[SOME_HEADER_NAME] == SOME_HEADER_VALUE
    assert headers[REFERER_HEADER_NAME] == REFERER_HEADER_VALUE
