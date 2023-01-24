# Copyright 2022 Google LLC.
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

import json
import os

import requests
import websockets


async def get_websocket():
    port = os.getenv('PORT', 8080)

    # Try to connect directly via WebSocket. If not available, connect via
    # WebDriver Classic with BiDi capabilities.
    try:
        websocket = await websockets.connect(f'ws://localhost:{port}/session')
        # Init BiDi session.
        await run_and_wait_command(
            {
                "id": 0,
                "method": "session.new",
                "params": {}
            }, websocket)
        return websocket
    except websockets.exceptions.InvalidStatusCode:
        # Fall back. Try to connect via WebDriver Classic, and upgrade to BiDi.
        # Create a WebDriver Classic session with BiDi capabilities.
        new_session = requests.post(f'http://localhost:{port}/session',
                                    json={
                                        "capabilities": {
                                            "alwaysMatch": {
                                                "acceptInsecureCerts": True,
                                                "webSocketUrl": True
                                            }
                                        }
                                    }).json()

        # Get BiDi websocket URL.
        ws_url = new_session["value"]["capabilities"]["webSocketUrl"]
        return await websockets.connect(ws_url)


async def send_JSON_command(command, websocket):
    await websocket.send(json.dumps(command))


async def read_JSON_message(websocket):
    return json.loads(await websocket.recv())


async def run_and_wait_command(command, websocket):
    command_id = command["id"]
    await send_JSON_command(command, websocket)

    # Read messages until the sent command is done.
    while True:
        message = await read_JSON_message(websocket)
        if "id" in message and message["id"] == command_id:
            return message
