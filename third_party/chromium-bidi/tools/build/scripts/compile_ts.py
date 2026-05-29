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
import json
import os
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--tsconfig_template", required=True)
    parser.add_argument("--tsconfig_output", required=True)
    parser.add_argument("--output_dir", required=True)
    parser.add_argument("--es-target")
    parser.add_argument("--es-libs", nargs="*")
    parser.add_argument("--no-emit", action="store_true")
    parser.add_argument("--deps", nargs="*")
    parser.add_argument("--sources", nargs="*")
    parser.add_argument("--root_dir")
    args = parser.parse_args()

    # Read template config
    with open(args.tsconfig_template) as f:
        config = json.load(f)

    # Resolve absolute path to repository root
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.abspath(os.path.join(script_dir, "../../.."))

    # Convert all source files to absolute paths
    sources = args.sources or []
    abs_sources = [os.path.abspath(src) for src in sources]

    # Override configurations dynamically based on GN parameters
    config["compilerOptions"]["outDir"] = os.path.abspath(args.output_dir)
    config["compilerOptions"]["rootDir"] = (
        os.path.abspath(args.root_dir) if args.root_dir else repo_root
    )
    config["files"] = abs_sources

    if args.es_target:
        config["compilerOptions"]["target"] = args.es_target

    if args.es_libs:
        config["compilerOptions"]["lib"] = args.es_libs

    if args.no_emit:
        config["compilerOptions"]["noEmit"] = True

    # Explicitly define the flat absolute path for tsBuildInfoFile
    tsconfig_output_abs = os.path.abspath(args.tsconfig_output)
    tsbuildinfo_name = os.path.basename(tsconfig_output_abs) + ".tsbuildinfo"
    config["compilerOptions"]["tsBuildInfoFile"] = os.path.join(
        os.path.dirname(tsconfig_output_abs), tsbuildinfo_name
    )

    # Map dependencies to tsconfig project references
    if args.deps:
        config["references"] = [{"path": os.path.abspath(dep)} for dep in args.deps]

    # Write target-specific generated tsconfig.json
    os.makedirs(os.path.dirname(args.tsconfig_output), exist_ok=True)
    with open(args.tsconfig_output, "w") as f:
        json.dump(config, f, indent=2)

    # If there are no sources, we can skip running tsc (just generated the config for references)
    if len(sources) == 0:
        return 0

    # Resolve absolute path to TSC under repository root
    tsc_path = os.path.join(repo_root, "node_modules/typescript/lib/tsc.js")

    # Execute TSC using system node
    cmd = ["node", tsc_path, "--project", args.tsconfig_output]
    result = subprocess.run(cmd, capture_output=True, text=True)

    if result.returncode != 0:
        sys.stderr.write(result.stdout + "\n" + result.stderr)
        sys.exit(result.returncode)

    # Touch output files to ensure Ninja freshness detection works correctly
    if os.path.exists(args.tsconfig_output):
        os.utime(args.tsconfig_output, None)

    tsbuildinfo_path = os.path.join(
        os.path.dirname(tsconfig_output_abs), tsbuildinfo_name
    )
    if os.path.exists(tsbuildinfo_path):
        os.utime(tsbuildinfo_path, None)

    if not args.no_emit and args.sources:
        for src in sources:
            rel_src = os.path.relpath(
                os.path.abspath(src),
                os.path.abspath(args.root_dir if args.root_dir else repo_root),
            )
            rel_dir, filename = os.path.split(rel_src)
            basename, _ = os.path.splitext(filename)

            js_path = os.path.join(args.output_dir, rel_dir, basename + ".js")
            map_path = os.path.join(args.output_dir, rel_dir, basename + ".js.map")
            dts_path = os.path.join(args.output_dir, rel_dir, basename + ".d.ts")

            for path in (js_path, map_path, dts_path):
                if os.path.exists(path):
                    os.utime(path, None)


if __name__ == "__main__":
    main()
