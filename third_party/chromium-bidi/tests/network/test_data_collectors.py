#  Copyright 2025 Google LLC.
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
import pytest_asyncio
from test_helpers import (ANY_UUID, AnyExtending, execute_command, goto_url,
                          send_JSON_command, subscribe, wait_for_event)

SOME_CONTENT = "some response content"
SOME_POST_REQUEST_CONTENT = "some post request content"
NETWORK_RESPONSE_STARTED_EVENT = "network.responseStarted"
SOME_UNKNOWN_COLLECTOR_ID = "SOME_UNKNOWN_COLLECTOR_ID"
MAX_TOTAL_COLLECTED_SIZE = 200_000_000  # Default CDP limit.


@pytest.fixture
def get_url(local_server_http):

    def get_url(content):
        return local_server_http.url_200(
            content,
            content_type="application/octet-stream",
            headers={"Access-Control-Allow-Origin": "*"})

    return get_url


@pytest_asyncio.fixture
async def init_response(websocket, read_messages, get_url):

    async def init_response(context_id, content):
        await subscribe(websocket, NETWORK_RESPONSE_STARTED_EVENT, context_id)
        command_id = await send_JSON_command(
            websocket, {
                "method": "script.callFunction",
                "params": {
                    "functionDeclaration": "(async (url)=>{ return (await fetch(url)).text(); })",
                    "arguments": [{
                        "type": "string",
                        "value": get_url(content)
                    }],
                    "target": {
                        "context": context_id
                    },
                    "awaitPromise": True,
                    "maxDomDepth": None,
                    "resultOwnership": "root"
                }
            })

        [command_result, response_started_event
         ] = await read_messages(2, check_no_other_messages=True)
        request_id = response_started_event["params"]["request"]["request"]

        # Assert the request is unblocked and the script received the response.
        assert command_result == AnyExtending({
            "id": command_id,
            "result": {
                "result": {
                    "type": "string",
                    "value": SOME_CONTENT,
                }
            }
        })

        return request_id

    return init_response


@pytest_asyncio.fixture
async def init_post_request(websocket, read_messages, url_echo):

    async def init_post_request(context_id, post_content):
        await subscribe(websocket, NETWORK_RESPONSE_STARTED_EVENT, context_id)
        command_id = await send_JSON_command(
            websocket, {
                "method": "script.callFunction",
                "params": {
                    "functionDeclaration": """(async (urlEcho, postContent)=>{
                        return (await fetch(urlEcho, {
                            method: 'POST',
                            body: postContent
                        })).text();
                    })""",
                    "arguments": [{
                        "type": "string",
                        "value": url_echo
                    }, {
                        "type": "string",
                        "value": post_content
                    }],
                    "target": {
                        "context": context_id
                    },
                    "awaitPromise": True,
                    "maxDomDepth": None,
                    "resultOwnership": "root"
                }
            })

        [command_result, response_started_event
         ] = await read_messages(2, check_no_other_messages=True)
        request_id = response_started_event["params"]["request"]["request"]

        # Assert the request is unblocked and the script received the response.
        assert command_result == AnyExtending({
            "id": command_id,
            "result": {
                "result": {
                    "type": "string",
                }
            }
        })

        return request_id

    return init_post_request


@pytest.mark.asyncio
async def test_network_collector_get_data_response_required_params(
        websocket, context_id, init_response):
    resp = await execute_command(
        websocket, {
            "method": "network.addDataCollector",
            "params": {
                "dataTypes": ["response"],
                "maxEncodedDataSize": MAX_TOTAL_COLLECTED_SIZE
            }
        })
    assert resp == {"collector": ANY_UUID}

    request_id = await init_response(context_id, SOME_CONTENT)

    # Assert data is collected.
    resp = await execute_command(
        websocket, {
            "method": "network.getData",
            "params": {
                "dataType": "response",
                "request": request_id
            }
        })
    assert resp == {'bytes': {'type': 'string', 'value': SOME_CONTENT}}

    # Assert data is available after collection.
    resp = await execute_command(
        websocket, {
            "method": "network.getData",
            "params": {
                "dataType": "response",
                "request": request_id
            }
        })
    assert resp == {'bytes': {'type': 'string', 'value': SOME_CONTENT}}


