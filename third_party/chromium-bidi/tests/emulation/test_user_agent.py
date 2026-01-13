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
from json import JSONDecoder

import pytest
import pytest_asyncio
from test_helpers import execute_command, goto_url

USER_AGENT_HEADER_NAME = "User-Agent"
SOME_USER_AGENT = "some user agent"
ANOTHER_USER_AGENT = "another user agent"


@pytest_asyncio.fixture
async def default_user_agent(context_id, get_navigator_user_agent):
    """
    Returns default user agent.
    """
    return await get_navigator_user_agent(context_id)


@pytest.fixture
def get_navigator_user_agent(websocket):

    async def get_navigator_user_agent(context_id):
        """
        Returns browsing context's current user agent accessed via navigator.
        """
        resp = await execute_command(
            websocket, {
                "method": "script.evaluate",
                "params": {
                    "expression": "window.navigator.userAgent",
                    "target": {
                        "context": context_id
                    },
                    "awaitPromise": True,
                }
            })

        return resp["result"]["value"] if "result" in resp else resp

    return get_navigator_user_agent


@pytest.fixture
def assert_network_user_agent(websocket, get_url_echo):
    """ Assert the `User-Agent` header is set during navigation and during fetch. """

    async def assert_network_user_agent(context_id, expected_user_agent,
                                        same_origin):
        # First navigate to the echo page.
        await goto_url(websocket, context_id, get_url_echo(same_origin))
        # Get the navigation headers.
        response = await execute_command(
            websocket, {
                'method': 'script.evaluate',
                'params': {
                    'expression': "window.document.body.textContent",
                    'awaitPromise': True,
                    'target': {
                        'context': context_id
                    }
                }
            })
        response_obj = JSONDecoder().decode(response['result']['value'])
        headers = response_obj['headers']
        # Assert the right user agent header was sent on the navigation request.
        assert USER_AGENT_HEADER_NAME in headers
        assert headers[USER_AGENT_HEADER_NAME] == expected_user_agent

        # Make a fetch request.
        response = await execute_command(
            websocket, {
                'method': 'script.evaluate',
                'params': {
                    'expression': f"fetch('{get_url_echo(same_origin)}').then(r => r.text())",
                    'awaitPromise': True,
                    'target': {
                        'context': context_id
                    }
                }
            })
        response_obj = JSONDecoder().decode(response['result']['value'])
        headers = response_obj['headers']
        # Assert the right user agent header was sent on the navigation request.
        assert USER_AGENT_HEADER_NAME in headers
        assert headers[USER_AGENT_HEADER_NAME] == expected_user_agent

    return assert_network_user_agent


@pytest.fixture
def assert_navigator_user_agent(get_navigator_user_agent):
    """ Assert the `window.navigator.userAgent` returns the expected value. """

    async def assert_navigator_user_agent(context_id, expected_user_agent):
        assert await get_navigator_user_agent(context_id
                                              ) == expected_user_agent

    return assert_navigator_user_agent


@pytest.fixture
def assert_user_agent(assert_navigator_user_agent, assert_network_user_agent):
    """ Assert the user agent is emulated properly both in DOM and in network. """

    async def assert_user_agent(context_id,
                                expected_user_agent,
                                same_origin=True):
        await assert_navigator_user_agent(context_id, expected_user_agent)
        await assert_network_user_agent(context_id, expected_user_agent,
                                        same_origin)

    return assert_user_agent


@pytest.mark.asyncio
async def test_user_agent_global_set_and_clear(websocket, context_id,
                                               default_user_agent,
                                               assert_user_agent,
                                               user_context_id,
                                               create_context):
    # Set global override.
    await execute_command(
        websocket, {
            'method': 'emulation.setUserAgentOverride',
            'params': {
                'userAgent': SOME_USER_AGENT
            }
        })

    # Assert the override applies to the existing context.
    await assert_user_agent(context_id, SOME_USER_AGENT)

    # Assert the override applies to new browsing contexts.
    browsing_context_id_1 = await create_context()
    await assert_user_agent(browsing_context_id_1, SOME_USER_AGENT)

    browsing_context_id_2 = await create_context(user_context_id)
    await assert_user_agent(browsing_context_id_2, SOME_USER_AGENT)

    await execute_command(websocket, {
        'method': 'emulation.setUserAgentOverride',
        'params': {
            'userAgent': None
        }
    })
    await assert_user_agent(context_id, default_user_agent)
    await assert_user_agent(browsing_context_id_1, default_user_agent)
    await assert_user_agent(browsing_context_id_2, default_user_agent)


