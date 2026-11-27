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
from test_helpers import (
    AnyExtending,
    execute_command,
    get_tree,
    subscribe,
    wait_for_event,
)

RESTRICTED_SESSION = {"goog:sessionType": "restricted"}


@pytest.mark.asyncio
async def test_session_changed_embedder_update_and_enforcement(
    create_mapper, context_id, html
):
    """Checks browser-initiated ACL updates end-to-end via `runMapperInstance`:

    1. `goog:session.changed` cannot be subscribed to and is always delivered
       to restricted sessions with normalized `acl` and optional `reason`.
    2. Commands and subscribed events immediately reflect the updated ACL.
    """
    mapper = await create_mapper(
        custom_capabilities=RESTRICTED_SESSION,
        restricted_mode=True,
        acl_handler=lambda acl, _reason: {"acl": acl},
    )

    # `goog:session.changed` is always delivered and not subscribable.
    with pytest.raises(Exception, match="invalid argument"):
        await subscribe(mapper, ["goog:session.changed"])

    await subscribe(mapper, ["browsingContext.navigationCommitted", "log.entryAdded"])
    await execute_command(
        mapper,
        {
            "method": "goog:session.setAcl",
            "params": {
                "acl": {
                    "default": {"commands": ["browsingContext.getTree"]},
                    "perNavigable": {
                        context_id: {
                            "commands": [
                                "browsingContext.navigate",
                                "browsingContext.reload",
                            ],
                            "events": [
                                "browsingContext.navigationCommitted",
                                "log.entryAdded",
                            ],
                        }
                    },
                }
            },
        },
    )

    # Embedder revokes `reload` and `log.entryAdded` with a reason; unsupported
    # entries and unknown navigables must be normalized away.
    reason = "The user revoked reload and console log access."
    await mapper.notify_acl_changed(
        {
            "default": {
                "commands": ["browsingContext.getTree", "network.addIntercept"],
                "events": ["goog:session.changed"],
            },
            "perNavigable": {
                context_id: {
                    "commands": ["browsingContext.navigate"],
                    "events": ["browsingContext.navigationCommitted"],
                },
                "unknown-context": {"commands": ["browsingContext.reload"]},
            },
        },
        reason,
    )
    revoked_event = await wait_for_event(mapper, "goog:session.changed")
    assert revoked_event == {
        "type": "event",
        "method": "goog:session.changed",
        "params": {
            "acl": {
                "default": {"commands": ["browsingContext.getTree"], "events": []},
                "perNavigable": {
                    context_id: {
                        "commands": ["browsingContext.navigate"],
                        "events": ["browsingContext.navigationCommitted"],
                    }
                },
            },
            "reason": reason,
        },
    }

    # Revoked command fails while granted command and event continue working
    # (and revoked `log.entryAdded` is withheld).
    with pytest.raises(Exception) as exception_info:
        await execute_command(
            mapper,
            {"method": "browsingContext.reload", "params": {"context": context_id}},
        )
    assert exception_info.value.args[0] == {
        "error": "unsupported operation",
        "message": (
            "Command 'browsingContext.reload' is not allowed by the session "
            f"ACL for browsing context '{context_id}'."
        ),
    }

    mapper.received_events.clear()
    url = html("<script>console.log('revoked log');</script>")
    await execute_command(
        mapper,
        {
            "method": "browsingContext.navigate",
            "params": {"context": context_id, "url": url, "wait": "complete"},
        },
    )
    # Flush the mapper's FIFO outgoing queue before checking `received_events`.
    await get_tree(mapper)
    assert mapper.received_events == [
        AnyExtending(
            {
                "type": "event",
                "method": "browsingContext.navigationCommitted",
                "params": {"context": context_id, "url": url},
            }
        )
    ]

    # Embedder pushes another update without a reason, revoking `navigate`.
    await mapper.notify_acl_changed(
        {"default": {"commands": ["browsingContext.getTree"]}}
    )
    cleared_event = await wait_for_event(mapper, "goog:session.changed")
    assert cleared_event == {
        "type": "event",
        "method": "goog:session.changed",
        "params": {
            "acl": {
                "default": {"commands": ["browsingContext.getTree"], "events": []},
                "perNavigable": {},
            },
        },
    }
    assert "reason" not in cleared_event["params"]

    with pytest.raises(Exception) as exception_info:
        await execute_command(
            mapper,
            {
                "method": "browsingContext.navigate",
                "params": {"context": context_id, "url": html(), "wait": "complete"},
            },
        )
    assert exception_info.value.args[0] == {
        "error": "unsupported operation",
        "message": (
            "Command 'browsingContext.navigate' is not allowed by the session "
            f"ACL for browsing context '{context_id}'."
        ),
    }
