# Copyright 2021 Google LLC.
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
from test_helpers import *


@pytest.mark.asyncio
async def test_nestedBrowsingContext_navigateToPageWithHash_contextInfoUpdated(
        websocket, iframe_id):
    url = "data:text/html,<h2>test</h2>"
    url_with_hash_1 = url + "#1"

    # Initial navigation.
    await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_with_hash_1,
                "wait": "complete",
                "context": iframe_id
            }
        })

    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {
            "root": iframe_id
        }
    })

    recursive_compare(
        {
            "contexts": [{
                "context": iframe_id,
                "children": [],
                "parent": any_string,
                "url": url_with_hash_1
            }]
        }, result)


@pytest.mark.asyncio
async def test_nestedBrowsingContext_navigateWaitNone_navigated(
        websocket, iframe_id):
    await subscribe(
        websocket,
        ["browsingContext.domContentLoaded", "browsingContext.load"])
    # Send command.
    await send_JSON_command(
        websocket, {
            "id": 13,
            "method": "browsingContext.navigate",
            "params": {
                "url": "data:text/html,<h2>test</h2>",
                "wait": "none",
                "context": iframe_id
            }
        })

    # Assert command done.
    resp = await read_JSON_message(websocket)
    recursive_compare(
        {
            "id": 13,
            "result": {
                "navigation": any_string,
                "url": "data:text/html,<h2>test</h2>"
            }
        }, resp)

    navigation_id = resp["result"]["navigation"]

    # Wait for `browsingContext.load` event.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "method": "browsingContext.load",
        "params": {
            "context": iframe_id,
            "navigation": navigation_id,
            "url": "data:text/html,<h2>test</h2>"
        }
    }

    # Wait for `browsingContext.domContentLoaded` event.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "method": "browsingContext.domContentLoaded",
        "params": {
            "context": iframe_id,
            "navigation": navigation_id,
            "url": "data:text/html,<h2>test</h2>"
        }
    }


@pytest.mark.asyncio
async def test_nestedBrowsingContext_navigateWaitInteractive_navigated(
        websocket, iframe_id):
    await subscribe(
        websocket,
        ["browsingContext.domContentLoaded", "browsingContext.load"])

    # Send command.
    command = {
        "id": 14,
        "method": "browsingContext.navigate",
        "params": {
            "url": "data:text/html,<h2>test</h2>",
            "wait": "interactive",
            "context": iframe_id
        }
    }
    await send_JSON_command(websocket, command)

    # Wait for `browsingContext.load` event.
    resp = await read_JSON_message(websocket)
    navigation_id = resp["params"]["navigation"]
    assert resp == {
        "method": "browsingContext.load",
        "params": {
            "context": iframe_id,
            "navigation": navigation_id,
            "url": "data:text/html,<h2>test</h2>"
        }
    }

    # Wait for `browsingContext.domContentLoaded` event.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "method": "browsingContext.domContentLoaded",
        "params": {
            "context": iframe_id,
            "navigation": navigation_id,
            "url": "data:text/html,<h2>test</h2>"
        }
    }

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id": 14,
        "result": {
            "navigation": navigation_id,
            "url": "data:text/html,<h2>test</h2>"
        }
    }


@pytest.mark.asyncio
async def test_nestedBrowsingContext_navigateWaitComplete_navigated(
        websocket, iframe_id):
    await subscribe(
        websocket,
        ["browsingContext.domContentLoaded", "browsingContext.load"])

    # Send command.
    command = {
        "id": 15,
        "method": "browsingContext.navigate",
        "params": {
            "url": "data:text/html,<h2>test</h2>",
            "wait": "complete",
            "context": iframe_id
        }
    }
    await send_JSON_command(websocket, command)

    # Wait for `browsingContext.load` event.
    resp = await read_JSON_message(websocket)
    navigation_id = resp["params"]["navigation"]
    assert resp == {
        "method": "browsingContext.load",
        "params": {
            "context": iframe_id,
            "navigation": navigation_id,
            "url": "data:text/html,<h2>test</h2>"
        }
    }

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id": 15,
        "result": {
            "navigation": navigation_id,
            "url": "data:text/html,<h2>test</h2>"
        }
    }

    # Wait for `browsingContext.domContentLoaded` event.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "method": "browsingContext.domContentLoaded",
        "params": {
            "context": iframe_id,
            "navigation": navigation_id,
            "url": "data:text/html,<h2>test</h2>"
        }
    }


@pytest.mark.asyncio
async def test_nestedBrowsingContext_navigateSameDocumentNavigation_waitNone_navigated(
        websocket, iframe_id):
    url = "data:text/html,<h2>test</h2>"
    url_with_hash_1 = url + "#1"
    url_with_hash_2 = url + "#2"

    # Initial navigation.
    await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url,
                "wait": "complete",
                "context": iframe_id
            }
        })

    resp = await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_with_hash_1,
                "wait": "none",
                "context": iframe_id
            }
        })
    assert resp == {'navigation': None, 'url': url_with_hash_1}

    resp = await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_with_hash_2,
                "wait": "none",
                "context": iframe_id
            }
        })
    assert resp == {'navigation': None, 'url': url_with_hash_2}


