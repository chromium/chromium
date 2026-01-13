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
from test_helpers import execute_command, goto_url

SOME_STRING = "SOME STRING"


@pytest_asyncio.fixture
async def is_scripting_enabled(websocket):

    async def is_scripting_enabled(target_context_id):
        resp = await execute_command(
            websocket, {
                "method": "script.evaluate",
                "params": {
                    "expression": """(() => {
                        const n = document.createElement('noscript');
                        n.innerHTML = '<div></div>';
                        return n.childElementCount === 0;
                    })()""",
                    "target": {
                        "context": target_context_id
                    },
                    "awaitPromise": True
                }
            })
        return resp["result"]["value"]

    return is_scripting_enabled


@pytest.mark.asyncio
async def test_script_disable_and_enabled(websocket, context_id,
                                          is_scripting_enabled):
    assert await is_scripting_enabled(context_id) is True

    await execute_command(
        websocket, {
            "method": "emulation.setScriptingEnabled",
            "params": {
                "enabled": False,
                "contexts": [context_id]
            }
        })
    assert await is_scripting_enabled(context_id) is False

    await execute_command(
        websocket, {
            "method": "emulation.setScriptingEnabled",
            "params": {
                "enabled": None,
                "contexts": [context_id]
            }
        })
    assert await is_scripting_enabled(context_id) is True


@pytest.mark.asyncio
async def test_script_disable_per_browsing_context(websocket, create_context,
                                                   is_scripting_enabled):

    browsing_context_id_1 = await create_context()
    browsing_context_id_2 = await create_context()

    assert await is_scripting_enabled(browsing_context_id_1) is True
    assert await is_scripting_enabled(browsing_context_id_2) is True

    await execute_command(
        websocket, {
            "method": "emulation.setScriptingEnabled",
            "params": {
                "enabled": False,
                'contexts': [browsing_context_id_1],
            }
        })
    assert await is_scripting_enabled(browsing_context_id_1) is False
    assert await is_scripting_enabled(browsing_context_id_2) is True

    await execute_command(
        websocket, {
            "method": "emulation.setScriptingEnabled",
            "params": {
                "enabled": False,
                'contexts': [browsing_context_id_2],
            }
        })
    assert await is_scripting_enabled(browsing_context_id_1) is False
    assert await is_scripting_enabled(browsing_context_id_2) is False

    await execute_command(
        websocket, {
            "method": "emulation.setScriptingEnabled",
            "params": {
                "enabled": None,
                'contexts': [browsing_context_id_1],
            }
        })
    assert await is_scripting_enabled(browsing_context_id_1) is True
    assert await is_scripting_enabled(browsing_context_id_2) is False

    await execute_command(
        websocket, {
            "method": "emulation.setScriptingEnabled",
            "params": {
                "enabled": None,
                'contexts': [browsing_context_id_2],
            }
        })
    assert await is_scripting_enabled(browsing_context_id_1) is True
    assert await is_scripting_enabled(browsing_context_id_2) is True


@pytest.mark.asyncio
async def test_script_disable_per_user_context(websocket, user_context_id,
                                               create_context,
                                               is_scripting_enabled):
    browsing_context_id_1 = await create_context()
    browsing_context_id_2 = await create_context(user_context_id)

    assert await is_scripting_enabled(browsing_context_id_1) is True
    assert await is_scripting_enabled(browsing_context_id_2) is True

    await execute_command(
        websocket, {
            "method": "emulation.setScriptingEnabled",
            "params": {
                "enabled": False,
                'userContexts': ["default"],
            }
        })
    assert await is_scripting_enabled(browsing_context_id_1) is False
    assert await is_scripting_enabled(await create_context()) is False
    assert await is_scripting_enabled(browsing_context_id_2) is True
    assert await is_scripting_enabled(await
                                      create_context(user_context_id)) is True

    await execute_command(
        websocket, {
            "method": "emulation.setScriptingEnabled",
            "params": {
                "enabled": False,
                'userContexts': [user_context_id],
            }
        })
    assert await is_scripting_enabled(browsing_context_id_1) is False
    assert await is_scripting_enabled(await create_context()) is False
    assert await is_scripting_enabled(browsing_context_id_2) is False
    assert await is_scripting_enabled(await
                                      create_context(user_context_id)) is False

    await execute_command(
        websocket, {
            "method": "emulation.setScriptingEnabled",
            "params": {
                "enabled": None,
                'userContexts': ["default"],
            }
        })
    assert await is_scripting_enabled(browsing_context_id_1) is True
    assert await is_scripting_enabled(await create_context()) is True
    assert await is_scripting_enabled(browsing_context_id_2) is False
    assert await is_scripting_enabled(await
                                      create_context(user_context_id)) is False

    await execute_command(
        websocket, {
            "method": "emulation.setScriptingEnabled",
            "params": {
                "enabled": None,
                'userContexts': [user_context_id],
            }
        })
    assert await is_scripting_enabled(browsing_context_id_1) is True
    assert await is_scripting_enabled(await create_context()) is True
    assert await is_scripting_enabled(browsing_context_id_2) is True
    assert await is_scripting_enabled(await
                                      create_context(user_context_id)) is True


@pytest.mark.asyncio
@pytest.mark.parametrize("same_origin", [True, False])
async def test_script_disable_iframe(websocket, context_id, iframe_id,
                                     is_scripting_enabled, same_origin, html):
    await goto_url(websocket, iframe_id, html("", same_origin=same_origin))

    assert await is_scripting_enabled(iframe_id) is True

    await execute_command(
        websocket, {
            "method": "emulation.setScriptingEnabled",
            "params": {
                "enabled": False,
                'contexts': [context_id],
            }
        })
    assert await is_scripting_enabled(iframe_id) is False

    await execute_command(
        websocket, {
            "method": "emulation.setScriptingEnabled",
            "params": {
                "enabled": None,
                'contexts': [context_id],
            }
        })
    assert await is_scripting_enabled(iframe_id) is True
