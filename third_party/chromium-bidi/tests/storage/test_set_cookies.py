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

from datetime import datetime, timedelta

import pytest
from storage import get_bidi_cookie, get_hostname_and_origin
from test_helpers import execute_command, goto_url

SOME_COOKIE_NAME = 'some_cookie_name'
SOME_COOKIE_VALUE = 'some_cookie_value'
ANOTHER_COOKIE_NAME = 'another_cookie_name'
ANOTHER_COOKIE_VALUE = 'another_cookie_value'


@pytest.mark.asyncio
async def test_cookie_set_with_required_fields(websocket, context_id,
                                               example_url):
    hostname, expected_origin = get_hostname_and_origin(example_url)
    await goto_url(websocket, context_id, example_url)
    await execute_command(
        websocket, {
            'method': 'storage.setCookie',
            'params': {
                'cookie': {
                    'name': SOME_COOKIE_NAME,
                    'value': {
                        'type': 'string',
                        'value': SOME_COOKIE_VALUE
                    },
                    'domain': hostname,
                }
            }
        })

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
        'cookies': [
            get_bidi_cookie(SOME_COOKIE_NAME,
                            SOME_COOKIE_VALUE,
                            hostname,
                            secure=False)
        ],
        'partitionKey': {
            'sourceOrigin': expected_origin,
        }
    }


@pytest.mark.asyncio
async def test_cookie_set_partitioned(websocket, context_id, example_url):
    hostname, expected_origin = get_hostname_and_origin(example_url)
    await goto_url(websocket, context_id, example_url)
    await execute_command(
        websocket, {
            'method': 'storage.setCookie',
            'params': {
                'cookie': {
                    'secure': True,
                    'name': SOME_COOKIE_NAME,
                    'value': {
                        'type': 'string',
                        'value': SOME_COOKIE_VALUE
                    },
                    'domain': hostname,
                },
                'partition': {
                    'type': 'context',
                    'context': context_id
                }
            }
        })

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
        'cookies': [
            get_bidi_cookie(SOME_COOKIE_NAME,
                            SOME_COOKIE_VALUE,
                            hostname,
                            secure=True)
        ],
        'partitionKey': {
            'sourceOrigin': expected_origin,
        }
    }


@pytest.mark.asyncio
async def test_cookie_set_with_all_fields(websocket, context_id, example_url):
    hostname, expected_origin = get_hostname_and_origin(example_url)
    await goto_url(websocket, context_id, example_url)
    some_path = "/SOME_PATH"
    http_only = True
    secure = True
    same_site = 'lax'
    expiry = int((datetime.now() + timedelta(hours=1)).timestamp())
    await execute_command(
        websocket, {
            'method': 'storage.setCookie',
            'params': {
                'cookie': {
                    'name': SOME_COOKIE_NAME,
                    'value': {
                        'type': 'string',
                        'value': SOME_COOKIE_VALUE
                    },
                    'domain': hostname,
                    'path': some_path,
                    'httpOnly': http_only,
                    'secure': secure,
                    'sameSite': same_site,
                    'expiry': expiry,
                },
            }
        })

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
        'cookies': [
            get_bidi_cookie(SOME_COOKIE_NAME, SOME_COOKIE_VALUE, hostname,
                            some_path, http_only, secure, same_site, expiry)
        ],
        'partitionKey': {
            'sourceOrigin': expected_origin,
        }
    }


@pytest.mark.asyncio
async def test_cookie_set_expired(websocket, context_id, example_url):
    hostname, expected_origin = get_hostname_and_origin(example_url)
    await goto_url(websocket, context_id, example_url)
    expiry = int((datetime.now() - timedelta(seconds=1)).timestamp())
    await execute_command(
        websocket, {
            'method': 'storage.setCookie',
            'params': {
                'cookie': get_bidi_cookie(SOME_COOKIE_NAME,
                                          SOME_COOKIE_VALUE,
                                          hostname,
                                          expiry=expiry),
            }
        })

    resp = await execute_command(websocket, {
        'method': 'storage.getCookies',
        'params': {}
    })
    assert resp == {'cookies': [], 'partitionKey': {}}
