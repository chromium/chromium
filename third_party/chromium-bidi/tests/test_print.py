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

import base64

import pytest
from anys import ANY_STR
from test_helpers import execute_command, goto_url


def save_pdf(pdf_bytes_or_str: bytes | str, output_file: str):
    pdf_bytes = pdf_bytes_or_str if isinstance(
        pdf_bytes_or_str, bytes) else base64.b64decode(pdf_bytes_or_str,
                                                       validate=True)
    if pdf_bytes[0:4] != b'%PDF':
        raise ValueError('Missing the PDF file signature')

    with open(output_file, 'wb') as f:
        f.write(pdf_bytes)


@pytest.mark.asyncio
async def test_print(websocket, context_id, html):
    await goto_url(websocket, context_id, html())

    result = await execute_command(
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
    assert result['data'] == ANY_STR
