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
import pytest_asyncio
from test_helpers import execute_command


@pytest_asyncio.fixture
async def get_scrollbar_width(websocket, context_id):

    async def _get_scrollbar_width(context_id=context_id):
        resp = await execute_command(
            websocket, {
                "method": "script.evaluate",
                "params": {
                    "expression": """(function() {
                        var outer = document.createElement('div');
                        outer.style.visibility = 'hidden';
                        outer.style.width = '100px';
                        outer.style.height = '100px';
                        outer.style.overflow = 'scroll';
                        document.body.appendChild(outer);
                        var width = outer.offsetWidth - outer.clientWidth;
                        outer.parentNode.removeChild(outer);
                        return width;
                    })()""",
                    "target": {
                        "context": context_id
                    },
                    "awaitPromise": True,
                }
            })
        return resp["result"]["value"]

    return _get_scrollbar_width


@pytest.mark.asyncio
async def test_scrollbar_type_overlay(websocket, context_id,
                                      get_scrollbar_width):
    # 1. Measure initial scrollbar width.
    initial_width = await get_scrollbar_width(context_id)

    # 2. Set scrollbar type to 'overlay'.
    await execute_command(
        websocket, {
            'method': 'emulation.setScrollbarTypeOverride',
            'params': {
                'contexts': [context_id],
                'scrollbarType': 'overlay'
            }
        })

    # 3. Verify scrollbar width becomes 0.
    overlay_width = await get_scrollbar_width(context_id)
    assert overlay_width == 0, f"Expected overlay scrollbar width to be 0, got {overlay_width}"

    # 4. Set scrollbar type to `classic`.
    await execute_command(
        websocket, {
            'method': 'emulation.setScrollbarTypeOverride',
            'params': {
                'contexts': [context_id],
                'scrollbarType': 'classic'
            }
        })

    # 5. Verify scrollbar width matches initial (if the system default is overlay, then
    # `classic` does not have effect).
    default_width = await get_scrollbar_width(context_id)
    assert default_width == initial_width, f"Expected default scrollbar width to be {initial_width}, got {default_width}"

    # 6. Reset override (null).
    await execute_command(
        websocket, {
            'method': 'emulation.setScrollbarTypeOverride',
            'params': {
                'contexts': [context_id],
                'scrollbarType': None
            }
        })

    default_width = await get_scrollbar_width(context_id)
    assert default_width == initial_width, f"Expected scrollbar width to revert to {initial_width}, got {default_width}"
