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
from anys import ANY_DICT
from test_helpers import (execute_command, goto_url, send_JSON_command,
                          subscribe)

SOME_STRING = "SOME STRING"


@pytest_asyncio.fixture
async def prepare_browsing_context(websocket, html):
    async def prepare_browsing_context(target_context_id, same_origin=True):
        url = html(
            f"<button onclick=\'alert(\"{SOME_STRING}\")'>some button</button>",
            same_origin)
        await goto_url(websocket, target_context_id, url)
        await subscribe(websocket, ["browsingContext.userPromptOpened"],
                        [target_context_id])

    return prepare_browsing_context


@pytest_asyncio.fixture
async def create_and_prepare_browsing_context(create_context,
                                              prepare_browsing_context):
    async def create_and_prepare_browsing_context(user_context_id=None):
        target_context_id = await create_context(user_context_id)
        await prepare_browsing_context(target_context_id)
        return target_context_id

    return create_and_prepare_browsing_context


@pytest_asyncio.fixture
async def click_element(websocket, query_selector):
    async def click_element(element_selector, context_id):
        target_element = await query_selector(element_selector, context_id)
        return await send_JSON_command(
            websocket, {
                "method": "input.performActions",
                "params": {
                    "context": context_id,
                    "actions": [{
                        "type": "pointer",
                        "id": "__puppeteer_mouse",
                        "actions": [{
                            "type": "pointerMove",
                            "x": 0,
                            "y": 0,
                            "origin": {
                                "type": "element",
                                "element": target_element
                            }
                        }, {
                            "type": "pointerDown",
                            "button": 0
                        }, {
                            "type": "pointerUp",
                            "button": 0
                        }]
                    }]
                }
            })

    return click_element


@pytest_asyncio.fixture
async def assert_scripting_disabled(click_element, read_messages,
                                    activate_main_tab):
    async def assert_scripting_disabled(target_context_id):
        await activate_main_tab(target_context_id)
        command_id = await click_element("button", target_context_id)

        # No `browsingContext.userPromptOpened` events expected.
        [click_command_result
         ] = await read_messages(1, check_no_other_messages=True)
        assert click_command_result == {
            'id': command_id,
            'result': {},
            'type': 'success',
        }

    return assert_scripting_disabled


@pytest_asyncio.fixture
async def assert_scripting_enabled(click_element, read_messages,
                                   activate_main_tab):
    async def assert_scripting_enabled(target_context_id):
        await activate_main_tab(target_context_id)

        command_id = await click_element("button", target_context_id)

        [click_command_result,
         prompt_event] = await read_messages(2, check_no_other_messages=True)

        assert prompt_event == {
            'method': 'browsingContext.userPromptOpened',
            'params': ANY_DICT,
            'type': 'event',
        }

        assert click_command_result == {
            'id': command_id,
            'result': {},
            'type': 'success',
        }

    return assert_scripting_enabled


@pytest.mark.asyncio
async def test_script_disable_and_enabled(websocket, context_id,
                                          prepare_browsing_context,
                                          assert_scripting_disabled,
                                          assert_scripting_enabled):
    await prepare_browsing_context(context_id)

    await assert_scripting_enabled(context_id)

    await execute_command(
        websocket, {
            "method": "emulation.setScriptingEnabled",
            "params": {
                "enabled": False,
                "contexts": [context_id]
            }
        })
    await assert_scripting_disabled(context_id)

    await execute_command(
        websocket, {
            "method": "emulation.setScriptingEnabled",
            "params": {
                "enabled": None,
                "contexts": [context_id]
            }
        })
    await assert_scripting_enabled(context_id)


