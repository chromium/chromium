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
from urllib.parse import urlparse

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
                    events: list[str] | str,
                    context_ids: list[str] | str | None = None,
                    goog_channel: str | None = None):
    if type(events) is str:
        events = [events]

    if type(context_ids) is str:
        context_ids = [context_ids]

    command: dict = {
        "method": "session.subscribe",
        "params": {
            "events": events,
        }
    }

    if context_ids is not None:
        command["params"]["contexts"] = context_ids
    if goog_channel is not None:
        command["goog:channel"] = goog_channel

    return await execute_command(websocket, command)


async def send_JSON_command(websocket, command: dict) -> int:
    if "id" not in command:
        command["id"] = get_next_command_id()
    await websocket.send(json.dumps(command))
    return command["id"]


async def read_JSON_message(websocket) -> dict:
    logging.info("calling websocket recv", exc_info=True)
    result = json.loads(await websocket.recv())
    logging.info("calling websocket recv: done", exc_info=True)
    return result


async def execute_command(websocket, command: dict, timeout: int = 5) -> dict:
    if "id" not in command:
        command["id"] = get_next_command_id()

    await send_JSON_command(websocket, command)

    logger.info(
        f"Executing command {command['id']} with method '{command['method']}' and params '{command['params']}'..."
    )
    try:
        result = await wait_for_command(websocket, command["id"], timeout)
        logger.info(f"Command {command['id']} finished.")
        # Result can be long, so log it only on-demand.
        logger.debug(f"Result: {result}")
        return result
    except Exception as e:
        logger.info(f"Command {command['id']} failed with {type(e)}, {e}")
        raise


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


def merge_dicts_recursively(default_dict, custom_dict):
    """
    Recursively merges two dictionaries, prioritizing values from the 'custom_dict'.
    Handles nested dictionaries and arrays.

    Args:
        default_dict: The dictionary with default values.
        custom_dict: The dictionary with custom values.

    Returns:
        A new dictionary that is the result of merging the two input dictionaries.
    """

    merged_dict = default_dict.copy()

    for key, custom_value in custom_dict.items():
        if key in default_dict:
            default_value = default_dict[key]
            if isinstance(default_value, dict) and isinstance(
                    custom_value, dict):
                merged_dict[key] = merge_dicts_recursively(
                    default_value, custom_value)
            elif isinstance(default_value, list) and isinstance(
                    custom_value, list):
                # Merge arrays by concatenating and removing duplicates
                merged_dict[key] = list(set(default_value + custom_value))
            else:
                merged_dict[key] = custom_value
        else:
            merged_dict[key] = custom_value

    return merged_dict


def test_merge_dicts_simple():
    default_dict = {"a": 1, "b": 2}
    custom_dict = {"b": 3, "c": 4}
    expected_result = {"a": 1, "b": 3, "c": 4}
    assert merge_dicts_recursively(default_dict,
                                   custom_dict) == expected_result


def test_merge_dicts_nested():
    default_dict = {"a": 1, "b": {"x": 10, "y": 20}}
    custom_dict = {"b": {"y": 30, "z": 40}, "c": 5}
    expected_result = {"a": 1, "b": {"x": 10, "y": 30, "z": 40}, "c": 5}
    assert merge_dicts_recursively(default_dict,
                                   custom_dict) == expected_result


def test_merge_dicts_empty_custom():
    default_dict = {"a": 1, "b": 2}
    custom_dict = {}
    expected_result = {"a": 1, "b": 2}
    assert merge_dicts_recursively(default_dict,
                                   custom_dict) == expected_result


def test_merge_dicts_empty_default():
    default_dict = {}
    custom_dict = {"a": 1, "b": 2}
    expected_result = {"a": 1, "b": 2}
    assert merge_dicts_recursively(default_dict,
                                   custom_dict) == expected_result


