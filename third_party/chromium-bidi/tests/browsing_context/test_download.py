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
import pytest_asyncio
from anys import ANY_STR
from test_helpers import (ANY_TIMESTAMP, ANY_UUID, execute_command, goto_url,
                          send_JSON_command, subscribe, wait_for_event)

# Can be the same through tests for testing behavior of multiple downloads.
FILE_NAME = str(uuid4()) + '.something.txt'


@pytest_asyncio.fixture
async def content():
    """Should be different for each test."""
    return "SOME_FILE_CONTENT " + str(uuid4())


@pytest_asyncio.fixture
async def prepare_context(websocket, html, downloadable_url):

    async def prepare_context(target_context_id, url=downloadable_url):
        page_url = html(
            f"""<a id="download_link" href="{url}" download="{FILE_NAME}">Download</a>"""
        )
        await goto_url(websocket, target_context_id, page_url)

        await subscribe(websocket, [
            "browsingContext.downloadWillBegin", "browsingContext.downloadEnd"
        ])

    return prepare_context


@pytest_asyncio.fixture
async def trigger_download(websocket):

    async def trigger_download(target_context_id):
        await send_JSON_command(
            websocket, {
                'method': 'script.evaluate',
                'params': {
                    'expression': 'download_link.click();',
                    'awaitPromise': True,
                    'target': {
                        'context': target_context_id,
                    },
                    'userActivation': True
                }
            })

    return trigger_download


@pytest_asyncio.fixture(params=['default', 'new'],
                        ids=["Default user context", "Custom user context"])
async def target_user_context_id(request, create_user_context):
    if request.param == 'default':
        return 'default'
    return await create_user_context()


@pytest_asyncio.fixture
async def target_context_id(target_user_context_id, context_id,
                            create_context):
    if target_user_context_id == 'default':
        return context_id

    return await create_context(target_user_context_id)


@pytest.fixture(params=['data', 'http'])
def downloadable_url(url_download, request, content):
    """Return a URL that triggers a download."""
    if request.param == 'data':
        return f"data:text/plain;charset=utf-8,{content}"

    return url_download(FILE_NAME, content)


@pytest_asyncio.fixture
async def assert_success_download_events(websocket, prepare_context, content,
                                         trigger_download, downloadable_url):

    async def assert_success_download_events(context_id):
        await prepare_context(context_id)
        await trigger_download(context_id)
        event = await wait_for_event(websocket,
                                     "browsingContext.downloadWillBegin")
        assert event == {
            'method': 'browsingContext.downloadWillBegin',
            'params': {
                'context': context_id,
                'navigation': ANY_UUID,
                'suggestedFilename': ANY_STR,
                'timestamp': ANY_TIMESTAMP,
                'url': downloadable_url,
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
                'filepath': ANY_STR,
                'status': 'complete',
                'timestamp': ANY_TIMESTAMP,
                'url': downloadable_url,
            },
            'type': 'event',
        }

        # Assert the file content is correct.
        with open(event["params"]["filepath"], encoding='utf-8') as file:
            file_content = file.read()
        assert file_content == content
        return event["params"]["filepath"]

    return assert_success_download_events


@pytest_asyncio.fixture
async def assert_denied_download_events(websocket, prepare_context,
                                        trigger_download, downloadable_url):

    async def assert_denied_download_events(context_id):
        await prepare_context(context_id)
        await trigger_download(context_id)
        event = await wait_for_event(websocket,
                                     "browsingContext.downloadWillBegin")
        assert event == {
            'method': 'browsingContext.downloadWillBegin',
            'params': {
                'context': context_id,
                'navigation': ANY_UUID,
                'suggestedFilename': ANY_STR,
                'timestamp': ANY_TIMESTAMP,
                'url': downloadable_url,
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
                'status': 'canceled',
                'timestamp': ANY_TIMESTAMP,
                'url': downloadable_url,
            },
            'type': 'event',
        }

    return assert_denied_download_events


