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
from test_helpers import execute_command, goto_url

SIMPLE_EXTENSION_FILES = {
    "manifest.json": """
    {
        "manifest_version": 3,
        "name": "Test Extension - Simple Background Page",
        "description": "Test Extension - Simple Background Page",
        "version": "0.0.1",
        "permissions": [],
        "background":  {
        "scripts": [ "background.js" ]
        }
    }
    """,
    "background.js": """
    console.log('Hello world');
    """,
}


async def install(websocket, path):
    return await execute_command(
        websocket, {
            "method": "webExtension.install",
            "params": {
                "extensionData": {
                    "type": "path",
                    "path": path,
                },
            }
        })


async def uninstall(websocket, extension_id):
    await execute_command(websocket, {
        "method": "webExtension.uninstall",
        "params": {
            "extension": extension_id
        }
    })


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args':
            ['--enable-unsafe-extension-debugging', '--remote-debugging-pipe']
    },
}],
                         indirect=True)
async def test_extensions_invalid_path(websocket, test_headless_mode):
    if test_headless_mode == "old":
        pytest.xfail("Old headless mode does not support extensions")
        return
    with pytest.raises(Exception, match='unknown error'):
        await install(websocket, "invalid-path")


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args':
            ['--enable-unsafe-extension-debugging', '--remote-debugging-pipe']
    },
}],
                         indirect=True)
async def test_extensions_can_install(websocket, unpacked_extension_location,
                                      test_headless_mode):
    if test_headless_mode == "old":
        pytest.xfail("Old headless mode does not support extensions")
        return
    path = unpacked_extension_location(SIMPLE_EXTENSION_FILES)
    result = await install(websocket, path)
    assert result['extension']


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args':
            ['--enable-unsafe-extension-debugging', '--remote-debugging-pipe']
    },
}],
                         indirect=True)
async def test_extensions_can_open_manifest(websocket,
                                            unpacked_extension_location,
                                            test_headless_mode,
                                            create_context):
    """ Verify the extension page can be opened. https://crbug.com/412926721 """
    if test_headless_mode == "old":
        pytest.xfail("Old headless mode does not support extensions")
        return
    path = unpacked_extension_location(SIMPLE_EXTENSION_FILES)
    result = await install(websocket, path)
    assert result['extension']
    extension_id = result['extension']

    context_id = await create_context()

    await goto_url(websocket, context_id,
                   f"chrome-extension://{extension_id}/manifest.json")

    await execute_command(websocket, {
        "method": "browsingContext.close",
        "params": {
            "context": context_id
        }
    })

    await create_context()


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args': ['--remote-debugging-pipe']
    },
}],
                         indirect=True)
async def test_extensions_cannot_install(websocket,
                                         unpacked_extension_location,
                                         test_headless_mode):
    if test_headless_mode == "old":
        pytest.xfail("Old headless mode does not support extensions")
        return
    path = unpacked_extension_location(SIMPLE_EXTENSION_FILES)
    with pytest.raises(Exception,
                       match=str({
                           'error': 'unknown error',
                           'message': 'Method not available.',
                       })):
        await install(websocket, path)


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args':
            ['--enable-unsafe-extension-debugging', '--remote-debugging-pipe']
    },
}],
                         indirect=True)
async def test_extensions_can_uninstall(websocket, unpacked_extension_location,
                                        test_headless_mode):
    if test_headless_mode == "old":
        pytest.xfail("Old headless mode does not support extensions")
        return
    path = unpacked_extension_location(SIMPLE_EXTENSION_FILES)
    result = await install(websocket, path)
    extension_id = result['extension']
    await uninstall(websocket, extension_id)


@pytest.mark.asyncio
@pytest.mark.parametrize('capabilities', [{
    'goog:chromeOptions': {
        'args':
            ['--enable-unsafe-extension-debugging', '--remote-debugging-pipe']
    },
}],
                         indirect=True)
async def test_extensions_no_such_exension(websocket, test_headless_mode):
    if test_headless_mode == "old":
        pytest.xfail("Old headless mode does not support extensions")
        return
    with pytest.raises(Exception,
                       match=str({
                           'error': 'no such web extension',
                           'message': 'no such web extension'
                       })):
        await uninstall(websocket, "wrong_id")
