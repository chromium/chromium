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
from pathlib import Path

import pytest
from anys import ANY_STR
from PIL import Image, ImageChops
from test_helpers import (execute_command, get_tree, goto_url,
                          read_JSON_message, send_JSON_command)


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
        png_bytes_or_str, bytes) else base64.b64decode(png_bytes_or_str,
                                                       validate=True)
    Image.open(io.BytesIO(png_bytes)).save(output_file, 'PNG')


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "png_filename", [
        "gradient_with_alpha_channel.png",
        "gradient_without_alpha_channel.png",
    ],
    ids=["gradient with alpha channel", "gradient without alpha channel"])
async def test_screenshot(websocket, context_id, png_filename):
    with open(Path(__file__).parent.resolve() / png_filename,
              'rb') as image_file:
        png_base64 = base64.b64encode(image_file.read()).decode('utf-8')

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


@pytest.mark.asyncio
@pytest.mark.skip(reason="TODO: fails on CI")
async def test_screenshot_oopif(websocket, context_id, html, iframe):
    await goto_url(websocket,
                   context_id,
                   html(iframe("https://www.example.com")),
                   wait="complete")

    iframe_context_id = (await get_tree(
        websocket, context_id))["contexts"][0]["children"][0]["context"]
    assert iframe_context_id != context_id

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
                "context": iframe_context_id
            }
        })

    resp = await read_JSON_message(websocket)
    assert resp["result"] == {'data': ANY_STR}

    png_filename = "oopif.png"
    with open(Path(__file__).parent.resolve() / png_filename,
              'rb') as image_file:
        png_base64 = base64.b64encode(image_file.read()).decode('utf-8')

        img1 = Image.open(io.BytesIO(base64.b64decode(resp["result"]["data"])))
        img2 = Image.open(io.BytesIO(base64.b64decode(png_base64)))
        assert_images_equal(img1, img2)
