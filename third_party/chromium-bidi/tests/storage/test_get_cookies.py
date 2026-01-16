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

import base64

import pytest
from anys import ANY_DICT
from storage import get_bidi_cookie, set_cookie
from test_helpers import AnyExtending, execute_command

SOME_COOKIE_NAME = 'some_cookie_name'
SOME_COOKIE_VALUE = 'some_cookie_value'
ANOTHER_COOKIE_NAME = 'another_cookie_name'
ANOTHER_COOKIE_VALUE = 'another_cookie_value'

SOME_DOMAIN = 'some_domain.com'
SOME_ORIGIN_WITHOUT_PORT = 'https://some_domain.com'
SOME_ORIGIN = 'https://some_domain.com:1234'
SOME_URL = 'https://some_domain.com:1234/some/path?some=query#some-fragment'

ANOTHER_DOMAIN = 'another_domain.com'
ANOTHER_ORIGIN = 'https://another_domain.com:1234'
ANOTHER_URL = 'https://another_domain.com:1234/some/path?some=query#some-fragment'


@pytest.mark.asyncio
async def test_cookies_get_params_empty(websocket, context_id):
    cookie = get_bidi_cookie(SOME_COOKIE_NAME, SOME_COOKIE_VALUE, SOME_DOMAIN)
    await set_cookie(websocket, context_id, cookie)

    res = await execute_command(websocket, {
        'method': 'storage.getCookies',
        'params': {}
    })

    assert res == {
        'cookies': [AnyExtending(cookie)],
        'partitionKey': {
            'userContext': 'default'
        },
    }


@pytest.mark.asyncio
async def test_cookies_get_result_cdp_specific_fields(websocket, context_id):
    cookie = get_bidi_cookie(SOME_COOKIE_NAME, SOME_COOKIE_VALUE, SOME_DOMAIN)
    await set_cookie(websocket, context_id, cookie)

    res = await execute_command(websocket, {
        'method': 'storage.getCookies',
        'params': {}
    })

    assert res == {
        'cookies': [
            cookie | {
                'goog:priority': 'Medium',
                'goog:session': True,
                'goog:sourcePort': 443,
                'goog:sourceScheme': 'Secure',
            }
        ],
        'partitionKey': {
            'userContext': 'default'
        },
    }


@pytest.mark.asyncio
async def test_cookies_get_partition_source_origin(websocket, context_id):
    source_origin_partition = {
        'type': 'storageKey',
        'sourceOrigin': SOME_URL,
    }

    cookie = get_bidi_cookie(SOME_COOKIE_NAME, SOME_COOKIE_VALUE, SOME_DOMAIN)
    await set_cookie(websocket, context_id, cookie)
    another_cookie = get_bidi_cookie(ANOTHER_COOKIE_NAME, ANOTHER_COOKIE_VALUE,
                                     SOME_DOMAIN)
    # Set `another_cookie` as a partitioned with an `origin` source origin.
    await set_cookie(websocket,
                     context_id,
                     another_cookie,
                     partition=source_origin_partition)

    res = await execute_command(
        websocket, {
            'method': 'storage.getCookies',
            'params': {
                'partition': source_origin_partition,
            }
        })

    # Expect only a partitioned cookie to be presented.
    assert res == {
        'cookies': [AnyExtending(another_cookie)],
        'partitionKey': {
            # CDP's `partitionKey` does not support port.
            'sourceOrigin': SOME_ORIGIN_WITHOUT_PORT,
            'userContext': 'default'
        },
    }


@pytest.mark.asyncio
async def test_cookies_get_partition_user_context(websocket, context_id,
                                                  user_context_id):
    user_context_partition = {
        'type': 'storageKey',
        'userContext': user_context_id,
    }

    cookie = get_bidi_cookie(SOME_COOKIE_NAME, SOME_COOKIE_VALUE, SOME_DOMAIN)
    # Set `cookie` to the new user context.
    await set_cookie(websocket, context_id, cookie, user_context_partition)

    another_cookie = get_bidi_cookie(ANOTHER_COOKIE_NAME, ANOTHER_COOKIE_VALUE,
                                     SOME_DOMAIN)
    # Set `another_cookie` to default user context.
    await set_cookie(websocket, context_id, another_cookie)

    res = await execute_command(
        websocket, {
            'method': 'storage.getCookies',
            'params': {
                'partition': user_context_partition,
            }
        })

    # Expect only a cookie from the new user context to be presented.
    assert res == {
        'cookies': [AnyExtending(cookie)],
        'partitionKey': {
            'userContext': user_context_id
        },
    }


