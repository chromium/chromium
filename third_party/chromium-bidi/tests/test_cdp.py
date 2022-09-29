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

from _helpers import *


@pytest.mark.asyncio
async def test_cdp_sendCommand_commandResultReturned(websocket):
    command_result = await execute_command(websocket, {
        "method": "PROTO.cdp.sendCommand",
        "params": {
            "cdpMethod": "Target.getTargets",
            "cdpParams": {}}})

    recursive_compare({
        "targetInfos": any_value},
        command_result)


@pytest.mark.asyncio
async def test_cdp_subscribeCdpEvents_cdpEventReceived(websocket, context_id):
    await subscribe(websocket, "PROTO.cdp.eventReceived")

    command_result = await execute_command(websocket, {
        "method": "PROTO.cdp.getSession",
        "params": {"context": context_id}})

    session_id = command_result["cdpSession"]

    await send_JSON_command(websocket, {
        "method": "PROTO.cdp.sendCommand",
        "params": {
            "cdpMethod": "Runtime.evaluate",
            "cdpParams": {
                "expression": "console.log(1)",
            },
            "cdpSession": session_id}})

    resp = await read_JSON_message(websocket)

    recursive_compare({
        "method": "PROTO.cdp.eventReceived",
        "params": {
            "cdpMethod": "Runtime.consoleAPICalled",
            "cdpParams": {
                "type": "log",
                "args": [{
                    "type": "number",
                    "value": 1,
                    "description": "1"}],
                "executionContextId": any_value,
                "timestamp": any_value,
                "stackTrace": any_value},
            "cdpSession": session_id}},
        resp)
