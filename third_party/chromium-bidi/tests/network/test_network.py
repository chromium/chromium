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
#
import json

import pytest
from anys import ANY_DICT, ANY_LIST, ANY_NUMBER, ANY_STR, AnyOr
from test_helpers import (ANY_TIMESTAMP, AnyExtending, execute_command,
                          goto_url, read_JSON_message, send_JSON_command,
                          subscribe, wait_for_event)


def compute_response_headers_size(headers) -> int:
    return sum(
        len(header['name']) + len(header['value']['value']) + 4
        for header in headers)


@pytest.mark.asyncio
async def test_network_before_request_sent_event_navigation(
        websocket, context_id, url_base):
    await subscribe(websocket, ["network.beforeRequestSent"], [context_id])

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_base,
                "wait": "complete",
                "context": context_id
            }
        })

    resp = await wait_for_event(websocket, "network.beforeRequestSent")

    assert resp == AnyExtending({
        'type': 'event',
        "method": "network.beforeRequestSent",
        "params": {
            "isBlocked": False,
            "context": context_id,
            "navigation": ANY_STR,
            "redirectCount": 0,
            "request": {
                "request": ANY_STR,
                "url": url_base,
                "method": "GET",
                "headers": ANY_LIST,
                "cookies": [],
                "headersSize": ANY_NUMBER,
                "bodySize": 0,
                "timings": ANY_DICT,
                "initiatorType": None,
                "destination": "document",
            },
            "initiator": {
                "type": "other"
            },
            "timestamp": ANY_TIMESTAMP
        }
    })


@pytest.mark.asyncio
async def test_network_before_request_sent_event_emitted_with_url_fragment(
        websocket, context_id, url_base):
    await subscribe(websocket, ["network.beforeRequestSent"], [context_id])

    url_fragment = "#test"
    url = f"{url_base}{url_fragment}"

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url,
                "wait": "complete",
                "context": context_id
            }
        })

    resp = await wait_for_event(websocket, "network.beforeRequestSent")

    assert resp == AnyExtending({
        'type': 'event',
        "method": "network.beforeRequestSent",
        "params": {
            "isBlocked": False,
            "context": context_id,
            "navigation": ANY_STR,
            "redirectCount": 0,
            "request": {
                "request": ANY_STR,
                "url": url,
                "method": "GET",
                "headers": ANY_LIST,
                "cookies": [],
                "headersSize": ANY_NUMBER,
                "bodySize": 0,
                "timings": ANY_DICT,
                "initiatorType": None,
                "destination": "document",
            },
            "initiator": {
                "type": "other"
            },
            "timestamp": ANY_TIMESTAMP
        }
    })


@pytest.mark.asyncio
async def test_network_global_subscription_enabled_in_new_context(
        websocket, create_context, url_base):
    await subscribe(websocket, ["network.beforeRequestSent"])

    new_context_id = await create_context()
    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_base,
                "wait": "complete",
                "context": new_context_id
            }
        })

    event = await wait_for_event(websocket, "network.beforeRequestSent")

    assert event == AnyExtending({
        'type': 'event',
        "method": "network.beforeRequestSent",
        "params": {
            "context": new_context_id
        }
    })


@pytest.mark.asyncio
async def test_network_before_request_sent_event_with_cookies_emitted(
        websocket, context_id, url_base, url_example):
    await goto_url(websocket, context_id, url_base)

    await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "document.cookie = 'foo=bar'",
                "target": {
                    "context": context_id,
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })

    await subscribe(websocket, ["network.beforeRequestSent"], [context_id])

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_example,
                "wait": "complete",
                "context": context_id
            }
        })

    resp = await wait_for_event(websocket, "network.beforeRequestSent")
    assert resp == AnyExtending({
        'type': 'event',
        "method": "network.beforeRequestSent",
        "params": {
            "isBlocked": False,
            "context": context_id,
            "navigation": ANY_STR,
            "redirectCount": 0,
            "request": {
                "request": ANY_STR,
                "url": url_example,
                "method": "GET",
                "headers": ANY_LIST,
                "cookies": AnyOr([
                    AnyExtending({
                        "domain": "localhost",
                        "httpOnly": False,
                        "name": "foo",
                        "path": "/",
                        "sameSite": "none",
                        "secure": False,
                        "size": 6,
                        "value": {
                            "type": "string",
                            "value": "bar"
                        },
                    })
                ], []),
                "headersSize": ANY_NUMBER,
                "bodySize": 0,
                "timings": ANY_DICT,
                "initiatorType": None,
                "destination": "document",
            },
            "initiator": {
                "type": "other"
            },
            "timestamp": ANY_TIMESTAMP
        }
    })


