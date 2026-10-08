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
from anys import AnyMatch
from test_helpers import AnyExtending, get_tree, send_JSON_command, subscribe


@pytest.mark.parametrize(
    "capabilities",
    [{"goog:chromeOptions": {"args": ["--auto-open-devtools-for-tabs"]}}],
    indirect=True,
)
@pytest.mark.asyncio
async def test_devtools_is_browsing_context_initial(websocket):
    tree_result = await get_tree(websocket)

    assert sorted(tree_result["contexts"], key=lambda c: c["url"]) == AnyExtending(
        [
            {"url": "about:blank"},
            {"url": AnyMatch(r"^devtools://devtools/bundled/devtools_app\.html")},
        ]
    )


@pytest.mark.parametrize(
    "capabilities",
    [{"goog:chromeOptions": {"args": ["--auto-open-devtools-for-tabs"]}}],
    indirect=True,
)
@pytest.mark.asyncio
async def test_devtools_is_browsing_context_new_tab(websocket, read_messages):
    await subscribe(websocket, ["browsingContext.contextCreated"])

    await send_JSON_command(
        websocket, {"method": "browsingContext.create", "params": {"type": "tab"}}
    )
    # Wait for the `browsingContext.create` response and the two
    # `browsingContext.contextCreated` events (new tab and its DevTools window).
    await read_messages(3)

    tree_result = await get_tree(websocket)

    assert sorted(tree_result["contexts"], key=lambda c: c["url"]) == AnyExtending(
        [
            # Initial tab and newly created tab.
            {"url": "about:blank"},
            {"url": "about:blank"},
            # DevTools windows for the initial tab and the newly created tab.
            {"url": AnyMatch(r"^devtools://devtools/bundled/devtools_app\.html")},
            {"url": AnyMatch(r"^devtools://devtools/bundled/devtools_app\.html")},
        ]
    )
