# Copyright 2023 Google LLC.
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
import pytest
from anys import ANY_STR
from test_helpers import (ANY_TIMESTAMP, ANY_UUID, AnyExtending,
                          execute_command, get_tree, goto_url,
                          read_JSON_message, send_JSON_command, subscribe)


@pytest.mark.asyncio
async def test_browsingContext_navigateWaitNone_navigated(
        websocket, context_id, html):
    url = html("<h2>test</h2>")

    await subscribe(
        websocket,
        ["browsingContext.domContentLoaded", "browsingContext.load"])

    await send_JSON_command(
        websocket, {
            "id": 13,
            "method": "browsingContext.navigate",
            "params": {
                "url": url,
                "wait": "none",
                "context": context_id
            }
        })

    # Assert command done.
    resp = await read_JSON_message(websocket)
    navigation_id = resp["result"]["navigation"]
    assert resp == {
        "id": 13,
        "type": "success",
        "result": {
            "navigation": navigation_id,
            "url": url
        }
    }

    # Wait for `browsingContext.domContentLoaded` event.
    resp = await read_JSON_message(websocket)
    assert resp == {
        'type': 'event',
        "method": "browsingContext.domContentLoaded",
        "params": {
            "context": context_id,
            "navigation": navigation_id,
            "timestamp": ANY_TIMESTAMP,
            "url": url
        }
    }

    # Wait for `browsingContext.load` event.
    resp = await read_JSON_message(websocket)
    assert resp == {
        'type': 'event',
        "method": "browsingContext.load",
        "params": {
            "context": context_id,
            "navigation": navigation_id,
            "timestamp": ANY_TIMESTAMP,
            "url": url
        }
    }


@pytest.mark.asyncio
async def test_browsingContext_navigateWaitInteractive_navigated(
        websocket, context_id, html):
    url = html("<h2>test</h2>")

    await subscribe(
        websocket,
        ["browsingContext.domContentLoaded", "browsingContext.load"])

    await send_JSON_command(
        websocket, {
            "id": 14,
            "method": "browsingContext.navigate",
            "params": {
                "url": url,
                "wait": "interactive",
                "context": context_id
            }
        })

    # Wait for `browsingContext.domContentLoaded` event.
    resp = await read_JSON_message(websocket)
    navigation_id = resp["params"]["navigation"]
    assert resp == {
        'type': 'event',
        "method": "browsingContext.domContentLoaded",
        "params": {
            "context": context_id,
            "navigation": navigation_id,
            "timestamp": ANY_TIMESTAMP,
            "url": url,
        }
    }

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id": 14,
        "type": "success",
        "result": {
            "navigation": navigation_id,
            "url": url
        }
    }

    # Wait for `browsingContext.load` event.
    resp = await read_JSON_message(websocket)
    assert resp == {
        'type': 'event',
        "method": "browsingContext.load",
        "params": {
            "context": context_id,
            "navigation": navigation_id,
            "timestamp": ANY_TIMESTAMP,
            "url": url
        }
    }


@pytest.mark.asyncio
async def test_browsingContext_navigateWaitComplete_navigated(
        websocket, context_id, html):
    url = html("<h2>test</h2>")

    await subscribe(
        websocket,
        ["browsingContext.domContentLoaded", "browsingContext.load"])

    await send_JSON_command(
        websocket, {
            "id": 15,
            "method": "browsingContext.navigate",
            "params": {
                "url": url,
                "wait": "complete",
                "context": context_id
            }
        })

    # Wait for `browsingContext.domContentLoaded` event.
    resp = await read_JSON_message(websocket)
    navigation_id = resp["params"]["navigation"]
    assert resp == {
        'type': 'event',
        "method": "browsingContext.domContentLoaded",
        "params": {
            "context": context_id,
            "navigation": navigation_id,
            "timestamp": ANY_TIMESTAMP,
            "url": url
        }
    }

    # Wait for `browsingContext.load` event.
    resp = await read_JSON_message(websocket)
    assert resp == {
        'type': 'event',
        "method": "browsingContext.load",
        "params": {
            "context": context_id,
            "navigation": navigation_id,
            "timestamp": ANY_TIMESTAMP,
            "url": url
        }
    }

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id": 15,
        "type": "success",
        "result": {
            "navigation": navigation_id,
            "url": url
        }
    }


@pytest.mark.asyncio
async def test_browsingContext_navigateSameDocumentNavigation_waitNone_navigated(
        websocket, context_id, html):
    url = html("<h2>test</h2>")
    url_with_hash_1 = url + "#1"
    url_with_hash_2 = url + "#2"

    # Initial navigation.
    await goto_url(websocket, context_id, url, "complete")

    resp = await goto_url(websocket, context_id, url_with_hash_1, "none")
    assert resp == {'navigation': ANY_UUID, 'url': url_with_hash_1}
    navigation_id = resp["navigation"]

    resp = await goto_url(websocket, context_id, url_with_hash_2, "none")
    assert resp == {'navigation': ANY_UUID, 'url': url_with_hash_2}
    # Navigation should be different.
    assert resp["navigation"] != navigation_id


