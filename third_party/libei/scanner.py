#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import subprocess

# Add jinja2 and markupsafe to the path.
script_dir = os.path.dirname(os.path.realpath(__file__))
src_dir = os.path.join(script_dir, "..", "..")
third_party_dir = os.path.join(src_dir, "third_party")

env = os.environ.copy()
python_path = env.get("PYTHONPATH", "")
new_python_path = [third_party_dir]
if python_path:
    new_python_path.append(python_path)
env["PYTHONPATH"] = os.pathsep.join(new_python_path)

scanner_path = os.path.join(script_dir, "src", "proto", "ei-scanner")

if __name__ == "__main__":
    result = subprocess.run([sys.executable, scanner_path] + sys.argv[1:], env=env)
    sys.exit(result.returncode)
