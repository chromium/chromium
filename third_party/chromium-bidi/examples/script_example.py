#!/usr/bin/env python
#
# Copyright 2021 Google LLC.
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
# This script implements Puppeteer's `examples/cross-browser.js` scenario using WebDriver BiDi.
# https://github.com/puppeteer/puppeteer/blob/4c3caaa3f99f0c31333a749ec50f56180507a374/examples/cross-browser.js

import asyncio
import itertools
import logging
from pathlib import Path

from _helpers import get_websocket, run_and_wait_command

ID = itertools.count(1000)

logging.basicConfig(
    format="%(message)s",
    level=logging.DEBUG,
)


async def main():
    # const browser = await puppeteer.launch();
    # https://github.com/puppeteer/puppeteer/blob/4c3caaa3f99f0c31333a749ec50f56180507a374/examples/cross-browser.js#L29
    websocket = await get_websocket()
    await run_and_wait_command(
        {
            "id": next(ID),
            "method": "session.new",
            "params": {}
        }, websocket)

    # Not implemented:
    # await browser.version();
    # https://github.com/puppeteer/puppeteer/blob/4c3caaa3f99f0c31333a749ec50f56180507a374/examples/cross-browser.js#L32

    # Puppeteer:
    # const page = await browser.newPage();
    #
    # Part 1. Execute BiDi command and wait for result.
    # await browser.newPage();
    # https://github.com/puppeteer/puppeteer/blob/4c3caaa3f99f0c31333a749ec50f56180507a374/examples/cross-browser.js#L31
    command_result = await run_and_wait_command(
        {
            "id": next(ID),
            "method": "browsingContext.create",
            "params": {
                "type": "tab"
            }
        }, websocket)

    # Puppeteer:
    # Part 2. Get the command result.
    # const page = ...;
    # https://github.com/puppeteer/puppeteer/blob/4c3caaa3f99f0c31333a749ec50f56180507a374/examples/cross-browser.js#L31
    # `command_result` should be like this:
    # {
    #     "id": __SOME_ID__,
    #     "result": {
    #         "context": "__SOME_CONTEXT_ID__",
    #         "url": "",
    #         "children": []
    #     }
    # }
    context_id = command_result['result']['context']

    # Puppeteer:
    # await page.goto('https://news.ycombinator.com/');
    # https://github.com/puppeteer/puppeteer/blob/4c3caaa3f99f0c31333a749ec50f56180507a374/examples/cross-browser.js#L34
    # To avoid network dependency in this test, use a local (static) copy.
    page_url = f'file://{Path(__file__).parent.resolve()}/app.html'
    await run_and_wait_command(
        {
            "id": next(ID),
            "method": "browsingContext.navigate",
            "params": {
                "url": page_url,
                "context": context_id,
                "wait": "complete"
            }
        }, websocket)

    # Puppeteer:
    # const resultsSelector = '.titlelink';
    # https://github.com/puppeteer/puppeteer/blob/4c3caaa3f99f0c31333a749ec50f56180507a374/examples/cross-browser.js#L37
    results_selector = {"type": "string", "value": ".titleline > a"}

    # Puppeteer:
    # const links = await page.evaluate((resultsSelector) => {...}, resultsSelector);
    #
    # Part 1. Execute BiDi command.
    # await page.evaluate((resultsSelector) => {...}, resultsSelector);
    # https://github.com/puppeteer/puppeteer/blob/4c3caaa3f99f0c31333a749ec50f56180507a374/examples/cross-browser.js#L38
    command_result = await run_and_wait_command(
        {
            "id": next(ID),
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": """(resultsSelector) => {
                const anchors = Array.from(document.querySelectorAll(resultsSelector));
                return anchors.map((anchor) => {
                    const title = anchor.textContent.trim();
                    return `${title} - ${anchor.href}`;
                });
            }""",
                "arguments": [results_selector],
                "target": {
                    "context": context_id
                },
                "awaitPromise": True
            }
        }, websocket)

    # Part 2. Get the command result.
    # const links = ...;
    # https://github.com/puppeteer/puppeteer/blob/4c3caaa3f99f0c31333a749ec50f56180507a374/examples/cross-browser.js#L38
    # `command_result` should be like this:
    # {
    #     "id": __SOME_ID__,
    #     "result": {
    #         "result": {
    #             "handle": "__SOME_OBJECT_ID__",
    #             "type": "array",
    #             "value": [
    #                 {
    #                     "type": "string",
    #                     "value": "__SOME_STRING_VALUE__"
    #                 },
    #                 ... // other values
    #             ]
    #         }
    #     }
    # }
    links = command_result["result"]["result"]

    # Assert the result is non-empty
    assert len(links["value"]) > 0, "The result should be non-empty."

    # Puppeteer:
    # console.log(links.join('\n'));
    # https://github.com/puppeteer/puppeteer/blob/4c3caaa3f99f0c31333a749ec50f56180507a374/examples/cross-browser.js#L45
    for item in links["value"]:
        print(item["value"])


loop = asyncio.new_event_loop()
asyncio.set_event_loop(loop)
result = loop.run_until_complete(main())