@pytest.mark.asyncio
async def test_browsingContext_navigateSameDocumentNavigation_waitInteractive_navigated(
        websocket, context_id, html):
    url = html("<h2>test</h2>")
    url_with_hash_1 = url + "#1"
    url_with_hash_2 = url + "#2"

    # Initial navigation.
    await goto_url(websocket, context_id, url, "complete")

    resp = await goto_url(websocket, context_id, url_with_hash_1,
                          "interactive")
    assert resp == {'navigation': ANY_UUID, 'url': url_with_hash_1}

    result = await get_tree(websocket, context_id)

    assert {
        "contexts": [{
            "context": context_id,
            "children": [],
            "parent": None,
            "url": url_with_hash_1,
            "userContext": "default",
            "originalOpener": None
        }]
    } == result

    resp = await goto_url(websocket, context_id, url_with_hash_2,
                          "interactive")
    assert resp == {'navigation': ANY_UUID, 'url': url_with_hash_2}

    result = await get_tree(websocket, context_id)

    assert {
        "contexts": [{
            "context": context_id,
            "children": [],
            "parent": None,
            "url": url_with_hash_2,
            "userContext": "default",
            "originalOpener": None
        }]
    } == result


@pytest.mark.asyncio
async def test_browsingContext_navigateSameDocumentNavigation_waitComplete_navigated(
        websocket, context_id, html):
    url = html("<h2>test</h2>")
    url_with_hash_1 = url + "#1"
    url_with_hash_2 = url + "#2"

    # Initial navigation.
    await goto_url(websocket, context_id, url, "complete")

    resp = await goto_url(websocket, context_id, url_with_hash_1, "complete")
    assert resp == {'navigation': ANY_UUID, 'url': url_with_hash_1}

    result = await get_tree(websocket, context_id)

    assert {
        "contexts": [{
            "context": context_id,
            "children": [],
            "parent": None,
            "url": url_with_hash_1,
            "userContext": "default",
            "originalOpener": None
        }]
    } == result

    resp = await goto_url(websocket, context_id, url_with_hash_2, "complete")
    assert resp == {'navigation': ANY_UUID, 'url': url_with_hash_2}

    result = await get_tree(websocket, context_id)

    assert {
        "contexts": [{
            "context": context_id,
            "children": [],
            "parent": None,
            "url": url_with_hash_2,
            "userContext": "default",
            "originalOpener": None
        }]
    } == result


@pytest.mark.asyncio
async def test_navigateToPageWithHash_contextInfoUpdated(
        websocket, context_id, html):
    url = html("<h2>test</h2>")
    url_with_hash_1 = url + "#1"

    # Initial navigation.
    await goto_url(websocket, context_id, url_with_hash_1, "complete")

    result = await get_tree(websocket)

    assert result == {
        "contexts": [{
            "context": context_id,
            "children": [],
            "parent": None,
            "url": url_with_hash_1,
            "userContext": "default",
            "originalOpener": None
        }]
    }


