#!/usr/bin/env python3

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
import re
import subprocess
import sys

from test_runner_utils import (
    extract_isolated_script_args,
    get_node_binary_path,
    resolve_test_files_and_patterns,
    setup_runtime_env,
    strip_leading_dashes,
)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--gen-dir", required=True)
    parser.add_argument("--node-py", required=True)
    args, unknown_args = parser.parse_known_args()

    # Prepare runtime dependencies (package.json and node_modules)
    setup_runtime_env(args.gen_dir)

    node_args = strip_leading_dashes(unknown_args)

    (
        cleaned_node_args,
        test_filters,
        _,
        file_globs_or_paths,
    ) = extract_isolated_script_args(node_args)

    target_test_files, test_name_patterns = resolve_test_files_and_patterns(
        file_globs_or_paths, test_filters
    )

    if test_filters and not target_test_files and not test_name_patterns:
        print("No unit tests matched the filter.")
        return 0

    node_bin = get_node_binary_path(args.node_py)

    cmd = [node_bin] + cleaned_node_args
    if test_name_patterns:
        escaped = []
        for p in test_name_patterns:
            if p.endswith("*"):
                escaped.append(re.escape(p[:-1]) + ".*")
            else:
                escaped.append(re.escape(p))
        pattern_regex = f"^({'|'.join(escaped)})$"
        cmd.append(f"--test-name-pattern={pattern_regex}")

    cmd.extend(target_test_files)
    return subprocess.call(cmd)


if __name__ == "__main__":
    sys.exit(main())
