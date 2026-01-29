# Copyright 2026 Google LLC.
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
from anys import ANY_BOOL, ANY_NUMBER
from test_helpers import execute_command, goto_url

# These values are important for headful runner in some conditions:
# - x and y should be large enough to avoid window decorations (e.g. macOS menu bar).
#   Reference values for menu bars / window decorations:
#   - macOS menu bar: ~22px to ~32px (depending on OS version and screen notch)
#   - Windows title bar: ~30px
#   - Linux (GNOME) top bar: ~27px
# - width and height should be small enough to fit in CI runners.
#   Reference values for GitHub runners:
#   - Windows: ~1024x768
#   - Linux (Xvfb): often ~1280x1024
#   - macOS: inconsistent, often behaves like ~1024x768 or similar
# - width should be large enough to be valid for Chromium (e.g. > 500).
SOME_X = 33
SOME_Y = 44
SOME_HEIGHT = 555
SOME_WIDTH = 666


@pytest.mark.asyncio
async def test_set_client_window_state_normal(websocket, context_id,
                                              client_window_id):
    await goto_url(websocket, context_id, "about:blank")

    result = await execute_command(
        websocket,
        {
            "method": "browser.setClientWindowState",
            "params": {
                "clientWindow": client_window_id,
                "state": "normal",
                "x": SOME_X,
                "y": SOME_Y,
                "width": SOME_WIDTH,
                "height": SOME_HEIGHT,
            },
        },
    )

    assert result == {
        "clientWindow": client_window_id,
        "state": "normal",
        "x": SOME_X,
        "y": SOME_Y,
        "width": SOME_WIDTH,
        "height": SOME_HEIGHT,
        "active": ANY_BOOL,
    }

    result = await execute_command(
        websocket,
        {
            "method": "browser.getClientWindows",
            "params": {},
        },
    )
    assert result["clientWindows"] == [{
        "clientWindow": client_window_id,
        "state": "normal",
        "x": SOME_X,
        "y": SOME_Y,
        "width": SOME_WIDTH,
        "height": SOME_HEIGHT,
        "active": ANY_BOOL,
    }]


@pytest.mark.asyncio
async def test_set_client_window_state_maximized(websocket, context_id,
                                                 client_window_id):
    await goto_url(websocket, context_id, "about:blank")

    result = await execute_command(
        websocket,
        {
            "method": "browser.setClientWindowState",
            "params": {
                "clientWindow": client_window_id,
                "state": "maximized"
            },
        },
    )

    assert result == {
        "clientWindow": client_window_id,
        "state": "maximized",
        "x": ANY_NUMBER,
        "y": ANY_NUMBER,
        "width": ANY_NUMBER,
        "height": ANY_NUMBER,
        "active": ANY_BOOL,
    }

    result = await execute_command(
        websocket,
        {
            "method": "browser.getClientWindows",
            "params": {},
        },
    )
    assert result["clientWindows"] == [{
        "clientWindow": client_window_id,
        "state": "maximized",
        "x": ANY_NUMBER,
        "y": ANY_NUMBER,
        "width": ANY_NUMBER,
        "height": ANY_NUMBER,
        "active": ANY_BOOL,
    }]


@pytest.mark.asyncio
async def test_set_client_window_state_minimized(websocket, context_id,
                                                 client_window_id):
    await goto_url(websocket, context_id, "about:blank")

    result = await execute_command(
        websocket,
        {
            "method": "browser.setClientWindowState",
            "params": {
                "clientWindow": client_window_id,
                "state": "minimized"
            },
        },
    )

    assert result == {
        "clientWindow": client_window_id,
        "state": "minimized",
        "x": ANY_NUMBER,
        "y": ANY_NUMBER,
        "width": ANY_NUMBER,
        "height": ANY_NUMBER,
        "active": ANY_BOOL,
    }

    result = await execute_command(
        websocket,
        {
            "method": "browser.getClientWindows",
            "params": {},
        },
    )
    assert result["clientWindows"] == [{
        "clientWindow": client_window_id,
        "state": "minimized",
        "x": ANY_NUMBER,
        "y": ANY_NUMBER,
        "width": ANY_NUMBER,
        "height": ANY_NUMBER,
        "active": ANY_BOOL,
    }]


@pytest.mark.asyncio
async def test_set_client_window_state_fullscreen(websocket, context_id,
                                                  client_window_id):
    await goto_url(websocket, context_id, "about:blank")

    result = await execute_command(
        websocket,
        {
            "method": "browser.setClientWindowState",
            "params": {
                "clientWindow": client_window_id,
                "state": "fullscreen"
            },
        },
    )

    assert result == {
        "clientWindow": client_window_id,
        "state": "fullscreen",
        "x": ANY_NUMBER,
        "y": ANY_NUMBER,
        "width": ANY_NUMBER,
        "height": ANY_NUMBER,
        "active": ANY_BOOL,
    }

    result = await execute_command(
        websocket,
        {
            "method": "browser.getClientWindows",
            "params": {},
        },
    )
    assert result["clientWindows"] == [{
        "clientWindow": client_window_id,
        "state": "fullscreen",
        "x": ANY_NUMBER,
        "y": ANY_NUMBER,
        "width": ANY_NUMBER,
        "height": ANY_NUMBER,
        "active": ANY_BOOL,
    }]
