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
from anys import ANY_DICT, ANY_STR
from test_helpers import (AnyExtending, execute_command, get_tree,
                          read_JSON_message, send_JSON_command, subscribe)


@pytest.mark.asyncio
async def test_addPreloadScript_nonExistingContext_exceptionReturned(
        websocket, context_id):
    DUMMY_CONTEXT = 'UNKNOWN_CONTEXT_ID'
    assert DUMMY_CONTEXT != context_id

    with pytest.raises(Exception) as exception_info:
        await execute_command(
            websocket, {
                "method": "script.addPreloadScript",
                "params": {
                    "expression": "() => { window.foo='bar'; }",
                    "context": DUMMY_CONTEXT,
                }
            })
    assert {
        'error': 'no such frame',
        'message': f'Context {DUMMY_CONTEXT} not found'
    } == exception_info.value.args[0]


@pytest.mark.asyncio
async def test_addPreloadScript_setGlobalVariable(websocket, context_id, html):
    result = await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "expression": "() => { window.foo='bar'; }",
                "context": context_id,
            }
        })
    assert result == {'script': ANY_STR}

    await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": html(),
                "wait": "complete",
                "context": context_id
            }
        })

    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "window.foo",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })
    assert result["result"] == {"type": "string", "value": 'bar'}


@pytest.mark.asyncio
async def test_addPreloadScript_logging(websocket, context_id, html):
    await subscribe(websocket, "log.entryAdded")

    result = await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "expression": "() => console.log('my preload script')",
                "context": context_id,
            }
        })
    assert result == {'script': ANY_STR}

    command_id = await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": html(),
                "wait": "complete",
                "context": context_id
            }
        })

    # Log event should happen before navigation.
    result = await read_JSON_message(websocket)
    assert result == AnyExtending({
        "method": "log.entryAdded",
        "params": {
            "args": [{
                "type": "string",
                "value": "my preload script"
            }]
        }
    })

    # Assert navigation is finished.
    result = await read_JSON_message(websocket)
    assert result == AnyExtending({"id": command_id})


@pytest.mark.asyncio
async def test_addPreloadScript_multipleScriptsAddedToSameContext(
        websocket, context_id, html):
    await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "expression": "() => { window.foo1=1; }",
                "context": context_id,
            }
        })
    await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "expression": "() => { window.foo2=2; }",
                "context": context_id,
            }
        })

    await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": html(),
                "wait": "complete",
                "context": context_id
            }
        })

    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "window.foo1 + window.foo2",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })
    assert result["result"] == {"type": "number", "value": 3}


@pytest.mark.asyncio
async def test_addPreloadScript_sameScript_multipleTimes_AddedToSameContext(
        websocket, context_id, html):
    EXPRESSION = "() => { window.foo1 = (window.foo1 ?? 0) + 1; }"

    await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "expression": EXPRESSION,
                "context": context_id,
            }
        })
    await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "expression": EXPRESSION,
                "context": context_id,
            }
        })

    await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": html(),
                "wait": "complete",
                "context": context_id
            }
        })

    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "window.foo1",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })
    assert result["result"] == {"type": "number", "value": 2}


@pytest.mark.asyncio
@pytest.mark.skip(reason="fail")
async def test_addPreloadScript_loadedInNewIframes(websocket, context_id,
                                                   url_all_origins, html):
    await subscribe(websocket, "log.entryAdded")

    await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "expression": "() => console.log('my preload script')",
                "context": context_id,
            }
        })

    command_id = await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": html(),
                "wait": "complete",
                "context": context_id
            }
        })

    # Log event should happen before navigation.
    result = await read_JSON_message(websocket)
    assert result == AnyExtending({
        "method": "log.entryAdded",
        "params": {
            "args": [{
                "type": "string",
                "value": "my preload script"
            }]
        }
    })

    # Assert navigation is finished.
    result = await read_JSON_message(websocket)
    assert result == {"id": command_id, "result": ANY_DICT}

    # Create a new iframe within the same context.
    result = await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": f"""const iframe = document.createElement('iframe');
                    iframe.src = '{url_all_origins}';
                    document.body.appendChild(iframe);""",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })

    # Asset that the preload script is executed in the new iframe.
    result = await read_JSON_message(websocket)
    assert result == AnyExtending({
        "method": "log.entryAdded",
        "params": {
            "args": [{
                "type": "string",
                "value": "my preload script"
            }]
        }
    })

    result = await read_JSON_message(websocket)


@pytest.mark.asyncio
async def test_addPreloadScript_loadedInNewIframes_withChildScript(
        websocket, context_id, html):
    await subscribe(websocket, "log.entryAdded")

    await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "expression": "() => console.log('my preload script')",
                "context": context_id,
            }
        })

    command_id = await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": html(),
                "wait": "complete",
                "context": context_id
            }
        })

    # Log event should happen before navigation.
    result = await read_JSON_message(websocket)
    assert result == AnyExtending({
        "method": "log.entryAdded",
        "params": {
            "args": [{
                "type": "string",
                "value": "my preload script"
            }]
        }
    })

    # Assert navigation is finished.
    result = await read_JSON_message(websocket)
    assert result == {"id": command_id, "result": ANY_DICT}

    # Create a new iframe within the same context.
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": f"""const iframe = document.createElement('iframe');
                    iframe.src = `{html('<script>console.log("I am the child");</script>')}`;
                    document.body.appendChild(iframe);""",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })

    # Asset that the preload script is executed in the new iframe.
    result = await read_JSON_message(websocket)
    assert result == AnyExtending({
        "method": "log.entryAdded",
        "params": {
            "args": [{
                "type": "string",
                "value": "my preload script"
            }]
        }
    })

    # Asset that the child script is executed last.
    result = await read_JSON_message(websocket)
    assert result == AnyExtending({
        "method": "log.entryAdded",
        "params": {
            "args": [{
                "type": "string",
                "value": "I am the child"
            }]
        }
    })


@pytest.mark.asyncio
@pytest.mark.parametrize("globally", [True, False])
async def test_addPreloadScript_loadedInMultipleContexts(
        websocket, context_id, globally, html):
    await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "expression": "() => { window.foo='bar'; }",
            } | ({} if globally else {
                "context": context_id
            })
        })

    await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": html(),
                "wait": "complete",
                "context": context_id
            }
        })

    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "window.foo",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })
    assert result["result"] == {"type": "string", "value": 'bar'}


@pytest.mark.asyncio
@pytest.mark.skip(reason="not implemented")
async def test_addPreloadScriptGlobally_loadedInMultipleContexts_withIframes(
        websocket, context_id, url_all_origins, html):
    await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "expression": "() => { window.foo='bar'; }",
            }
        })

    await execute_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": html(),
                "wait": "complete",
                "context": context_id
            }
        })

    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "window.foo",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })
    assert result["result"] == {"type": "string", "value": 'bar'}

    # Create a new iframe within the same context.
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": f"""const iframe = document.createElement('iframe');
                    iframe.src = `{url_all_origins}`;
                    document.body.appendChild(iframe);""",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })
    iframe_context_id = (await get_tree(
        websocket, context_id))["contexts"][0]["children"][0]["context"]
    assert iframe_context_id != context_id

    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "window.foo",
                "target": {
                    "context": iframe_context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })
    assert result["result"] == {"type": "string", "value": 'bar'}
