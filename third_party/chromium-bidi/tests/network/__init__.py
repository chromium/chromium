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

from test_helpers import (create_request_via_fetch, execute_command, subscribe,
                          wait_for_event)


async def create_blocked_request(websocket, context_id: str, url: str,
                                 phase: Literal["beforeRequestSent",
                                                "responseStarted",
                                                "authRequired"]):
    """Creates a dummy blocked network request and returns its network id."""

    event = f"network.{phase}"

    await subscribe(websocket, [event], context_ids=[context_id])

    await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": [phase],
                "urlPatterns": [{
                    "type": "string",
                    "pattern": url,
                }, ],
            },
        })

    await create_request_via_fetch(websocket, context_id, url)

    event_response = await wait_for_event(websocket, event)
    network_id = event_response["params"]["request"]["request"]

    return network_id
