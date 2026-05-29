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

import argparse
import os
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", required=True)
    parser.add_argument("--entrypoint", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--gen-dir", required=True)
    args = parser.parse_args()

    # Resolve absolute path to rollup under repository root
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.abspath(os.path.join(script_dir, "../../.."))
    rollup_path = os.path.join(repo_root, "node_modules/rollup/dist/bin/rollup")

    # Invoke rollup via system node, setting the working directory to the repository root
    cmd = [
        "node",
        rollup_path,
        "--config",
        os.path.abspath(args.config),
        "--input",
        os.path.abspath(args.entrypoint),
        "--file",
        os.path.abspath(args.output),
        "--format",
        "iife",
        "--environment",
        f"GEN_DIR:{args.gen_dir}",
    ]

    result = subprocess.run(cmd, cwd=repo_root, capture_output=True, text=True)

    if result.returncode != 0:
        sys.stderr.write(result.stdout + "\n" + result.stderr)
        sys.exit(result.returncode)


if __name__ == "__main__":
    main()
