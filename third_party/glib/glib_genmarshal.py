#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

import glib_version

VERSION = glib_version.get_version()

script_dir = os.path.dirname(os.path.abspath(__file__))
template_path = os.path.join(script_dir, "src/gobject/glib-genmarshal.in")

with open(template_path, "r", encoding="utf-8") as f:
    content = f.read()

content = content.replace("@PYTHON@", sys.executable)
content = content.replace("@VERSION@", VERSION)

# "--chdir DIR" (must come first) runs the tool in DIR, so that input paths
# relative to DIR appear as such in the generated code, e.g. in #include
# directives and comments. All other paths must then be relative to DIR.
if len(sys.argv) > 2 and sys.argv[1] == "--chdir":
    os.chdir(sys.argv[2])
    del sys.argv[1:3]

# Execute the script
exec(content)
