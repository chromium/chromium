#   Copyright 2025 Google LLC.
#   Copyright (c) Microsoft Corporation.
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#
from uuid import uuid4

import pytest
from anys import ANY_STR
from test_helpers import (ANY_TIMESTAMP, ANY_UUID, goto_url, send_JSON_command,
                          subscribe, wait_for_event)

CONTENT = "SOME_FILE_CONTENT"


@pytest.fixture(params=['data', 'http'])
def file_url(url_download, request, filename):
    """Return a URL that triggers a download."""
    if request.param == 'data':
        return f"data:text/plain;charset=utf-8,{CONTENT}"

    return url_download(filename, CONTENT)


@pytest.fixture
def filename():
    return str(uuid4()) + '.txt'


@pytest.mark.asyncio
async def test_browsing_context_download_will_begin(websocket, context_id,
                                                    file_url, html, filename):
    page_url = html(
        f"""<a id="download_link" href="{file_url}" download="{filename}">Download</a>"""
    )
    await goto_url(websocket, context_id, page_url)

    await subscribe(websocket, ["browsingContext.downloadWillBegin"])

    await send_JSON_command(
        websocket, {
            'method': 'script.evaluate',
            'params': {
                'expression': 'download_link.click();',
                'awaitPromise': True,
                'target': {
                    'context': context_id,
                },
                'userActivation': True
            }
        })

    event = await wait_for_event(websocket,
                                 "browsingContext.downloadWillBegin")

    assert event == {
        'method': 'browsingContext.downloadWillBegin',
        'params': {
            'context': context_id,
            'navigation': ANY_UUID,
            'suggestedFilename': filename,
            'timestamp': ANY_TIMESTAMP,
            'url': file_url
        },
        'type': 'event',
    }


@pytest.mark.asyncio
async def test_browsing_context_download_finished_complete(
        websocket, test_headless_mode, context_id, file_url, html, filename):
    if test_headless_mode == "old":
        pytest.xfail("Old headless cancels downloads")

    page_url = html(
        f"""<a id="download_link" href="{file_url}" download="{filename}">Download</a>"""
    )
    await goto_url(websocket, context_id, page_url)

    await subscribe(
        websocket,
        ["browsingContext.downloadWillBegin", "browsingContext.downloadEnd"])

    await send_JSON_command(
        websocket, {
            'method': 'script.evaluate',
            'params': {
                'expression': 'download_link.click();',
                'awaitPromise': True,
                'target': {
                    'context': context_id,
                },
                'userActivation': True
            }
        })

    event = await wait_for_event(websocket,
                                 "browsingContext.downloadWillBegin")

    assert event == {
        'method': 'browsingContext.downloadWillBegin',
        'params': {
            'context': context_id,
            'navigation': ANY_UUID,
            'suggestedFilename': filename,
            'timestamp': ANY_TIMESTAMP,
            'url': file_url
        },
        'type': 'event',
    }

    navigation_id = event['params']['navigation']

    event = await wait_for_event(websocket, "browsingContext.downloadEnd")
    assert event == {
        'method': 'browsingContext.downloadEnd',
        'params': {
            'context': context_id,
            'navigation': navigation_id,
            'status': 'complete',
            'filepath': ANY_STR,
            'timestamp': ANY_TIMESTAMP,
            'url': file_url
        },
        'type': 'event',
    }

    # Assert suggested name is used.
    assert event["params"]["filepath"].endswith(filename)

    # Assert the file content is correct.
    with open(event["params"]["filepath"], encoding='utf-8') as file:
        file_content = file.read()
    assert file_content == CONTENT


@pytest.mark.asyncio
async def test_browsing_context_download_finished_canceled(
        websocket, test_headless_mode, url_hang_forever_download, context_id,
        html, get_cdp_session_id, filename):

    page_url = html(
        f"""<a id="download_link" href="{url_hang_forever_download()}" download="{filename}">Download</a>"""
    )
    await goto_url(websocket, context_id, page_url)

    await subscribe(
        websocket,
        ["browsingContext.downloadWillBegin", "browsingContext.downloadEnd"])

    cdp_session_id = await get_cdp_session_id(context_id)

    await send_JSON_command(
        websocket, {
            'method': 'script.evaluate',
            'params': {
                'expression': 'download_link.click();',
                'awaitPromise': True,
                'target': {
                    'context': context_id,
                },
                'userActivation': True
            }
        })

    event = await wait_for_event(websocket,
                                 "browsingContext.downloadWillBegin")

    assert event == {
        'method': 'browsingContext.downloadWillBegin',
        'params': {
            'context': context_id,
            'navigation': ANY_UUID,
            'suggestedFilename': ANY_STR,
            'timestamp': ANY_TIMESTAMP,
            'url': ANY_STR,
        },
        'type': 'event',
    }

    navigation_id = event['params']['navigation']

    if test_headless_mode != "old":
        # Cancel download via CDP. Old headless cancels it automatically.
        await send_JSON_command(
            websocket, {
                "method": "goog:cdp.sendCommand",
                "params": {
                    "method": "Browser.cancelDownload",
                    "params": {
                        "guid": navigation_id
                    },
                    "session": cdp_session_id
                }
            })

    event = await wait_for_event(
        websocket,
        "browsingContext.downloadEnd",
    )
    assert event == {
        'method': 'browsingContext.downloadEnd',
        'params': {
            'context': context_id,
            'navigation': navigation_id,
            'status': 'canceled',
            'timestamp': ANY_TIMESTAMP,
            'url': url_hang_forever_download(),
        },
        'type': 'event',
    }
