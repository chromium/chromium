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
import io

import pytest
from anys import ANY_STR
from PIL import Image, ImageChops
from test_helpers import (execute_command, goto_url, read_JSON_message,
                          send_JSON_command)


def assert_images_equal(img1: Image, img2: Image):
    """Assert that the given images are equal."""
    equal_size = (img1.height == img2.height) and (img1.width == img2.width)

    if img1.mode == img2.mode == "RGBA":
        img1_alphas = [pixel[3] for pixel in img1.getdata()]
        img2_alphas = [pixel[3] for pixel in img2.getdata()]
        equal_alphas = img1_alphas == img2_alphas
    else:
        equal_alphas = True

    equal_content = not ImageChops.difference(img1.convert("RGB"),
                                              img2.convert("RGB")).getbbox()

    assert equal_alphas
    assert equal_size
    assert equal_content


def save_png(png_bytes_or_str: bytes | str, output_file: str):
    """Save the given PNG (bytes or base64 string representation) to the given output file."""
    png_bytes = png_bytes_or_str if isinstance(
        png_bytes_or_str, bytes) else base64.b64decode(png_bytes_or_str)
    Image.open(io.BytesIO(png_bytes)).save(output_file, 'PNG')


@pytest.mark.asyncio
@pytest.mark.parametrize("png_base64", [
    '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',
],
                         ids=[
                             "gradient with alpha channel",
                             "gradient without alpha channel"
                         ])
async def test_screenshot(websocket, context_id, png_base64):
    await goto_url(websocket, context_id,
                   f'data:image/png;base64,{png_base64}')

    command_result = await execute_command(websocket, {
        "method": "cdp.getSession",
        "params": {
            "context": context_id
        }
    })
    session_id = command_result["cdpSession"]

    # Set a fixed viewport to make the test deterministic.
    await execute_command(
        websocket, {
            "method": "cdp.sendCommand",
            "params": {
                "cdpMethod": "Emulation.setDeviceMetricsOverride",
                "cdpParams": {
                    "width": 200,
                    "height": 200,
                    "deviceScaleFactor": 1.0,
                    "mobile": False,
                },
                "cdpSession": session_id
            }
        })

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.captureScreenshot",
            "params": {
                "context": context_id
            }
        })

    resp = await read_JSON_message(websocket)
    assert resp["result"] == {'data': ANY_STR}

    img1 = Image.open(io.BytesIO(base64.b64decode(resp["result"]["data"])))
    img2 = Image.open(io.BytesIO(base64.b64decode(png_base64)))
    assert_images_equal(img1, img2)