@pytest.mark.asyncio
async def test_nestedBrowsingContext_navigateSameDocumentNavigation_waitInteractive_navigated(
        websocket, iframe_id):
    url = "data:text/html,<h2>test</h2>"
    url_with_hash_1 = url + "#1"
    url_with_hash_2 = url + "#2"

    # Initial navigation.
    await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url,
                "wait": "complete",
                "context": iframe_id
            }
        })

    resp = await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_with_hash_1,
                "wait": "interactive",
                "context": iframe_id
            }
        })
    assert resp == {'navigation': None, 'url': url_with_hash_1}

    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {
            "root": iframe_id
        }
    })

    recursive_compare(
        {
            "contexts": [{
                "context": iframe_id,
                "children": [],
                "parent": any_string,
                "url": url_with_hash_1
            }]
        }, result)

    resp = await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_with_hash_2,
                "wait": "interactive",
                "context": iframe_id
            }
        })
    assert resp == {'navigation': None, 'url': url_with_hash_2}

    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {
            "root": iframe_id
        }
    })

    recursive_compare(
        {
            "contexts": [{
                "context": iframe_id,
                "children": [],
                "parent": any_string,
                "url": url_with_hash_2
            }]
        }, result)


@pytest.mark.asyncio
async def test_nestedBrowsingContext_navigateSameDocumentNavigation_waitComplete_navigated(
        websocket, iframe_id):
    url = "data:text/html,<h2>test</h2>"
    url_with_hash_1 = url + "#1"
    url_with_hash_2 = url + "#2"

    # Initial navigation.
    await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url,
                "wait": "complete",
                "context": iframe_id
            }
        })

    resp = await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_with_hash_1,
                "wait": "complete",
                "context": iframe_id
            }
        })
    assert resp == {'navigation': None, 'url': url_with_hash_1}

    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {
            "root": iframe_id
        }
    })

    recursive_compare(
        {
            "contexts": [{
                "context": iframe_id,
                "children": [],
                "parent": any_string,
                "url": url_with_hash_1
            }]
        }, result)

    resp = await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url_with_hash_2,
                "wait": "complete",
                "context": iframe_id
            }
        })
    assert resp == {'navigation': None, 'url': url_with_hash_2}

    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {
            "root": iframe_id
        }
    })

    recursive_compare(
        {
            "contexts": [{
                "context": iframe_id,
                "children": [],
                "parent": any_string,
                "url": url_with_hash_2
            }]
        }, result)


# TODO(sadym): make offline.
@pytest.mark.asyncio
async def test_nestedBrowsingContext_afterNavigation_getTreeWithNestedCrossOriginContexts_contextsReturned(
        websocket, iframe_id):
    nested_iframe = 'https://example.com/'
    another_nested_iframe = 'https://example.org/'
    page_with_nested_iframe = f'data:text/html,<h1>MAIN_PAGE</h1>' \
                              f'<iframe src="{nested_iframe}" />'
    another_page_with_nested_iframe = f'data:text/html,<h1>ANOTHER_MAIN_PAGE</h1>' \
                                      f'<iframe src="{another_nested_iframe}" />'

    await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": page_with_nested_iframe,
                "wait": "complete",
                "context": iframe_id
            }
        })

    await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": another_page_with_nested_iframe,
                "wait": "complete",
                "context": iframe_id
            }
        })

    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {
            "root": iframe_id
        }
    })

    recursive_compare(
        {
            "contexts": [{
                "context": iframe_id,
                "children": [{
                    "context": any_string,
                    "url": another_nested_iframe,
                    "children": []
                }],
                "parent": any_string,
                "url": another_page_with_nested_iframe
            }]
        }, result)


@pytest.mark.asyncio
async def test_nestedBrowsingContext_afterNavigation_getTreeWithNestedContexts_contextsReturned(
        websocket, iframe_id):
    nested_iframe = 'data:text/html,<h2>IFRAME</h2>'
    another_nested_iframe = 'data:text/html,<h2>ANOTHER_IFRAME</h2>'
    page_with_nested_iframe = f'data:text/html,<h1>MAIN_PAGE</h1>' \
                              f'<iframe src="{nested_iframe}" />'
    another_page_with_nested_iframe = f'data:text/html,<h1>ANOTHER_MAIN_PAGE</h1>' \
                                      f'<iframe src="{another_nested_iframe}" />'

    await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": page_with_nested_iframe,
                "wait": "complete",
                "context": iframe_id
            }
        })

    await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": another_page_with_nested_iframe,
                "wait": "complete",
                "context": iframe_id
            }
        })

    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {
            "root": iframe_id
        }
    })

    recursive_compare(
        {
            "contexts": [{
                "context": iframe_id,
                "url": another_page_with_nested_iframe,
                "children": [{
                    "context": any_string,
                    "url": another_nested_iframe,
                    "children": []
                }],
                "parent": any_string
            }]
        }, result)