@pytest.mark.asyncio
async def test_browsingContext_navigationStartedEvent_viaScript(
        websocket, context_id, url_base):

    serialized_url = {"type": "string", "value": url_base}

    await subscribe(websocket, ["browsingContext.navigationStarted"])
    await send_JSON_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": """(url) => {
                    location.href = url;
                }""",
                "arguments": [serialized_url],
                "target": {
                    "context": context_id
                },
                "awaitPromise": True
            }
        })

    response = await read_JSON_message(websocket)
    assert response == {
        'type': 'event',
        "method": "browsingContext.navigationStarted",
        "params": {
            "context": context_id,
            "navigation": ANY_UUID,
            "timestamp": ANY_TIMESTAMP,
            # TODO: Should report correct string
            "url": ANY_STR,
        }
    }


@pytest.mark.asyncio
async def test_browsingContext_navigationStartedEvent_iframe_viaCommand(
        websocket, context_id, url_base, html, iframe):
    await subscribe(websocket, ["browsingContext.navigationStarted"])
    iframe_url = html("<h1>FRAME</h1>")
    page_url = html(iframe(iframe_url))
    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "context": context_id,
                "url": page_url,
                "wait": "complete",
            }
        })

    response = await read_JSON_message(websocket)
    assert response == {
        'type': 'event',
        "method": "browsingContext.navigationStarted",
        "params": {
            "context": context_id,
            "navigation": ANY_UUID,
            "timestamp": ANY_TIMESTAMP,
            "url": page_url,
        }
    }

    response = await read_JSON_message(websocket)
    assert response == {
        'type': 'event',
        "method": "browsingContext.navigationStarted",
        "params": {
            "context": ANY_STR,
            "navigation": ANY_UUID,
            "timestamp": ANY_TIMESTAMP,
            "url": iframe_url,
        }
    }


@pytest.mark.asyncio
async def test_browsingContext_navigationStartedEvent_iframe_viaScript(
        websocket, context_id, url_base, html, iframe):
    await subscribe(websocket, ["browsingContext.navigationStarted"])
    iframe_url = html("<h1>FRAME</h1>")
    page_url = html(iframe(iframe_url))

    command_id = await send_JSON_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": """(url) => {
                    location.href = url;
                }""",
                "arguments": [{
                    "type": "string",
                    "value": page_url
                }],
                "target": {
                    "context": context_id
                },
                "awaitPromise": True
            }
        })

    response = await read_JSON_message(websocket)
    assert response == {
        'type': 'event',
        "method": "browsingContext.navigationStarted",
        "params": {
            "context": context_id,
            "navigation": ANY_UUID,
            "timestamp": ANY_TIMESTAMP,
            "url": page_url,
        }
    }

    response = await read_JSON_message(websocket)
    assert response == AnyExtending({"id": command_id, "type": "success"})

    response = await read_JSON_message(websocket)
    assert response == {
        'type': 'event',
        "method": "browsingContext.navigationStarted",
        "params": {
            "context": ANY_STR,
            "navigation": ANY_UUID,
            "timestamp": ANY_TIMESTAMP,
            "url": iframe_url,
        }
    }


@pytest.mark.asyncio
async def test_browsingContext_navigationStartedEvent_viaCommand(
        websocket, context_id, html):
    url = html()

    await subscribe(websocket, ["browsingContext.navigationStarted"])

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "context": context_id,
                "url": url,
                "wait": "complete",
            }
        })

    response = await read_JSON_message(websocket)
    assert response == {
        'type': 'event',
        "method": "browsingContext.navigationStarted",
        "params": {
            "context": context_id,
            "navigation": ANY_UUID,
            "timestamp": ANY_TIMESTAMP,
            # TODO: Should report correct string
            "url": ANY_STR,
        }
    }


@pytest.mark.asyncio
async def test_browsingContext_navigationStarted_browsingContextClosedBeforeNavigationEnded_navigationFailed(
        websocket, context_id, read_sorted_messages, url_hang_forever):
    navigate_command_id = await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "context": context_id,
                "url": url_hang_forever,
                "wait": "complete",
            }
        })

    close_command_id = await send_JSON_command(websocket, {
        "method": "browsingContext.close",
        "params": {
            "context": context_id
        }
    })

    # Command result order is not guaranteed.
    [navigation_command_result,
     close_command_result] = await read_sorted_messages(2)

    assert navigation_command_result == AnyExtending({
        'id': navigate_command_id,
        'type': 'error',
        'error': 'unknown error',
        'message': 'navigation canceled by context disposal',
    })

    assert close_command_result == AnyExtending({
        'id': close_command_id,
        'type': 'success',
    })


@pytest.mark.asyncio
async def test_browsingContext_navigationStarted_sameDocumentNavigation(
        websocket, context_id, url_base):
    await subscribe(
        websocket,
        ["browsingContext.navigationStarted", "browsingContext.load"])

    # Make an initial navigation.
    command_id = await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_base,
                "context": context_id,
                "wait": "none"
            }
        })

    # Assert that the navigation command was finished.
    response = await read_JSON_message(websocket)
    assert response == AnyExtending({
        'id': command_id,
        'result': {
            'navigation': ANY_UUID,
            'url': url_base
        }
    })
    navigation_id = response["result"]["navigation"]

    # Assert that the navigation started event was received with the correct
    # navigation id.
    response = await read_JSON_message(websocket)
    assert response == {
        'type': 'event',
        "method": "browsingContext.navigationStarted",
        "params": {
            "context": context_id,
            "navigation": navigation_id,
            "timestamp": ANY_TIMESTAMP,
            "url": url_base,
        }
    }

    # Assert that the page is loaded.
    response = await read_JSON_message(websocket)
    assert response == AnyExtending({
        'method': 'browsingContext.load',
        'params': {
            'context': context_id,
            'navigation': navigation_id,
        },
        'type': 'event',
    })

    # Make same-document navigation.
    await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "location.href = '#test';",
                "target": {
                    "context": context_id,
                },
                "awaitPromise": False
            }
        })

    response = await read_JSON_message(websocket)
    assert response == AnyExtending({
        'type': 'event',
        "method": "browsingContext.navigationStarted",
        "params": {
            "context": context_id,
            "navigation": ANY_UUID,
            "timestamp": ANY_TIMESTAMP,
            "url": url_base + "#test",
        }
    })

    new_navigation_id = response["params"]["navigation"]
    assert new_navigation_id != navigation_id


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{}, {
    'acceptInsecureCerts': True
}, {
    'acceptInsecureCerts': False
}],
                         indirect=True)
async def test_browsingContext_acceptInsecureCertsCapability_respected(
        websocket, context_id, url_bad_ssl, capabilities):
    async def navigate():
        await execute_command(
            websocket, {
                'method': "browsingContext.navigate",
                'params': {
                    'url': url_bad_ssl,
                    'wait': 'complete',
                    'context': context_id
                }
            })

    if capabilities.get('acceptInsecureCerts'):
        await navigate()
    else:
        with pytest.raises(Exception,
                           match=str({
                               'error': 'unknown error',
                               'message': 'net::ERR_CERT_AUTHORITY_INVALID'
                           })):
            await navigate()
