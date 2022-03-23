#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
from pathlib import Path

_HERE_DIR = Path(__file__).parent
_SOURCE_MAP_CREATOR = (_HERE_DIR / 'create_js_source_maps.js').resolve()

_NODE_PATH = (_HERE_DIR.parent.parent.parent / 'third_party' / 'node').resolve()
sys.path.append(str(_NODE_PATH))
import node

# Invokes "node create_js_source_maps.js (args)""
# We can't use third_party/node/node.py directly from the gni template because
# we don't have a good way to specify the path to create_js_source_maps.js in a
# gni template.
node.RunNode([str(_SOURCE_MAP_CREATOR)] + sys.argv[1:])
