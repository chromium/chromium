#  Copyright 2024 Google LLC.
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
async def test_cookies_delete_params_empty(websocket, context_id):
    source_origin_partition = {
        'type': 'storageKey',
        'sourceOrigin': SOME_URL,
    }

    not_partitioned_cookie = get_bidi_cookie(SOME_COOKIE_NAME,
                                             SOME_COOKIE_VALUE, SOME_DOMAIN)
    await set_cookie(websocket, context_id, not_partitioned_cookie)
    partitioned_cookie = get_bidi_cookie(ANOTHER_COOKIE_NAME,
                                         ANOTHER_COOKIE_VALUE, SOME_DOMAIN)
    await set_cookie(websocket,
                     context_id,
                     partitioned_cookie,
                     partition=source_origin_partition)

    # Delete all cookies.
    resp = await execute_command(websocket, {
        'method': 'storage.deleteCookies',
        'params': {}
    })
    assert resp == {'partitionKey': {'userContext': 'default'}}

    # Expect no cookies to be presented.
    await assert_only_cookies_present(websocket, [])


@pytest.mark.asyncio
async def test_cookies_delete_partition_source_origin(websocket, context_id):
    source_origin_partition = {'type': 'storageKey', 'sourceOrigin': SOME_URL}

    not_partitioned_cookie = get_bidi_cookie(SOME_COOKIE_NAME,
                                             SOME_COOKIE_VALUE, SOME_DOMAIN)
    await set_cookie(websocket, context_id, not_partitioned_cookie)
    partitioned_cookie = get_bidi_cookie(ANOTHER_COOKIE_NAME,
                                         ANOTHER_COOKIE_VALUE, SOME_DOMAIN)
    # Set `partitioned_cookie` as a partitioned with an `origin` source origin.
    await set_cookie(websocket,
                     context_id,
                     partitioned_cookie,
                     partition=source_origin_partition)

    res = await execute_command(
        websocket, {
            'method': 'storage.deleteCookies',
            'params': {
                'partition': source_origin_partition,
            }
        })
    assert res == {
        'partitionKey': {
            # CDP's `partitionKey` does not support port.
            'sourceOrigin': SOME_ORIGIN_WITHOUT_PORT,
            'userContext': 'default'
        },
    }

    # Expect only a not partitioned cookie to be presented.
    await assert_only_cookies_present(websocket, [not_partitioned_cookie])


@pytest.mark.asyncio
async def test_cookies_delete_partition_user_context(websocket, context_id,
                                                     user_context_id):
    user_context_partition = {
        'type': 'storageKey',
        'userContext': user_context_id,
    }

    new_user_context_cookie = get_bidi_cookie(SOME_COOKIE_NAME,
                                              SOME_COOKIE_VALUE, SOME_DOMAIN)
    # Set `cookie` to the new user context.
    await set_cookie(websocket, context_id, new_user_context_cookie,
                     user_context_partition)

    default_user_context_cookie = get_bidi_cookie(ANOTHER_COOKIE_NAME,
                                                  ANOTHER_COOKIE_VALUE,
                                                  SOME_DOMAIN)
    # Set `another_cookie` to default user context.
    await set_cookie(websocket, context_id, default_user_context_cookie)

    res = await execute_command(
        websocket, {
            'method': 'storage.deleteCookies',
            'params': {
                'partition': user_context_partition,
            }
        })
    assert res == {
        'partitionKey': {
            'userContext': user_context_id
        },
    }

    # Expect only a default user context cookie is present.
    await assert_only_cookies_present(websocket, [default_user_context_cookie])


