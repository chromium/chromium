# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Provides the version of the in-tree glib for the codegen wrappers.

The version is read from src/meson.build so that it always matches the DEPS
revision and doesn't need updating when glib is rolled.
"""

import os
import re

_MESON_BUILD = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'src',
                            'meson.build')


def get_version():
    with open(_MESON_BUILD) as f:
        match = re.search(r"project\(.*?version\s*:\s*'([\d.]+)'", f.read(),
                          re.DOTALL)
    if not match:
        raise ValueError(f'No project version found in {_MESON_BUILD}')
    return match.group(1)
