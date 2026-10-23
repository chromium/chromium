#   Copyright 2026 Google LLC.
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

import pytest
from test_helpers import execute_command

DEFAULT_COLOR = "rgb(255, 0, 0)"
LIGHT_THEME_COLOR = "rgb(0, 0, 0)"
DARK_THEME_COLOR = "rgb(255, 255, 255)"


@pytest.fixture
def get_forced_colors_state(websocket, context_id):

    async def get_forced_colors_state(context_id=context_id):
        resp = await execute_command(
            websocket,
            {
                "method": "script.evaluate",
                "params": {
                    "expression": f"""(() => {{
                        const div = document.createElement('div');
                        div.style.color = '{DEFAULT_COLOR}';
                        document.body.appendChild(div);
                        const color = window.getComputedStyle(div).color;
                        div.remove();
                        return {{
                            active: window.matchMedia('(forced-colors: active)').matches,
                            color,
                        }};
                    }})()""",
                    "target": {"context": context_id},
                    "awaitPromise": True,
                },
            },
        )
        return {k: v["value"] for k, v in resp["result"]["value"]}

    return get_forced_colors_state


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "theme, expected_color",
    [
        ("light", LIGHT_THEME_COLOR),
        ("dark", DARK_THEME_COLOR),
    ],
)
async def test_set_forced_colors_mode_theme_per_browsing_context(
    websocket, context_id, get_forced_colors_state, theme, expected_color
):
    assert await get_forced_colors_state() == {
        "active": False,
        "color": DEFAULT_COLOR,
    }

    await execute_command(
        websocket,
        {
            "method": "emulation.setForcedColorsModeThemeOverride",
            "params": {
                "contexts": [context_id],
                "theme": theme,
            },
        },
    )
    assert await get_forced_colors_state() == {
        "active": True,
        "color": expected_color,
    }

    await execute_command(
        websocket,
        {
            "method": "emulation.setForcedColorsModeThemeOverride",
            "params": {
                "contexts": [context_id],
                "theme": None,
            },
        },
    )
    assert await get_forced_colors_state() == {
        "active": False,
        "color": DEFAULT_COLOR,
    }


@pytest.mark.asyncio
async def test_set_forced_colors_mode_theme_per_user_context(
    websocket, user_context_id, create_context, get_forced_colors_state
):
    await execute_command(
        websocket,
        {
            "method": "emulation.setForcedColorsModeThemeOverride",
            "params": {
                "userContexts": [user_context_id],
                "theme": "dark",
            },
        },
    )

    assert await get_forced_colors_state() == {
        "active": False,
        "color": DEFAULT_COLOR,
    }

    new_context_id = await create_context(user_context_id)
    assert await get_forced_colors_state(new_context_id) == {
        "active": True,
        "color": DARK_THEME_COLOR,
    }

    await execute_command(
        websocket,
        {
            "method": "emulation.setForcedColorsModeThemeOverride",
            "params": {
                "userContexts": [user_context_id],
                "theme": None,
            },
        },
    )
    assert await get_forced_colors_state(new_context_id) == {
        "active": False,
        "color": DEFAULT_COLOR,
    }


@pytest.mark.asyncio
async def test_set_forced_colors_mode_theme_globally(
    websocket, context_id, create_context, get_forced_colors_state
):
    await execute_command(
        websocket,
        {
            "method": "emulation.setForcedColorsModeThemeOverride",
            "params": {
                "theme": "dark",
            },
        },
    )

    assert await get_forced_colors_state() == {
        "active": True,
        "color": DARK_THEME_COLOR,
    }

    new_context_id = await create_context()
    assert await get_forced_colors_state(new_context_id) == {
        "active": True,
        "color": DARK_THEME_COLOR,
    }

    await execute_command(
        websocket,
        {
            "method": "emulation.setForcedColorsModeThemeOverride",
            "params": {
                "theme": None,
            },
        },
    )
    assert await get_forced_colors_state() == {
        "active": False,
        "color": DEFAULT_COLOR,
    }
    assert await get_forced_colors_state(new_context_id) == {
        "active": False,
        "color": DEFAULT_COLOR,
    }


