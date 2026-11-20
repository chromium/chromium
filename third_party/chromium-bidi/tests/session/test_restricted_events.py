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

# Tests for events in restricted sessions: clients can subscribe to any event,
# but get only the events that the session ACL allows.

import pytest
import pytest_asyncio
from test_helpers import AnyExtending, execute_command, send_JSON_command, subscribe

RESTRICTED_SESSION = {"goog:sessionType": "restricted"}
# `browsingContext.getTree` backs the `context_id` and `read_messages` fixtures,
# and `browsingContext.create` backs the `another_context_id` fixture.
COMMANDS = [
    "browsingContext.create",
    "browsingContext.getTree",
    "browsingContext.navigate",
]
NAVIGATION_COMMITTED_ACL = {
    "default": {
        "commands": COMMANDS,
        "events": ["browsingContext.navigationCommitted"],
    }
}
NO_EVENTS_ACL = {"default": {"commands": COMMANDS}}


async def set_acl(websocket, acl):
    return await execute_command(
        websocket,
        {
            "method": "goog:session.setAcl",
            "params": {"acl": acl, "reason": "Follow the navigation."},
        },
    )


@pytest_asyncio.fixture(autouse=True)
async def initial_acl(request, websocket):
    # Runs before the `context_id` and `another_context_id` fixtures, which need
    # `getTree` and `create`.
    await set_acl(websocket, getattr(request, "param", NO_EVENTS_ACL))


async def navigate(websocket, context_id, url):
    return await send_JSON_command(
        websocket,
        {
            "method": "browsingContext.navigate",
            "params": {"url": url, "wait": "complete", "context": context_id},
        },
    )


@pytest.mark.asyncio
@pytest.mark.parametrize("capabilities", [RESTRICTED_SESSION], indirect=True)
@pytest.mark.parametrize("initial_acl", [NAVIGATION_COMMITTED_ACL], indirect=True)
async def test_restricted_events_module_subscription_gets_only_acl_events(
    websocket, context_id, html, read_messages
):
    await subscribe(websocket, ["browsingContext"])

    command_id = await navigate(websocket, context_id, html())

    # `navigationStarted`, `domContentLoaded` and `load` are not in the ACL.
    messages = await read_messages(2, check_no_other_messages=True)
    assert messages == [
        AnyExtending({"id": command_id, "type": "success"}),
        AnyExtending(
            {
                "method": "browsingContext.navigationCommitted",
                "params": {"context": context_id},
            }
        ),
    ]


@pytest.mark.asyncio
@pytest.mark.parametrize("capabilities", [RESTRICTED_SESSION], indirect=True)
@pytest.mark.parametrize("initial_acl", [NO_EVENTS_ACL], indirect=True)
async def test_restricted_events_event_not_in_acl_not_sent(
    websocket, context_id, html, read_messages
):
    await subscribe(websocket, ["browsingContext.navigationCommitted"])

    command_id = await navigate(websocket, context_id, html())

    messages = await read_messages(1, check_no_other_messages=True)
    assert messages == [AnyExtending({"id": command_id, "type": "success"})]


@pytest.mark.asyncio
@pytest.mark.parametrize("capabilities", [RESTRICTED_SESSION], indirect=True)
@pytest.mark.parametrize("initial_acl", [NO_EVENTS_ACL], indirect=True)
async def test_restricted_events_set_acl_applies_to_existing_subscription(
    websocket, context_id, html, read_messages
):
    await subscribe(websocket, ["browsingContext.navigationCommitted"])
    await set_acl(
        websocket,
        {
            "default": {"commands": COMMANDS},
            "perNavigable": {
                context_id: {"events": ["browsingContext.navigationCommitted"]}
            },
        },
    )

    command_id = await navigate(websocket, context_id, html())

    messages = await read_messages(2, check_no_other_messages=True)
    assert messages == [
        AnyExtending({"id": command_id, "type": "success"}),
        AnyExtending(
            {
                "method": "browsingContext.navigationCommitted",
                "params": {"context": context_id},
            }
        ),
    ]

    # Revoking the event stops it on the same subscription.
    await set_acl(websocket, NO_EVENTS_ACL)

    command_id = await navigate(websocket, context_id, html())

    messages = await read_messages(1, check_no_other_messages=True)
    assert messages == [AnyExtending({"id": command_id, "type": "success"})]


@pytest.mark.asyncio
@pytest.mark.parametrize("capabilities", [RESTRICTED_SESSION], indirect=True)
@pytest.mark.parametrize("initial_acl", [NO_EVENTS_ACL], indirect=True)
async def test_restricted_events_other_tab_events_not_sent(
    websocket, context_id, another_context_id, html, read_messages
):
    await set_acl(
        websocket,
        {
            "default": {"commands": COMMANDS},
            "perNavigable": {
                context_id: {"events": ["browsingContext.navigationCommitted"]}
            },
        },
    )
    await subscribe(websocket, ["browsingContext.navigationCommitted"])

    other_command_id = await navigate(websocket, another_context_id, html())
    command_id = await navigate(websocket, context_id, html())

    # `navigationCommitted` of `another_context_id` is not in the ACL.
    messages = await read_messages(3, check_no_other_messages=True)
    assert messages == [
        AnyExtending({"id": other_command_id, "type": "success"}),
        AnyExtending({"id": command_id, "type": "success"}),
        AnyExtending(
            {
                "method": "browsingContext.navigationCommitted",
                "params": {"context": context_id},
            }
        ),
    ]


@pytest.mark.asyncio
@pytest.mark.parametrize("capabilities", [RESTRICTED_SESSION], indirect=True)
@pytest.mark.parametrize("initial_acl", [NO_EVENTS_ACL], indirect=True)
async def test_restricted_events_not_buffered_before_subscribe(
    websocket, context_id, html, read_messages
):
    await execute_command(
        websocket,
        {
            "method": "browsingContext.navigate",
            "params": {
                "url": html("<script>console.log('before grant')</script>"),
                "wait": "complete",
                "context": context_id,
            },
        },
    )

    await set_acl(
        websocket,
        {
            "default": {"commands": COMMANDS},
            "perNavigable": {context_id: {"events": ["log.entryAdded"]}},
        },
    )
    await subscribe(websocket, ["log.entryAdded"])

    await read_messages(0, check_no_other_messages=True)

    command_id = await navigate(
        websocket,
        context_id,
        html("<script>console.log('after subscribe')</script>"),
    )
    messages = await read_messages(2, check_no_other_messages=True)
    assert messages == [
        AnyExtending({"id": command_id, "type": "success"}),
        AnyExtending(
            {
                "method": "log.entryAdded",
                "params": {"text": "after subscribe"},
            }
        ),
    ]
