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
from datetime import datetime, timedelta

import pytest
from storage import get_bidi_cookie
from test_helpers import AnyExtending, execute_command

SOME_COOKIE_NAME = 'some_cookie_name'
SOME_COOKIE_VALUE = 'some_cookie_value'
ANOTHER_COOKIE_NAME = 'another_cookie_name'
ANOTHER_COOKIE_VALUE = 'another_cookie_value'

SOME_DOMAIN = 'some_domain.com'
OTHER_DOMAIN = 'other_domain.com'
SOME_ORIGIN_WITHOUT_PORT = 'https://some_domain.com'
SOME_ORIGIN = 'https://some_domain.com:1234'
SOME_URL = 'https://some_domain.com:1234/some/path?some=query#some-fragment'


@pytest.mark.asyncio
async def test_cookie_set_required_fields(websocket, context_id):
    resp = await execute_command(
        websocket, {
            'method': 'storage.setCookie',
            'params': {
                'cookie': {
                    'name': SOME_COOKIE_NAME,
                    'value': {
                        'type': 'string',
                        'value': SOME_COOKIE_VALUE
                    },
                    'domain': SOME_DOMAIN,
                }
            }
        })
    assert resp == {'partitionKey': {'userContext': 'default'}}

    resp = await execute_command(websocket, {
        'method': 'storage.getCookies',
        'params': {}
    })
    assert resp == {
        'cookies': [
            AnyExtending(
                get_bidi_cookie(SOME_COOKIE_NAME,
                                SOME_COOKIE_VALUE,
                                SOME_DOMAIN,
                                secure=False))
        ],
        'partitionKey': {
            'userContext': 'default'
        }
    }


@pytest.mark.asyncio
async def test_cookie_set_base64_value(websocket, context_id):
    resp = await execute_command(
        websocket, {
            'method': 'storage.setCookie',
            'params': {
                'cookie': {
                    'name': SOME_COOKIE_NAME,
                    'value': {
                        'type': 'base64',
                        'value': base64.b64encode(
                            SOME_COOKIE_VALUE.encode('ascii')).decode('ascii')
                    },
                    'domain': SOME_DOMAIN,
                }
            }
        })
    assert resp == {'partitionKey': {'userContext': 'default'}}

    resp = await execute_command(websocket, {
        'method': 'storage.getCookies',
        'params': {}
    })
    assert resp == {
        'cookies': [
            AnyExtending(
                get_bidi_cookie(SOME_COOKIE_NAME,
                                SOME_COOKIE_VALUE,
                                SOME_DOMAIN,
                                secure=False))
        ],
        'partitionKey': {
            'userContext': 'default'
        }
    }


@pytest.mark.asyncio
async def test_cookie_set_partition_browsing_context(websocket, context_id):
    resp = await execute_command(
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
                    'domain': SOME_DOMAIN,
                },
                'partition': {
                    'type': 'context',
                    'context': context_id
                }
            }
        })
    assert resp == {'partitionKey': {'userContext': 'default'}}

    resp = await execute_command(websocket, {
        'method': 'storage.getCookies',
        'params': {}
    })
    assert resp == {
        'cookies': [
            AnyExtending(
                get_bidi_cookie(SOME_COOKIE_NAME,
                                SOME_COOKIE_VALUE,
                                SOME_DOMAIN,
                                secure=True))
        ],
        'partitionKey': {
            'userContext': 'default'
        }
    }


@pytest.mark.asyncio
async def test_cookie_set_partition_user_context(websocket, context_id,
                                                 user_context_id):
    user_context_partition = {
        'type': 'storageKey',
        'userContext': user_context_id,
    }
    resp = await execute_command(
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
                    'domain': SOME_DOMAIN,
                },
                'partition': user_context_partition
            }
        })
    assert resp == {'partitionKey': {'userContext': user_context_id}}

    resp = await execute_command(
        websocket, {
            'method': 'storage.getCookies',
            'params': {
                'partition': user_context_partition
            }
        })
    assert resp == {
        'cookies': [
            AnyExtending(
                get_bidi_cookie(SOME_COOKIE_NAME,
                                SOME_COOKIE_VALUE,
                                SOME_DOMAIN,
                                secure=True))
        ],
        'partitionKey': {
            'userContext': user_context_id
        }
    }


@pytest.mark.asyncio
async def test_cookie_set_partition_user_context_unknown(
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
                'method': 'storage.setCookie',
                'params': {
                    'cookie': {
                        'secure': True,
                        'name': SOME_COOKIE_NAME,
                        'value': {
                            'type': 'string',
                            'value': SOME_COOKIE_VALUE
                        },
                        'domain': SOME_DOMAIN,
                    },
                    'partition': user_context_partition
                }
            })