@pytest.mark.asyncio
async def test_network_collector_get_data_request_required_params(
        websocket, context_id, init_post_request, html):
    # Navigate to the origin.
    await goto_url(websocket, context_id, html())

    resp = await execute_command(
        websocket, {
            "method": "network.addDataCollector",
            "params": {
                "dataTypes": ["request"],
                "maxEncodedDataSize": MAX_TOTAL_COLLECTED_SIZE
            }
        })
    assert resp == {"collector": ANY_UUID}

    request_id = await init_post_request(context_id, SOME_POST_REQUEST_CONTENT)

    # Assert data is collected.
    resp = await execute_command(
        websocket, {
            "method": "network.getData",
            "params": {
                "dataType": "request",
                "request": request_id
            }
        })
    assert resp == {
        'bytes': {
            'type': 'string',
            'value': SOME_POST_REQUEST_CONTENT
        }
    }

    # Assert data is available after collection.
    resp = await execute_command(
        websocket, {
            "method": "network.getData",
            "params": {
                "dataType": "request",
                "request": request_id
            }
        })
    assert resp == {
        'bytes': {
            'type': 'string',
            'value': SOME_POST_REQUEST_CONTENT
        }
    }


@pytest.mark.asyncio
async def test_network_collector_get_data_response_collector(
        websocket, context_id, init_response):
    resp = await execute_command(
        websocket, {
            "method": "network.addDataCollector",
            "params": {
                "dataTypes": ["response"],
                "maxEncodedDataSize": MAX_TOTAL_COLLECTED_SIZE
            }
        })
    assert resp == {"collector": ANY_UUID}
    collector_id = resp["collector"]

    request_id = await init_response(context_id, SOME_CONTENT)

    # Assert data is collected.
    resp = await execute_command(
        websocket, {
            "method": "network.getData",
            "params": {
                "dataType": "response",
                "request": request_id,
                "collector": collector_id
            }
        })
    assert resp == {'bytes': {'type': 'string', 'value': SOME_CONTENT}}

    # Assert data is still available after collecting.
    resp = await execute_command(
        websocket, {
            "method": "network.getData",
            "params": {
                "dataType": "response",
                "request": request_id,
            }
        })
    assert resp == {'bytes': {'type': 'string', 'value': SOME_CONTENT}}


@pytest.mark.asyncio
async def test_network_collector_get_data_response_unknown_collector(
        websocket, context_id, init_response):
    request_id = await init_response(context_id, SOME_CONTENT)

    with pytest.raises(
            Exception,
            match=str({
                "error": "no such network collector",
                "message": f"Unknown collector {SOME_UNKNOWN_COLLECTOR_ID}"
            })):
        await execute_command(
            websocket, {
                "method": "network.getData",
                "params": {
                    "dataType": "response",
                    "request": request_id,
                    "collector": "SOME_UNKNOWN_COLLECTOR_ID"
                }
            })


@pytest.mark.asyncio
async def test_network_collector_get_data_response_disown_no_collector(
        websocket, context_id, init_response):
    await execute_command(
        websocket, {
            "method": "network.addDataCollector",
            "params": {
                "dataTypes": ["response"],
                "maxEncodedDataSize": MAX_TOTAL_COLLECTED_SIZE
            }
        })

    request_id = await init_response(context_id, SOME_CONTENT)

    with pytest.raises(
            Exception,
            match=str({
                'error': 'invalid argument',
                'message': 'Cannot disown collected data without collector ID'
            })):
        await execute_command(
            websocket, {
                "method": "network.getData",
                "params": {
                    "dataType": "response",
                    "request": request_id,
                    "disown": True
                }
            })


@pytest.mark.asyncio
async def test_network_collector_get_data_response_disown_removes_data(
        websocket, context_id, init_response):
    resp = await execute_command(
        websocket, {
            "method": "network.addDataCollector",
            "params": {
                "dataTypes": ["response"],
                "maxEncodedDataSize": MAX_TOTAL_COLLECTED_SIZE
            }
        })
    assert resp == {"collector": ANY_UUID}
    collector_id = resp["collector"]

    request_id = await init_response(context_id, SOME_CONTENT)

    # Assert data is collected.
    resp = await execute_command(
        websocket, {
            "method": "network.getData",
            "params": {
                "dataType": "response",
                "request": request_id,
                "collector": collector_id,
                "disown": True
            }
        })
    assert resp == {'bytes': {'type': 'string', 'value': SOME_CONTENT}}

    # Assert data is not available anymore.
    with pytest.raises(Exception,
                       match=str({
                           "error": "no such network data",
                           "message": "No collected response data"
                       })):
        await execute_command(
            websocket, {
                "method": "network.getData",
                "params": {
                    "dataType": "response",
                    "request": request_id,
                }
            })


