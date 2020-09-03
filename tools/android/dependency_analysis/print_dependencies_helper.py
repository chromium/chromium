# Lint as: python3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Various helpers for printing dependencies."""

from typing import List


def get_valid_package_keys_matching(all_keys: List,
                                    input_key: str) -> List[str]:
    """Return a list of keys of graph nodes that match a package input.

    For our use case (matching user input to package nodes),
    a valid key is one that ends with the input, case insensitive.
    For example, 'apphooks' matches 'org.chromium.browser.AppHooks'.
    """
    input_key_lower = input_key.lower()
    return [key for key in all_keys if key.lower().endswith(input_key_lower)]


def get_valid_class_keys_matching(all_keys: List, input_key: str) -> List[str]:
    """Return a list of keys of graph nodes that match a class input.

    For our use case (matching user input to class nodes),
    a valid key is one that matches fully the input either fully qualified or
    ignoring package, case sensitive.
    For example, the inputs 'org.chromium.browser.AppHooks' and 'AppHooks'
    match the node 'org.chromium.browser.AppHooks' but 'Hooks' does not.
    """
    if '.' in input_key:
        # Match full name with package only.
        return [input_key] if input_key in all_keys else []
    else:
        # Match class name in any package.
        return [key for key in all_keys if key.endswith(f'.{input_key}')]
