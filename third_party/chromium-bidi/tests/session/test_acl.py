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

# Tests for restricted sessions: with `"goog:sessionType": "restricted"`, a
# session starts with an empty ACL (`{}`) and can run only the commands that its
# ACL grants via `goog:session.setAcl`.

import pytest
import pytest_asyncio
from test_helpers import AnyExtending, execute_command

RESTRICTED_SESSION = {"goog:sessionType": "restricted"}
GET_TREE_ACL = {"default": {"commands": ["browsingContext.getTree"]}}


@pytest_asyncio.fixture(autouse=True)
async def initial_acl(request, websocket):
    # Runs before `context_id`/`sandbox_realm` fixtures so tests can seed required ACLs.
    acl = getattr(request, "param", None)
    if acl is not None:
        await execute_command(
            websocket,
            {"method": "goog:session.setAcl", "params": {"acl": acl}},
        )


@pytest.mark.asyncio
@pytest.mark.parametrize("capabilities", [RESTRICTED_SESSION], indirect=True)
async def test_acl_starts_empty(websocket):
    with pytest.raises(Exception) as exception_info:
        await execute_command(
            websocket, {"method": "browsingContext.getTree", "params": {}}
        )

    assert exception_info.value.args[0] == {
        "error": "unsupported operation",
        "message": "Command 'browsingContext.getTree' is not allowed by the session ACL.",
    }


@pytest.mark.asyncio
@pytest.mark.parametrize("capabilities", [RESTRICTED_SESSION], indirect=True)
@pytest.mark.parametrize("initial_acl", [GET_TREE_ACL], indirect=True)
async def test_acl_granted_command_allowed(websocket):
    result = await execute_command(
        websocket, {"method": "browsingContext.getTree", "params": {}}
    )

    assert result == {"contexts": [AnyExtending({"url": "about:blank"})]}


@pytest.mark.asyncio
@pytest.mark.parametrize("capabilities", [RESTRICTED_SESSION], indirect=True)
@pytest.mark.parametrize("initial_acl", [GET_TREE_ACL], indirect=True)
async def test_acl_not_granted_command_rejected(websocket):
    with pytest.raises(Exception) as exception_info:
        await execute_command(
            websocket,
            {"method": "browsingContext.create", "params": {"type": "tab"}},
        )

    assert exception_info.value.args[0] == {
        "error": "unsupported operation",
        "message": "Command 'browsingContext.create' is not allowed by the session ACL.",
    }


@pytest.mark.asyncio
@pytest.mark.parametrize("capabilities", [RESTRICTED_SESSION], indirect=True)
@pytest.mark.parametrize("initial_acl", [GET_TREE_ACL], indirect=True)
async def test_acl_not_granted_command_rejected_for_context(websocket, context_id):
    with pytest.raises(Exception) as exception_info:
        await execute_command(
            websocket,
            {
                "method": "browsingContext.navigate",
                "params": {
                    "context": context_id,
                    "url": "about:blank",
                    "wait": "complete",
                },
            },
        )

    assert exception_info.value.args[0] == {
        "error": "unsupported operation",
        "message": f"Command 'browsingContext.navigate' is not allowed by the session ACL for browsing context '{context_id}'.",
    }


@pytest.mark.asyncio
@pytest.mark.parametrize("capabilities", [RESTRICTED_SESSION], indirect=True)
@pytest.mark.parametrize("initial_acl", [GET_TREE_ACL], indirect=True)
async def test_acl_checked_before_params_validation(websocket, context_id):
    with pytest.raises(Exception) as exception_info:
        # Invalid parameters: `url` is required.
        await execute_command(
            websocket,
            {"method": "browsingContext.navigate", "params": {"context": context_id}},
        )

    assert exception_info.value.args[0] == {
        "error": "unsupported operation",
        "message": f"Command 'browsingContext.navigate' is not allowed by the session ACL for browsing context '{context_id}'.",
    }


@pytest.mark.asyncio
@pytest.mark.parametrize("capabilities", [RESTRICTED_SESSION], indirect=True)
@pytest.mark.parametrize(
    "initial_acl",
    [
        {
            "default": {
                "commands": [
                    "browsingContext.getTree",
                    "browsingContext.navigate",
                ]
            }
        }
    ],
    indirect=True,
)
async def test_acl_default_command_allowed_for_context(websocket, context_id):
    result = await execute_command(
        websocket,
        {
            "method": "browsingContext.navigate",
            "params": {"context": context_id, "url": "about:blank", "wait": "complete"},
        },
    )

    assert result == AnyExtending({"url": "about:blank"})


