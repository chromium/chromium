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
async def test_iframe_navigateWaitNone_navigated(websocket, iframe_id):
    await subscribe(websocket, ["browsingContext.domContentLoaded",
                                "browsingContext.load"])
    # Send command.
    await send_JSON_command(websocket, {
        "id": 13,
        "method": "browsingContext.navigate",
        "params": {
            "url": "data:text/html,<h2>test</h2>",
            "wait": "none",
            "context": iframe_id}})

    # Assert command done.
    resp = await read_JSON_message(websocket)
    navigation_id = resp["result"]["navigation"]
    assert resp == {
        "id": 13,
        "result": {
            "navigation": navigation_id,
            "url": "data:text/html,<h2>test</h2>"}}

    # Wait for `browsingContext.load` event.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "method": "browsingContext.load",
        "params": {
            "context": iframe_id,
            "navigation": navigation_id}}

    # Wait for `browsingContext.domContentLoaded` event.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "method": "browsingContext.domContentLoaded",
        "params": {
            "context": iframe_id,
            "navigation": navigation_id}}


@pytest.mark.asyncio
async def test_iframe_navigateWaitInteractive_navigated(websocket, iframe_id):
    await subscribe(websocket, ["browsingContext.domContentLoaded",
                                "browsingContext.load"])

    # Send command.
    command = {
        "id": 14,
        "method": "browsingContext.navigate",
        "params": {
            "url": "data:text/html,<h2>test</h2>",
            "wait": "interactive",
            "context": iframe_id}}
    await send_JSON_command(websocket, command)

    # Wait for `browsingContext.load` event.
    resp = await read_JSON_message(websocket)
    navigation_id = resp["params"]["navigation"]
    assert resp == {
        "method": "browsingContext.load",
        "params": {
            "context": iframe_id,
            "navigation": navigation_id}}

    # Wait for `browsingContext.domContentLoaded` event.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "method": "browsingContext.domContentLoaded",
        "params": {
            "context": iframe_id,
            "navigation": navigation_id}}

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id": 14,
        "result": {
            "navigation": navigation_id,
            "url": "data:text/html,<h2>test</h2>"}}


@pytest.mark.asyncio
async def test_iframe_navigateWaitComplete_navigated(websocket, iframe_id):
    await subscribe(websocket, ["browsingContext.domContentLoaded",
                                "browsingContext.load"])

    # Send command.
    command = {
        "id": 15,
        "method": "browsingContext.navigate",
        "params": {
            "url": "data:text/html,<h2>test</h2>",
            "wait": "complete",
            "context": iframe_id}}
    await send_JSON_command(websocket, command)

    # Wait for `browsingContext.load` event.
    resp = await read_JSON_message(websocket)
    navigation_id = resp["params"]["navigation"]
    assert resp == {
        "method": "browsingContext.load",
        "params": {
            "context": iframe_id,
            "navigation": navigation_id}}

    # Assert command done.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "id": 15,
        "result": {
            "navigation": navigation_id,
            "url": "data:text/html,<h2>test</h2>"}}

    # Wait for `browsingContext.domContentLoaded` event.
    resp = await read_JSON_message(websocket)
    assert resp == {
        "method": "browsingContext.domContentLoaded",
        "params": {
            "context": iframe_id,
            "navigation": navigation_id}}
