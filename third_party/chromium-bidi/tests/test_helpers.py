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
from __future__ import annotations

import asyncio
import base64
import io
import itertools
import json
import logging
from collections.abc import Callable
from typing import Literal

from anys import (ANY_NUMBER, ANY_STR, AnyFullmatch, AnyGT, AnyLT, AnyMatch,
                  AnyWithEntries)
from PIL import Image, ImageChops

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

_command_counter = itertools.count(1)


def get_next_command_id() -> int:
    """
    >>> x = get_next_command_id()
    >>> y = get_next_command_id()
    >>> assert x + 1 == y
    """
    return next(_command_counter)


async def subscribe(websocket,
                    events: list[str],
                    context_ids: list[str] | None = None,
                    channel: str | None = None):
    command: dict = {
        "method": "session.subscribe",
        "params": {
            "events": events,
        }
    }

    if context_ids is not None:
        command["params"]["contexts"] = context_ids
    if channel is not None:
        command["channel"] = channel

    await execute_command(websocket, command)


async def send_JSON_command(websocket, command: dict) -> int:
    if "id" not in command:
        command["id"] = get_next_command_id()
    await websocket.send(json.dumps(command))
    return command["id"]


async def read_JSON_message(websocket) -> dict:
    return json.loads(await websocket.recv())


async def execute_command(websocket, command: dict, timeout: int = 5) -> dict:
    if "id" not in command:
        command["id"] = get_next_command_id()

    await send_JSON_command(websocket, command)

    logger.info(
        f"Executing command with method '{command['method']}' and params '{command['params']}'..."
    )
    return await wait_for_command(websocket, command["id"], timeout)


async def wait_for_command(websocket,
                           command_id: int,
                           timeout: int = 5) -> dict:
    def _filter(resp):
        return "id" in resp and resp["id"] == command_id

    resp = await wait_for_message(websocket, _filter, timeout)
    if "result" in resp:
        return resp["result"]
    raise Exception({"error": resp["error"], "message": resp["message"]})


async def wait_for_message(websocket,
                           filter_lambda: Callable[[dict], bool],
                           timeout: int = 5):
    async def future():
        while True:
            # Wait for the command to be finished.
            resp = await read_JSON_message(websocket)
            if filter_lambda(resp):
                return resp

    # Throws `asyncio.exceptions.TimeoutError` if the future does not resolve
    # within the given timeout.
    return await asyncio.wait_for(
        future(),
        timeout,
    )


async def get_tree(websocket, context_id: str | None = None) -> dict:
    """Get the tree of browsing contexts."""
    params = {}
    if context_id is not None:
        params["root"] = context_id
    return await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": params
    })


async def goto_url(
        websocket,
        context_id: str,
        url: str,
        wait: Literal["none", "interactive", "complete"] = "complete") -> dict:
    """Open given URL in the given context."""
    logger.info(
        f"Navigating to url '{url}' with wait '{wait}' in context '{context_id}'..."
    )
    return await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url,
                "context": context_id,
                "wait": wait
            }
        })


async def set_html_content(websocket, context_id: str, html_content: str):
    """Sets the current page content without navigation."""
    return await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "(html_content) => { document.body.innerHTML = html_content }",
                "arguments": [{
                    'type': 'string',
                    'value': html_content
                }],
                "target": {
                    "context": context_id
                },
                "awaitPromise": True
            }
        })


async def wait_for_event(websocket, event_method: str) -> dict:
    """
    Wait for and return a specific event prefix from BiDi server.

    The following examples match the `browsingContext.domContentLoaded` event:

        wait_for_event(websocket, "browsingContext.domContentLoaded")
        wait_for_event(websocket, "browsingContext.")
        wait_for_event(websocket, "browsingContext")
    """
    return await wait_for_events(websocket, [event_method])


async def wait_for_events(websocket, event_methods: list[str]) -> dict:
    """Wait and return any of the given event prefixes from BiDi server."""
    logger.info(f"Waiting for any of the events '{event_methods}'...")
    return await wait_for_filtered_event(
        websocket, lambda event_response: any([
            event_response["method"].startswith(event_method)
            for event_method in event_methods
        ]))


async def wait_for_filtered_event(
        websocket, filter_lambda: Callable[[dict], bool]) -> dict:
    """Wait and return any of the given event satisfying filter. Ignores """
    def filter_lambda_wrapper(resp):
        if 'type' in resp and resp['type'] == 'event':
            return filter_lambda(resp)

    return await wait_for_message(websocket, filter_lambda_wrapper)


