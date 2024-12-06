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
from syrupy.filters import props
from test_helpers import goto_url, send_JSON_command, subscribe

SNAPSHOT_EXCLUDE = props("timestamp", "message", "timings", "headers",
                         "stacktrace", "response")
KEYS_TO_STABILIZE = ['context', 'navigation', 'id', 'url', 'request']


@pytest.mark.asyncio
async def test_navigate_scriptRedirect_checkEvents(websocket, context_id, html,
                                                   url_example,
                                                   read_sorted_messages,
                                                   snapshot):
    pytest.xfail(
        reason=  # noqa: E251. The line is too long.
        "TODO: https://github.com/GoogleChromeLabs/chromium-bidi/issues/2856")

    await subscribe(websocket, ["browsingContext", "network"])

    initial_url = html(f"<script>window.location='{url_example}';</script>")

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": initial_url,
                "wait": "complete",
                "context": context_id
            }
        })

    messages = await read_sorted_messages(12,
                                          keys_to_stabilize=KEYS_TO_STABILIZE,
                                          check_no_other_messages=True)
    assert messages == snapshot(exclude=SNAPSHOT_EXCLUDE)


@pytest.mark.asyncio
async def test_navigate_scriptFragmentRedirect_checkEvents(
        websocket, context_id, html, url_example, read_sorted_messages,
        snapshot):
    pytest.xfail(
        reason=  # noqa: E251. The line is too long.
        "TODO: https://github.com/GoogleChromeLabs/chromium-bidi/issues/2856")

    await subscribe(websocket, ["browsingContext", "network"])

    initial_url = html("<script>window.location='#test';</script>")

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": initial_url,
                "wait": "complete",
                "context": context_id
            }
        })

    messages = await read_sorted_messages(8,
                                          keys_to_stabilize=KEYS_TO_STABILIZE,
                                          check_no_other_messages=True)
    assert messages == snapshot(exclude=SNAPSHOT_EXCLUDE)


@pytest.mark.asyncio
async def test_nested_navigate_scriptFragmentRedirect_checkEvents(
        websocket, iframe_id, html, url_example, read_sorted_messages,
        snapshot):
    pytest.xfail(
        reason=  # noqa: E251. The line is too long.
        "TODO: https://github.com/GoogleChromeLabs/chromium-bidi/issues/2856")

    await subscribe(websocket, ["browsingContext", "network"])

    initial_url = html("<script>window.location='#test';</script>")

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": initial_url,
                "wait": "complete",
                "context": iframe_id
            }
        })

    messages = await read_sorted_messages(8,
                                          keys_to_stabilize=KEYS_TO_STABILIZE,
                                          check_no_other_messages=True)
    assert messages == snapshot(exclude=SNAPSHOT_EXCLUDE)


@pytest.mark.asyncio
async def test_navigate_aboutBlank_checkEvents(websocket, context_id,
                                               url_example,
                                               read_sorted_messages, snapshot):
    await goto_url(websocket, context_id, url_example)

    await subscribe(websocket, ["browsingContext", "network"])

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": "about:blank",
                "wait": "complete",
                "context": context_id
            }
        })

    messages = await read_sorted_messages(4,
                                          keys_to_stabilize=KEYS_TO_STABILIZE,
                                          check_no_other_messages=True)
    assert messages == snapshot(exclude=SNAPSHOT_EXCLUDE)


@pytest.mark.asyncio
async def test_navigate_dataUrl_checkEvents(websocket, context_id, url_example,
                                            read_sorted_messages, snapshot):
    data_url = "data:text/html;,<h2>header</h2>"
    await goto_url(websocket, context_id, url_example)

    await subscribe(websocket, ["browsingContext", "network"])

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": data_url,
                "wait": "complete",
                "context": context_id
            }
        })

    messages = await read_sorted_messages(7,
                                          keys_to_stabilize=KEYS_TO_STABILIZE,
                                          check_no_other_messages=True)
    assert messages == snapshot(exclude=SNAPSHOT_EXCLUDE)


@pytest.mark.asyncio
async def test_scriptNavigate_aboutBlank_checkEvents(websocket, context_id,
                                                     url_example, html,
                                                     read_sorted_messages,
                                                     snapshot):
    pytest.xfail(
        reason=  # noqa: E251. The line is too long.
        "TODO: https://github.com/GoogleChromeLabs/chromium-bidi/issues/2856")

    await subscribe(websocket, ["browsingContext", "network"])

    about_blank_url = 'about:blank'
    initial_url = html(
        f"<script>window.location='{about_blank_url}';</script>")

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": initial_url,
                "wait": "complete",
                "context": context_id
            }
        })

    messages = await read_sorted_messages(9,
                                          keys_to_stabilize=KEYS_TO_STABILIZE,
                                          check_no_other_messages=True)
    assert messages == snapshot(exclude=SNAPSHOT_EXCLUDE)