@pytest.mark.asyncio
async def test_cookies_get_partition_user_context_unknown(
        websocket, context_id):
    user_context_partition = {
        'type': 'storageKey',
        'userContext': 'UNKNOWN_USER_CONTEXT',
    }

    with pytest.raises(Exception,
                       match=str({
                           'error': 'no such user context',
                           'message': '.*'
                       })):
        await execute_command(
            websocket, {
                'method': 'storage.getCookies',
                'params': {
                    'partition': user_context_partition,
                }
            })


@pytest.mark.asyncio
async def test_cookies_get_partition_unsupported_key(websocket, context_id):
    cookie = get_bidi_cookie(SOME_COOKIE_NAME, SOME_COOKIE_VALUE, SOME_DOMAIN)
    await set_cookie(websocket, context_id, cookie)

    unknown_partition_key = 'UNKNOWN_PARTITION_KEY'
    unknown_partition_value = 'UNKNOWN_PARTITION_VALUE'

    res = await execute_command(
        websocket, {
            'method': 'storage.getCookies',
            'params': {
                'partition': {
                    'type': 'storageKey',
                    f'{unknown_partition_key}': unknown_partition_value,
                },
            }
        })

    assert res == {
        'cookies': [AnyExtending(cookie)],
        'partitionKey': {
            'userContext': 'default'
        },
    }


@pytest.mark.asyncio
async def test_cookies_get_partition_browsing_context(websocket, context_id):
    cookie = get_bidi_cookie(SOME_COOKIE_NAME, SOME_COOKIE_VALUE, SOME_DOMAIN)
    await set_cookie(websocket, context_id, cookie)

    resp = await execute_command(
        websocket, {
            'method': 'storage.getCookies',
            'params': {
                'partition': {
                    'type': 'context',
                    'context': context_id
                }
            }
        })

    assert resp == {
        'cookies': [AnyExtending(cookie)],
        'partitionKey': {
            'userContext': 'default'
        },
    }


@pytest.mark.asyncio
async def test_cookies_get_partition_browsing_context_from_user_context(
        websocket, create_context, user_context_id):
    context_id = await create_context(user_context_id)

    cookie = get_bidi_cookie(SOME_COOKIE_NAME, SOME_COOKIE_VALUE, SOME_DOMAIN)
    # Set `cookie` to the new user context.
    await set_cookie(websocket, context_id, cookie, {
        'type': 'storageKey',
        'userContext': user_context_id,
    })

    another_cookie = get_bidi_cookie(ANOTHER_COOKIE_NAME, ANOTHER_COOKIE_VALUE,
                                     SOME_DOMAIN)
    # Set `another_cookie` to default user context.
    await set_cookie(websocket, context_id, another_cookie)

    res = await execute_command(
        websocket, {
            'method': 'storage.getCookies',
            'params': {
                'partition': {
                    'type': 'context',
                    'context': context_id
                }
            }
        })

    # Expect only a cookie from the new user context to be presented.
    assert res == {
        'cookies': [AnyExtending(cookie)],
        'partitionKey': {
            'userContext': user_context_id
        },
    }


@pytest.mark.asyncio
async def test_cookies_get_params_filter(websocket, context_id):
    some_cookie = get_bidi_cookie(SOME_COOKIE_NAME, SOME_COOKIE_VALUE,
                                  SOME_DOMAIN)
    await set_cookie(websocket, context_id, some_cookie)

    another_cookie = get_bidi_cookie(ANOTHER_COOKIE_NAME, ANOTHER_COOKIE_VALUE,
                                     ANOTHER_DOMAIN)
    await set_cookie(websocket, context_id, another_cookie)

    # # Filter by domain.
    await assert_cookie_filter(websocket, context_id,
                               {'domain': ANOTHER_DOMAIN}, another_cookie)
    # # Filter by name.
    await assert_cookie_filter(websocket, context_id,
                               {'name': SOME_COOKIE_NAME}, some_cookie)

    # Filter by string value.
    await assert_cookie_filter(
        websocket, context_id,
        {'value': {
            'type': 'string',
            'value': ANOTHER_COOKIE_VALUE
        }}, another_cookie)

    # Filter by base64 value.
    await assert_cookie_filter(
        websocket, context_id, {
            'value': {
                'type': 'base64',
                'value': base64.b64encode(ANOTHER_COOKIE_VALUE.encode('ascii')
                                          ).decode('ascii')
            }
        }, another_cookie)

    # TODO: test other filters.


async def assert_cookie_filter(websocket, context_id, cookie_filter,
                               expected_cookie):
    resp = await execute_command(websocket, {
        'method': 'storage.getCookies',
        'params': {
            'filter': cookie_filter
        }
    })
    assert resp == {
        'cookies': [AnyExtending(expected_cookie)],
        'partitionKey': ANY_DICT
    }