@pytest.mark.asyncio
async def test_network_collector_remove_data_collector(websocket, context_id,
                                                       init_response):
    resp = await execute_command(
        websocket, {
            "method": "network.addDataCollector",
            "params": {
                "dataTypes": ["response"],
                "maxEncodedDataSize": MAX_TOTAL_COLLECTED_SIZE
            }
        })
    assert resp == {"collector": ANY_UUID}
    collector_id = resp["collector"]

    request_id = await init_response(context_id, SOME_CONTENT)

    # Assert data is collected.
    resp = await execute_command(
        websocket, {
            "method": "network.getData",
            "params": {
                "dataType": "response",
                "request": request_id,
            }
        })
    assert resp == {'bytes': {'type': 'string', 'value': SOME_CONTENT}}

    await execute_command(
        websocket, {
            "method": "network.removeDataCollector",
            "params": {
                "collector": collector_id
            }
        })

    # Assert the collector cannot be removed twice.
    with pytest.raises(
            Exception,
            match=str({
                "error": "no such network collector",
                "message": f"Collector {collector_id} does not exist"
            })):
        await execute_command(
            websocket, {
                "method": "network.removeDataCollector",
                "params": {
                    "collector": collector_id
                }
            })

    # Assert the collected data is removed.
    with pytest.raises(Exception,
                       match=str({
                           "error": "no such network data",
                           "message": "No collected response data"
                       })):
        await execute_command(
            websocket, {
                "method": "network.getData",
                "params": {
                    "dataType": "response",
                    "request": request_id,
                }
            })


@pytest.mark.asyncio
async def test_network_collector_disown_data(websocket, context_id,
                                             init_response):
    resp = await execute_command(
        websocket, {
            "method": "network.addDataCollector",
            "params": {
                "dataTypes": ["response"],
                "maxEncodedDataSize": MAX_TOTAL_COLLECTED_SIZE
            }
        })
    assert resp == {"collector": ANY_UUID}
    collector_id = resp["collector"]

    request_id = await init_response(context_id, SOME_CONTENT)

    # Assert data is collected.
    resp = await execute_command(
        websocket, {
            "method": "network.getData",
            "params": {
                "dataType": "response",
                "request": request_id,
            }
        })
    assert resp == {'bytes': {'type': 'string', 'value': SOME_CONTENT}}

    await execute_command(
        websocket, {
            "method": "network.disownData",
            "params": {
                "dataType": "response",
                "collector": collector_id,
                "request": request_id
            }
        })

    # Assert the collected data is not available anymore.
    with pytest.raises(Exception,
                       match=str({
                           "error": "no such network data",
                           "message": "No collected response data"
                       })):
        await execute_command(
            websocket, {
                "method": "network.getData",
                "params": {
                    "dataType": "response",
                    "request": request_id
                }
            })


@pytest.mark.asyncio
async def test_network_collector_scoped_to_context(websocket, context_id,
                                                   another_context_id,
                                                   init_response):
    resp = await execute_command(
        websocket, {
            "method": "network.addDataCollector",
            "params": {
                "dataTypes": ["response"],
                "maxEncodedDataSize": MAX_TOTAL_COLLECTED_SIZE,
                "contexts": [another_context_id]
            }
        })
    assert resp == {"collector": ANY_UUID}

    request_id = await init_response(context_id, SOME_CONTENT)

    # Assert the data is not collected.
    with pytest.raises(Exception,
                       match=str({
                           "error": "no such network data",
                           "message": "No collected response data"
                       })):
        await execute_command(
            websocket, {
                "method": "network.getData",
                "params": {
                    "dataType": "response",
                    "request": request_id
                }
            })


@pytest.mark.asyncio
@pytest.mark.skip(reason="http://b/450771615")
async def test_network_collector_get_data_response_oopif(
        websocket, context_id, html):
    await goto_url(websocket, context_id, html())

    await subscribe(websocket, ['network.responseCompleted'])

    resp = await execute_command(
        websocket, {
            "method": "network.addDataCollector",
            "params": {
                "dataTypes": ["response"],
                "maxEncodedDataSize": MAX_TOTAL_COLLECTED_SIZE
            }
        })
    assert resp == {"collector": ANY_UUID}

    await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": f"""
                    const iframe = document.createElement('iframe');
                    iframe.src = '{html(same_origin=True)}';
                    document.body.appendChild(iframe);
                """,
                "target": {
                    "context": context_id
                },
                "awaitPromise": True
            }
        })

    event = await wait_for_event(websocket, 'network.responseCompleted')
    iframe_id = event['params']['context']
    same_process_request_id = event["params"]["request"]["request"]

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": html(same_origin=False),
                "context": iframe_id,
                "wait": "none"
            }
        })

    event = await wait_for_event(websocket, 'network.responseCompleted')
    another_process_request_id = event["params"]["request"]["request"]

    await execute_command(
        websocket, {
            "method": "network.getData",
            "params": {
                "dataType": "response",
                "request": same_process_request_id
            }
        })

    await execute_command(
        websocket, {
            "method": "network.getData",
            "params": {
                "dataType": "response",
                "request": another_process_request_id
            }
        })
