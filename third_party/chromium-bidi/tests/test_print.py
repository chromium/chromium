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
from anys import ANY_STR
from test_helpers import goto_url, read_JSON_message, send_JSON_command


@pytest.mark.asyncio
async def test_print(websocket, context_id):
    await goto_url(websocket, context_id, 'about:blank')

    await send_JSON_command(
        websocket, {
            "id": 1,
            "method": "browsingContext.print",
            "params": {
                "context": context_id,
                "background": False,
                "orientation": "portrait",
                "page": {
                    "width": 800,
                    "height": 600,
                },
                "pageRanges": ["1", "1-", "-1", "1-1", 1],
                "scale": 1.0,
            }
        })

    resp = await read_JSON_message(websocket)

    # 'data' is not deterministic.
    # There is always ~half a dozen characters that differ between runs.
    assert resp == {'id': 1, 'result': {'data': ANY_STR}}
