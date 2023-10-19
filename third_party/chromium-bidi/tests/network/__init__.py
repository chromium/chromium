#  Copyright 2023 Google LLC.
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
from typing import Literal

from test_helpers import (execute_command, send_JSON_command, subscribe,
                          wait_for_event)


async def create_blocked_request(websocket, context_id: str, *, url: str,
                                 phases: list[Literal["beforeRequestSent",
                                                      "responseStarted",
                                                      "authRequired"]]):
    """Creates a dummy blocked network request and returns its network id."""

    await subscribe(websocket, ["cdp.Fetch.requestPaused"])

    await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": phases,
                "urlPatterns": [{
                    "type": "string",
                    "pattern": url,
                }, ],
            },
        })
    # TODO: How about we try to evaluate a fetch instead of navigating that way.
    # Then are less likely to have another event and would speed up the test.
    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": url,
                "context": context_id,
            }
        })
    event_response = await wait_for_event(websocket, "cdp.Fetch.requestPaused")
    network_id = event_response["params"]["params"]["networkId"]

    return network_id
