# Copyright 2026 Google LLC.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for changes affecting third_party/chromium-bidi.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import fnmatch
import os

PRESUBMIT_VERSION = "2.0.0"

_EXCLUDED_PATTERNS = (
    "node_modules/*",
    "out/*",
    "build/*",
    "lib/*",
    "logs/*",
    "third_party/*",
    "playwright/*",
    "puppeteer/*",
    "src/protocol/generated/*",
    "src/protocol-parser/generated/*",
)


def _GetAffectedFiles(input_api, extensions, exclude_patterns=_EXCLUDED_PATTERNS):
    """Returns affected files matching extensions."""
    presubmit_dir = input_api.PresubmitLocalPath()
    files = []
    for f in input_api.AffectedFiles(include_deletes=False):
        rel_path = input_api.os_path.relpath(f.AbsoluteLocalPath(), presubmit_dir)
        rel_path_posix = rel_path.replace(os.path.sep, "/")
        if not rel_path.endswith(extensions):
            continue
        if exclude_patterns and any(
            fnmatch.fnmatch(rel_path_posix, pat) for pat in exclude_patterns
        ):
            continue
        files.append(rel_path)
    return files


def _CheckNodeModules(input_api, output_api):
    """Ensures node_modules is installed before running node-based linters."""
    presubmit_dir = input_api.PresubmitLocalPath()
    node_modules_dir = input_api.os_path.join(presubmit_dir, "node_modules")
    if not input_api.os_path.exists(node_modules_dir):
        msg = (
            "node_modules directory is missing in third_party/chromium-bidi. "
            "Please run `gclient sync` to download dependencies."
        )
        if input_api.is_committing:
            return [output_api.PresubmitError(msg)]
        return [output_api.PresubmitPromptWarning(msg)]
    return []


def CheckRuff(input_api, output_api):
    """Runs Ruff check on affected Python files using canned check."""
    return input_api.RunTests(
        input_api.canned_checks.GetRuff(
            input_api,
            output_api,
        )
    )


def CheckESLint(input_api, output_api):
    """Runs ESLint on affected JS/TS files."""
    affected_files = _GetAffectedFiles(
        input_api,
        extensions=(".js", ".mjs", ".ts"),
    )
    if not affected_files:
        return []

    node_modules_check = _CheckNodeModules(input_api, output_api)
    if node_modules_check:
        return node_modules_check

    presubmit_dir = input_api.PresubmitLocalPath()
    node_py = input_api.os_path.join(presubmit_dir, "tools", "node.py")
    eslint_bin = input_api.os_path.join(
        presubmit_dir, "node_modules", "eslint", "bin", "eslint.js"
    )

    error_type = (
        output_api.PresubmitError
        if input_api.is_committing
        else output_api.PresubmitPromptWarning
    )

    cmd = [input_api.python3_executable, node_py, eslint_bin] + affected_files
    return input_api.RunTests(
        [
            input_api.Command(
                name=f"ESLint ({len(affected_files)} files)",
                cmd=cmd,
                kwargs={"cwd": presubmit_dir},
                message=error_type,
            )
        ]
    )


def CheckPrettier(input_api, output_api):
    """Runs Prettier check on affected JS/TS/JSON/MD files."""
    affected_files = _GetAffectedFiles(
        input_api,
        extensions=(".js", ".mjs", ".ts", ".json", ".md"),
        exclude_patterns=_EXCLUDED_PATTERNS
        + (
            "CHANGELOG.md",
            "package-lock.json",
            "Pipfile.lock",
        ),
    )
    if not affected_files:
        return []

    node_modules_check = _CheckNodeModules(input_api, output_api)
    if node_modules_check:
        return node_modules_check

    presubmit_dir = input_api.PresubmitLocalPath()
    node_py = input_api.os_path.join(presubmit_dir, "tools", "node.py")
    prettier_bin = input_api.os_path.join(
        presubmit_dir, "node_modules", "prettier", "bin", "prettier.cjs"
    )

    error_type = (
        output_api.PresubmitError
        if input_api.is_committing
        else output_api.PresubmitPromptWarning
    )

    cmd = [
        input_api.python3_executable,
        node_py,
        prettier_bin,
        "--check",
    ] + affected_files
    return input_api.RunTests(
        [
            input_api.Command(
                name=f"Prettier ({len(affected_files)} files)",
                cmd=cmd,
                kwargs={"cwd": presubmit_dir},
                message=error_type,
            )
        ]
    )