@pytest.mark.asyncio
async def test_network_response_completed_event_emitted(
        websocket, context_id, url_base):
    await subscribe(websocket, ["network.responseCompleted"], [context_id])

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_base,
                "wait": "complete",
                "context": context_id
            }
        })

    resp = await wait_for_event(websocket, "network.responseCompleted")
    headers_size = compute_response_headers_size(
        resp["params"]["response"]["headers"])

    assert resp == AnyExtending({
        'type': 'event',
        "method": "network.responseCompleted",
        "params": {
            "isBlocked": False,
            "context": context_id,
            "navigation": ANY_STR,
            "redirectCount": AnyOr(0, 1),
            "request": {
                "request": ANY_STR,
                "url": url_base,
                "method": "GET",
                "headers": ANY_LIST,
                "cookies": [],
                "headersSize": ANY_NUMBER,
                "bodySize": 0,
                "timings": ANY_DICT
            },
            "timestamp": ANY_TIMESTAMP,
            "response": {
                "url": url_base,
                "protocol": AnyOr("http/1.0", "http/1.1", "h2"),
                "status": AnyOr(200, 307),
                "statusText": AnyOr("OK", "", "Temporary Redirect"),
                "fromCache": False,
                "headers": ANY_LIST,
                "mimeType": AnyOr("", "text/html"),
                "bytesReceived": ANY_NUMBER,
                "headersSize": headers_size,
                "bodySize": ANY_NUMBER,
                "content": {
                    "size": ANY_NUMBER
                }
            }
        }
    })


@pytest.mark.asyncio
async def test_network_response_started_event_emitted(websocket, context_id,
                                                      url_base):
    await subscribe(websocket, ["network.responseStarted"], [context_id])

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_base,
                "wait": "complete",
                "context": context_id
            }
        })

    resp = await read_JSON_message(websocket)
    headers_size = compute_response_headers_size(
        resp["params"]["response"]["headers"])

    assert resp == AnyExtending({
        'type': 'event',
        "method": "network.responseStarted",
        "params": {
            "isBlocked": False,
            "context": context_id,
            "navigation": ANY_STR,
            "redirectCount": AnyOr(0, 1),
            "request": {
                "request": ANY_STR,
                "url": url_base,
                "method": "GET",
                "headers": ANY_LIST,
                "cookies": [],
                "headersSize": ANY_NUMBER,
                "bodySize": 0,
                "timings": ANY_DICT
            },
            "timestamp": ANY_TIMESTAMP,
            "response": {
                "url": url_base,
                "protocol": AnyOr("http/1.0", "http/1.1", "h2"),
                "status": AnyOr(200, 307),
                "statusText": AnyOr("OK", "", "Temporary Redirect"),
                "fromCache": False,
                "headers": ANY_LIST,
                "mimeType": AnyOr("", "text/html"),
                "bytesReceived": ANY_NUMBER,
                "headersSize": headers_size,
                "bodySize": ANY_NUMBER,
                "content": {
                    "size": 0
                }
            }
        }
    })