def test_merge_dicts_complex_nested():
    default_dict = {"a": 1, "b": {"x": 10, "y": {"p": 100, "q": 200}}, "c": 3}
    custom_dict = {"b": {"y": {"q": 300, "r": 400}, "z": 500}, "d": 4}
    expected_result = {
        "a": 1,
        "b": {
            "x": 10,
            "y": {
                "p": 100,
                "q": 300,
                "r": 400
            },
            "z": 500
        },
        "c": 3,
        "d": 4
    }
    assert merge_dicts_recursively(default_dict,
                                   custom_dict) == expected_result


def test_merge_dicts_with_arrays():
    default_dict = {"a": [1, 2, 3], "b": 2}
    custom_dict = {"a": [3, 4, 5], "c": 4}
    expected_result = {"a": [1, 2, 3, 4, 5], "b": 2, "c": 4}
    assert merge_dicts_recursively(default_dict,
                                   custom_dict) == expected_result


def test_merge_dicts_nested_with_arrays():
    default_dict = {"a": 1, "b": {"x": [10, 20], "y": 20}}
    custom_dict = {"b": {"x": [20, 30], "z": 40}, "c": 5}
    expected_result = {
        "a": 1,
        "b": {
            "x": [10, 20, 30],
            "y": 20,
            "z": 40
        },
        "c": 5
    }
    assert merge_dicts_recursively(default_dict,
                                   custom_dict) == expected_result