@pytest.mark.asyncio
async def test_browsing_context_download_will_begin(websocket,
                                                    target_context_id,
                                                    downloadable_url,
                                                    prepare_context,
                                                    trigger_download):
    await prepare_context(target_context_id)
    await trigger_download(target_context_id)

    event = await wait_for_event(websocket,
                                 "browsingContext.downloadWillBegin")

    assert event == {
        'method': 'browsingContext.downloadWillBegin',
        'params': {
            'context': target_context_id,
            'navigation': ANY_UUID,
            'suggestedFilename': FILE_NAME,
            'timestamp': ANY_TIMESTAMP,
            'url': downloadable_url
        },
        'type': 'event',
    }


@pytest.mark.asyncio
async def test_browsing_context_download_default_behavior(
        websocket, target_context_id, downloadable_url, prepare_context,
        trigger_download, target_user_context_id, test_headless_mode, content):
    await prepare_context(target_context_id)

    await trigger_download(target_context_id)

    event = await wait_for_event(websocket,
                                 "browsingContext.downloadWillBegin")

    assert event == {
        'method': 'browsingContext.downloadWillBegin',
        'params': {
            'context': target_context_id,
            'navigation': ANY_UUID,
            'suggestedFilename': FILE_NAME,
            'timestamp': ANY_TIMESTAMP,
            'url': downloadable_url
        },
        'type': 'event',
    }

    navigation_id = event['params']['navigation']

    if target_user_context_id != "default" and test_headless_mode == "false":
        pytest.xfail(
            "Default headful download behavior for non-default user context is"
            "to show the save dialog, which cannot be interacted via protocol")

    event = await wait_for_event(websocket, "browsingContext.downloadEnd")

    if target_user_context_id != "default" or test_headless_mode == "old":
        # Non-default contexts or headless shell's default behavior is to cancel
        # the download.
        assert event == {
            'method': 'browsingContext.downloadEnd',
            'params': {
                'context': target_context_id,
                'navigation': navigation_id,
                'status': 'canceled',
                'timestamp': ANY_TIMESTAMP,
                'url': downloadable_url,
            },
            'type': 'event',
        }
        return

    assert event == {
        'method': 'browsingContext.downloadEnd',
        'params': {
            'context': target_context_id,
            'navigation': navigation_id,
            'status': 'complete',
            'filepath': ANY_STR,
            'timestamp': ANY_TIMESTAMP,
            'url': downloadable_url
        },
        'type': 'event',
    }

    # Assert suggested name is used.
    assert FILE_NAME.split(".")[-1] in event["params"]["filepath"]

    # Assert the file content is correct.
    with open(event["params"]["filepath"], encoding='utf-8') as file:
        file_content = file.read()
    assert file_content == content


@pytest.mark.asyncio
async def test_browsing_context_download_end_canceled(
        websocket, url_hang_forever_download, target_context_id, tmp_path,
        target_user_context_id, get_cdp_session_id, prepare_context,
        trigger_download):
    # Old headless and custom user context require the `destinationFolder` to be
    # specified.
    await execute_command(
        websocket, {
            'method': 'browser.setDownloadBehavior',
            'params': {
                'downloadBehavior': {
                    'type': 'allowed',
                    'destinationFolder': str(tmp_path)
                },
                'userContexts': [target_user_context_id]
            }
        })

    await prepare_context(target_context_id, url_hang_forever_download())
    await trigger_download(target_context_id)

    event = await wait_for_event(websocket,
                                 "browsingContext.downloadWillBegin")

    assert event == {
        'method': 'browsingContext.downloadWillBegin',
        'params': {
            'context': target_context_id,
            'navigation': ANY_UUID,
            'suggestedFilename': ANY_STR,
            'timestamp': ANY_TIMESTAMP,
            'url': ANY_STR,
        },
        'type': 'event',
    }

    navigation_id = event['params']['navigation']

    # Cancel download via CDP.
    await send_JSON_command(
        websocket, {
            "method": "goog:cdp.sendCommand",
            "params": {
                "method": "Browser.cancelDownload",
                "params": {
                    "guid": navigation_id,
                    **({
                        "browserContextId": target_user_context_id
                    } if target_user_context_id != "default" else {})
                },
                "session": await get_cdp_session_id(target_context_id)
            }
        })

    event = await wait_for_event(
        websocket,
        "browsingContext.downloadEnd",
    )
    assert event == {
        'method': 'browsingContext.downloadEnd',
        'params': {
            'context': target_context_id,
            'navigation': navigation_id,
            'status': 'canceled',
            'timestamp': ANY_TIMESTAMP,
            'url': url_hang_forever_download(),
        },
        'type': 'event',
    }


