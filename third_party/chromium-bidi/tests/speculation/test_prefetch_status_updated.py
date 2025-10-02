# Copyright 2025 Google LLC.
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
from test_helpers import send_JSON_command, subscribe, wait_for_events


@pytest.mark.asyncio
async def test_speculation_rules_generate_ready_events(websocket, context_id,
                                                       html):
    """Test that speculation rules actually generate prefetch status events."""
    await subscribe(websocket, ["speculation.prefetchStatusUpdated"])

    # Create a simple target page to prefetch
    target_page = html("""
        <h1>Target Page</h1>
        <p>This is the page that should be prefetched</p>
    """)

    # Create a main page with speculation rules pointing to the target page
    main_page = html(f"""
        <head>
            <title>Speculation Rules Test</title>
        </head>
        <body>
            <h1>Speculation Rules Test</h1>
            <a href="{target_page}" id="prefetch-page">Go to Target Page</a>

            <script type="speculationrules">
            {{
                "prefetch": [
                    {{
                        "eagerness": "immediate",
                        "where": {{"href_matches": "/*"}}
                    }}
                ]
            }}
            </script>
        </body>
    """)

    # Initiate navigation but don't wait for the command to be finished.
    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": main_page,
                "context": context_id,
                "wait": "none"
            }
        })

    # Wait for all the events
    events = [
        await wait_for_events(websocket,
                              ["speculation.prefetchStatusUpdated"]), await
        wait_for_events(websocket, ["speculation.prefetchStatusUpdated"])
    ]

    # Verify all events have correct structure
    assert len(events) == 2, "Expected 2 prefetch events (pending and ready)"
    assert events == [{
        "type": "event",
        "method": "speculation.prefetchStatusUpdated",
        "params": {
            "url": target_page,
            "status": "pending",
            "context": context_id
        }
    }, {
        "type": "event",
        "method": "speculation.prefetchStatusUpdated",
        "params": {
            "url": target_page,
            "status": "ready",
            "context": context_id
        }
    }]


@pytest.mark.asyncio
async def test_speculation_rules_generate_events_with_navigation(
        websocket, context_id, html):
    """Test that speculation rules generate prefetch status events including success events when navigating."""
    await subscribe(websocket, ["speculation.prefetchStatusUpdated"])

    # Create a simple target page to prefetch
    target_page = html("""
        <h1>Target Page</h1>
        <p>This is the page that should be prefetched</p>
    """)

    # Create a main page with speculation rules pointing to the target page
    main_page = html(f"""
        <head>
            <title>Speculation Rules Test</title>
        </head>
        <body>
            <h1>Speculation Rules Test</h1>
            <a href="{target_page}" id="prefetch-page">Go to Target Page</a>

            <script type="speculationrules">
            {{
                "prefetch": [
                    {{
                        "eagerness": "immediate",
                        "where": {{"href_matches": "/*"}}
                    }}
                ]
            }}
            </script>
        </body>
    """)

    # Initiate navigation but don't wait for the command to be finished.
    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": main_page,
                "context": context_id,
                "wait": "none"
            }
        })

    # Wait for initial events
    events = [
        await wait_for_events(websocket,
                              ["speculation.prefetchStatusUpdated"]), await
        wait_for_events(websocket, ["speculation.prefetchStatusUpdated"])
    ]

    # Navigate by clicking the link (user-initiated navigation to trigger success event)
    click_script = """
    const link = document.getElementById('prefetch-page');
    if (link) {
        link.click();
    }
    """
    click_command = {
        "method": "script.evaluate",
        "params": {
            "expression": click_script,
            "target": {
                "context": context_id
            },
            "awaitPromise": False
        },
        "wait": "none"
    }
    await send_JSON_command(websocket, click_command)

    events.append(await wait_for_events(websocket,
                                        ["speculation.prefetchStatusUpdated"]))

    # Verify all events have correct structure
    assert len(
        events
    ) == 3, f"Expected 3 prefetch events (pending, ready, and success), got {len(events)}"
    assert events == [{
        "type": "event",
        "method": "speculation.prefetchStatusUpdated",
        "params": {
            "url": target_page,
            "status": "pending",
            "context": context_id
        }
    }, {
        "type": "event",
        "method": "speculation.prefetchStatusUpdated",
        "params": {
            "url": target_page,
            "status": "ready",
            "context": context_id
        }
    }, {
        "type": "event",
        "method": "speculation.prefetchStatusUpdated",
        "params": {
            "url": target_page,
            "status": "success",
            "context": context_id
        }
    }]


@pytest.mark.asyncio
async def test_speculation_rules_generate_failure_events(
        websocket, context_id, html):
    """Test that speculation rules generate failure status events for failed prefetch."""
    await subscribe(websocket, ["speculation.prefetchStatusUpdated"])

    # Create a target page that will return 404
    success_page = html("This page will work")
    failed_target = success_page.replace("/200/", "/404/")

    # Create a main page with speculation rules pointing to the 404 page
    main_page = html(f"""
        <head>
            <title>Speculation Rules Failure Test</title>
        </head>
        <body>
            <h1>Speculation Rules Failure Test</h1>
            <a href="{failed_target}" id="prefetch-page">Go to 404 Page</a>

            <script type="speculationrules">
            {{
                "prefetch": [
                    {{
                        "eagerness": "immediate",
                        "where": {{"href_matches": "/*"}}
                    }}
                ]
            }}
            </script>
        </body>
    """)

    # Initiate navigation but don't wait for the command to be finished.
    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": main_page,
                "context": context_id,
                "wait": "none"
            }
        })

    # Wait for all the events
    events = [
        await wait_for_events(websocket,
                              ["speculation.prefetchStatusUpdated"]), await
        wait_for_events(websocket, ["speculation.prefetchStatusUpdated"])
    ]

    # Verify all events have correct structure
    assert len(
        events
    ) == 2, f"Expected 2 prefetch events (pending and failure), got {len(events)}"
    assert events == [{
        "type": "event",
        "method": "speculation.prefetchStatusUpdated",
        "params": {
            "url": failed_target,
            "status": "pending",
            "context": context_id
        }
    }, {
        "type": "event",
        "method": "speculation.prefetchStatusUpdated",
        "params": {
            "url": failed_target,
            "status": "failure",
            "context": context_id
        }
    }]