@pytest.mark.asyncio
async def test_user_agent_per_user_context(websocket, user_context_id,
                                           create_context, assert_user_agent):
    # Set different user_agent overrides for different user contexts.
    await execute_command(
        websocket, {
            'method': 'emulation.setUserAgentOverride',
            'params': {
                'userContexts': ["default"],
                'userAgent': SOME_USER_AGENT
            }
        })
    await execute_command(
        websocket, {
            'method': 'emulation.setUserAgentOverride',
            'params': {
                'userContexts': [user_context_id],
                'userAgent': SOME_USER_AGENT
            }
        })

    # Assert the overrides applied for the right contexts.
    browsing_context_id_1 = await create_context()
    await assert_user_agent(browsing_context_id_1, SOME_USER_AGENT)

    browsing_context_id_2 = await create_context(user_context_id)
    await assert_user_agent(browsing_context_id_2, SOME_USER_AGENT)


@pytest.mark.asyncio
async def test_user_agent_per_browsing_context(websocket, context_id,
                                               another_context_id,
                                               assert_user_agent):
    # Set different user_agent overrides for different user contexts.
    await execute_command(
        websocket, {
            'method': 'emulation.setUserAgentOverride',
            'params': {
                'contexts': [context_id],
                'userAgent': SOME_USER_AGENT
            }
        })
    await execute_command(
        websocket, {
            'method': 'emulation.setUserAgentOverride',
            'params': {
                'contexts': [another_context_id],
                'userAgent': SOME_USER_AGENT
            }
        })

    await assert_user_agent(context_id, SOME_USER_AGENT)
    await assert_user_agent(another_context_id, SOME_USER_AGENT)


@pytest.mark.asyncio
async def test_user_agent_affects_iframe(websocket, context_id, iframe_id,
                                         html, default_user_agent,
                                         assert_user_agent):
    await execute_command(
        websocket, {
            'method': 'emulation.setUserAgentOverride',
            'params': {
                'contexts': [context_id],
                'userAgent': SOME_USER_AGENT
            }
        })

    await assert_user_agent(iframe_id, SOME_USER_AGENT)

    # Move iframe out of process
    await goto_url(websocket, iframe_id,
                   html("<h1>FRAME</h1>", same_origin=False))
    # Assert user_agent emulation persisted.
    await assert_user_agent(iframe_id, SOME_USER_AGENT, same_origin=False)

    await execute_command(
        websocket, {
            'method': 'emulation.setUserAgentOverride',
            'params': {
                'contexts': [context_id],
                'userAgent': SOME_USER_AGENT
            }
        })
    await assert_user_agent(iframe_id, SOME_USER_AGENT, same_origin=False)

    await execute_command(
        websocket, {
            'method': 'emulation.setUserAgentOverride',
            'params': {
                'contexts': [context_id],
                'userAgent': None
            }
        })
    await assert_user_agent(iframe_id, default_user_agent, same_origin=False)


@pytest.mark.asyncio
async def test_user_agent_per_iframe(websocket, context_id, iframe_id, html,
                                     default_user_agent, assert_user_agent):
    with pytest.raises(
            Exception,
            match=str({
                "error": "invalid argument",
                "message": "The command is only supported on the top-level context"
            })):
        await execute_command(
            websocket, {
                'method': 'emulation.setUserAgentOverride',
                'params': {
                    'contexts': [iframe_id],
                    'userAgent': SOME_USER_AGENT
                }
            })


@pytest.mark.asyncio
async def test_user_agent_invalid_value(websocket, context_id):
    invalid_user_agent = ""
    with pytest.raises(
            Exception,
            match=str({
                "error": "unsupported operation",
                "message": "empty user agent string is not supported"
            })):
        await execute_command(
            websocket, {
                'method': 'emulation.setUserAgentOverride',
                'params': {
                    'contexts': [context_id],
                    'userAgent': invalid_user_agent
                }
            })
