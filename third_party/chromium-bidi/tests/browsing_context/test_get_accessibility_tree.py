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

import re

import pytest
from test_helpers import ANY_SHARED_ID, execute_command, goto_url


async def get_accessibility_tree(websocket, context):
    resp = await execute_command(
        websocket,
        {
            "method": "goog:browsingContext.getAccessibilityTree",
            "params": {"context": context},
        },
    )
    return resp["root"]


def flatten(root):
    """Return the nodes of the tree in pre-order."""
    nodes = []
    stack = [root]
    while stack:
        node = stack.pop()
        nodes.append(node)
        stack.extend(reversed(node["children"]))
    return nodes


async def get_shared_id(websocket, context, expression):
    resp = await execute_command(
        websocket,
        {
            "method": "script.evaluate",
            "params": {
                "expression": expression,
                "target": {"context": context},
                "awaitPromise": False,
            },
        },
    )
    return resp["result"]["sharedId"]


def find_nodes(root, role, name=None):
    return [
        node
        for node in flatten(root)
        if node.get("role") == role and (name is None or node.get("name") == name)
    ]


@pytest.mark.asyncio
async def test_get_accessibility_tree(websocket, context_id, html):
    await goto_url(
        websocket,
        context_id,
        html(
            "<h1>Title</h1>"
            '<button aria-description="Sends the form" disabled>Submit</button>'
            '<input type="checkbox" aria-label="Agree" checked />'
        ),
    )

    root = await get_accessibility_tree(websocket, context_id)

    # The root node is the document.
    assert root["role"] == "RootWebArea"
    assert root["sharedId"] == await get_shared_id(websocket, context_id, "document")

    nodes = flatten(root)
    # The accessibility IDs are unique.
    assert len({node["accessibilityId"] for node in nodes}) == len(nodes)
    for node in nodes:
        assert isinstance(node["accessibilityId"], str)
        assert isinstance(node["children"], list)
        # The node contains only the fields of the specification draft.
        assert node.keys() <= {
            "accessibilityId",
            "children",
            "name",
            "properties",
            "role",
            "sharedId",
        }
        # The optional fields are absent if they are empty.
        assert node.get("name") != ""
        assert node.get("properties") != {}
        for value in node.get("properties", {}).values():
            assert isinstance(value, str)
        if "sharedId" in node:
            assert node["sharedId"] == ANY_SHARED_ID

    [heading] = find_nodes(root, "heading", "Title")
    assert heading["properties"]["level"] == "1"
    assert heading["sharedId"] == await get_shared_id(
        websocket, context_id, "document.querySelector('h1')"
    )

    [button] = find_nodes(root, "button", "Submit")
    assert button["properties"]["description"] == "Sends the form"
    assert button["properties"]["disabled"] == "true"
    assert button["sharedId"] == await get_shared_id(
        websocket, context_id, "document.querySelector('button')"
    )

    [checkbox] = find_nodes(root, "checkbox", "Agree")
    assert checkbox["properties"]["checked"] == "true"
    assert checkbox["sharedId"] == await get_shared_id(
        websocket, context_id, "document.querySelector('input')"
    )


@pytest.mark.asyncio
async def test_get_accessibility_tree_shared_id_is_usable(websocket, context_id, html):
    await goto_url(websocket, context_id, html("<button>Submit</button>"))

    root = await get_accessibility_tree(websocket, context_id)
    [button] = find_nodes(root, "button", "Submit")

    resp = await execute_command(
        websocket,
        {
            "method": "script.callFunction",
            "params": {
                "functionDeclaration": "(element) => element.textContent",
                "arguments": [{"sharedId": button["sharedId"]}],
                "target": {"context": context_id},
                "awaitPromise": False,
            },
        },
    )
    assert resp["result"] == {"type": "string", "value": "Submit"}


@pytest.mark.asyncio
async def test_get_accessibility_tree_accessibility_ids_are_consistent(
    websocket, context_id, html
):
    await goto_url(websocket, context_id, html("<button>Submit</button>"))

    [button_1] = find_nodes(
        await get_accessibility_tree(websocket, context_id), "button", "Submit"
    )
    [button_2] = find_nodes(
        await get_accessibility_tree(websocket, context_id), "button", "Submit"
    )
    assert button_1 == button_2


@pytest.mark.asyncio
async def test_get_accessibility_tree_relation_properties(websocket, context_id, html):
    await goto_url(
        websocket,
        context_id,
        html(
            '<span id="first">First</span><span id="second">Second</span>'
            '<button aria-labelledby="first second">Submit</button>'
        ),
    )

    root = await get_accessibility_tree(websocket, context_id)

    [button] = find_nodes(root, "button", "First Second")
    # The value is the space-separated list of the shared IDs.
    assert button["properties"]["labelledby"] == " ".join(
        [
            await get_shared_id(
                websocket, context_id, "document.getElementById('first')"
            ),
            await get_shared_id(
                websocket, context_id, "document.getElementById('second')"
            ),
        ]
    )


@pytest.mark.asyncio
async def test_get_accessibility_tree_excludes_ignored_nodes(
    websocket, context_id, html
):
    await goto_url(
        websocket,
        context_id,
        html(
            '<ul><div role="presentation"><li>Item</li></div></ul>'
            '<button aria-hidden="true">Hidden</button>'
        ),
    )

    root = await get_accessibility_tree(websocket, context_id)

    # The children of an ignored node replace the ignored node.
    [list_node] = find_nodes(root, "list")
    assert [child.get("role") for child in list_node["children"]] == ["listitem"]
    assert find_nodes(root, "none") == []
    assert find_nodes(root, "button") == []


@pytest.mark.parametrize("same_origin", [True, False])
@pytest.mark.asyncio
async def test_get_accessibility_tree_excludes_iframe_document(
    websocket, context_id, html, iframe, same_origin
):
    await goto_url(
        websocket,
        context_id,
        html(iframe(html("<h1>FRAME</h1>", same_origin=same_origin))),
    )

    root = await get_accessibility_tree(websocket, context_id)

    # The tree includes the iframe element of the top-level document.
    [iframe_node] = find_nodes(root, "Iframe")
    assert iframe_node["sharedId"] == await get_shared_id(
        websocket, context_id, "document.querySelector('iframe')"
    )
    # The tree does not include the nodes of the iframe document.
    assert iframe_node["children"] == []
    assert find_nodes(root, "heading", "FRAME") == []
    assert len(find_nodes(root, "RootWebArea")) == 1


@pytest.mark.asyncio
async def test_get_accessibility_tree_iframe_not_supported(websocket, iframe_id):
    with pytest.raises(
        Exception,
        match=re.escape(
            str(
                {
                    "error": "invalid argument",
                    "message": f"Non-top-level 'context' ({iframe_id}) is not supported",
                }
            )
        ),
    ):
        await get_accessibility_tree(websocket, iframe_id)


@pytest.mark.asyncio
async def test_get_accessibility_tree_unknown_context(websocket):
    with pytest.raises(
        Exception,
        match=str(
            {
                "error": "no such frame",
                "message": "Context UNKNOWN_CONTEXT not found",
            }
        ),
    ):
        await get_accessibility_tree(websocket, "UNKNOWN_CONTEXT")


@pytest.mark.asyncio
async def test_get_accessibility_tree_invalid_params(websocket):
    with pytest.raises(Exception, match="invalid argument"):
        await execute_command(
            websocket,
            {
                "method": "goog:browsingContext.getAccessibilityTree",
                "params": {"context": 42},
            },
        )
