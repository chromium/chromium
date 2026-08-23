#!/usr/bin/env python3
# Copyright 2026 Google LLC.
# Copyright (c) Microsoft Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import shutil
import subprocess
import sys


def execute(cmd):
    if sys.platform == "win32":
        sys.exit(subprocess.call(cmd))
    else:
        os.execv(cmd[0], cmd)


def main():
    node_bin = shutil.which("node")

    if node_bin:
        execute([node_bin] + sys.argv[1:])
    else:
        # Assuming this project is part of the chromium checkout at third_party/chromium-bidi
        script_dir = os.path.dirname(os.path.abspath(__file__))
        chromium_src = os.path.abspath(os.path.join(script_dir, "..", "..", ".."))
        chromium_node_py = os.path.join(chromium_src, "third_party", "node", "node.py")

        if os.path.exists(chromium_node_py):
            execute([sys.executable, chromium_node_py] + sys.argv[1:])
        else:
            print(
                f"node not found in PATH and {chromium_node_py} does not exist.",
                file=sys.stderr,
            )
            sys.exit(1)


if __name__ == "__main__":
    main()
