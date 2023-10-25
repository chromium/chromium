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
from test_helpers import (ANY_UUID, AnyExtending, execute_command, goto_url,
                          read_JSON_message, send_JSON_command, subscribe)


@pytest.mark.asyncio
async def test_preloadScript_add_setGlobalVariable(websocket, context_id,
                                                   html):
    # Preload script creates if not created and adds `PRELOAD_SCRIPT` to a
    # global array `SOME_VAR` .
    result = await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "functionDeclaration": """
                    () => {
                        window.SOME_VAR=[
                            ...(window.SOME_VAR ?? []),
                            'PRELOAD_SCRIPT'];
                    }""",
            }
        })
    assert result == {'script': ANY_UUID}

    # Navigate to a page with a script, which creates if not created and adds
    # `HTML_SCRIPT` to a global array `SOME_VAR` .
    await goto_url(
        websocket, context_id,
        html("<script>"
             "window.SOME_VAR=["
             "...(window.SOME_VAR ?? []), "
             "'HTML_SCRIPT']"
             "</script>"))

    # Assert scripts were run in the right order.
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "window.SOME_VAR.join(', ')",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })
    assert result["result"] == {
        "type": "string",
        "value": 'PRELOAD_SCRIPT, HTML_SCRIPT'
    }


@pytest.mark.asyncio
async def test_preloadScript_add_logging(websocket, context_id, html):
    await subscribe(websocket, ["log.entryAdded"])

    result = await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "functionDeclaration": "() => console.log('my preload script')",
            }
        })
    assert result == {'script': ANY_UUID}

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
        "type": "event",
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
    assert result == AnyExtending({"type": "success", "id": command_id})


@pytest.mark.asyncio
async def test_preloadScript_add_multipleScripts(websocket, context_id, html):
    result = await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "functionDeclaration": """
                    () => {
                        window.SOME_VAR=[
                            ...(window.SOME_VAR ?? []),
                            'PRELOAD_SCRIPT_1'];
                   }""",
            }
        })
    id1 = result['script']
    assert id1 == ANY_UUID

    result = await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "functionDeclaration": """
                    () => {
                        window.SOME_VAR=[
                            ...(window.SOME_VAR ?? []),
                            'PRELOAD_SCRIPT_2'];
                    }""",
            }
        })
    id2 = result['script']
    assert id2 == ANY_UUID

    # Assert scripts have different IDs.
    assert id1 != id2

    await goto_url(websocket, context_id, html())

    # Assert scripts were run in the right order.
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "window.SOME_VAR.join(', ')",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })
    assert result["result"] == {
        "type": "string",
        "value": "PRELOAD_SCRIPT_1, PRELOAD_SCRIPT_2"
    }


@pytest.mark.asyncio
async def test_preloadScript_add_sameScriptMultipleTimes(
        websocket, context_id, html):
    preload_script = """
        () => {
            window.SOME_VAR=[
                ...(window.SOME_VAR ?? []),
                'PRELOAD_SCRIPT'];
        }"""

    result = await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "functionDeclaration": preload_script
            }
        })
    id1 = result['script']
    assert id1 == ANY_UUID

    result = await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "functionDeclaration": preload_script
            }
        })
    id2 = result['script']
    assert id2 == ANY_UUID

    # Assert scripts have different IDs.
    assert id1 != id2

    await goto_url(websocket, context_id, html())

    # Assert scripts were run in the right order.
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "window.SOME_VAR.join(', ')",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })
    assert result["result"] == {
        "type": "string",
        "value": "PRELOAD_SCRIPT, PRELOAD_SCRIPT"
    }


