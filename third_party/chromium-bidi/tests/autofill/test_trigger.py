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

import json

import pytest
from test_helpers import execute_command, goto_url

CARD = {
    "number": "4444444444444444",
    "name": "John Smith",
    "expiryMonth": "01",
    "expiryYear": "2030",
    "cvc": "123",
}

CARD_FORM = """
<form>
  <input id="name" autocomplete="cc-name">
  <input id="number" autocomplete="cc-number">
  <input id="month" autocomplete="cc-exp-month">
  <input id="year" autocomplete="cc-exp-year">
</form>
"""

ADDRESS = {
    "fields": [
        {"name": "given-name", "value": "John"},
        {"name": "family-name", "value": "Smith"},
        {"name": "address-line1", "value": "123 Main St"},
        {"name": "address-level2", "value": "San Francisco"},
        {"name": "address-level1", "value": "CA"},
        {"name": "postal-code", "value": "94102"},
        {"name": "country", "value": "US"},
    ]
}

ADDRESS_FORM = """
<form>
  <input id="given-name" autocomplete="given-name">
  <input id="family-name" autocomplete="family-name">
  <input id="address-line1" autocomplete="address-line1">
  <input id="address-level2" autocomplete="address-level2">
  <input id="address-level1" autocomplete="address-level1">
  <input id="postal-code" autocomplete="postal-code">
</form>
"""


async def get_field_values(websocket, context_id):
    """Returns the values of all the inputs in the context, keyed by input id."""
    result = await execute_command(
        websocket,
        {
            "method": "script.evaluate",
            "params": {
                "expression": "JSON.stringify(Object.fromEntries("
                "[...document.querySelectorAll('input')]"
                ".map((input) => [input.id, input.value])))",
                "awaitPromise": False,
                "target": {"context": context_id},
            },
        },
    )
    return json.loads(result["result"]["value"])


@pytest.mark.asyncio
async def test_autofill_trigger_card(websocket, context_id, html, query_selector):
    await goto_url(websocket, context_id, html(CARD_FORM))
    element = await query_selector("#number")

    result = await execute_command(
        websocket,
        {
            "method": "autofill.trigger",
            "params": {"context": context_id, "element": element, "card": CARD},
        },
    )
    assert result == {}

    assert await get_field_values(websocket, context_id) == {
        "name": "John Smith",
        "number": "4444444444444444",
        "month": "01",
        "year": "2030",
    }


@pytest.mark.asyncio
async def test_autofill_trigger_address(websocket, context_id, html, query_selector):
    await goto_url(websocket, context_id, html(ADDRESS_FORM))
    element = await query_selector("#given-name")

    result = await execute_command(
        websocket,
        {
            "method": "autofill.trigger",
            "params": {
                "context": context_id,
                "element": element,
                "address": ADDRESS,
            },
        },
    )
    assert result == {}

    assert await get_field_values(websocket, context_id) == {
        "given-name": "John",
        "family-name": "Smith",
        "address-line1": "123 Main St",
        "address-level2": "San Francisco",
        "address-level1": "CA",
        "postal-code": "94102",
    }