ANY_SHARED_ID = ANY_STR & AnyMatch("f\\..*\\.d\\..*\\.e\\..*")

# Check if the timestamp has the proper order of magnitude between
#  - "2020-01-01 00:00:00" (1577833200000) and
#  - "2100-01-01 00:00:00" (4102441200000).
ANY_TIMESTAMP = ANY_NUMBER & AnyGT(1577833200000) & AnyLT(4102441200000)

# Check if the UUID is a valid UUID v4.
ANY_UUID = AnyFullmatch(
    r'^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$')


def AnyExtending(expected: list | dict):
    """
    When compared to an actual value, `AnyExtending` will verify that the expected
    object is a subset of the actual object, except the arrays, which should be equal.

    # Equal lists should pass.
    >>> assert [1, 2] == AnyExtending([1, 2])

    # Lists should not be extendable.
    >>> assert [1, 2] != AnyExtending([1])
    >>> assert [1] != AnyExtending([1, 2])

    # Nested lists should work as well.
    >>> assert [[1, 2], 3] == AnyExtending([[1, 2], 3])

    # Equal dicts should pass.
    >>> assert {"a": 1} == AnyExtending({"a": 1})

    # Extra fields are allowed in actual dict.
    >>> assert {"a": 1, "b": 2} == AnyExtending({"a": 1})

    # Missing fields are not allowed in actual dict.
    >>> assert {"a": 1, "b": 2} != AnyExtending({"a": 1, "c": 3})

    # Nested dicts should work as well.
    >>> assert {"a": {"a1": 1}, "b": 2} == AnyExtending({"a": {"a1": 1}})

    # Mixed nested dict and list.
    >>> assert {"a": {"a1": [1, 2]}, "b": 2} == AnyExtending({"a": {"a1": [1, 2]}})
    """
    if type(expected) is list:
        list_result = []
        for index, _ in enumerate(expected):
            list_result.append(AnyExtending(expected[index]))
        return list_result

    if type(expected) is dict:
        dict_result = {}
        for key in expected.keys():
            dict_result[key] = AnyExtending(expected[key])
        return AnyWithEntries(dict_result)

    return expected


def assert_images_similar(img1: Image.Image | str,
                          img2: Image.Image | str,
                          percent=0.90):
    """Assert that the given images are similar based on the given percent."""
    if isinstance(img1, str):
        img1 = Image.open(io.BytesIO(base64.b64decode(img1)))
    if isinstance(img2, str):
        img2 = Image.open(io.BytesIO(base64.b64decode(img2)))

    equal_size = (img1.height == img2.height) and (img1.width == img2.width)

    if img1.mode == img2.mode == "RGBA":
        img1_alphas = [pixel[3] for pixel in img1.getdata()]
        img2_alphas = [pixel[3] for pixel in img2.getdata()]
        equal_alphas = img1_alphas == img2_alphas
    else:
        equal_alphas = True

    difference = ImageChops.difference(img1.convert("RGB"),
                                       img2.convert("RGB")).getdata()
    pixel_count = 0
    for pixel in difference:
        if pixel == (0, 0, 0):
            pixel_count += 1

    equal_content = pixel_count / len(difference) > percent

    assert equal_alphas
    assert equal_size
    assert equal_content


def save_png(png_bytes_or_str: bytes | str, output_file: str):
    """Save the given PNG (bytes or base64 string representation) to the given output file."""
    png_bytes = png_bytes_or_str if isinstance(
        png_bytes_or_str, bytes) else base64.b64decode(png_bytes_or_str,
                                                       validate=True)
    Image.open(io.BytesIO(png_bytes)).save(output_file, 'PNG')


def save_pdf(pdf_bytes_or_str: bytes | str, output_file: str):
    pdf_bytes = pdf_bytes_or_str if isinstance(
        pdf_bytes_or_str, bytes) else base64.b64decode(pdf_bytes_or_str,
                                                       validate=True)
    if pdf_bytes[0:4] != b'%PDF':
        raise ValueError('Missing the PDF file signature')

    with open(output_file, 'wb') as f:
        f.write(pdf_bytes)


async def create_request_via_fetch(websocket, context_id: str,
                                   url: str) -> int:
    return await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": f"fetch('{url}')",
                "target": {
                    "context": context_id,
                },
                "awaitPromise": False
            }
        })
