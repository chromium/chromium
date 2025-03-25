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

import pytest
from test_helpers import (ANY_TIMESTAMP, ANY_UUID, goto_url, send_JSON_command,
                          subscribe, wait_for_event)


@pytest.mark.asyncio
async def test_browsing_context_download_will_begin_data_url(
        websocket, context_id, html):
    content = "SOME_FILE_CONTENT"
    filename = 'some_file_name.txt'
    download_url = f"data:text/plain;charset=utf-8,{content}"
    url = html(
        f"""<a id="download_link" href="{download_url}" download="{filename}">Download</a>"""
    )
    await goto_url(websocket, context_id, url)

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
            'url': download_url
        },
        'type': 'event',
    }


@pytest.mark.asyncio
async def test_browsing_context_download_will_begin_url(
        websocket, context_id, url_download, html):
    content = "SOME_FILE_CONTENT"
    download_attribute = "download-attribute.txt"
    header_file_name = "download-attribute.txt"
    download_url = url_download(header_file_name, content)
    url = html(
        f"""<a id="download_link" href="{download_url}" download="{download_attribute}">Download</a>"""
    )
    await goto_url(websocket, context_id, url)

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
            'suggestedFilename': header_file_name,
            'timestamp': ANY_TIMESTAMP,
            'url': download_url
        },
        'type': 'event',
    }
