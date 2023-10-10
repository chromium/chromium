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
import pytest
from anys import ANY_DICT, ANY_LIST, ANY_STR
from test_helpers import (ANY_TIMESTAMP, ANY_UUID, AnyExtending,
                          execute_command, send_JSON_command, subscribe,
                          wait_for_event)


@pytest.mark.asyncio
async def test_add_intercept_invalid_empty_phases(websocket):
    with pytest.raises(Exception,
                       match=str({
                           "error": "invalid argument",
                           "message": "At least one phase must be specified."
                       })):
        await execute_command(
            websocket, {
                "method": "network.addIntercept",
                "params": {
                    "phases": [],
                    "urlPatterns": [{
                        "type": 'string',
                        "pattern": "https://www.example.com/*",
                    }],
                },
            })


@pytest.mark.asyncio
async def test_add_intercept_returns_intercept_id(websocket):
    result = await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": ["beforeRequestSent"],
                "urlPatterns": [{
                    "type": "string",
                    "pattern": "https://www.example.com/*"
                }],
            },
        })

    assert result == {
        "intercept": ANY_UUID,
    }


@pytest.mark.asyncio
async def test_add_intercept_type_pattern_valid(websocket):
    result = await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": ["beforeRequestSent"],
                "urlPatterns": [{
                    "type": "pattern",
                    "protocol": "https",
                    "hostname": "www.example.com",
                    "path": "/*",
                }],
            },
        })

    assert result == {
        "intercept": ANY_UUID,
    }


@pytest.mark.asyncio
async def test_add_intercept_type_pattern_invalid(websocket):
    with pytest.raises(
            Exception,
            match=str({
                "error": "invalid argument",
                "message": "TypeError: Failed to construct 'URL': Invalid URL"
            })):
        await execute_command(
            websocket, {
                "method": "network.addIntercept",
                "params": {
                    "phases": ["beforeRequestSent"],
                    "urlPatterns": [{
                        "type": "pattern",
                        "hostname": "foo"
                    }],
                },
            })


@pytest.mark.asyncio
async def test_add_intercept_type_string_invalid(websocket):
    with pytest.raises(
            Exception,
            match=str({
                "error": "invalid argument",
                "message": "Invalid URL 'foo': TypeError: Failed to construct 'URL': Invalid URL"
            })):
        await execute_command(
            websocket, {
                "method": "network.addIntercept",
                "params": {
                    "phases": ["beforeRequestSent"],
                    "urlPatterns": [{
                        "type": "string",
                        "pattern": "foo",
                    }],
                },
            })


@pytest.mark.asyncio
async def test_add_intercept_type_string_one_valid_and_one_invalid(websocket):
    with pytest.raises(
            Exception,
            match=str({
                "error": "invalid argument",
                "message": "Invalid URL 'foo': TypeError: Failed to construct 'URL': Invalid URL"
            })):
        await execute_command(
            websocket, {
                "method": "network.addIntercept",
                "params": {
                    "phases": ["beforeRequestSent"],
                    "urlPatterns": [{
                        "type": "string",
                        "pattern": "foo",
                    }, {
                        "type": "string",
                        "pattern": "https://www.example.com/",
                    }],
                },
            })


@pytest.mark.asyncio
async def test_add_intercept_type_pattern_protocol_empty_invalid(websocket):
    with pytest.raises(Exception,
                       match=str({
                           "error": "invalid argument",
                           "message": "URL pattern must specify a protocol"
                       })):
        await execute_command(
            websocket, {
                "method": "network.addIntercept",
                "params": {
                    "phases": ["beforeRequestSent"],
                    "urlPatterns": [{
                        "type": "pattern",
                        "protocol": "",
                        "hostname": "www.example.com",
                        "port": "80",
                    }],
                },
            })


@pytest.mark.asyncio
async def test_add_intercept_type_pattern_protocol_non_special_success(
        websocket):
    result = await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": ["beforeRequestSent"],
                "urlPatterns": [{
                    "type": "pattern",
                    "protocol": "sftp",
                    "hostname": "www.example.com",
                    "port": "22",
                }],
            },
        })

    assert result == {
        "intercept": ANY_UUID,
    }


@pytest.mark.asyncio
async def test_add_intercept_type_pattern_hostname_empty_invalid(websocket):
    with pytest.raises(Exception,
                       match=str({
                           "error": "invalid argument",
                           "message": "URL pattern must specify a hostname"
                       })):
        await execute_command(
            websocket, {
                "method": "network.addIntercept",
                "params": {
                    "phases": ["beforeRequestSent"],
                    "urlPatterns": [{
                        "type": "pattern",
                        "protocol": "https",
                        "hostname": "",
                        "port": "80",
                    }],
                },
            })


