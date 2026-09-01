#   Copyright 2026 Google LLC.
#   Copyright (c) Microsoft Corporation.
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import pytest
from test_helpers import execute_command


@pytest.fixture
def get_media_feature_matches(websocket, context_id):

    async def get_media_feature_matches(query, context_id=context_id):
        resp = await execute_command(
            websocket,
            {
                "method": "script.evaluate",
                "params": {
                    "expression": f"window.matchMedia('{query}').matches",
                    "target": {"context": context_id},
                    "awaitPromise": True,
                },
            },
        )
        return resp["result"]["value"]

    return get_media_feature_matches


@pytest.mark.asyncio
async def test_set_media_features_per_browsing_context(
    websocket, context_id, get_media_feature_matches
):
    await execute_command(
        websocket,
        {
            "method": "emulation.setMediaFeaturesOverride",
            "params": {
                "contexts": [context_id],
                "features": {"prefers-color-scheme": "dark"},
            },
        },
    )
    assert await get_media_feature_matches("(prefers-color-scheme: dark)")

    await execute_command(
        websocket,
        {
            "method": "emulation.setMediaFeaturesOverride",
            "params": {
                "contexts": [context_id],
                "features": None,
            },
        },
    )


@pytest.mark.asyncio
async def test_set_media_features_per_user_context(
    websocket, user_context_id, create_context, get_media_feature_matches
):
    await execute_command(
        websocket,
        {
            "method": "emulation.setMediaFeaturesOverride",
            "params": {
                "userContexts": [user_context_id],
                "features": {"prefers-color-scheme": "dark"},
            },
        },
    )

    context_id = await create_context(user_context_id)
    assert await get_media_feature_matches("(prefers-color-scheme: dark)", context_id)

    await execute_command(
        websocket,
        {
            "method": "emulation.setMediaFeaturesOverride",
            "params": {
                "userContexts": [user_context_id],
                "features": None,
            },
        },
    )


@pytest.mark.asyncio
async def test_set_media_features_globally(
    websocket, context_id, create_context, get_media_feature_matches
):
    await execute_command(
        websocket,
        {
            "method": "emulation.setMediaFeaturesOverride",
            "params": {
                "features": {"prefers-color-scheme": "dark"},
            },
        },
    )

    assert await get_media_feature_matches("(prefers-color-scheme: dark)")

    new_context_id = await create_context()
    assert await get_media_feature_matches(
        "(prefers-color-scheme: dark)", new_context_id
    )

    await execute_command(
        websocket,
        {
            "method": "emulation.setMediaFeaturesOverride",
            "params": {
                "features": None,
            },
        },
    )


@pytest.mark.asyncio
async def test_set_media_features_unsupported_feature(websocket, context_id):
    with pytest.raises(Exception, match="unsupported operation"):
        await execute_command(
            websocket,
            {
                "method": "emulation.setMediaFeaturesOverride",
                "params": {
                    "contexts": [context_id],
                    "features": {"hover": "hover"},
                },
            },
        )