@pytest.mark.asyncio
@pytest.mark.parametrize("capabilities", [RESTRICTED_SESSION], indirect=True)
@pytest.mark.parametrize(
    "initial_acl",
    [
        {
            "default": {
                # An ACL can't grant these commands.
                "commands": [
                    "browser.getUserContexts",
                    "goog:cdp.sendCommand",
                    "script.getRealms",
                ]
            }
        }
    ],
    indirect=True,
)
@pytest.mark.parametrize(
    "command",
    [
        {"method": "browser.getUserContexts", "params": {}},
        {"method": "goog:cdp.sendCommand", "params": {"method": "Browser.getVersion"}},
        {"method": "script.getRealms", "params": {}},
    ],
)
async def test_acl_unsupported_command_rejected(websocket, command):
    with pytest.raises(Exception) as exception_info:
        await execute_command(websocket, command)

    assert exception_info.value.args[0] == {
        "error": "unsupported operation",
        "message": f"Command '{command['method']}' is not supported in restricted sessions.",
    }


@pytest.mark.asyncio
@pytest.mark.parametrize("capabilities", [RESTRICTED_SESSION], indirect=True)
async def test_acl_session_status_allowed(websocket):
    status = await execute_command(
        websocket, {"method": "session.status", "params": {}}
    )

    assert status == AnyExtending({"ready": False, "message": "already connected"})


@pytest.mark.asyncio
@pytest.mark.parametrize("capabilities", [RESTRICTED_SESSION], indirect=True)
@pytest.mark.parametrize("initial_acl", [GET_TREE_ACL], indirect=True)
async def test_acl_not_granted_script_rejected_for_target_context(
    websocket, context_id
):
    with pytest.raises(Exception) as exception_info:
        await execute_command(
            websocket,
            {
                "method": "script.evaluate",
                "params": {
                    "expression": "1 + 1",
                    "target": {"context": context_id, "sandbox": "agent"},
                    "awaitPromise": False,
                },
            },
        )

    assert exception_info.value.args[0] == {
        "error": "unsupported operation",
        "message": f"Command 'script.evaluate' is not allowed by the session ACL for browsing context '{context_id}'.",
    }


@pytest.mark.asyncio
@pytest.mark.parametrize("capabilities", [RESTRICTED_SESSION], indirect=True)
@pytest.mark.parametrize(
    "initial_acl",
    [{"default": {"commands": ["browsingContext.getTree", "script.evaluate"]}}],
    indirect=True,
)
@pytest.mark.parametrize(
    "make_target",
    [
        lambda context_id, _sandbox_realm: {"context": context_id},
        lambda context_id, _sandbox_realm: {"context": context_id, "sandbox": ""},
        lambda _context_id, sandbox_realm: {"realm": sandbox_realm},
    ],
    ids=["missing_sandbox", "empty_sandbox", "realm_target"],
)
async def test_acl_script_requires_non_empty_sandbox(
    websocket, context_id, sandbox_realm, make_target
):
    with pytest.raises(Exception) as exception_info:
        await execute_command(
            websocket,
            {
                "method": "script.evaluate",
                "params": {
                    "expression": "1 + 1",
                    "target": make_target(context_id, sandbox_realm),
                    "awaitPromise": False,
                },
            },
        )

    assert exception_info.value.args[0] == {
        "error": "unsupported operation",
        "message": "Command 'script.evaluate' in restricted sessions requires a browsing context target with a non-empty 'sandbox'.",
    }


@pytest.mark.asyncio
@pytest.mark.parametrize("capabilities", [RESTRICTED_SESSION], indirect=True)
@pytest.mark.parametrize(
    "initial_acl",
    [
        {
            "default": {
                "commands": [
                    "browsingContext.getTree",
                    "browsingContext.navigate",
                    "script.evaluate",
                ]
            }
        }
    ],
    indirect=True,
)
async def test_acl_default_script_allowed_in_sandbox(websocket, context_id, html):
    url = html("<script>window.PAGE_GLOBAL = 'from_page';</script>")
    await execute_command(
        websocket,
        {
            "method": "browsingContext.navigate",
            "params": {"context": context_id, "url": url, "wait": "complete"},
        },
    )

    result = await execute_command(
        websocket,
        {
            "method": "script.evaluate",
            "params": {
                "expression": "[1 + 1, typeof window.PAGE_GLOBAL]",
                "target": {"context": context_id, "sandbox": "agent"},
                "awaitPromise": False,
            },
        },
    )

    assert result == AnyExtending(
        {
            "type": "success",
            "result": {
                "type": "array",
                "value": [
                    {"type": "number", "value": 2},
                    {"type": "string", "value": "undefined"},
                ],
            },
        }
    )


@pytest.mark.asyncio
async def test_no_acl_all_commands_allowed(websocket):
    result = await execute_command(
        websocket, {"method": "browser.getUserContexts", "params": {}}
    )

    assert result == {"userContexts": [{"userContext": "default"}]}