@pytest.mark.asyncio
@pytest.mark.parametrize("hostname", ["abc:com", "abc::com"])
async def test_add_intercept_type_pattern_hostname_invalid(
        websocket, hostname):
    with pytest.raises(
            Exception,
            match=str({
                "error": "invalid argument",
                "message": "URL pattern hostname must not contain a colon"
            })):
        await execute_command(
            websocket, {
                "method": "network.addIntercept",
                "params": {
                    "phases": ["beforeRequestSent"],
                    "urlPatterns": [{
                        "type": "pattern",
                        "hostname": hostname,
                    }],
                },
            })


@pytest.mark.asyncio
async def test_add_intercept_type_pattern_port_empty_invalid(websocket):
    with pytest.raises(Exception,
                       match=str({
                           "error": "invalid argument",
                           "message": "URL pattern must specify a port"
                       })):
        await execute_command(
            websocket, {
                "method": "network.addIntercept",
                "params": {
                    "phases": ["beforeRequestSent"],
                    "urlPatterns": [{
                        "type": "pattern",
                        "protocol": "https",
                        "hostname": "www.example.com",
                        "port": "",
                    }],
                },
            })


@pytest.mark.asyncio
@pytest.mark.parametrize("url_patterns", [
    [
        {
            "type": "string",
            "pattern": "https://www.example.com/",
        },
    ],
    [
        {
            "type": "pattern",
            "protocol": "https",
            "hostname": "www.example.com",
            "pathname": "/",
        },
    ],
    [
        {
            "type": "string",
            "pattern": "https://www.example.com/",
        },
        {
            "type": "pattern",
            "protocol": "https",
            "hostname": "www.example.com",
            "pathname": "/",
        },
    ],
],
                         ids=[
                             "string",
                             "pattern",
                             "string and pattern",
                         ])
async def test_add_intercept_blocks_use_cdp_events(websocket, context_id,
                                                   url_patterns, example_url):
    await subscribe(websocket, ["cdp.Fetch.requestPaused"])

    result = await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": ["beforeRequestSent"],
                "urlPatterns": url_patterns,
            },
        })

    assert result == {
        "intercept": ANY_UUID,
    }

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": example_url,
                "context": context_id,
            }
        })

    event_response = await wait_for_event(websocket, "cdp.Fetch.requestPaused")
    assert event_response == {
        "method": "cdp.Fetch.requestPaused",
        "params": {
            "event": "Fetch.requestPaused",
            "params": {
                "frameId": context_id,
                "networkId": ANY_STR,
                "request": AnyExtending({
                    "headers": ANY_DICT,
                    "url": example_url,
                }),
                "requestId": ANY_STR,
                "resourceType": "Document",
            },
            "session": ANY_STR,
        },
        "type": "event",
    }


@pytest.mark.asyncio
@pytest.mark.parametrize("url_patterns", [
    [
        {
            "type": "string",
            "pattern": "https://www.example.com/",
        },
    ],
    [
        {
            "type": "pattern",
            "protocol": "https",
            "hostname": "www.example.com",
            "pathname": "/",
        },
    ],
    [
        {
            "type": "string",
            "pattern": "https://www.example.com/",
        },
        {
            "type": "pattern",
            "protocol": "https",
            "hostname": "www.example.com",
            "pathname": "/",
        },
    ],
],
                         ids=[
                             "string",
                             "pattern",
                             "string and pattern",
                         ])
async def test_add_intercept_blocks_use_bidi_events(websocket, context_id,
                                                    url_patterns, example_url):
    await subscribe(websocket, ["network.beforeRequestSent"])

    result = await execute_command(
        websocket, {
            "method": "network.addIntercept",
            "params": {
                "phases": ["beforeRequestSent"],
                "urlPatterns": url_patterns,
            },
        })

    assert result == {
        "intercept": ANY_UUID,
    }

    await send_JSON_command(
        websocket, {
            "method": "browsingContext.navigate",
            "params": {
                "url": example_url,
                "context": context_id,
            }
        })

    event_response = await wait_for_event(websocket,
                                          "network.beforeRequestSent")
    assert event_response == {
        "method": "network.beforeRequestSent",
        "params": {
            "isBlocked": True,
            "initiator": {
                "type": "other",
            },
            "context": context_id,
            "navigation": ANY_STR,
            "redirectCount": 0,
            "request": {
                "request": ANY_STR,
                "url": example_url,
                "method": "GET",
                "headers": ANY_LIST,
                "cookies": [],
                "headersSize": -1,
                "bodySize": 0,
                "timings": ANY_DICT
            },
            "timestamp": ANY_TIMESTAMP,
        },
        "type": "event",
    }
