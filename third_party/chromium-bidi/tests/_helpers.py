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

import asyncio
import json
import os
import pytest
import websockets

_command_counter = 1


def get_next_command_id():
    global _command_counter
    _command_counter += 1
    return _command_counter


@pytest.fixture
async def websocket():
    port = os.getenv("PORT", 8080)
    url = f"ws://localhost:{port}"
    async with websockets.connect(url) as connection:
        yield connection


# noinspection PyUnusedFunction
@pytest.fixture
async def context_id(websocket):
    # Note: there can be a race condition between initially created context's
    # events and following subscription commands. Sometimes subscribe is called
    # before the initial context emitted `browsingContext.contextCreated`,
    # `browsingContext.domContentLoaded`, or `browsingContext.load` events,
    # which makes events verification way harder. Navigation command guarantees
    # there will be no follow-up events, as it uses `interactive` flag.
    # TODO: find a way to avoid mentioned race condition properly.

    open_context_id = await get_open_context_id(websocket)
    await goto_url(websocket, open_context_id, "about:blank")
    return open_context_id


@pytest.fixture
async def iframe_id(context_id, websocket):
    page_with_nested_iframe = f'data:text/html,<h1>MAIN_PAGE</h1>' \
                              f'<iframe src="about:blank" />'

    await goto_url(websocket, context_id, page_with_nested_iframe)
    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {"root": context_id}})

    iframe_id = result["contexts"][0]["children"][0]["context"]
    return iframe_id


@pytest.fixture(autouse=True)
async def before_each_test(websocket):
    # This method can be used for browser state preparation.
    assert True


async def subscribe(websocket, event_names, context_ids=None):
    if isinstance(event_names, str):
        event_names = [event_names]
    command = {
        "method": "session.subscribe",
        "params": {
            "events": event_names}}

    if context_ids is not None:
        command["params"]["contexts"] = context_ids

    await execute_command(websocket, command)


# Compares 2 objects recursively.
# Expected value can be a callable delegate, asserting the value.
def recursive_compare(expected, actual):
    if callable(expected):
        return expected(actual)
    assert type(expected) == type(actual)
    if type(expected) is list:
        assert len(expected) == len(actual)
        for index, val in enumerate(expected):
            recursive_compare(expected[index], actual[index])
        return

    if type(expected) is dict:
        assert expected.keys() == actual.keys(), \
            f"Key sets should be the same: " \
            f"\nNot present: {set(expected.keys()) - set(actual.keys())}" \
            f"\nUnexpected: {set(actual.keys()) - set(expected.keys())}"
        for index, val in enumerate(expected):
            recursive_compare(expected[val], actual[val])
        return

    assert expected == actual


def any_string(expected):
    assert isinstance(expected, str), \
        f"'{expected}' should be string, " \
        f"but is {type(expected)} instead."


def any_timestamp(expected):
    assert isinstance(expected, int), \
        f"'{expected}' should be an integer, " \
        f"but is {type(expected)} instead."
    # Check if the timestamp has the proper order of magnitude between
    # "2020-01-01 00:00:00" (1577833200000) and
    # "2100-01-01 00:00:00" (4102441200000).
    assert 1577833200000 < expected < 4102441200000, \
        f"'{expected}' should be in epoch milliseconds format."


# noinspection PyUnusedLocal
def any_value(expected):
    return


# Returns an id of an open context.
async def get_open_context_id(websocket):
    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {}})
    return result["contexts"][0]["context"]


async def send_JSON_command(websocket, command):
    if "id" not in command:
        command_id = get_next_command_id()
        command["id"] = command_id
    await websocket.send(json.dumps(command))


async def read_JSON_message(websocket):
    return json.loads(await websocket.recv())


# Open given URL in the given context.
async def goto_url(websocket, context_id, url):
    await execute_command(websocket, {
        "method": "browsingContext.navigate",
        "params": {
            "url": url,
            "context": context_id,
            "wait": "interactive"}})


# noinspection PySameParameterValue
async def execute_command(websocket, command, result_field="result"):
    command_id = get_next_command_id()
    command["id"] = command_id

    await send_JSON_command(websocket, command)

    while True:
        # Wait for the command to be finished.
        resp = await read_JSON_message(websocket)
        if "id" in resp and resp["id"] == command_id:
            assert result_field in resp, \
                f"Field `{result_field}` should be in the result object:" \
                f"\n {resp}"
            return resp[result_field]


# Wait and return a specific event from Bidi server
async def wait_for_event(websocket, event_method):
    while True:
        event_response = await read_JSON_message(websocket)
        if "method" in event_response and event_response[
            "method"] == event_method:
            return event_response