@pytest.mark.asyncio
async def test_preloadScript_add_loadedInNewIframes(websocket, context_id,
                                                    url_all_origins, html,
                                                    read_sorted_messages):
    await subscribe(websocket, ["log.entryAdded"])

    await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "functionDeclaration": "() => console.log('my preload script')",
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

    # Log event should happen before navigation is finished.
    result = await read_JSON_message(websocket)
    assert result == AnyExtending({
        "type": "event",
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
    assert result == {"type": "success", "id": command_id, "result": ANY_DICT}

    # Create a new iframe within the same context.
    command_id = await send_JSON_command(
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

    # Event order is not guaranteed, so read 2 messages, sort them and assert.
    [command_result, log_entry_added] = await read_sorted_messages(2)

    assert command_result == {
        "type": "success",
        "id": command_id,
        "result": ANY_DICT
    }

    # Asset that the preload script is executed in the new iframe.
    assert log_entry_added == AnyExtending({
        "type": "event",
        "method": "log.entryAdded",
        "params": {
            "args": [{
                "type": "string",
                "value": "my preload script"
            }]
        }
    })


@pytest.mark.asyncio
async def test_preloadScript_add_loadedInNewIframes_withChildScript(
        websocket, context_id, html):
    await subscribe(websocket, ["log.entryAdded"])

    await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "functionDeclaration": "() => console.log('my preload script')",
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
        "type": "event",
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
    assert result == {"type": "success", "id": command_id, "result": ANY_DICT}

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
        "type": "event",
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
        "type": "event",
        "method": "log.entryAdded",
        "params": {
            "args": [{
                "type": "string",
                "value": "I am the child"
            }]
        }
    })


@pytest.mark.asyncio
async def test_preloadScript_add_loadedInMultipleContexts(
        websocket, context_id, html):
    await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "functionDeclaration": "() => { window.foo='bar'; }",
            }
        })

    await goto_url(websocket, context_id, html())

    response = await execute_command(
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
    assert response["result"] == {"type": "string", "value": 'bar'}


@pytest.mark.asyncio
async def test_preloadScript_add_loadedInMultipleContexts_withIframes(
        websocket, context_id, url_all_origins, html, read_sorted_messages):
    await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "functionDeclaration": "() => { window.foo='bar'; }",
            }
        })

    await goto_url(websocket, context_id, html())

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

    # Needed to make sure the iFrame loaded.
    await subscribe(websocket, ["browsingContext.load"])

    # Create a new iframe within the same context.
    command_id = await send_JSON_command(
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

    # Depending on the URL, the iframe can be loaded before or after the script
    # is done.
    [command_result, browsing_context_load] = await read_sorted_messages(2)
    assert command_result == {
        "type": "success",
        "id": command_id,
        "result": ANY_DICT
    }
    assert browsing_context_load == {
        'type': 'event',
        "method": "browsingContext.load",
        "params": AnyExtending({
            "context": ANY_STR,
            "url": url_all_origins
        })
    }

    iframe_context_id = browsing_context_load["params"]["context"]
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


@pytest.mark.asyncio
async def test_preloadScript_add_loadedInNewContexts(websocket, context_id,
                                                     create_context, html):
    await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "functionDeclaration": "() => { window.foo='bar'; }",
            }
        })

    await goto_url(websocket, context_id, html())

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

    new_context_id = await create_context()

    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "window.foo",
                "target": {
                    "context": new_context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })
    assert result["result"] == {"type": "string", "value": 'bar'}

    await goto_url(websocket, new_context_id, html())

    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "window.foo",
                "target": {
                    "context": new_context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })
    assert result["result"] == {"type": "string", "value": 'bar'}


@pytest.mark.asyncio
async def test_preloadScript_add_sandbox_new_context(websocket, html):
    result = await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "functionDeclaration": "() => { window.foo='bar'; }",
                "sandbox": "MY_SANDBOX",
            }
        })
    assert result == {'script': ANY_UUID}

    result = await execute_command(websocket, {
        "method": "browsingContext.create",
        "params": {
            "type": "tab"
        }
    })
    new_context_id = result["context"]

    # Assert preload script takes no effect in the standard sandbox.
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "window.foo",
                "target": {
                    "context": new_context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })
    assert result["result"] == {"type": "undefined"}

    # Assert preload script takes effect in the custom sandbox.
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "window.foo",
                "target": {
                    "context": new_context_id,
                    "sandbox": "MY_SANDBOX",
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })
    assert result["result"] == {"type": "string", "value": 'bar'}


@pytest.mark.asyncio
async def test_preloadScript_add_sandbox_existing_context(
        websocket, context_id, html):
    # Add preload script, which checks if the page script was already executed.
    await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "functionDeclaration": """
                    () => {
                        window.SOME_VAR=[
                            ...(window.SOME_VAR ?? []),
                            'MY_SANDBOX_PRELOAD_SCRIPT'];
                    }""",
                "sandbox": "MY_SANDBOX",
            }
        })

    # Load a page with a custom script.
    await goto_url(
        websocket, context_id,
        html("<script>"
             "window.SOME_VAR=["
             "...(window.SOME_VAR ?? []), "
             "'HTML_SCRIPT']"
             "</script>"))

    # Evaluate in the standard sandbox, page script takes effect,
    # while preload does not.
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "window.SOME_VAR.join(', ')",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })
    assert result["result"] == {"type": "string", "value": 'HTML_SCRIPT'}

    # Evaluate in custom sandbox, page script takes no effect,
    # while preload takes.
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "window.SOME_VAR.join(', ')",
                "target": {
                    "context": context_id,
                    "sandbox": "MY_SANDBOX",
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })
    assert result["result"] == {
        "type": "string",
        "value": 'MY_SANDBOX_PRELOAD_SCRIPT'
    }


