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
# Not implemented yet.
async def _ignore_test_consoleLog_logEntryAddedEventEmitted(websocket,
      context_id):
    # Send command.
    await send_JSON_command(websocket, {
        "id": 33,
        "method": "script.evaluate",
        "params": {
            "expression": "console.log('some log message')",
            "target": {"context": context_id}}})

    # Assert "log.entryAdded" event emitted.
    resp = await read_JSON_message(websocket)
    recursiveCompare({
        "method": "log.entryAdded",
        "params": {
            # BaseLogEntry
            "level": "info",
            "text": "some log message",
            "timestamp": "__any_value__",
            "stackTrace": [{
                "url": "__any_value__",
                "functionName": "",
                "lineNumber": 0,
                "columnNumber": 8}],
            # ConsoleLogEntry
            "type": "console",
            "method": "log",
            # TODO: replace `PROTO.context` with `realm`.
            "PROTO.context": context_id,
            "args": [{
                "type": "string",
                "value": "some log message"}]}
    }, resp, ["timestamp", "url"])

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id": 33,
        "result": {"type": "undefined"}}


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_consoleInfo_logEntryWithMethodInfoEmitted(websocket,
      context_id):
    # Send command.
    await send_JSON_command(websocket, {
        "id": 43,
        "method": "script.evaluate",
        "params": {
            "expression": "console.info('some log message')",
            "target": {"context": context_id}}})

    # Assert method "info".
    resp = await read_JSON_message(websocket)

    assert resp["method"] == "log.entryAdded"
    assert resp["params"]["method"] == "info"

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id": 43,
        "result": {"type": "undefined"}}


@pytest.mark.asyncio
# Not implemented yet.
async def _ignore_test_consoleError_logEntryWithMethodErrorEmitted(websocket,
      context_id):
    # Send command.
    await send_JSON_command(websocket, {
        "id": 44,
        "method": "script.evaluate",
        "params": {
            "expression": "console.error('some log message')",
            "target": {"context": context_id}}})

    # Assert method "error".
    resp = await read_JSON_message(websocket)

    assert resp["method"] == "log.entryAdded"
    assert resp["params"]["method"] == "error"

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id": 44,
        "result": {"type": "undefined"}}
