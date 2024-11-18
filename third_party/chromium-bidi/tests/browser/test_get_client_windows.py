# Copyright 2024 Google LLC.
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
from anys import ANY_NUMBER, ANY_STR
from test_helpers import execute_command


@pytest.mark.asyncio
async def test_browser_get_client_windows_single_tab(websocket, context_id):
    resp = await execute_command(websocket, {
        "method": "browser.getClientWindows",
        "params": {}
    })
    assert resp == {
        'clientWindows': [
            {
                # `active` is not implemented yet
                'active': False,
                'clientWindow': ANY_STR,
                'height': ANY_NUMBER,
                'state': 'normal',
                'width': ANY_NUMBER,
                'x': ANY_NUMBER,
                'y': ANY_NUMBER,
            },
        ],
    }


@pytest.mark.asyncio
async def test_browser_get_client_windows_two_tabs(websocket, context_id,
                                                   create_context,
                                                   test_headless_mode):
    if test_headless_mode == "old":
        pytest.xfail("In old headless mode, each tab is in a separate window")

    await create_context(context_type='tab')

    resp = await execute_command(websocket, {
        "method": "browser.getClientWindows",
        "params": {}
    })
    assert resp == {
        'clientWindows': [
            {
                # `active` is not implemented yet
                'active': False,
                'clientWindow': ANY_STR,
                'height': ANY_NUMBER,
                'state': 'normal',
                'width': ANY_NUMBER,
                'x': ANY_NUMBER,
                'y': ANY_NUMBER,
            },
        ],
    }


@pytest.mark.asyncio
async def test_browser_get_client_windows_two_windows(websocket, context_id,
                                                      create_context):
    await create_context(context_type='window')

    resp = await execute_command(websocket, {
        "method": "browser.getClientWindows",
        "params": {}
    })
    assert resp == {
        'clientWindows': [
            {
                # `active` is not implemented yet
                'active': False,
                'clientWindow': ANY_STR,
                'height': ANY_NUMBER,
                'state': 'normal',
                'width': ANY_NUMBER,
                'x': ANY_NUMBER,
                'y': ANY_NUMBER,
            },
            {
                # `active` is not implemented yet
                'active': False,
                'clientWindow': ANY_STR,
                'height': ANY_NUMBER,
                'state': 'normal',
                'width': ANY_NUMBER,
                'x': ANY_NUMBER,
                'y': ANY_NUMBER,
            },
        ],
    }