@pytest.mark.asyncio
async def test_cookie_set_partition_browsing_context_from_user_context(
        websocket, create_context, user_context_id):
    context_id = await create_context(user_context_id)
    resp = await execute_command(
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
                    'domain': SOME_DOMAIN,
                },
                'partition': {
                    'type': 'context',
                    'context': context_id,
                }
            }
        })
    assert resp == {'partitionKey': {'userContext': user_context_id, }}

    resp = await execute_command(
        websocket, {
            'method': 'storage.getCookies',
            'params': {
                'partition': {
                    'type': 'storageKey',
                    'userContext': user_context_id,
                }
            }
        })
    assert resp == {
        'cookies': [
            AnyExtending(
                get_bidi_cookie(SOME_COOKIE_NAME,
                                SOME_COOKIE_VALUE,
                                SOME_DOMAIN,
                                secure=True))
        ],
        'partitionKey': {
            'userContext': user_context_id,
        }
    }


@pytest.mark.asyncio
async def test_cookie_set_partition_source_origin(websocket, context_id):
    resp = await execute_command(
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
                    'domain': SOME_DOMAIN,
                },
                'partition': {
                    'type': 'storageKey',
                    'sourceOrigin': SOME_ORIGIN,
                }
            }
        })
    assert resp == {
        'partitionKey': {
            'sourceOrigin': SOME_ORIGIN_WITHOUT_PORT,
            'userContext': 'default'
        }
    }

    resp = await execute_command(websocket, {
        'method': 'storage.getCookies',
        'params': {}
    })
    assert resp == {
        'cookies': [
            AnyExtending(
                get_bidi_cookie(SOME_COOKIE_NAME,
                                SOME_COOKIE_VALUE,
                                SOME_DOMAIN,
                                secure=True))
        ],
        'partitionKey': {
            'userContext': 'default'
        }
    }


@pytest.mark.asyncio
async def test_cookie_set_params_cookie_all_fields(websocket, context_id):
    some_path = "/SOME_PATH"
    http_only = True
    secure = True
    same_site = 'lax'
    expiry = int((datetime.now() + timedelta(hours=1)).timestamp())

    resp = await execute_command(
        websocket, {
            'method': 'storage.setCookie',
            'params': {
                'cookie': {
                    'name': SOME_COOKIE_NAME,
                    'value': {
                        'type': 'string',
                        'value': SOME_COOKIE_VALUE
                    },
                    'domain': SOME_DOMAIN,
                    'path': some_path,
                    'httpOnly': http_only,
                    'secure': secure,
                    'sameSite': same_site,
                    'expiry': expiry,
                },
            }
        })
    assert resp == {'partitionKey': {'userContext': 'default'}}

    resp = await execute_command(websocket, {
        'method': 'storage.getCookies',
        'params': {}
    })
    assert resp == {
        'cookies': [
            AnyExtending(
                get_bidi_cookie(SOME_COOKIE_NAME, SOME_COOKIE_VALUE,
                                SOME_DOMAIN, some_path, http_only, secure,
                                same_site, expiry))
        ],
        'partitionKey': {
            'userContext': 'default'
        }
    }


@pytest.mark.asyncio
async def test_cookie_set_params_cookie_expired(websocket, context_id):
    expiry = int((datetime.now() - timedelta(seconds=1)).timestamp())
    resp = await execute_command(
        websocket, {
            'method': 'storage.setCookie',
            'params': {
                'cookie': get_bidi_cookie(SOME_COOKIE_NAME,
                                          SOME_COOKIE_VALUE,
                                          SOME_DOMAIN,
                                          expiry=expiry),
            }
        })
    assert resp == {'partitionKey': {'userContext': 'default'}}

    resp = await execute_command(websocket, {
        'method': 'storage.getCookies',
        'params': {}
    })
    assert resp == {'cookies': [], 'partitionKey': {'userContext': 'default'}}


@pytest.mark.asyncio
async def test_cookies_set_params_cookie_cdp_specific_fields(
        websocket, context_id):
    resp = await execute_command(
        websocket,
        {
            'method': 'storage.setCookie',
            'params': {
                'cookie': get_bidi_cookie(SOME_COOKIE_NAME,
                                          SOME_COOKIE_VALUE,
                                          SOME_DOMAIN,
                                          secure=True) | {
                                              # CDP-specific fields.
                                              'goog:url': SOME_URL,
                                              'goog:priority': 'High',
                                              'goog:sourceScheme': 'Secure',
                                              'goog:sourcePort': 1234
                                          },
                'partition': {
                    'type': 'storageKey',
                    'sourceOrigin': SOME_ORIGIN,
                }
            }
        })
    assert resp == {
        'partitionKey': {
            'sourceOrigin': SOME_ORIGIN_WITHOUT_PORT,
            'userContext': 'default',
        }
    }

    resp = await execute_command(websocket, {
        'method': 'storage.getCookies',
        'params': {}
    })
    assert resp == {
        'cookies': [
            get_bidi_cookie(
                SOME_COOKIE_NAME, SOME_COOKIE_VALUE, SOME_DOMAIN, secure=True)
            | {
                # CDP-specific fields.
                'goog:partitionKey': {
                    'topLevelSite': SOME_ORIGIN_WITHOUT_PORT,
                    'hasCrossSiteAncestor': False,
                },
                'goog:priority': 'High',
                'goog:session': True,
                'goog:sourcePort': 1234,
                'goog:sourceScheme': 'Secure',
            }
        ],
        'partitionKey': {
            'userContext': 'default'
        }
    }