@pytest.mark.asyncio
async def test_preloadScript_add_withUserGesture_blankTargetLink(
        websocket, context_id, html, read_sorted_messages):
    LINK_WITH_BLANK_TARGET = html(
        '<a href="https://example.com" target="_blank">new tab</a>')

    await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "functionDeclaration": """
                    () => {
                        console.log('my preload script', window.location.href);
                    }""",
            }
        })

    await goto_url(websocket, context_id, LINK_WITH_BLANK_TARGET)

    await subscribe(websocket, ["log.entryAdded"])

    command_id = await send_JSON_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": """document.querySelector('a').click();""",
                "awaitPromise": True,
                "target": {
                    "context": context_id,
                },
                "userActivation": True
            }
        })

    [command_result, log_entry_added] = await read_sorted_messages(2)
    assert command_result == AnyExtending({
        "id": command_id,
        "type": "success",
        "result": ANY_DICT
    })
    assert log_entry_added == AnyExtending({
        "type": "event",
        "method": "log.entryAdded",
        "params": {
            "args": [{
                "type": "string",
                "value": "my preload script"
            }, {
                'type': 'string',
                'value': 'https://example.com/',
            }]
        }
    })


@pytest.mark.asyncio
async def test_preloadScript_channel_navigate(websocket, context_id, html,
                                              read_sorted_messages):
    await subscribe(websocket, ["script.message"])

    result = await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "functionDeclaration": """
                    (channel) => {
                        setTimeout(() => {
                            channel({'foo': 'bar', 'baz': {'1': 2}})
                        }, 1);
                    }""",
                "arguments": [{
                    "type": "channel",
                    "value": {
                        "channel": "channel_name",
                        "serializationOptions": {
                            "maxObjectDepth": 0
                        },
                    },
                }, ],
                "context": context_id,
            }
        })
    assert result == {'script': ANY_UUID}

    command_id = await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": html(),
                "wait": "complete",
                "context": context_id
            }
        })

    [command_result, channel_message] = await read_sorted_messages(2)
    assert command_result == {
        "type": "success",
        "id": command_id,
        "result": ANY_DICT
    }

    assert channel_message == AnyExtending({
        "type": "event",
        "method": "script.message",
        "params": {
            "channel": "channel_name",
            "data": {
                "type": "object"
            },
            "source": {
                "realm": ANY_STR,
                "context": context_id,
            },
        }
    })


@pytest.mark.asyncio
async def test_preloadScript_channel_newContext(websocket,
                                                read_sorted_messages):
    await subscribe(websocket, ["script.message"])

    result = await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "functionDeclaration": """
                    (channel) => {
                        channel({'foo': 'bar', 'baz': {'1': 2}});
                    }""",
                "arguments": [{
                    "type": "channel",
                    "value": {
                        "channel": "channel_name"
                    },
                }, ],
            }
        })
    assert result == {'script': ANY_UUID}

    command_id = await send_JSON_command(websocket, {
        "method": "browsingContext.create",
        "params": {
            "type": "tab"
        }
    })

    [command_result, channel_message] = await read_sorted_messages(2)
    assert command_result == {
        "type": "success",
        "id": command_id,
        "result": ANY_DICT
    }
    new_context_id = command_result["result"]["context"]

    assert channel_message == AnyExtending({
        "type": "event",
        "method": "script.message",
        "params": {
            "channel": "channel_name",
            "data": {
                "type": "object"
            },
            "source": {
                "realm": ANY_STR,
                "context": new_context_id,
            },
        }
    })


@pytest.mark.asyncio
async def test_preloadScript_add_respectContextsForOldContexts(
        websocket, context_id, html):

    # Create a new context prior to adding PreloadScript
    result = await execute_command(websocket, {
        "method": "browsingContext.create",
        "params": {
            "type": "tab"
        }
    })
    new_context_id = result['context']

    # Add the PreloadScript only to a specific context
    await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "functionDeclaration": """
                    () => {
                        window.FOO = "BAR"
                    }""",
                "contexts": [context_id]
            }
        })

    # Navigate both contexts to trigger PreloadScripts
    await goto_url(websocket, context_id, html())
    await goto_url(websocket, new_context_id, html())

    # Expect context with context_id to be affected by PreloadScript
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "window.FOO",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })
    assert result["result"] == {"type": "string", "value": 'BAR'}

    # Expect context with new_context_id to not be affected by PreloadScript
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "window.FOO",
                "target": {
                    "context": new_context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })
    assert result["result"] == {"type": "undefined"}


@pytest.mark.asyncio
async def test_preloadScript_add_respectContextsForNewContexts(
        websocket, context_id, html):

    # Add the PreloadScript only to a specific context
    await execute_command(
        websocket, {
            "method": "script.addPreloadScript",
            "params": {
                "functionDeclaration": """
                    () => {
                        window.FOO = "BAR"
                    }""",
                "contexts": [context_id]
            }
        })

    # Create a new context after adding PreloadScript
    result = await execute_command(websocket, {
        "method": "browsingContext.create",
        "params": {
            "type": "tab"
        }
    })
    new_context_id = result['context']

    # Navigate both contexts to trigger PreloadScripts
    await goto_url(websocket, context_id, html())
    await goto_url(websocket, new_context_id, html())

    # Expect context with context_id to be affected by PreloadScript
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "window.FOO",
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })
    assert result["result"] == {"type": "string", "value": 'BAR'}

    # Expect context with new_context_id to not be affected by PreloadScript
    result = await execute_command(
        websocket, {
            "method": "script.evaluate",
            "params": {
                "expression": "window.FOO",
                "target": {
                    "context": new_context_id
                },
                "awaitPromise": True,
                "resultOwnership": "root"
            }
        })
    assert result["result"] == {"type": "undefined"}