@pytest.mark.asyncio
async def test_cookies_delete_partition_unsupported_key(websocket, context_id):
    cookie = get_bidi_cookie(SOME_COOKIE_NAME, SOME_COOKIE_VALUE, SOME_DOMAIN)
    await set_cookie(websocket, context_id, cookie)

    unknown_partition_key = 'UNKNOWN_PARTITION_KEY'
    unknown_partition_value = 'UNKNOWN_PARTITION_VALUE'

    resp = await execute_command(
        websocket, {
            'method': 'storage.deleteCookies',
            'params': {
                'partition': {
                    'type': 'storageKey',
                    f'{unknown_partition_key}': unknown_partition_value,
                },
            }
        })
    assert resp == {'partitionKey': {'userContext': 'default'}}

    # Expect no cookies to be presented.
    await assert_only_cookies_present(websocket, [])


@pytest.mark.asyncio
async def test_cookies_delete_partition_browsing_context(
        websocket, context_id):
    cookie = get_bidi_cookie(SOME_COOKIE_NAME, SOME_COOKIE_VALUE, SOME_DOMAIN)
    await set_cookie(websocket, context_id, cookie)

    resp = await execute_command(
        websocket, {
            'method': 'storage.deleteCookies',
            'params': {
                'partition': {
                    'type': 'context',
                    'context': context_id
                }
            }
        })

    assert resp == {'partitionKey': {'userContext': 'default'}}

    # Expect no cookies to be presented.
    await assert_only_cookies_present(websocket, [])


@pytest.mark.asyncio
async def test_cookies_delete_partition_browsing_context_from_user_context(
        websocket, create_context, user_context_id):
    context_id = await create_context(user_context_id)

    new_user_context_cookie = get_bidi_cookie(SOME_COOKIE_NAME,
                                              SOME_COOKIE_VALUE, SOME_DOMAIN)
    await set_cookie(websocket, context_id, new_user_context_cookie, {
        'type': 'storageKey',
        'userContext': user_context_id,
    })

    default_user_context_cookie = get_bidi_cookie(ANOTHER_COOKIE_NAME,
                                                  ANOTHER_COOKIE_VALUE,
                                                  SOME_DOMAIN)
    await set_cookie(websocket, context_id, default_user_context_cookie)

    res = await execute_command(
        websocket, {
            'method': 'storage.deleteCookies',
            'params': {
                'partition': {
                    'type': 'context',
                    'context': context_id
                }
            }
        })
    assert res == {'partitionKey': {'userContext': user_context_id}}

    # Expect only a cookie from the default user context to be presented.
    await assert_only_cookies_present(websocket, [default_user_context_cookie])


@pytest.mark.parametrize("cookie_filter", [
    {
        'domain': SOME_DOMAIN
    },
    {
        'name': SOME_COOKIE_NAME
    },
    {
        'value': {
            'type': 'string',
            'value': SOME_COOKIE_VALUE
        }
    },
])
@pytest.mark.asyncio
async def test_cookies_delete_params_filter(websocket, context_id,
                                            cookie_filter):
    some_cookie = get_bidi_cookie(SOME_COOKIE_NAME, SOME_COOKIE_VALUE,
                                  SOME_DOMAIN)
    await set_cookie(websocket, context_id, some_cookie)

    another_cookie = get_bidi_cookie(ANOTHER_COOKIE_NAME, ANOTHER_COOKIE_VALUE,
                                     ANOTHER_DOMAIN)
    await set_cookie(websocket, context_id, another_cookie)

    # Delete cookies by filter.
    resp = await execute_command(websocket, {
        'method': 'storage.deleteCookies',
        'params': {
            'filter': cookie_filter
        }
    })
    assert resp == {'partitionKey': {'userContext': 'default'}}

    await assert_only_cookies_present(websocket, [another_cookie])


async def assert_only_cookies_present(websocket, expected_cookies):
    # Get all cookies.
    resp = await execute_command(websocket, {
        'method': 'storage.getCookies',
        'params': {}
    })
    # Assert only expected cookie is presented.
    assert resp == {
        'cookies': list(
            map(lambda cookie: AnyExtending(cookie), expected_cookies)),
        'partitionKey': ANY_DICT
    }
