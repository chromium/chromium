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


@pytest.fixture
async def websocket():
    port = os.getenv('PORT', 8080)
    url = f'ws://localhost:{port}'
    async with websockets.connect(url) as connection:
        yield connection


@pytest.fixture(autouse=True)
async def before_each_test(websocket):
    # Read initial event `browsingContext.contextCreated`
    resp = await read_JSON_message(websocket)
    assert resp['method'] == 'browsingContext.contextCreated'


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


# Returns the only open contextID.
# Throws an exception the context is not unique.
async def get_open_context_id(websocket):
    # Send "browsingContext.getTree" command.
    command = {"id": 9999, "method": "browsingContext.getTree", "params": {}}
    await send_JSON_command(websocket, command)
    # Get open context ID.
    resp = await read_JSON_message(websocket)
    assert resp['id'] == 9999
    [context] = resp['result']['contexts']
    context_id = context['context']
    return context_id


async def send_JSON_command(websocket, command):
    await websocket.send(json.dumps(command))


async def read_JSON_message(websocket):
    return json.loads(await websocket.recv())


# Open given URL in the given context.
async def goto_url(websocket, context_id, url):
    # Send "browsingContext.navigate" command.
    command = {
        "id": 9998,
        "method": "browsingContext.navigate",
        "params": {
            "url": url,
            "context": context_id}}
    await send_JSON_command(websocket, command)

    # Assert "browsingContext.navigate" command done.
    resp = await read_JSON_message(websocket)
    assert resp["id"] == 9998
    return context_id