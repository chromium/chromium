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

import pytest
from anys import ANY_DICT
from storage import get_bidi_cookie, get_hostname_and_origin, set_cookie
from test_helpers import execute_command, goto_url

SOME_COOKIE_NAME = 'some_cookie_name'
SOME_COOKIE_VALUE = 'some_cookie_value'
ANOTHER_COOKIE_NAME = 'another_cookie_name'
ANOTHER_COOKIE_VALUE = 'another_cookie_value'


@pytest.mark.asyncio
async def test_cookies_get_with_empty_params(websocket, context_id,
                                             example_url):
    await goto_url(websocket, context_id, example_url)

    hostname, origin = get_hostname_and_origin(example_url)
    cookie = get_bidi_cookie(SOME_COOKIE_NAME, SOME_COOKIE_VALUE, hostname)
    await set_cookie(websocket, context_id, cookie)

    res = await execute_command(websocket, {
        'method': 'storage.getCookies',
        'params': {}
    })

    assert res == {
        'cookies': [cookie],
        'partitionKey': {},
    }


@pytest.mark.asyncio
async def test_cookies_get_with_partition_source_origin(
        websocket, context_id, example_url):
    await goto_url(websocket, context_id, example_url)

    hostname, origin = get_hostname_and_origin(example_url)
    cookie = get_bidi_cookie(SOME_COOKIE_NAME, SOME_COOKIE_VALUE, hostname)
    await set_cookie(websocket, context_id, cookie)

    res = await execute_command(
        websocket, {
            'method': 'storage.getCookies',
            'params': {
                'partition': {
                    'type': 'storageKey',
                    'sourceOrigin': origin,
                },
            }
        })

    assert res == {
        'cookies': [cookie],
        'partitionKey': {
            'sourceOrigin': origin,
        },
    }


@pytest.mark.asyncio
async def test_cookies_get_with_unsupported_partition_key(
        websocket, context_id, example_url):
    await goto_url(websocket, context_id, example_url)
    hostname, origin = get_hostname_and_origin(example_url)
    cookie = get_bidi_cookie(SOME_COOKIE_NAME, SOME_COOKIE_VALUE, hostname)
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
                    'sourceOrigin': origin,
                },
            }
        })

    assert res == {
        'cookies': [cookie],
        'partitionKey': {
            'sourceOrigin': origin,
        },
    }


@pytest.mark.asyncio
async def test_cookies_get_with_partition_browsing_context(
        websocket, context_id, example_url):
    await goto_url(websocket, context_id, example_url)

    hostname, origin = get_hostname_and_origin(example_url)
    cookie = get_bidi_cookie(SOME_COOKIE_NAME, SOME_COOKIE_VALUE, hostname)
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
        'cookies': [cookie],
        'partitionKey': {
            'sourceOrigin': origin,
        },
    }


@pytest.mark.asyncio
async def test_cookies_get_with_partition_browsing_context_about_blank(
        websocket, context_id):
    await goto_url(websocket, context_id, 'about:blank')

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
        'cookies': [],
        'partitionKey': {
            'sourceOrigin': 'null',
        },
    }


@pytest.mark.asyncio
async def test_cookies_get_with_filter(websocket, context_id, example_url,
                                       another_example_url):
    await goto_url(websocket, context_id, example_url)

    domain, origin = get_hostname_and_origin(example_url)
    some_cookie = get_bidi_cookie(SOME_COOKIE_NAME, SOME_COOKIE_VALUE, domain)
    await set_cookie(websocket, context_id, some_cookie)

    another_domain, another_origin = get_hostname_and_origin(
        another_example_url)
    another_cookie = get_bidi_cookie(ANOTHER_COOKIE_NAME, ANOTHER_COOKIE_VALUE,
                                     another_domain)
    await set_cookie(websocket, context_id, another_cookie)

    # # Filter by domain.
    await assert_cookie_filter(websocket, context_id, {'domain': domain},
                               some_cookie)
    # # Filter by name.
    await assert_cookie_filter(websocket, context_id,
                               {'name': SOME_COOKIE_NAME}, some_cookie)

    # Filter by value.
    await assert_cookie_filter(
        websocket, context_id,
        {'value': {
            'type': 'string',
            'value': ANOTHER_COOKIE_VALUE
        }}, another_cookie)
    # TODO: test other filters.


async def assert_cookie_filter(websocket, context_id, cookie_filter,
                               expected_cookie):
    resp = await execute_command(
        websocket, {
            'method': 'storage.getCookies',
            'params': {
                'filter': cookie_filter,
                'partition': {
                    'type': 'context',
                    'context': context_id
                }
            }
        })
    assert resp == {'cookies': [expected_cookie], 'partitionKey': ANY_DICT}
