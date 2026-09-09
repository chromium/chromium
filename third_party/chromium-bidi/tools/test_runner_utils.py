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
CANONICAL_TEST_ID_RE = re.compile(
    r"""
    [+-]?                                           # Optional filter inclusion (+) or exclusion (-) prefix
    (?:ninja://\S+?:[^\s/]+/)?                      # Optional Ninja build target prefix (e.g. ninja://dir:target/)
    :chromium-bidi!\w+                              # ResultDB module prefix and scheme (e.g. :chromium-bidi!pytest)
    :[^#:\s]*                                       # Coarse directory path (e.g. :tests/bluetooth/)
    (?::[^#:\s]*)?                                  # Fine file name (e.g. :test_characteristic_emulation.py)
    (?:\#[^:\s\n\r]*(?::(?!\S+[:/]|-)[\w\s-]+)*)?   # Optional case name, allowing colons in sub-titles (#suite:test)
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

    canonical_tokens = []

    def mask_canonical(match):
        idx = len(canonical_tokens)
        canonical_tokens.append(match.group(0).strip())
        return f" __CANONICAL_{idx}__ "

    masked_str = CANONICAL_TEST_ID_RE.sub(mask_canonical, filter_str)
    raw_tokens = re.split(r":{3,}|(?<!:):(?!:)", masked_str)

    patterns = []
    for raw in raw_tokens:
        raw = raw.strip()
        if not raw:
            continue

        if "__CANONICAL_" in raw:
            for piece in raw.split():
                if piece.startswith("__CANONICAL_") and piece.endswith("__"):
                    idx = int(piece[len("__CANONICAL_") : -2])
                    patterns.append(canonical_tokens[idx])
                elif piece and piece.strip(":") != "":
                    patterns.append(piece)
        elif "::" in raw:
            sub_tokens = [s.strip() for s in raw.split("::") if s.strip()]
            i = 0
            while i < len(sub_tokens):
                st = sub_tokens[i]
                is_py = (
                    st.endswith(".py")
                    or ".py:" in st
                    or (st.startswith("-") and st[1:].endswith(".py"))
                )
                is_js_ts = (
                    st.endswith(".js")
                    or st.endswith(".ts")
                    or (
                        st.startswith("-")
                        and (st[1:].endswith(".js") or st[1:].endswith(".ts"))
                    )
                )
                if (
                    (is_py or is_js_ts)
                    and i + 1 < len(sub_tokens)
                    and not (
                        sub_tokens[i + 1].endswith(".py")
                        or sub_tokens[i + 1].endswith(".js")
                        or sub_tokens[i + 1].endswith(".ts")
                    )
                    and not sub_tokens[i + 1].startswith(":")
                    and not sub_tokens[i + 1].startswith("tests/")
                    and not sub_tokens[i + 1].startswith("src/")
                ):
                    delimiter = "::" if is_py else "#"
                    patterns.append(f"{st}{delimiter}{sub_tokens[i + 1]}")
                    i += 2
                else:
                    patterns.append(st)
                    i += 1
        else:
            patterns.append(raw)
    return patterns


def parse_filter_file(filepath: str) -> list[str]:
    """Reads a filter file in Chromium Test List Format."""
    filters = []
    tag_regex = re.compile(
        r"\[[^\]]*\]|Bug\([^)]*\)|crbug\.com/\S*|skbug\.com/\S*|webkit\.org/\S*",
        re.VERBOSE,
    )
    with open(filepath, encoding="utf-8") as f:
        for line in f:
            raw_line = re.split(r"(?:\s|^)#", line)[0].strip()
            if not raw_line:
                continue
            is_skip = "[ Skip ]" in raw_line or "[ Failure ]" in raw_line
            cleaned_line = tag_regex.sub("", raw_line).strip()
            if cleaned_line:
                for token in parse_filter_tokens(cleaned_line):
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