@pytest.mark.asyncio
async def test_browsing_context_download_behavior_deny(
        websocket, target_context_id, target_user_context_id,
        assert_denied_download_events):
    await execute_command(
        websocket, {
            'method': 'browser.setDownloadBehavior',
            'params': {
                'downloadBehavior': {
                    'type': 'denied'
                },
                'userContexts': [target_user_context_id]
            }
        })

    await assert_denied_download_events(target_context_id)


@pytest.mark.asyncio
async def test_browsing_context_download_behavior_allowed_with_destination_folder(
        websocket, target_context_id, target_user_context_id, tmp_path,
        assert_success_download_events):
    await execute_command(
        websocket, {
            'method': 'browser.setDownloadBehavior',
            'params': {
                'downloadBehavior': {
                    'type': 'allowed',
                    'destinationFolder': str(tmp_path),
                },
                'userContexts': [target_user_context_id]
            }
        })

    download_path = await assert_success_download_events(target_context_id)
    assert download_path.startswith(str(tmp_path))


@pytest.mark.asyncio
async def test_browsing_context_download_behavior_allow_global(
        websocket, context_id, create_user_context, create_context, tmp_path,
        assert_success_download_events):
    some_user_context = await create_user_context()
    context_in_some_user_context = await create_context(some_user_context)

    await execute_command(
        websocket, {
            'method': 'browser.setDownloadBehavior',
            'params': {
                'downloadBehavior': {
                    'type': 'allowed',
                    'destinationFolder': str(tmp_path),
                },
            }
        })

    # Assert the behavior applied for existing context in the default user context.
    download_path = await assert_success_download_events(context_id)
    assert download_path.startswith(str(tmp_path))
    # Assert the behavior applied for new context in custom user context.
    download_path = await assert_success_download_events(await
                                                         create_context())
    assert download_path.startswith(str(tmp_path))
    # Assert the behavior applied for existing context in custom user context.
    download_path = await assert_success_download_events(
        context_in_some_user_context)
    assert download_path.startswith(str(tmp_path))
    # Assert the behavior applied for new context in existing user context.
    download_path = await assert_success_download_events(
        await create_context(some_user_context))
    assert download_path.startswith(str(tmp_path))
    # Assert the behavior applied for new context in new user context.
    download_path = await assert_success_download_events(await create_context(
        await create_user_context()))
    assert download_path.startswith(str(tmp_path))


@pytest.mark.asyncio
async def test_browsing_context_download_behavior_default_global(websocket):
    # Assert the behavior can be set to default globally.
    await execute_command(
        websocket, {
            'method': 'browser.setDownloadBehavior',
            'params': {
                'downloadBehavior': None,
            }
        })


@pytest.mark.asyncio
async def test_browsing_context_download_behavior_global_and_per_user_context(
        websocket, context_id, tmp_path, assert_success_download_events,
        assert_denied_download_events):

    await execute_command(
        websocket, {
            'method': 'browser.setDownloadBehavior',
            'params': {
                'downloadBehavior': {
                    'type': 'allowed',
                    'destinationFolder': str(tmp_path),
                },
                'userContexts': ['default']
            }
        })
    await assert_success_download_events(context_id)

    await execute_command(
        websocket, {
            'method': 'browser.setDownloadBehavior',
            'params': {
                'downloadBehavior': {
                    'type': 'denied',
                    'destinationFolder': str(tmp_path),
                },
            }
        })
    # Global config should affect user context's one.
    await assert_success_download_events(context_id)

    await execute_command(
        websocket, {
            'method': 'browser.setDownloadBehavior',
            'params': {
                'downloadBehavior': None,
                'userContexts': ['default']
            }
        })
    await assert_denied_download_events(context_id)
