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

import itertools
import json

from anys import ANY_NUMBER, ANY_STR, AnyContains, AnyGT, AnyLT, AnyWithEntries

_command_counter = itertools.count(1)


def get_next_command_id():
    """
    >>> x = get_next_command_id()
    >>> y = get_next_command_id()
    >>> assert x + 1 == y
    """
    return next(_command_counter)


async def subscribe(websocket, event_names, context_ids=None, channel=None):
    if isinstance(event_names, str):
        event_names = [event_names]
    if isinstance(context_ids, str):
        context_ids = [context_ids]
    command = {
        "method": "session.subscribe",
        "params": {
            "events": event_names
        }
    }

    if context_ids is not None:
        command["params"]["contexts"] = context_ids
    if channel is not None:
        command["channel"] = channel

    await execute_command(websocket, command)


ANY_SHARED_ID = ANY_STR & AnyContains("_element_")

# Check if the timestamp has the proper order of magnitude between
# "2020-01-01 00:00:00" (1577833200000) and
# "2100-01-01 00:00:00" (4102441200000).
ANY_TIMESTAMP = ANY_NUMBER & AnyGT(1577833200000) & AnyLT(4102441200000)


async def send_JSON_command(websocket, command):
    if "id" not in command:
        command_id = get_next_command_id()
        command["id"] = command_id
    await websocket.send(json.dumps(command))
    return command["id"]


async def read_JSON_message(websocket):
    return json.loads(await websocket.recv())


# Sets the current page content without navigation.
async def set_html_content(websocket, context_id, html_content):
    await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": f"document.body.innerHTML = '{html_content}'",
                "target": {
                    "context": context_id,
                },
                "awaitPromise": True
            }
        })


# Get the tree of browsing contexts.
async def get_tree(websocket, context_id=None):
    params = {}
    if context_id is not None:
        params["root"] = context_id
    return await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": params
    })


# Open given URL in the given context.
async def goto_url(websocket, context_id, url, wait="interactive"):
    return await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url,
                "context": context_id,
                "wait": wait
            }
        })


# noinspection PySameParameterValue
async def execute_command(websocket, command):
    command_id = get_next_command_id()
    command["id"] = command_id

    await send_JSON_command(websocket, command)

    while True:
        # Wait for the command to be finished.
        resp = await read_JSON_message(websocket)
        if "id" in resp and resp["id"] == command_id:
            if "result" in resp:
                return resp["result"]
            raise Exception({
                "error": resp["error"],
                "message": resp["message"]
            })


# Wait and return a specific event from Bidi server
async def wait_for_event(websocket, event_method):
    while True:
        event_response = await read_JSON_message(websocket)
        if "method" in event_response and event_response[
                "method"] == event_method:
            return event_response


def AnyExtending(expected):
    """
    When compared to an actual value, `AnyExtending` will verify that the expected
    object is a subset of the actual object, except the arrays, which should be equal.

    # Lists should not be extendable.
    >>> assert [1, 2] != AnyExtending([1])
    >>> assert [1] != AnyExtending([1, 2])

    # Equal dicts should pass.
    >>> assert {"a": 1} == AnyExtending({"a": 1})

    # Extra fields are allowed in actual dict.
    >>> assert {"a": 1, "b": 2} == AnyExtending({"a": 1})

    # Missing fields are not allowed in actual dict.
    >>> assert {"a": 1, "b": 2} != AnyExtending({"a": 1, "c": 3})
    """
    if type(expected) is list:
        result = []
        for index, _ in enumerate(expected):
            result.append(AnyExtending(expected[index]))
        return result

    if type(expected) is dict:
        result = {}
        for key in expected.keys():
            result[key] = AnyExtending(expected[key])
        return AnyWithEntries(result)

    return expected
