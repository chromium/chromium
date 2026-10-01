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

import fnmatch
import os
import re
import shutil
import sys

# ResultDB canonical test IDs format:
#   [+-]?[ninja://<target_path>:<target_name>/]:chromium-bidi!<scheme>:<coarse_name>:<fine_name>[#<case_name>]
#
# Examples:
#   :chromium-bidi!pytest:tests/bluetooth/:test_characteristic_emulation.py#test_foo
#   ninja://third_party/chromium-bidi:webdriver_bidi_e2e_tests/:chromium-bidi!pytest:tests/bidi/:test_bidi.py
#   ninja://third_party/chromium-bidi:webdriver_bidi_unittests/:chromium-bidi!mocha:src/utils/:assert.test.ts#assert:should not throw
# The isolated-test API uses :: between rules. For backwards compatibility,
# mixed GTest-style single-colon filters also recognize a following path or
# exclusion. That legacy syntax is ambiguous with path-like nested titles;
# escape literal colons in a component and use :: between canonical rules.
CANONICAL_TEST_ID_RE = re.compile(
    r"""
    [+-]?                                           # Optional filter inclusion (+) or exclusion (-) prefix
    (?:ninja://\S+?:[^\s/]+/)?                      # Optional Ninja build target prefix (e.g. ninja://dir:target/)
    :chromium-bidi!\w+                              # ResultDB module prefix and scheme (e.g. :chromium-bidi!pytest)
    :(?:\\.|[^#:\\\r\n])*                          # Coarse directory path
    (?::(?:\\.|[^#:\\\r\n])*)?                     # Fine file name
    (?:\#(?:
        \\.                                        # Escaped ResultDB punctuation
        |[^:\\\r\n]                                # Spaces are part of case names
        |:(?!:|[*?]|[+-]|ninja://|[^\s:]*[/\\]|[^\s:]*\.(?:py|js|ts)(?=[:\#]|$))
                                                    # Nested case component, not a following filter;
                                                    # wildcard-leading rules keep their existing boundary
    )*)?
    """,
    re.VERBOSE,
)


def get_repo_root() -> str:
    """Returns the root directory of the chromium-bidi repository."""
    return os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


def setup_runtime_env(
    gen_dir: str, src_dir: str | None = None, force: bool = False
) -> str:
    """Sets up package.json and node_modules in the gen directory for tests.

    Uses symlinking for node_modules where possible for performance,
    falling back to copying if symlinks are not permitted.
    Returns the absolute path to the prepared directory inside gen_dir.
    """
    if src_dir is None:
        src_dir = get_repo_root()

    dst_dir = os.path.abspath(os.path.join(gen_dir, "third_party", "chromium-bidi"))
    os.makedirs(dst_dir, exist_ok=True)

    pkg_src = os.path.join(src_dir, "package.json")
    pkg_dst = os.path.join(dst_dir, "package.json")
    if os.path.exists(pkg_src) and (force or not os.path.exists(pkg_dst)):
        shutil.copy2(pkg_src, pkg_dst)

    nm_src = os.path.join(src_dir, "node_modules")
    nm_dst = os.path.join(dst_dir, "node_modules")
    if os.path.exists(nm_src):
        if force and os.path.exists(nm_dst):
            if os.path.islink(nm_dst):
                os.unlink(nm_dst)
            else:
                shutil.rmtree(nm_dst)

        if not os.path.exists(nm_dst):
            try:
                os.symlink(nm_src, nm_dst)
            except OSError:
                shutil.copytree(
                    nm_src,
                    nm_dst,
                    symlinks=False,
                    ignore=shutil.ignore_patterns(".bin"),
                    ignore_dangling_symlinks=True,
                )

    return dst_dir


