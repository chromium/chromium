# Copyright 2026 Google LLC.
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
import pytest_asyncio
from test_helpers import AnyExtending, execute_command, get_tree, subscribe

RESTRICTED_SESSION = {"goog:sessionType": "restricted"}
GET_TREE_ACL = {"default": {"commands": ["browsingContext.getTree"]}}
REASON = "Fill in the checkout form."


async def set_acl(websocket, params):
    return await execute_command(
        websocket,
        {
            "method": "goog:session.setAcl",
            "params": params,
        },
    )


@pytest_asyncio.fixture(autouse=True)
async def initial_acl(request, websocket):
    # Runs before `context_id` so tests can seed `browsingContext.getTree`.
    acl = getattr(request, "param", None)
    if acl is not None:
        await set_acl(websocket, {"acl": acl})


@pytest.mark.asyncio
@pytest.mark.parametrize("capabilities", [RESTRICTED_SESSION], indirect=True)
@pytest.mark.parametrize("initial_acl", [GET_TREE_ACL], indirect=True)
async def test_set_acl_returns_acl_in_effect(websocket, context_id):
    result = await set_acl(
        websocket,
        {
            "acl": {
                "default": {
                    "commands": [
                        "browsingContext.getTree",
                        # Unsupported commands grant nothing.
                        "network.addIntercept",
                    ]
                },
                "perNavigable": {
                    context_id: {"commands": ["browsingContext.navigate"]},
                    # Unknown or non-top-level navigables grant nothing.
                    "unknown-context": {"commands": ["browsingContext.navigate"]},
                },
            },
            "reason": REASON,
        },
    )

    # Nothing is refused, so there is no `denied`.
    assert result == {
        "acl": {
            "default": {"commands": ["browsingContext.getTree"], "events": []},
            "perNavigable": {
                context_id: {"commands": ["browsingContext.navigate"], "events": []},
            },
        }
    }


@pytest.mark.asyncio
@pytest.mark.parametrize("capabilities", [RESTRICTED_SESSION], indirect=True)
@pytest.mark.parametrize("initial_acl", [GET_TREE_ACL], indirect=True)
async def test_set_acl_is_idempotent(websocket, context_id):
    acl = {
        "default": {"commands": ["browsingContext.getTree"], "events": []},
        "perNavigable": {
            context_id: {"commands": ["browsingContext.navigate"], "events": []},
        },
    }

    first = await set_acl(websocket, {"acl": acl, "reason": REASON})
    second = await set_acl(websocket, {"acl": acl, "reason": REASON})

    assert first == second == {"acl": acl}


@pytest.mark.asyncio
@pytest.mark.parametrize("capabilities", [RESTRICTED_SESSION], indirect=True)
async def test_set_acl_grants_and_retracts_access(websocket):
    # A restricted session starts with an empty ACL, so request `getTree` before querying `context_id`.
    await set_acl(
        websocket, {"acl": {"default": {"commands": ["browsingContext.getTree"]}}}
    )
    tree = await execute_command(
        websocket, {"method": "browsingContext.getTree", "params": {}}
    )
    context_id = tree["contexts"][0]["context"]
    navigate_command = {
        "method": "browsingContext.navigate",
        "params": {"context": context_id, "url": "about:blank", "wait": "complete"},
    }
    with pytest.raises(Exception) as exception_info:
        await execute_command(websocket, navigate_command)
    assert exception_info.value.args[0] == {
        "error": "unsupported operation",
        "message": f"Command 'browsingContext.navigate' is not allowed by the session ACL for browsing context '{context_id}'.",
    }

    acl = {
        "default": {"commands": ["browsingContext.getTree"], "events": []},
        "perNavigable": {
            context_id: {"commands": ["browsingContext.navigate"], "events": []},
        },
    }
    assert await set_acl(websocket, {"acl": acl, "reason": REASON}) == {"acl": acl}
    assert await execute_command(websocket, navigate_command) == AnyExtending(
        {"url": "about:blank"}
    )

    result = await set_acl(
        websocket, {"acl": {"default": {"commands": ["browsingContext.getTree"]}}}
    )
    assert result == {
        "acl": {
            "default": {"commands": ["browsingContext.getTree"], "events": []},
            "perNavigable": {},
        }
    }
    with pytest.raises(Exception) as exception_info:
        await execute_command(websocket, navigate_command)
    assert exception_info.value.args[0] == {
        "error": "unsupported operation",
        "message": f"Command 'browsingContext.navigate' is not allowed by the session ACL for browsing context '{context_id}'.",
    }


