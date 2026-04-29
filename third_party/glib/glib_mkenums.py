#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import os

# Set version
VERSION = "2.85.4"

script_dir = os.path.dirname(os.path.abspath(__file__))
template_path = os.path.join(script_dir, "src/gobject/glib-mkenums.in")

with open(template_path, "r", encoding="utf-8") as f:
    content = f.read()

content = content.replace("@PYTHON@", sys.executable)
content = content.replace("@VERSION@", VERSION)

# Execute the script
exec(content)