@pytest.mark.asyncio
async def test_set_forced_colors_mode_theme_with_media_features(
    websocket, context_id, get_forced_colors_state
):
    await execute_command(
        websocket,
        {
            "method": "emulation.setMediaFeaturesOverride",
            "params": {
                "contexts": [context_id],
                "features": {"prefers-reduced-motion": "reduce"},
            },
        },
    )
    await execute_command(
        websocket,
        {
            "method": "emulation.setForcedColorsModeThemeOverride",
            "params": {
                "contexts": [context_id],
                "theme": "dark",
            },
        },
    )

    assert await get_forced_colors_state() == {
        "active": True,
        "color": DARK_THEME_COLOR,
    }
    resp = await execute_command(
        websocket,
        {
            "method": "script.evaluate",
            "params": {
                "expression": "window.matchMedia('(prefers-reduced-motion: reduce)').matches",
                "target": {"context": context_id},
                "awaitPromise": True,
            },
        },
    )
    assert resp["result"]["value"] is True

    await execute_command(
        websocket,
        {
            "method": "emulation.setForcedColorsModeThemeOverride",
            "params": {
                "contexts": [context_id],
                "theme": None,
            },
        },
    )
    assert await get_forced_colors_state() == {
        "active": False,
        "color": DEFAULT_COLOR,
    }
    resp = await execute_command(
        websocket,
        {
            "method": "script.evaluate",
            "params": {
                "expression": "window.matchMedia('(prefers-reduced-motion: reduce)').matches",
                "target": {"context": context_id},
                "awaitPromise": True,
            },
        },
    )
    assert resp["result"]["value"] is True


@pytest.mark.asyncio
async def test_set_forced_colors_mode_theme_media_features_precedence(
    websocket, context_id, get_forced_colors_state
):
    await execute_command(
        websocket,
        {
            "method": "emulation.setForcedColorsModeThemeOverride",
            "params": {
                "contexts": [context_id],
                "theme": "dark",
            },
        },
    )
    assert await get_forced_colors_state() == {
        "active": True,
        "color": DARK_THEME_COLOR,
    }

    # Explicit `prefers-color-scheme` media feature override takes precedence
    # over the forced-colors theme override (and selects the light palette in
    # Blink).
    await execute_command(
        websocket,
        {
            "method": "emulation.setMediaFeaturesOverride",
            "params": {
                "contexts": [context_id],
                "features": {"prefers-color-scheme": "light"},
            },
        },
    )
    assert await get_forced_colors_state() == {
        "active": True,
        "color": LIGHT_THEME_COLOR,
    }
    resp = await execute_command(
        websocket,
        {
            "method": "script.evaluate",
            "params": {
                "expression": "window.matchMedia('(prefers-color-scheme: light)').matches",
                "target": {"context": context_id},
                "awaitPromise": True,
            },
        },
    )
    assert resp["result"]["value"] is True

    # Explicit `forced-colors: none` media feature override takes precedence and
    # disables forced colors.
    await execute_command(
        websocket,
        {
            "method": "emulation.setMediaFeaturesOverride",
            "params": {
                "contexts": [context_id],
                "features": {"forced-colors": "none"},
            },
        },
    )
    assert await get_forced_colors_state() == {
        "active": False,
        "color": DEFAULT_COLOR,
    }

    # Clearing the media features override restores the active forced-colors
    # theme override.
    await execute_command(
        websocket,
        {
            "method": "emulation.setMediaFeaturesOverride",
            "params": {
                "contexts": [context_id],
                "features": None,
            },
        },
    )
    assert await get_forced_colors_state() == {
        "active": True,
        "color": DARK_THEME_COLOR,
    }
