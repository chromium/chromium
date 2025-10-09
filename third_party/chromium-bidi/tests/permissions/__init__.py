#  Copyright 2024 Google LLC.
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

from test_helpers import execute_command


async def query_permission(websocket, context_id, name):
    """ Queries a permission via the script.callFunction command."""

    result = await execute_command(
        websocket, {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": """() => {
                    return navigator.permissions.query({ name: '%s' })
                      .then(val => val.state, err => err.message)
                  }""" % name,
                "target": {
                    "context": context_id
                },
                "awaitPromise": True,
            }
        })

    return result['result']['value']


async def set_permission(websocket,
                         origin,
                         descriptor,
                         state,
                         user_context=None,
                         embedded_origin=None):
    """ Set a permission via the permissions.setPermission command."""
    return await execute_command(
        websocket, {
            'method': 'permissions.setPermission',
            'params': {
                'origin': origin,
                'descriptor': descriptor,
                'state': state,
                'goog:userContext': user_context,
                **({} if embedded_origin is None else {
                       "embeddedOrigin": embedded_origin
                   })
            }
        })
