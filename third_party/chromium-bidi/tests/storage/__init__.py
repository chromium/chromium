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

from test_helpers import execute_command


def get_bidi_cookie(cookie_name,
                    cookie_value,
                    domain,
                    path="/",
                    http_only=False,
                    secure=True,
                    same_site='none',
                    expiry=None):
    """ Return a cookie object with the given parameters."""
    return {
        'domain': domain,
        'httpOnly': http_only,
        'name': cookie_name,
        'path': path,
        'sameSite': same_site,
        'secure': secure,
        'size': len(cookie_name) + len(cookie_value),
        'value': {
            'type': 'string',
            'value': cookie_value,
        },
    } | ({
        'expiry': expiry
    } if expiry is not None else {})


async def set_cookie(websocket, context_id, bidi_cookie, partition=None):
    """ Set cookie via BiDi command."""
    await execute_command(
        websocket, {
            'method': 'storage.setCookie',
            'params': {
                'cookie': bidi_cookie,
            } | ({
                'partition': partition
            } if partition is not None else {})
        })
