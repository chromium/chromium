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
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REQUIRED_CDDLCONV_VERSION = "0.1.10"

SPECS = [
    {
        "name": "webdriver-bidi",
        "repo": "https://github.com/w3c/webdriver-bidi.git",
        "doc": None,  # uses scripts/cddl/generate.js directly
        "cddl_name": "all.cddl",
        "ts_file": "src/protocol/generated/webdriver-bidi.ts",
        "zod_file": "src/protocol-parser/generated/webdriver-bidi.ts",
    },
    {
        "name": "permissions",
        "repo": "https://github.com/w3c/permissions.git",
        "doc": "index.html",
        "cddl_name": "permissions.cddl",
        "ts_file": "src/protocol/generated/webdriver-bidi-permissions.ts",
        "zod_file": "src/protocol-parser/generated/webdriver-bidi-permissions.ts",
    },
    {
        "name": "web-bluetooth",
        "repo": "https://github.com/WebBluetoothCG/web-bluetooth.git",
        "doc": "index.bs",
        "cddl_name": "web-bluetooth.cddl",
        "ts_file": "src/protocol/generated/webdriver-bidi-bluetooth.ts",
        "zod_file": "src/protocol-parser/generated/webdriver-bidi-bluetooth.ts",
    },
    {
        "name": "nav-speculation",
        "repo": "https://github.com/WICG/nav-speculation.git",
        "doc": "prefetch.bs",
        "cddl_name": "nav-speculation.cddl",
        "ts_file": "src/protocol/generated/webdriver-bidi-nav-speculation.ts",
        "zod_file": "src/protocol-parser/generated/webdriver-bidi-nav-speculation.ts",
    },
    {
        "name": "ua-client-hints",
        "repo": "https://github.com/WICG/ua-client-hints.git",
        "doc": "index.bs",
        "cddl_name": "ua-client-hints.cddl",
        "ts_file": "src/protocol/generated/webdriver-bidi-ua-client-hints.ts",
        "zod_file": "src/protocol-parser/generated/webdriver-bidi-ua-client-hints.ts",
    },
    {
        "name": "digital-credentials",
        "repo": "https://github.com/w3c-fedid/digital-credentials.git",
        "doc": "index.html",
        "cddl_name": "digital-credentials.cddl",
        "ts_file": "src/protocol/generated/webdriver-bidi-digital-credentials.ts",
        "zod_file": "src/protocol-parser/generated/webdriver-bidi-digital-credentials.ts",
    },
]


def setup_node_path():
    if shutil.which("npm"):
        try:
            res = subprocess.run(
                ["npm", "root", "-g"], capture_output=True, text=True, check=True
            )
            global_node_modules = res.stdout.strip()
            if global_node_modules and os.path.isdir(global_node_modules):
                existing = os.environ.get("NODE_PATH", "")
                os.environ["NODE_PATH"] = (
                    f"{global_node_modules}:{existing}"
                    if existing
                    else global_node_modules
                )
        except Exception:
            pass


def check_prerequisites(check_parse5: bool = True):
    cddlconv = shutil.which("cddlconv")
    if not cddlconv:
        print("Error: 'cddlconv' is required but not installed.", file=sys.stderr)
        print(
            f"Please install it using cargo: 'cargo install cddlconv@{REQUIRED_CDDLCONV_VERSION}'",
            file=sys.stderr,
        )
        sys.exit(1)

    result = subprocess.run(["cddlconv", "--version"], capture_output=True, text=True)
    version = result.stdout.strip().split()[-1] if result.returncode == 0 else ""
    if version != REQUIRED_CDDLCONV_VERSION:
        print(
            f"Error: 'cddlconv' version {REQUIRED_CDDLCONV_VERSION} is required, but found {version}.",
            file=sys.stderr,
        )
        print(
            f"Please install it using cargo: 'cargo install cddlconv@{REQUIRED_CDDLCONV_VERSION}'",
            file=sys.stderr,
        )
        sys.exit(1)

    if check_parse5:
        res = subprocess.run(["node", "-e", "require('parse5')"], capture_output=True)
        if res.returncode != 0:
            print(
                "Error: 'parse5' is required but not installed.",
                file=sys.stderr,
            )
            print(
                "Please install it using npm: 'npm install -g parse5'",
                file=sys.stderr,
            )
            sys.exit(1)


def get_cddlconv_version() -> str:
    res = subprocess.run(["cddlconv", "--version"], capture_output=True, text=True)
    return res.stdout.strip()