@pytest.mark.asyncio
async def test_set_acl_in_standard_session_is_unsupported(websocket):
    with pytest.raises(Exception) as exception_info:
        await set_acl(websocket, {"acl": {}})

    assert exception_info.value.args[0] == {
        "error": "unsupported operation",
        "message": (
            "Command 'goog:session.setAcl' requires a restricted session. "
            'Request capability "goog:sessionType": "restricted" in '
            "session.new."
        ),
    }


@pytest.mark.asyncio
@pytest.mark.parametrize("capabilities", [RESTRICTED_SESSION], indirect=True)
async def test_set_acl_malformed_acl_is_invalid_argument(websocket):
    with pytest.raises(Exception, match="invalid argument"):
        await set_acl(websocket, {"acl": None})


@pytest.mark.asyncio
@pytest.mark.parametrize("capabilities", [RESTRICTED_SESSION], indirect=True)
async def test_set_acl_non_string_reason_is_invalid_argument(websocket):
    with pytest.raises(Exception, match="invalid argument"):
        await set_acl(websocket, {"acl": {}, "reason": 1})


@pytest.mark.asyncio
@pytest.mark.parametrize("capabilities", [RESTRICTED_SESSION], indirect=True)
async def test_set_acl_without_acl_is_invalid_argument(websocket):
    with pytest.raises(Exception, match="invalid argument"):
        await set_acl(websocket, {})


