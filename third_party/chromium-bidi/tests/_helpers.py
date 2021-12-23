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
    port = os.getenv('PORT', 8080)
    url = f'ws://localhost:{port}'
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


@pytest.fixture(autouse=True)
async def before_each_test(websocket):
    # This method can be used for browser state preparation.
    assert True


async def subscribe(websocket, event_names, context_ids=None):
    command = {
        "method": "session.subscribe",
        "params": {
            "events": event_names}}

    if context_ids is not None:
        command["params"]["contexts"] = context_ids

    await execute_command(websocket, command)


# Compares 2 objects recursively ignoring values of specific attributes.
def recursiveCompare(expected, actual, ignore_attributes):
    assert type(expected) == type(actual)
    if type(expected) is list:
        assert len(expected) == len(actual)
        for index, val in enumerate(expected):
            recursiveCompare(expected[index], actual[index], ignore_attributes)
        return

    if type(expected) is dict:
        assert expected.keys() == actual.keys()
        for index, val in enumerate(expected):
            if val not in ignore_attributes:
                recursiveCompare(expected[val], actual[val], ignore_attributes)
        return

    assert expected == actual


# Returns an id of an open context.
async def get_open_context_id(websocket):
    result = await execute_command(websocket, {
        "method": "browsingContext.getTree",
        "params": {}})
    return result['contexts'][0]['context']


async def send_JSON_command(websocket, command):
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
async def execute_command(websocket, command, result_field='result'):
    command_id = get_next_command_id()
    command['id'] = command_id

    await send_JSON_command(websocket, command)

    while True:
        # Wait for the command to be finished.
        resp = await read_JSON_message(websocket)
        if 'id' in resp and resp['id'] == command_id:
            assert result_field in resp
            return resp[result_field]
