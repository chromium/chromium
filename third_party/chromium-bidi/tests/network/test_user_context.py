#  Copyright 2026 Google LLC.
#  Copyright (c) Microsoft Corporation.
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
import pytest
from test_helpers import (
    AnyExtending,
    execute_command,
    goto_url,
    wait_for_filtered_event,
)


@pytest.mark.asyncio
async def test_worker_network_events_with_user_context_subscription(
    websocket, context_id, url_base
):
    await execute_command(
        websocket,
        {
            "method": "session.subscribe",
            "params": {
                "events": ["network.beforeRequestSent"],
                "userContexts": ["default"],
            },
        },
    )

    await goto_url(websocket, context_id, url_base)

    script = f"""
    const blob = new Blob(
        ["fetch('{url_base}/worker-data');"],
        {{type: 'application/javascript'}}
    );
    const worker = new Worker(URL.createObjectURL(blob));
    """

    await execute_command(
        websocket,
        {
            "method": "script.evaluate",
            "params": {
                "expression": script,
                "target": {"context": context_id},
                "awaitPromise": True,
            },
        },
    )

    # Wait for the network event for the worker's fetch.
    event = await wait_for_filtered_event(
        websocket,
        lambda e: (
            e["method"] == "network.beforeRequestSent"
            and e["params"]["request"]["url"] == f"{url_base}/worker-data"
        ),
    )

    assert event == AnyExtending(
        {
            "type": "event",
            "method": "network.beforeRequestSent",
            "params": {
                "userContext": "default",
                "request": {
                    "url": f"{url_base}/worker-data",
                },
            },
        }
    )