@pytest.mark.asyncio
async def test_script_disable_per_browsing_context(
        websocket, create_and_prepare_browsing_context,
        assert_scripting_disabled, assert_scripting_enabled,
        test_headless_mode):
    if test_headless_mode != "false":
        pytest.xfail(
            "Headless mode is flaky when rapidly switching between contexts")

    browsing_context_id_1 = await create_and_prepare_browsing_context()
    browsing_context_id_2 = await create_and_prepare_browsing_context()

    await assert_scripting_enabled(browsing_context_id_1)
    await assert_scripting_enabled(browsing_context_id_2)

    await execute_command(
        websocket, {
            "method": "emulation.setScriptingEnabled",
            "params": {
                "enabled": False,
                'contexts': [browsing_context_id_1],
            }
        })
    await assert_scripting_disabled(browsing_context_id_1)
    await assert_scripting_enabled(browsing_context_id_2)

    await execute_command(
        websocket, {
            "method": "emulation.setScriptingEnabled",
            "params": {
                "enabled": False,
                'contexts': [browsing_context_id_2],
            }
        })
    await assert_scripting_disabled(browsing_context_id_1)
    await assert_scripting_disabled(browsing_context_id_2)

    await execute_command(
        websocket, {
            "method": "emulation.setScriptingEnabled",
            "params": {
                "enabled": None,
                'contexts': [browsing_context_id_1],
            }
        })
    await assert_scripting_enabled(browsing_context_id_1)
    await assert_scripting_disabled(browsing_context_id_2)

    await execute_command(
        websocket, {
            "method": "emulation.setScriptingEnabled",
            "params": {
                "enabled": None,
                'contexts': [browsing_context_id_2],
            }
        })
    await assert_scripting_enabled(browsing_context_id_1)
    await assert_scripting_enabled(browsing_context_id_2)


@pytest.mark.asyncio
async def test_script_disable_per_user_context(
        websocket, user_context_id, create_and_prepare_browsing_context,
        assert_scripting_disabled, assert_scripting_enabled):
    browsing_context_id_1 = await create_and_prepare_browsing_context(None)
    browsing_context_id_2 = await create_and_prepare_browsing_context(
        user_context_id)

    await assert_scripting_enabled(browsing_context_id_1)
    await assert_scripting_enabled(browsing_context_id_2)

    await execute_command(
        websocket, {
            "method": "emulation.setScriptingEnabled",
            "params": {
                "enabled": False,
                'userContexts': ["default"],
            }
        })
    await assert_scripting_disabled(browsing_context_id_1)
    await assert_scripting_disabled(await
                                    create_and_prepare_browsing_context())
    await assert_scripting_enabled(browsing_context_id_2)
    await assert_scripting_enabled(
        await create_and_prepare_browsing_context(user_context_id))

    await execute_command(
        websocket, {
            "method": "emulation.setScriptingEnabled",
            "params": {
                "enabled": False,
                'userContexts': [user_context_id],
            }
        })
    await assert_scripting_disabled(browsing_context_id_1)
    await assert_scripting_disabled(await
                                    create_and_prepare_browsing_context())
    await assert_scripting_disabled(browsing_context_id_2)
    await assert_scripting_disabled(
        await create_and_prepare_browsing_context(user_context_id))

    await execute_command(
        websocket, {
            "method": "emulation.setScriptingEnabled",
            "params": {
                "enabled": None,
                'userContexts': ["default"],
            }
        })
    await assert_scripting_enabled(browsing_context_id_1)
    await assert_scripting_enabled(await create_and_prepare_browsing_context())
    await assert_scripting_disabled(browsing_context_id_2)
    await assert_scripting_disabled(
        await create_and_prepare_browsing_context(user_context_id))

    await execute_command(
        websocket, {
            "method": "emulation.setScriptingEnabled",
            "params": {
                "enabled": None,
                'userContexts': [user_context_id],
            }
        })
    await assert_scripting_enabled(browsing_context_id_1)
    await assert_scripting_enabled(await create_and_prepare_browsing_context())
    await assert_scripting_enabled(browsing_context_id_2)
    await assert_scripting_enabled(
        await create_and_prepare_browsing_context(user_context_id))


@pytest.mark.asyncio
@pytest.mark.parametrize("same_origin", [True, False])
async def test_script_disable_iframe(websocket, context_id, iframe_id,
                                     prepare_browsing_context,
                                     assert_scripting_disabled,
                                     assert_scripting_enabled, same_origin):
    await prepare_browsing_context(iframe_id, same_origin)
    await assert_scripting_enabled(iframe_id)

    await execute_command(
        websocket, {
            "method": "emulation.setScriptingEnabled",
            "params": {
                "enabled": False,
                'contexts': [context_id],
            }
        })
    await assert_scripting_disabled(iframe_id)

    await execute_command(
        websocket, {
            "method": "emulation.setScriptingEnabled",
            "params": {
                "enabled": None,
                'contexts': [context_id],
            }
        })
    await assert_scripting_enabled(iframe_id)
