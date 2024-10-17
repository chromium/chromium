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
from test_helpers import execute_command, goto_url


@pytest.mark.asyncio
async def test_print_top_level_context(websocket, context_id, html):
    await goto_url(websocket, context_id, html())

    print_result = await execute_command(
        websocket, {
            "method": "browsingContext.print",
            "params": {
                "context": context_id,
                "page": {
                    "width": 100,
                    "height": 100,
                },
                "scale": 1.0,
            }
        })

    # 'data' is not deterministic, ~a dozen characters differ between runs.
    assert print_result["data"] == ANY_STR

    try:
        await goto_url(websocket, context_id,
                       f'data:application/pdf,base64;{print_result["data"]}')
    except Exception as e:
        assert e.args[0] == {
            'error': 'unknown error',
            'message': 'net::ERR_ABORTED'
        }
        pytest.xfail("PDF viewer not available in headless.")


@pytest.mark.asyncio
async def test_print_iframe(websocket, iframe_id, html):
    with pytest.raises(
            Exception,
            match=str({
                'error': 'unsupported operation',
                'message': 'Printing of non-top level contexts is not supported'
            })):
        await execute_command(websocket, {
            "method": "browsingContext.print",
            "params": {
                "context": iframe_id,
            }
        })