def stabilize_key_values(obj,
                         keys_to_stabilize: list[str],
                         known_values: dict[str, str] | None = None):
    """
    Recursively traverses the provided object and replaces the values associated
    with specified keys with stable, predictable placeholders. This ensures
    consistent test assertions even if the original values vary between test
    runs, e.g. UUIDs.

    This stabilization process guarantees:
    - Consistent Replacement: Identical original values associated with a given
      key are replaced by the same placeholder.
    - Key and Order Dependence: The placeholder value depends on the key and the
      order in which unique values are encountered for that key. For example,
      the first unique value for the "id" key is replaced with "id_0",
      the second with "id_1", and so on. Subsequent occurrences of the first
      value are also replaced with "id_0".

    :return a mapping of original values to their corresponding placeholders,
            which can be reused for consistent comparisons across multiple
            objects.

    # empty object
    >>> x={}; stabilize_key_values(x, ['KEY_1']); x;
    {}
    {}

    # empty fields to hash
    >>> x={'KEY_1': 1}; stabilize_key_values(x, ['KEY_1']); x;
    {'1': 'stable_0'}
    {'KEY_1': 'stable_0'}

    # simple replace
    >>> x={'KEY_1': 1}; stabilize_key_values(x, ['KEY_1']); x;
    {'1': 'stable_0'}
    {'KEY_1': 'stable_0'}

    # nested replace
    >>> x={'KEY_1': {'KEY_2': 1}}; stabilize_key_values(x, ['KEY_2']); x;
    {'1': 'stable_0'}
    {'KEY_1': {'KEY_2': 'stable_0'}}

    # list replace
    >>> x={'KEY_1': [1, 2]}; stabilize_key_values(x, ['KEY_1']); x;
    {'[1, 2]': 'stable_0'}
    {'KEY_1': 'stable_0'}

    # nested list replace
    >>> x={'KEY_1': [{'KEY_2': 1}, {'KEY_2': 2}]}; stabilize_key_values(x, ['KEY_2']); x;
    {'1': 'stable_0', '2': 'stable_1'}
    {'KEY_1': [{'KEY_2': 'stable_0'}, {'KEY_2': 'stable_1'}]}

    # different keys same values
    >>> x={'KEY_1': 1, 'KEY_2': 1}; stabilize_key_values(x, ['KEY_1', 'KEY_2']); x;
    {'1': 'stable_0'}
    {'KEY_1': 'stable_0', 'KEY_2': 'stable_0'}

    # same key different values
    >>> x={'KEY_1': 1, 'KEY_2': 2}; stabilize_key_values(x, ['KEY_1', 'KEY_2']); x;
    {'1': 'stable_0', '2': 'stable_1'}
    {'KEY_1': 'stable_0', 'KEY_2': 'stable_1'}

    # same key different values
    >>> x={'KEY_2': 2, 'KEY_1': 1}; stabilize_key_values(x, ['KEY_1', 'KEY_2']); x;
    {'1': 'stable_0', '2': 'stable_1'}
    {'KEY_2': 'stable_1', 'KEY_1': 'stable_0'}

    # value of list
    >>> x={'KEY_1': [1, 2, 1], 'KEY_2': {'KEY_1': [1, 2, 1],}}; stabilize_key_values(x, ['KEY_1']); x;
    {'[1, 2, 1]': 'stable_0'}
    {'KEY_1': 'stable_0', 'KEY_2': {'KEY_1': 'stable_0'}}

    # nested values of list
    >>> x={'KEY_1': [1, 2, {'KEY_2': 1}, {'KEY_2': 2}, {'KEY_2': 1}]}; stabilize_key_values(x, ['KEY_2']); x;
    {'1': 'stable_0', '2': 'stable_1'}
    {'KEY_1': [1, 2, {'KEY_2': 'stable_0'}, {'KEY_2': 'stable_1'}, {'KEY_2': 'stable_0'}]}

    # nested stable values
    >>> x = {'a': {'b': 1, 'c': 2}, 'b': {'b': 1, 'd': 3}}; stabilize_key_values(x, ['b','c','d']); x;
    {'1': 'stable_0', '2': 'stable_1', '3': 'stable_2', '{"b": "stable_0", "d": "stable_2"}': 'stable_3'}
    {'a': {'b': 'stable_0', 'c': 'stable_1'}, 'b': 'stable_3'}

     # nested stable values with lists
    >>> x = {'a': [{'b': 1, 'c': [2]}, {'b': 3, 'c': 4}], 'b': [{'b': 1}]}; stabilize_key_values(x, ['a', 'b','c']); x;
    {'1': 'stable_0', '[2]': 'stable_1', '3': 'stable_2', '4': 'stable_3', '[{"b": "stable_0", "c": "stable_1"}, {"b": "stable_2", "c": "stable_3"}]': 'stable_4', '[{"b": "stable_0"}]': 'stable_5'}
    {'a': 'stable_4', 'b': 'stable_5'}

    # dict in dict
    >>> x = {'a': {'b': 1}, 'c': {'b': 1}}; stabilize_key_values(x, ['a', 'b', 'c']); x
    {'1': 'stable_0', '{"b": "stable_0"}': 'stable_1'}
    {'a': 'stable_1', 'c': 'stable_1'}

    # dict property order does not metter
    >>> x = {'a': 1, 'c': {'b': 1}}; y = {'c': {'b': 1}, 'a': 1}; stabilize_key_values(x, ['a', 'b', 'c']); stabilize_key_values(y, ['a', 'b', 'c']);
    {'1': 'stable_0', '{"b": "stable_0"}': 'stable_1'}
    {'1': 'stable_0', '{"b": "stable_0"}': 'stable_1'}
    """

    if known_values is None:
        known_values = {}

    if len(keys_to_stabilize) == 0:
        return known_values

    if type(obj) is list:
        for index, value in enumerate(obj):
            stabilize_key_values(value, keys_to_stabilize, known_values)

    if type(obj) is dict:
        keys = list(obj.keys())
        keys.sort()
        for key in keys:
            # First stabilize content to produce a stable JSON
            stabilize_key_values(obj[key], keys_to_stabilize, known_values)

            if key in keys_to_stabilize:
                old_value = obj[key]
                value_key = json.dumps(old_value, sort_keys=True)

                if value_key in known_values:
                    new_value = known_values[value_key]
                else:
                    new_value = f"stable_{len(known_values)}"
                    known_values[value_key] = new_value
                obj[key] = new_value

    return known_values


def get_origin(url):
    """ Return the origin for the given url."""
    parts = urlparse(url)
    return parts.scheme + '://' + parts.netloc


def to_base64(str):
    return base64.b64encode(str.encode('utf-8')).decode()
