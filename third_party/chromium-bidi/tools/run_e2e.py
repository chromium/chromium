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

from __future__ import annotations

import argparse
import datetime
import json
import os
import subprocess
import sys

from run_bidi_server import (
    BiDiServerProcess,
    get_log_file_path,
    resolve_binary_path,
)
from test_runner_utils import (
    get_default_chromedriver_bin,
    get_repo_root,
    setup_runtime_env,
    strip_leading_dashes,
)


def main():
    parser = argparse.ArgumentParser(
        description="Runs chromium-bidi end-to-end tests via pytest"
    )
    parser.add_argument("--gen-dir", default=None)
    parser.add_argument("--node-py", default=None)
    parser.add_argument("--browser-bin", default=os.environ.get("BROWSER_BIN"))
    parser.add_argument("--chromedriver-bin", default=get_default_chromedriver_bin())
    parser.add_argument(
        "--python-bin",
        default=os.environ.get("TESTING_PYTHON_BIN", "vpython3"),
    )
    parser.add_argument("--python-spec", default=None)
    parser.add_argument("-k", default=None, help="pytest filter expression")
    parser.add_argument("-s", action="store_true", help="pytest disable capture")
    parser.add_argument(
        "--repeat-times",
        type=int,
        default=int(os.environ.get("REPEAT_TIMES", 1)),
    )
    parser.add_argument(
        "--reruns-times",
        type=int,
        default=int(os.environ.get("RERUNS_TIMES", 0)),
    )
    parser.add_argument(
        "--total-shards",
        type=int,
        default=int(
            os.environ.get(
                "GTEST_TOTAL_SHARDS",
                os.environ.get("PYTEST_TOTAL_SHARDS", 1),
            )
        ),
    )
    parser.add_argument(
        "--shard-id",
        type=int,
        default=int(
            os.environ.get("GTEST_SHARD_INDEX", os.environ.get("PYTEST_SHARD_ID", 0))
        ),
    )
    parser.add_argument("--test-filter", default=None)
    parser.add_argument("--test-filter-file", default=None)
    parser.add_argument("--update-snapshot", action="store_true")

    args, unknown_args = parser.parse_known_args()

    repo_root = get_repo_root()
    gen_dir = (
        os.path.abspath(args.gen_dir)
        if args.gen_dir
        else os.path.join(repo_root, "out", "Default", "gen")
    )
    node_py = os.path.abspath(args.node_py) if args.node_py else None
    browser_bin = resolve_binary_path(args.browser_bin)
    chromedriver_bin = resolve_binary_path(
        args.chromedriver_bin or get_default_chromedriver_bin()
    )

    # Prepare runtime node_modules and package.json
    setup_runtime_env(gen_dir, src_dir=repo_root)

    extra_args = strip_leading_dashes(unknown_args)

    # Process isolated script and gtest arguments
    i = 0
    isolated_script_test_output = None
    filtered_extra_args = []
    while i < len(extra_args):
        arg = extra_args[i]
        if arg.startswith("--isolated-script-test-filter="):
            args.test_filter = arg.split("=", 1)[1]
            i += 1
        elif arg == "--isolated-script-test-filter":
            if i + 1 < len(extra_args):
                args.test_filter = extra_args[i + 1]
                i += 2
            else:
                i += 1
        elif arg.startswith("--isolated-script-test-filter-file="):
            args.test_filter_file = os.path.abspath(arg.split("=", 1)[1])
            i += 1
        elif arg == "--isolated-script-test-filter-file":
            if i + 1 < len(extra_args):
                args.test_filter_file = os.path.abspath(extra_args[i + 1])
                i += 2
            else:
                i += 1
        elif arg.startswith("--isolated-script-test-output="):
            isolated_script_test_output = os.path.abspath(arg.split("=", 1)[1])
            i += 1
        elif arg == "--isolated-script-test-output":
            if i + 1 < len(extra_args):
                isolated_script_test_output = os.path.abspath(extra_args[i + 1])
                i += 2
            else:
                i += 1
        elif arg.startswith("--isolated-script-test-repeat="):
            args.repeat_times = int(arg.split("=", 1)[1])
            i += 1
        elif arg.startswith("--isolated-script-test-launcher-retry-limit="):
            args.reruns_times = int(arg.split("=", 1)[1])
            i += 1
        elif arg.startswith("--shards="):
            args.total_shards = int(arg.split("=", 1)[1])
            i += 1
        elif (
            arg.startswith("--isolated-script-test-")
            or arg.startswith("--isolated-outdir")
            or arg.startswith("--gtest")
        ):
            # Consume isolated script args without passing directly to pytest
            if (
                "=" not in arg
                and i + 1 < len(extra_args)
                and not extra_args[i + 1].startswith("-")
            ):
                i += 2
            else:
                i += 1
        else:
            filtered_extra_args.append(arg)
            i += 1

    if args.test_filter_file:
        args.test_filter_file = os.path.abspath(args.test_filter_file)

    log_file = get_log_file_path("e2e")
    print(f"(run_e2e.py) Logging to {log_file}")

    server = BiDiServerProcess(
        gen_dir=gen_dir,
        node_py=node_py,
        browser_bin=browser_bin,
        chromedriver_bin=chromedriver_bin,
        verbose=False,
        log_file=log_file,
    )

    server.start()
    try:
        if not server.wait_until_ready(timeout=10.0):
            print(
                f"(run_e2e.py) ChromeDriver failed to start within timeout. Server log: {log_file}",
                file=sys.stderr,
            )
            if server.server_logs:
                print("--- Server logs tail ---", file=sys.stderr)
                for line in server.server_logs[-30:]:
                    print(line.rstrip(), file=sys.stderr)
                print("------------------------", file=sys.stderr)
            return 1

        # Construct pytest command
        python_bin_parts = args.python_bin.split()
        pytest_cmd = list(python_bin_parts)
        python_spec = args.python_spec
        if not python_spec and python_bin_parts[0] == "vpython3":
            default_spec = os.path.join(repo_root, ".vpython3")
            if os.path.exists(default_spec):
                python_spec = default_spec

        if python_spec and python_bin_parts[0] == "vpython3":
            pytest_cmd.extend(["-vpython-spec", python_spec])

        pytest_cmd.extend(
            [
                "-m",
                "pytest",
                "--verbose",
                "-vv",
                "--snapshot-warn-unused",
            ]
        )

        if args.update_snapshot:
            pytest_cmd.append("--snapshot-update")

        if args.repeat_times > 1:
            pytest_cmd.append(f"--count={args.repeat_times}")

        if args.reruns_times > 0:
            pytest_cmd.append(f"--reruns={args.reruns_times}")

        if args.total_shards > 1:
            pytest_cmd.extend(
                [
                    "--num-shards",
                    str(args.total_shards),
                    "--shard-id",
                    str(args.shard_id),
                ]
            )

        headless = os.environ.get("HEADLESS", "new")
        if headless not in ("old", "new", "false"):
            headless = "new"

        if headless == "false" and not args.k:
            pytest_cmd.append("--ignore=tests/input")

        if args.k:
            pytest_cmd.extend(["-k", args.k])

        if args.s:
            pytest_cmd.append("-s")

        if args.test_filter:
            pytest_cmd.append(f"--test-filter={args.test_filter}")

        if args.test_filter_file:
            pytest_cmd.append(f"--test-filter-file={args.test_filter_file}")

        pytest_cmd.extend(filtered_extra_args)

        env = os.environ.copy()
        env["PYTHONUNBUFFERED"] = "1"
        env["HEADLESS"] = headless
        if browser_bin:
            env["BROWSER_BIN"] = browser_bin
        if chromedriver_bin:
            env["CHROMEDRIVER_BIN"] = chromedriver_bin
            env["CHROMEDRIVER"] = "true"

        print(f"(run_e2e.py) Running pytest: {' '.join(pytest_cmd)}")
        result = subprocess.run(pytest_cmd, cwd=repo_root, env=env)

        if isolated_script_test_output:
            try:
                os.makedirs(os.path.dirname(isolated_script_test_output), exist_ok=True)
                if not os.path.exists(isolated_script_test_output):
                    passed = result.returncode == 0
                    output_data = {
                        "version": 3,
                        "interrupted": False,
                        "path_delimiter": "/",
                        "seconds_since_epoch": int(
                            datetime.datetime.now(datetime.timezone.utc).timestamp()
                        ),
                        "num_failures_by_type": {
                            "PASS": 1 if passed else 0,
                            "FAIL": 0 if passed else 1,
                        },
                        "tests": {},
                    }
                    with open(isolated_script_test_output, "w", encoding="utf-8") as f:
                        json.dump(output_data, f)
            except Exception as e:
                print(
                    f"(run_e2e.py) Warning: could not write isolated script output: {e}",
                    file=sys.stderr,
                )

        if result.returncode != 0:
            print(
                f"\n(run_e2e.py) Tests failed. Server log: {log_file}",
                file=sys.stderr,
            )
        return result.returncode
    finally:
        server.stop()


if __name__ == "__main__":
    sys.exit(main())
