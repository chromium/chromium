#!/usr/bin/env python
#
# Copyright 2023 Google LLC.
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
#
# This script shows how to interoperate WebDriver Classic and WebDriver BiDi.

import asyncio
import itertools
import logging
import os
from pathlib import Path

import requests
from _helpers import get_webdriver_session, run_and_wait_command, websockets

ID = itertools.count(1000)

logging.basicConfig(
    format="%(message)s",
    level=logging.DEBUG,
)


async def main():
    # Prepare WebDriver.
    port = os.getenv('PORT', 8080)
    new_session = await get_webdriver_session()
    websocket = await websockets.connect(
        new_session["capabilities"]["webSocketUrl"])
    session_id = new_session["sessionId"]
    classic_session_prefix = f'http://localhost:{port}/session/{session_id}'

    # Get browsing context (window handle in WebDriver Classic).
    window_response = requests.get(f'{classic_session_prefix}/window',
                                   timeout=10).json()
    browsing_context = window_response["value"]

    # WebDriver Classic: navigate to page.
    page_url = f'file://{Path(__file__).parent.resolve()}/app.html'
    requests.post(f'{classic_session_prefix}/url',
                  json={
                      "url": page_url
                  },
                  timeout=10).json()

    # WebDriver Classic: find all titles' links.
    classic_result = requests.post(f'{classic_session_prefix}/elements',
                                   json={
                                       "using": "css selector",
                                       "value": ".titleline > a"
                                   },
                                   timeout=10).json()

    # `classic_result` has the following format:
    # {'value':
    #     [
    #         {'element-6066-11e4-a52e-4f735466cecf': 'SOME_ELEMENT_ID'},
    #         {'element-6066-11e4-a52e-4f735466cecf': 'ANOTHER_ELEMENT_ID'},
    #         ...
    #     ]
    # }

    # Collect element references into an array.
    raw_element_ids = list(
        map(lambda _element: _element["element-6066-11e4-a52e-4f735466cecf"],
            classic_result["value"]))

    # Collect element ids into an array of BiDi shared references.
    bidi_element_references = list(
        map(lambda _id: {"sharedId": _id}, raw_element_ids))

    # Pass BiDi shared references to BiDi script.
    command_result = await run_and_wait_command(
        {
            "id": next(ID),
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": """(...anchors) => {
                return anchors.map((anchor) => {
                    const title = anchor.textContent.trim();
                    return `${title} - ${anchor.href}`;
                });
            }""",
                "arguments": bidi_element_references,
                "target": {
                    "context": browsing_context
                },
                "awaitPromise": True
            }
        }, websocket)

    links = command_result["result"]["result"]

    # Assert the result is non-empty
    assert len(links["value"]) > 0, "The result should be non-empty."

    for item in links["value"]:
        print(item["value"])


loop = asyncio.new_event_loop()
asyncio.set_event_loop(loop)
result = loop.run_until_complete(main())