def get_node_binary_path(node_py_path: str | None = None) -> str:
    """Resolves the Node.js executable binary path."""
    if node_py_path:
        node_dir = os.path.dirname(os.path.abspath(node_py_path))
        if node_dir not in sys.path:
            sys.path.insert(0, node_dir)
        import node  # type: ignore

        return node.GetBinaryPath()

    # Fallback to Chromium's third_party/node/node.py relative to repo root
    chromium_node_py = os.path.abspath(
        os.path.join(get_repo_root(), "..", "..", "third_party", "node", "node.py")
    )
    if os.path.exists(chromium_node_py):
        node_dir = os.path.dirname(chromium_node_py)
        if node_dir not in sys.path:
            sys.path.insert(0, node_dir)
        import node  # type: ignore

        return node.GetBinaryPath()

    node_bin = shutil.which("node")
    if node_bin:
        return node_bin

    raise RuntimeError(
        f"Node binary could not be found via {chromium_node_py} or PATH."
    )


def get_default_chromedriver_bin() -> str | None:
    """Returns the default ChromeDriver binary path if available."""
    if os.environ.get("CHROMEDRIVER_BIN"):
        return os.environ["CHROMEDRIVER_BIN"]
    repo_root = get_repo_root()
    candidate = os.path.abspath(
        os.path.join(repo_root, "..", "..", "out", "Default", "chromedriver")
    )
    if os.path.exists(candidate):
        return candidate
    candidate = os.path.abspath(
        os.path.join(repo_root, "out", "Default", "chromedriver")
    )
    if os.path.exists(candidate):
        return candidate
    return None


def strip_leading_dashes(args: list[str]) -> list[str]:
    """Strips leading '--' from an arguments list."""
    res = list(args)
    while res and res[0] == "--":
        res = res[1:]
    return res


def parse_filter_tokens(filter_str: str) -> list[str]:
    """Parses a filter string into individual filter patterns.

    Handles ResultDB canonical test IDs, GTest colon-separated filters,
    and legacy joined nodeids (file.py::func_name).
    """
    if not filter_str:
        return []

    # Extract complete canonical spans at rule boundaries, not inside legacy
    # parameters. Do not invent placeholders that could be real test names.
    patterns = []
    end = 0
    index = 0
    in_parameter = False
    at_boundary = True
    while index < len(filter_str):
        if not in_parameter and at_boundary:
            match = CANONICAL_TEST_ID_RE.match(filter_str, index)
            if match:
                patterns.extend(_parse_legacy_filter_tokens(filter_str[end:index]))
                patterns.append(match.group().strip())
                index = end = match.end()
                at_boundary = False
                continue
        char = filter_str[index]
        if char == "[":
            in_parameter = True
        elif char == "]":
            in_parameter = False
        if char == ":" and not in_parameter:
            at_boundary = True
        elif not char.isspace():
            at_boundary = False
        index += 1
    patterns.extend(_parse_legacy_filter_tokens(filter_str[end:]))
    return patterns


def _parse_legacy_filter_tokens(filter_str: str) -> list[str]:
    # Preserve spaces, URLs and double colons inside a pytest parameter.
    # Raw parameter IDs are arbitrary strings: extra '[' characters do not
    # create nesting, and a backslash does not escape the closing ']'.
    parts = []
    start = 0
    in_parameter = False
    index = 0
    separator = ""
    while index < len(filter_str):
        char = filter_str[index]
        if char == "[":
            in_parameter = True
        elif char == "]":
            in_parameter = False
        elif char == ":" and not in_parameter:
            parts.append((separator, filter_str[start:index].strip()))
            start = index
            while index < len(filter_str) and filter_str[index] == ":":
                index += 1
            separator = filter_str[start:index]
            start = index
            continue
        index += 1
    parts.append((separator, filter_str[start:].strip()))

    patterns = []
    node_kind = None
    for separator, token in parts:
        if not token:
            continue
        is_file = token.endswith((".py", ".js", ".ts")) or token.startswith(
            ("tests/", "src/", "-tests/", "-src/")
        )
        if separator == "::" and node_kind and not is_file:
            delimiter = "::" if node_kind == "pytest" else "#"
            patterns[-1] += delimiter + token
            # Preserve the legacy file::case grammar. A following :: rule
            # (including a bare exclusion or glob) must remain independent.
            # Canonical IDs unambiguously represent nested class/suite names.
            node_kind = None
        else:
            patterns.append(token)
            node_kind = (
                "pytest"
                if token.endswith(".py")
                else "node"
                if token.endswith((".js", ".ts"))
                else None
            )
    return patterns


