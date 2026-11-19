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
from test_helpers import AnyExtending, execute_command

RESTRICTED_SESSION = {"goog:sessionType": "restricted"}
REASON = "Fill in the checkout form."


async def set_acl(websocket, params):
    return await execute_command(
        websocket,
        {
            "method": "goog:session.setAcl",
            "params": params,
        },
    )


@pytest.mark.asyncio
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
            "default": {"commands": ["browsingContext.getTree"]},
            "perNavigable": {
                context_id: {"commands": ["browsingContext.navigate"]},
            },
        }
    }


@pytest.mark.asyncio
async def test_set_acl_is_idempotent(websocket, context_id):
    acl = {
        "default": {"commands": ["browsingContext.getTree"]},
        "perNavigable": {
            context_id: {"commands": ["browsingContext.navigate"]},
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
        "default": {"commands": ["browsingContext.getTree"]},
        "perNavigable": {
            context_id: {"commands": ["browsingContext.navigate"]},
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
            "default": {"commands": ["browsingContext.getTree"]},
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
async def test_set_acl_malformed_acl_is_invalid_argument(websocket):
    with pytest.raises(Exception, match="invalid argument"):
        await set_acl(websocket, {"acl": None})


@pytest.mark.asyncio
async def test_set_acl_non_string_reason_is_invalid_argument(websocket):
    with pytest.raises(Exception, match="invalid argument"):
        await set_acl(websocket, {"acl": {}, "reason": 1})


@pytest.mark.asyncio
async def test_set_acl_without_acl_is_invalid_argument(websocket):
    with pytest.raises(Exception, match="invalid argument"):
        await set_acl(websocket, {})
