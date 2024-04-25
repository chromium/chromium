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
from pathlib import Path

import pytest
from anys import ANY_STR
from test_helpers import (assert_images_similar, execute_command, get_tree,
                          goto_url, read_JSON_message, send_JSON_command)


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

        # Set a fixed viewport to make the test deterministic.
        await send_JSON_command(
            websocket, {
                "method": "browsingContext.setViewport",
                "params": {
                    "context": context_id,
                    "viewport": {
                        "width": 200,
                        "height": 200,
                    },
                    "devicePixelRatio": 1.0,
                }
            })
        await read_JSON_message(websocket)

        await send_JSON_command(
            websocket, {
                "method": "browsingContext.captureScreenshot",
                "params": {
                    "context": context_id
                }
            })

        resp = await read_JSON_message(websocket)
        assert resp["result"] == {'data': ANY_STR}
        with open(Path(__file__).parent.resolve() / png_filename, 'wb') as im:
            im.write(base64.b64decode(resp["result"]["data"]))

        assert_images_similar(resp["result"]["data"], png_base64)


@pytest.mark.asyncio
async def test_screenshot_element(websocket, context_id, query_selector, html):
    await goto_url(websocket, context_id, html('<div>hello</div>'))

    # Set a fixed viewport to make the test deterministic.
    await send_JSON_command(
        websocket, {
            "method": "browsingContext.setViewport",
            "params": {
                "context": context_id,
                "viewport": {
                    "width": 200,
                    "height": 200,
                },
                "devicePixelRatio": 1.0,
            }
        })
    await read_JSON_message(websocket)

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.captureScreenshot",
            "params": {
                "context": context_id,
                "clip": {
                    "type": "element",
                    "element": await query_selector("div")
                }
            }
        })

    resp = await read_JSON_message(websocket)
    assert resp["result"] == {'data': ANY_STR}

    with open(Path(__file__).parent.resolve() / 'element.png',
              'rb') as image_file:
        assert_images_similar(
            resp["result"]["data"],
            base64.b64encode(image_file.read()).decode('utf-8'))


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

    # Set a fixed viewport to make the test deterministic.
    await send_JSON_command(
        websocket, {
            "method": "browsingContext.setViewport",
            "params": {
                "context": context_id,
                "viewport": {
                    "width": 200,
                    "height": 200,
                },
                "devicePixelRatio": 1.0,
            }
        })
    await read_JSON_message(websocket)

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

        assert_images_similar(resp["result"]["data"], png_base64)


@pytest.mark.asyncio
async def test_screenshot_document(websocket, context_id, query_selector,
                                   html):
    await goto_url(
        websocket, context_id,
        html('<div style="width: 100px; height: 100px; background: red"></div>'
             ))

    # Set a fixed viewport to make the test deterministic.
    await send_JSON_command(
        websocket, {
            "method": "browsingContext.setViewport",
            "params": {
                "context": context_id,
                "viewport": {
                    "width": 50,
                    "height": 50,
                },
                "devicePixelRatio": 1.0,
            }
        })
    await read_JSON_message(websocket)

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.captureScreenshot",
            "params": {
                "context": context_id,
                "origin": "document",
                "clip": {
                    "type": "element",
                    "element": await query_selector("div")
                }
            }
        })

    resp = await read_JSON_message(websocket)
    assert resp["result"] == {'data': ANY_STR}

    with open(Path(__file__).parent.resolve() / 'element-document.png',
              'rb') as image_file:
        assert_images_similar(
            resp["result"]["data"],
            base64.b64encode(image_file.read()).decode('utf-8'))


@pytest.mark.asyncio
async def test_screenshot_viewport_clip_scroll(websocket, context_id,
                                               query_selector, html):
    """
    The test checks the screenshot in a scrolled viewport origin. The clip
    should be relative to the viewport, not the document.
    """
    await goto_url(
        websocket, context_id,
        html(
            '<div id="test_element" style="margin-top: 100px; width: 100px; height: 100px; background: red"></div>'
        ))

    # Set a fixed viewport to make the test deterministic. The element and
    # scrollbars should be there.
    await execute_command(
        websocket, {
            "method": "browsingContext.setViewport",
            "params": {
                "context": context_id,
                "viewport": {
                    "width": 150,
                    "height": 150,
                },
                "devicePixelRatio": 1.0,
            }
        })

    # Scroll the page down.
    await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "window.scrollTo(0, 100)",
                "target": {
                    "context": context_id,
                },
                "awaitPromise": True
            }
        })

    # Get the element's relative position.
    resp = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": """(()=>{
                        const rect = document.getElementById('test_element').getBoundingClientRect();
                        return [rect.x, rect.y];
                    })()""",
                "target": {
                    "context": context_id,
                },
                "awaitPromise": True
            }
        })
    element_relative_x = resp["result"]["value"][0]["value"]
    element_relative_y = resp["result"]["value"][1]["value"]

    # Capture a screenshot with a clip relative to the viewport.
    resp = await execute_command(
        websocket, {
            "method": "browsingContext.captureScreenshot",
            "params": {
                "context": context_id,
                "origin": "viewport",
                "clip": {
                    "height": 100,
                    "type": "box",
                    "width": 100,
                    "x": element_relative_x,
                    "y": element_relative_y
                }
            }
        })

    assert resp == {'data': ANY_STR}

    with open(Path(__file__).parent.resolve() / 'element-document.png',
              'rb') as image_file:
        assert_images_similar(
            resp["data"],
            base64.b64encode(image_file.read()).decode('utf-8'))