@pytest.mark.asyncio
async def test_network_bad_ssl(websocket, context_id, url_bad_ssl):
    await subscribe(websocket, ["network.fetchError"], [context_id])

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_bad_ssl,
                "wait": "complete",
                "context": context_id
            }
        })

    resp = await read_JSON_message(websocket)
    assert resp == AnyExtending({
        'type': 'event',
        "method": "network.fetchError",
        "params": {
            "isBlocked": False,
            "context": context_id,
            "navigation": ANY_STR,
            "redirectCount": 0,
            "request": {
                "request": ANY_STR,
                "url": url_bad_ssl,
                "method": "GET",
                "headers": ANY_LIST,
                "cookies": [],
                "headersSize": ANY_NUMBER,
                "bodySize": 0,
                "timings": ANY_DICT
            },
            "timestamp": ANY_TIMESTAMP,
            "errorText": "net::ERR_CERT_AUTHORITY_INVALID"
        }
    })


@pytest.mark.asyncio
async def test_network_before_request_sent_event_with_data_url_emitted(
        websocket, context_id):
    await subscribe(websocket, ["network.beforeRequestSent"], [context_id])

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": "data:text/html,hello",
                "wait": "complete",
                "context": context_id
            }
        })
    resp = await read_JSON_message(websocket)
    assert resp == AnyExtending({
        'type': 'event',
        "method": "network.beforeRequestSent",
        "params": {
            "isBlocked": False,
            "context": context_id,
            "navigation": ANY_STR,
            "redirectCount": 0,
            "request": {
                "request": ANY_STR,
                "url": "data:text/html,hello",
                "method": "GET",
                "headers": ANY_LIST,
                "cookies": ANY_LIST,
                "headersSize": 0,
                "bodySize": 0,
                "timings": ANY_DICT,
                "initiatorType": None,
                "destination": "document",
            },
            "initiator": {
                "type": "other"
            },
            "timestamp": ANY_TIMESTAMP
        }
    })


@pytest.mark.asyncio
async def test_network_sends_only_included_cookies(websocket, context_id,
                                                   url_example_another_origin,
                                                   url_secure_context):

    await goto_url(websocket, context_id, url_secure_context)

    await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "document.cookie = 'foo=bar;secure'",
                "target": {
                    "context": context_id,
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })

    await subscribe(websocket, ["network.beforeRequestSent"])

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_example_another_origin,
                "wait": "complete",
                "context": context_id
            }
        })

    response = await wait_for_event(websocket, "network.beforeRequestSent")
    assert response == AnyExtending({
        'type': 'event',
        "method": "network.beforeRequestSent",
        "params": {
            "isBlocked": False,
            "context": context_id,
            "navigation": ANY_STR,
            "redirectCount": 0,
            "request": {
                "request": ANY_STR,
                "url": url_example_another_origin,
                "method": "GET",
                "headers": ANY_LIST,
                "cookies": [],
                "headersSize": ANY_NUMBER,
                "bodySize": 0,
                "timings": ANY_DICT,
                "initiatorType": None,
                "destination": "document",
            },
            "initiator": {
                "type": "other"
            },
            "timestamp": ANY_TIMESTAMP
        }
    })


@pytest.mark.asyncio
async def test_network_should_not_block_queue_shared_workers_with_data_url(
        websocket, context_id, url_base):

    await goto_url(websocket, context_id, url_base)

    await subscribe(websocket, ["network.beforeRequestSent"])

    await send_JSON_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "() => {new SharedWorker('data:text/javascript,console.log(\"hi\")');}",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
            }
        })

    response = await wait_for_event(websocket, 'network.beforeRequestSent')
    assert response == AnyExtending({
        'type': 'event',
        "method": "network.beforeRequestSent",
        "params": {
            "isBlocked": False,
            "context": context_id,
            "navigation": None,
            "redirectCount": 0,
            "request": {
                "request": ANY_STR,
                "url": 'data:text/javascript,console.log("hi")',
                "method": "GET",
                "headers": ANY_LIST,
                "cookies": [],
                "headersSize": ANY_NUMBER,
                "bodySize": 0,
                "timings": ANY_DICT,
                "initiatorType": None,
                "destination": "script",
            },
            "initiator": {
                "type": "other"
            },
            "timestamp": ANY_TIMESTAMP
        }
    })