@pytest.mark.asyncio
async def test_set_acl_embedder_partial_grant_and_enforcement(
    create_mapper, context_id, html
):
    """Checks end-to-end embedder `aclHandler` wiring via `runMapperInstance`:

    1. The Mapper normalizes the client's `goog:session.setAcl` request
       (dropping unsupported commands and unknown navigables) before invoking
       `aclHandler.setAcl(acl, reason)`.
    2. The Mapper stores and returns the embedder's decision, which may grant
       less or more than requested (including `default` or `perNavigable`
       entries the client did not explicitly request) and report `denied`
       entries.
    3. Subsequent commands and subscribed events are enforced against the
       resulting ACL in the browser.
    """
    received_calls: list[tuple[dict, str | None]] = []

    # Subset of the client's request that the embedder refuses, echoed back in
    # the `goog:session.setAcl` result's `denied` field.
    denied = {
        "acl": {
            "default": {"commands": ["browsingContext.create"]},
            "perNavigable": {
                context_id: {
                    "commands": ["browsingContext.reload"],
                    "events": ["log.entryAdded"],
                }
            },
        },
        "reason": "User denied create, reload, and logs.",
    }

    async def handle_set_acl(acl: dict, reason: str | None) -> dict:
        received_calls.append((acl, reason))
        # Grant only part of the client's request (`navigationCommitted` and
        # per-navigable `navigate`), refuse the rest (`denied`), and add an
        # embedder-chosen default grant (`getTree`) the client did not ask for.
        return {
            "acl": {
                "default": {
                    "commands": ["browsingContext.getTree"],
                    "events": ["browsingContext.navigationCommitted"],
                },
                "perNavigable": {
                    context_id: {
                        "commands": ["browsingContext.navigate"],
                    }
                },
            },
            "denied": denied,
        }

    # Start an in-tab Mapper in restricted mode with the Python `handle_set_acl`
    # callback bridged as `aclHandler.setAcl`.
    mapper = await create_mapper(
        custom_capabilities=RESTRICTED_SESSION,
        restricted_mode=True,
        acl_handler=handle_set_acl,
    )
    # `session.subscribe` is ACL-exempt, so subscribing to both events succeeds
    # before `setAcl`; delivery will later be filtered by the granted ACL.
    await subscribe(mapper, ["browsingContext.navigationCommitted", "log.entryAdded"])

    # Step 1: Request a mix of supported/unsupported commands, known/unknown
    # navigables, and events.
    result = await set_acl(
        mapper,
        {
            "acl": {
                "default": {
                    "commands": [
                        "browsingContext.create",
                        # Unsupported commands are dropped before calling the handler.
                        "network.addIntercept",
                    ],
                    "events": ["browsingContext.navigationCommitted"],
                },
                "perNavigable": {
                    context_id: {
                        "commands": [
                            "browsingContext.navigate",
                            "browsingContext.reload",
                        ],
                        "events": ["log.entryAdded"],
                    },
                    # Unknown navigables are dropped before calling the handler.
                    "unknown-context": {"commands": ["browsingContext.navigate"]},
                },
            },
            "reason": REASON,
        },
    )

    # The embedder receives only the normalized request (`network.addIntercept`
    # and `unknown-context` stripped) alongside the client's `reason`.
    assert received_calls == [
        (
            {
                "default": {
                    "commands": ["browsingContext.create"],
                    "events": ["browsingContext.navigationCommitted"],
                },
                "perNavigable": {
                    context_id: {
                        "commands": [
                            "browsingContext.navigate",
                            "browsingContext.reload",
                        ],
                        "events": ["log.entryAdded"],
                    }
                },
            },
            REASON,
        )
    ]

    # Step 2: The client receives the normalized ACL actually in effect (with
    # missing `events` lists filled in as `[]`) plus the embedder's `denied`
    # payload.
    assert result == {
        "acl": {
            "default": {
                "commands": ["browsingContext.getTree"],
                "events": ["browsingContext.navigationCommitted"],
            },
            "perNavigable": {
                context_id: {
                    "commands": ["browsingContext.navigate"],
                    "events": [],
                }
            },
        },
        "denied": denied,
    }

    # Step 3a: Embedder-added default command (`browsingContext.getTree`) is
    # allowed even though the client did not request it.
    tree = await get_tree(mapper)
    assert [c["context"] for c in tree["contexts"]] == [context_id]

    # Step 3b: Refused per-navigable command (`browsingContext.reload`) is
    # blocked by the active ACL.
    with pytest.raises(Exception) as exception_info:
        await execute_command(
            mapper,
            {"method": "browsingContext.reload", "params": {"context": context_id}},
        )
    assert exception_info.value.args[0] == {
        "error": "unsupported operation",
        "message": f"Command 'browsingContext.reload' is not allowed by the session ACL for browsing context '{context_id}'.",
    }

    # Step 3c: Granted command (`browsingContext.navigate`) succeeds on a page
    # that emits both a granted event (`navigationCommitted`) and a denied
    # event (`log.entryAdded`). Clear prior events first so we only inspect
    # events from this navigation.
    mapper.received_events.clear()
    url = html("<script>console.log('denied log');</script>")
    await execute_command(
        mapper,
        {
            "method": "browsingContext.navigate",
            "params": {"context": context_id, "url": url, "wait": "complete"},
        },
    )
    # Send one more command to flush the Mapper's FIFO outgoing queue so any
    # leaked `log.entryAdded` event would be recorded in `received_events`.
    await get_tree(mapper)

    # Only the granted `navigationCommitted` event was delivered; the denied
    # `log.entryAdded` event was filtered out.
    assert mapper.received_events == [
        AnyExtending(
            {
                "type": "event",
                "method": "browsingContext.navigationCommitted",
                "params": {"context": context_id, "url": url},
            }
        )
    ]