def parse_filter_file(filepath: str) -> list[str]:
    """Reads a filter file in Chromium Test List Format."""
    filters = []
    metadata = re.compile(
        r"^(?:\[([^\]]*)\]|Bug\([^)]*\)|(?:crbug\.com|skbug\.com|webkit\.org)/\S+)(?:\s+|$)"
    )
    with open(filepath, encoding="utf-8") as f:
        for line in f:
            raw_line = re.split(r"(?:\s|^)#", line)[0].strip()
            if not raw_line:
                continue
            tags = []
            while match := metadata.match(raw_line):
                tags.extend((match.group(1) or "").split())
                raw_line = raw_line[match.end() :].strip()
            # Expectation tags follow the test name, separated by whitespace.
            # Brackets attached to a parameterized node ID are not metadata.
            while match := re.search(r"\s+\[([^\[\]]*)\]$", raw_line):
                tags.extend(match.group(1).split())
                raw_line = raw_line[: match.start()].rstrip()
            is_skip = "Skip" in tags or "Failure" in tags
            for token in parse_filter_tokens(raw_line):
                if is_skip and not token.startswith("-"):
                    token = "-" + token
                filters.append(token)
    return filters


def parse_filter_pattern(pattern: str) -> tuple[bool, str | None, str | None]:
    """Extracts exclusion status, file pattern, and test case pattern from a filter rule."""
    is_exclusion = pattern.startswith("-")
    if is_exclusion:
        pattern = pattern[1:]

    # Strip ninja target prefix if present: e.g. ninja://third_party/chromium-bidi:webdriver_bidi_unittests/
    pattern = re.sub(r"^ninja://\S+?:[^\s/]+/", "", pattern)

    file_pattern = None
    case_pattern = None

    if "#" in pattern:
        file_part, case_part = pattern.split("#", 1)
        if ":" in case_part and not case_part.endswith(r"\:"):
            sub_parts = re.split(r"(?<!\\):", case_part)
            case_pattern = sub_parts[-1].replace(r"\:", ":")
        else:
            case_pattern = case_part.replace(r"\:", ":")
    else:
        file_part = pattern

    if file_part.startswith(":chromium-bidi!"):
        parts = file_part.split("!")[1].split(":")
        if len(parts) >= 3:
            coarse = parts[1].replace(r"\:", ":").rstrip("/")
            fine = parts[2].replace(r"\:", ":")
            file_pattern = f"{coarse}/{fine}" if coarse else fine
        elif len(parts) == 2:
            file_pattern = parts[1].replace(r"\:", ":")
    else:
        file_pattern = file_part

    return is_exclusion, file_pattern, case_pattern


def matches_file(file_path: str, file_pattern: str) -> bool:
    """Checks if a test file matches the given file pattern."""
    if not file_pattern or file_pattern == "*":
        return True

    norm_path = file_path.replace(os.path.sep, "/")
    norm_pattern = file_pattern.replace(os.path.sep, "/")

    base_path = re.sub(r"\.(ts|js|py)$", "", norm_path)
    base_pattern = re.sub(r"\.(ts|js|py)$", "", norm_pattern)

    if (
        norm_path == norm_pattern
        or norm_path.endswith("/" + norm_pattern)
        or base_path == base_pattern
        or base_path.endswith("/" + base_pattern)
    ):
        return True

    if (
        fnmatch.fnmatch(norm_path, norm_pattern)
        or fnmatch.fnmatch(norm_path, f"*/{norm_pattern}")
        or fnmatch.fnmatch(base_path, base_pattern)
        or fnmatch.fnmatch(base_path, f"*/{base_pattern}")
    ):
        return True

    return False