@pytest.mark.asyncio
async def test_network_preflight(websocket, context_id, html, url_example,
                                 read_messages):
    """ https://github.com/GoogleChromeLabs/chromium-bidi/issues/3570 """
    url = html("hello world!",
               same_origin=False,
               headers={
                   "Access-Control-Allow-Origin": "*",
                   "Access-Control-Allow-Headers": "x-ping"
               })

    await goto_url(websocket, context_id, url)

    await subscribe(websocket, ["network.beforeRequestSent"])

    # Initiate a non-trivial CORS request.
    await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": f"""
                    fetch('{url_example}', {{
                        method: 'PUT',
                    }})
                """,
                "target": {
                    "context": context_id
                },
                "awaitPromise": False,
            }
        })

    [_, event_1, event_2] = await read_messages(3, sort=True)
    preflight_request = event_1 if event_1["params"]["initiator"][
        "type"] == "preflight" else event_2
    post_request = event_1 if preflight_request != event_1 else event_2

    assert preflight_request == AnyExtending({
        'method': 'network.beforeRequestSent',
        'params': {
            'context': context_id,
            'initiator': {
                'request': ANY_STR,
                'type': 'preflight',
            },
            'request': {
                'method': 'OPTIONS',
                'request': ANY_STR,
                'url': url_example,
            },
        },
        'type': 'event',
    })
    initiator_request_id = preflight_request['params']['initiator']['request']

    assert post_request == AnyExtending({
        'method': 'network.beforeRequestSent',
        'params': {
            'context': context_id,
            'request': {
                'initiatorType': 'fetch',
                'method': 'PUT',
                'request': initiator_request_id,
                'url': url_example,
            },
        },
        'type': 'event',
    })


@pytest.mark.asyncio
async def test_network_before_request_sent_event_fetch_post(
        websocket, context_id, url_example, url_echo):

    await goto_url(websocket, context_id, url_example)

    await subscribe(websocket, ["network.beforeRequestSent"], [context_id])

    body = json.dumps({'foo': 'bar'})

    # Initiate a non-trivial CORS request.
    await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": f"""
                    fetch('{url_echo}', {{
                    method: 'POST',
                    body: '{body}',
                    }})
                """,
                "target": {
                    "context": context_id
                },
                "awaitPromise": False,
            }
        })

    resp = await wait_for_event(websocket, "network.beforeRequestSent")

    assert resp == AnyExtending({
        'type': 'event',
        "method": "network.beforeRequestSent",
        "params": {
            "isBlocked": False,
            "context": context_id,
            "navigation": None,
            "redirectCount": 0,
            "request": {
                "request": ANY_STR,
                "url": url_echo,
                "method": "POST",
                "headers": ANY_LIST,
                "cookies": [],
                "headersSize": ANY_NUMBER,
                "bodySize": len(body),
                "timings": ANY_DICT,
                "initiatorType": "fetch",
                "destination": "",
            },
            "initiator": {
                "type": "script"
            },
            "timestamp": ANY_TIMESTAMP
        }
    })


@pytest.mark.asyncio
async def test_network_before_request_sent_event_redirects_with_data_collectors(
        websocket, context_id, url_example, url_permanent_redirect,
        read_messages):

    await execute_command(
        websocket, {
            "method": "network.addDataCollector",
            "params": {
                "dataTypes": ["request", "response"],
                "maxEncodedDataSize": 200_000_000
            }
        })

    await subscribe(websocket, ["network.beforeRequestSent"], [context_id])

    # Init navigation.
    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "context": context_id,
                "url": url_permanent_redirect,
                "wait": "complete",
            }
        })

    messages = await read_messages(2, sort=False)
    # Assert the 2 navigation events are received.
    assert messages == [
        AnyExtending({
            "type": "event",
            "method": "network.beforeRequestSent",
            "params": {
                "context": context_id,
                "redirectCount": 0,
                "request": {
                    "method": "GET",
                    "url": url_permanent_redirect
                },
            },
        }),
        AnyExtending({
            "type": "event",
            "method": "network.beforeRequestSent",
            "params": {
                "context": context_id,
                "redirectCount": 1,
                "request": {
                    "method": "GET",
                    "url": url_example
                },
            },
        })
    ]
