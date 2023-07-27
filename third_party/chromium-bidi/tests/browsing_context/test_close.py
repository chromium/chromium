# Copyright 2023 Google LLC.
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
import pytest
from test_helpers import (get_tree, read_JSON_message, send_JSON_command,
                          subscribe)


@pytest.mark.asyncio
async def test_browsingContext_close(websocket, context_id):
    await subscribe(websocket, ["browsingContext.contextDestroyed"])

    command_id = await send_JSON_command(websocket, {
        "method": "browsingContext.close",
        "params": {
            "context": context_id
        }
    })

    # Assert "browsingContext.contextCreated" event emitted.
    resp = await read_JSON_message(websocket)
    assert resp == {
        'type': 'event',
        "method": "browsingContext.contextDestroyed",
        "params": {
            "context": context_id,
            "parent": None,
            "url": "about:blank",
            "children": None
        }
    }

    resp = await read_JSON_message(websocket)
    assert resp == {"type": "success", "id": command_id, "result": {}}

    result = await get_tree(websocket)

    # Assert context is closed.
    assert result == {'contexts': []}