def run_cddlconv_and_write(cddl_path: Path, output_path: Path, format_type: str = "ts"):
    root_dir = Path(__file__).resolve().parent.parent
    cddlconv_ver = get_cddlconv_version()
    year = datetime.datetime.now().year
    header = f"""/**
 * Copyright {year} Google LLC.
 * Copyright (c) Microsoft Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * THIS FILE IS AUTOGENERATED by {cddlconv_ver}.
 * Run `tools/update_bidi_types.py` to regenerate.
 * @see https://github.com/w3c/webdriver-bidi/blob/master/index.bs
 */

"""
    cmd = ["cddlconv"]
    if format_type == "zod":
        cmd.extend(["--format", "zod"])
    cmd.append(str(cddl_path))

    res = subprocess.run(cmd, cwd=root_dir, capture_output=True, text=True, check=True)
    content = header + res.stdout

    # Post-process imports for extension modules
    if not output_path.name.endswith("webdriver-bidi.ts"):
        if format_type == "zod":
            if "EmptyResultSchema" in content:
                content = content.replace(
                    "import z from 'zod';",
                    "import z from 'zod';\nimport {EmptyResultSchema} from './webdriver-bidi.js';",
                )
        else:
            if "EmptyResult" in content:
                content = content.replace(
                    header,
                    header
                    + "import type {EmptyResult} from './webdriver-bidi.js';\n\n",
                )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(content, encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(
        description="Updates WebDriver BiDi types from CDDL specifications"
    )
    parser.add_argument(
        "--cddl-file", help="Path to a single CDDL file to generate types for"
    )
    parser.add_argument("--ts-file", default="src/protocol/generated/webdriver-bidi.ts")
    parser.add_argument(
        "--zod-file", default="src/protocol-parser/generated/webdriver-bidi.ts"
    )
    parser.add_argument("--spec-repo", default="w3c/webdriver-bidi")
    parser.add_argument("--spec-ref", default="main")
    args = parser.parse_args()

    setup_node_path()
    check_prerequisites(check_parse5=not bool(args.cddl_file))
    repo_root = Path(__file__).resolve().parent.parent

    # Single-file mode (equivalent to generate-bidi-types.mjs)
    if args.cddl_file:
        cddl_p = Path(args.cddl_file).resolve()
        run_cddlconv_and_write(cddl_p, repo_root / args.ts_file, format_type="ts")
        run_cddlconv_and_write(cddl_p, repo_root / args.zod_file, format_type="zod")
        print(f"Generated types for {args.cddl_file}")
        return

    # Full update mode (equivalent to update-bidi-types.sh)
    with tempfile.TemporaryDirectory(prefix="bidi-types-") as tmpdir_str:
        tmpdir = Path(tmpdir_str)
        print(f"Cloning specifications into {tmpdir}...")

        # 1. Main webdriver-bidi repo
        bidi_dir = tmpdir / "webdriver-bidi"
        subprocess.run(
            [
                "git",
                "clone",
                "--depth",
                "1",
                f"https://github.com/{args.spec_repo}.git",
                str(bidi_dir),
            ],
            check=True,
        )
        subprocess.run(["git", "checkout", args.spec_ref], cwd=bidi_dir, check=False)
        subprocess.run(["node", "./scripts/cddl/generate.js"], cwd=bidi_dir, check=True)
        cddl_files = {"all.cddl": bidi_dir / "all.cddl"}

        # 2. Extension specifications
        gen_script = bidi_dir / "scripts" / "cddl" / "generate.js"
        for spec in SPECS:
            if spec["name"] == "webdriver-bidi":
                continue
            spec_dir = tmpdir / spec["name"]
            print(f"Cloning {spec['name']}...")
            subprocess.run(
                ["git", "clone", "--depth", "1", spec["repo"], str(spec_dir)],
                check=True,
            )
            subprocess.run(
                ["node", str(gen_script), f"./{spec['doc']}"], cwd=spec_dir, check=True
            )
            cddl_files[spec["cddl_name"]] = spec_dir / "all.cddl"

        # 3. Generate TypeScript and Zod definitions
        for spec in SPECS:
            cddl_file = cddl_files[spec["cddl_name"]]
            print(f"Generating types for {spec['name']} from {cddl_file}...")
            run_cddlconv_and_write(
                cddl_file, repo_root / spec["ts_file"], format_type="ts"
            )
            run_cddlconv_and_write(
                cddl_file, repo_root / spec["zod_file"], format_type="zod"
            )

    # 4. Format generated files
    print("Formatting generated files...")
    node_py = repo_root / "tools" / "node.py"
    if node_py.exists():
        subprocess.run(
            [
                sys.executable,
                str(node_py),
                "node_modules/prettier/bin/prettier.cjs",
                "--cache",
                "--write",
                ".",
            ],
            cwd=repo_root,
            check=False,
        )
        subprocess.run(
            [
                sys.executable,
                str(node_py),
                "node_modules/eslint/bin/eslint.js",
                "--cache",
                "--fix",
                ".",
            ],
            cwd=repo_root,
            check=False,
        )
    print("Done updating BiDi types!")


if __name__ == "__main__":
    main()