def extract_isolated_script_args(
    args: list[str],
) -> tuple[list[str], list[str], list[str], list[str]]:
    """Extracts test filters, filter files, and file globs/paths from arguments.

    Returns:
        (cleaned_args, test_filters, test_filter_files, file_globs_or_paths)
    """
    test_filters = []
    test_filter_files = []
    cleaned_args = []
    file_globs_or_paths = []

    i = 0
    while i < len(args):
        arg = args[i]
        if (
            arg.startswith("--isolated-script-test-filter=")
            or arg.startswith("--test-filter=")
            or arg.startswith("--gtest_filter=")
            or arg.startswith("--gtest-filter=")
        ):
            test_filters.append(arg.split("=", 1)[1])
            i += 1
        elif arg in (
            "--isolated-script-test-filter",
            "--test-filter",
            "--gtest_filter",
            "--gtest-filter",
        ):
            if i + 1 < len(args):
                test_filters.append(args[i + 1])
                i += 2
            else:
                i += 1
        elif arg.startswith("--isolated-script-test-filter-file="):
            test_filter_files.append(arg[len("--isolated-script-test-filter-file=") :])
            i += 1
        elif arg in ("--isolated-script-test-filter-file", "--test-filter-file"):
            if i + 1 < len(args):
                test_filter_files.append(args[i + 1])
                i += 2
            else:
                i += 1
        elif arg.startswith("--test-filter-file="):
            test_filter_files.append(arg[len("--test-filter-file=") :])
            i += 1
        elif (
            arg.startswith("--isolated-script-test-")
            or arg.startswith("--isolated-outdir")
            or arg == "--isolated-script-test-also-run-disabled-tests"
            or arg.startswith("--gtest_repeat")
            or arg.startswith("--gtest-repeat")
            or arg.startswith("--shards")
        ):
            if (
                "=" not in arg
                and arg != "--isolated-script-test-also-run-disabled-tests"
                and i + 1 < len(args)
                and not args[i + 1].startswith("-")
            ):
                i += 2
            else:
                i += 1
        elif arg.endswith(".test.js") or ".test.js" in arg or "*" in arg:
            file_globs_or_paths.append(arg)
            i += 1
        else:
            cleaned_args.append(arg)
            i += 1

    env_filter = (
        os.environ.get("ISOLATED_SCRIPT_TEST_FILTER")
        or os.environ.get("TEST_FILTER")
        or os.environ.get("GTEST_FILTER")
    )
    if env_filter:
        test_filters.append(env_filter)

    env_filter_file = os.environ.get(
        "ISOLATED_SCRIPT_TEST_FILTER_FILE"
    ) or os.environ.get("TEST_FILTER_FILE")
    if env_filter_file:
        test_filter_files.append(env_filter_file)

    for f_path in test_filter_files:
        if os.path.exists(f_path):
            test_filters.extend(parse_filter_file(f_path))

    return cleaned_args, test_filters, test_filter_files, file_globs_or_paths


def resolve_test_files_and_patterns(
    file_globs_or_paths: list[str],
    test_filters: list[str],
) -> tuple[list[str], list[str]]:
    """Resolves target test files from globs and applies test filters.

    Returns:
        (target_test_files, test_name_patterns)
    """
    import glob

    all_test_files = []
    for arg in file_globs_or_paths:
        if "*" in arg:
            matches = glob.glob(arg, recursive=True)
            if matches:
                all_test_files.extend(matches)
            else:
                all_test_files.append(arg)
        else:
            all_test_files.append(arg)

    target_test_files = all_test_files
    test_name_patterns = []

    if test_filters:
        parsed_rules = []
        for f_str in test_filters:
            tokens = parse_filter_tokens(f_str)
            for token in tokens:
                parsed_rules.append(parse_filter_pattern(token))

        inclusion_rules = [r for r in parsed_rules if not r[0]]
        exclusion_rules = [r for r in parsed_rules if r[0]]

        if inclusion_rules:
            matched_files = set()
            for _, f_pat, c_pat in inclusion_rules:
                has_file_match = False
                for tf in all_test_files:
                    if matches_file(tf, f_pat):
                        matched_files.add(tf)
                        has_file_match = True
                if c_pat:
                    test_name_patterns.append(c_pat)
                elif not has_file_match and f_pat:
                    test_name_patterns.append(f_pat)
            target_test_files = [f for f in all_test_files if f in matched_files]

        for _, f_pat, c_pat in exclusion_rules:
            if f_pat and not c_pat:
                target_test_files = [
                    f for f in target_test_files if not matches_file(f, f_pat)
                ]

    return target_test_files, test_name_patterns
