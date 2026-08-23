#!/usr/bin/env vpython3

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

import argparse
import os
import shutil
import subprocess
import sys


# Used by Chromium targets to run tests relying on node_modules.
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--gen-dir", required=True)
    parser.add_argument("--node-py", required=True)
    parser.add_argument("args", nargs=argparse.REMAINDER)
    args = parser.parse_args()

    # The current directory may be the root of the checkout or the build dir (e.g. out/Default).
    # Use __file__ to reliably find the source directory for chromium-bidi.
    src_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    dst_dir = os.path.abspath(
        os.path.join(args.gen_dir, "third_party", "chromium-bidi")
    )

    # Ensure dst_dir exists
    os.makedirs(dst_dir, exist_ok=True)

    # Copy package.json and node_modules to the gen dir
    for name in ["package.json", "node_modules"]:
        src = os.path.join(src_dir, name)
        dst = os.path.join(dst_dir, name)
        if os.path.exists(src):
            if os.path.isdir(src):
                if os.path.exists(dst):
                    shutil.rmtree(dst)
                shutil.copytree(src, dst)
            else:
                shutil.copy2(src, dst)

    node_args = args.args
    if node_args and node_args[0] == "--":
        node_args = node_args[1:]

    cmd = [sys.executable, args.node_py] + node_args
    return subprocess.call(cmd)


if __name__ == "__main__":
    sys.exit(main())
