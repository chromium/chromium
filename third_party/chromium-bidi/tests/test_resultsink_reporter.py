# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import tempfile

import pytest
from resultsink_reporter import (
    TestFilter,
    TestFilterGroup,
    format_test_id,
    parse_filter_tokens,
)

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools"))
from test_runner_utils import (
    matches_file,
    parse_filter_file,
    parse_filter_pattern,
)


def test_format_test_id_standard():
    nodeid = "tests/bluetooth/test_characteristic_emulation.py::test_bluetooth_add_same_characteristic_uuid_twice"
    test_id, structured = format_test_id(nodeid)

    assert (
        test_id
        == ":chromium-bidi!pytest:tests/bluetooth/:test_characteristic_emulation.py#test_bluetooth_add_same_characteristic_uuid_twice"
    )
    assert structured == {
        "moduleName": "chromium-bidi",
        "moduleScheme": "pytest",
        "coarseName": "tests/bluetooth/",
        "fineName": "test_characteristic_emulation.py",
        "caseNameComponents": ["test_bluetooth_add_same_characteristic_uuid_twice"],
    }


def test_format_test_id_parameterized():
    nodeid = "tests/browser/test_create_user_context.py::test_browser_create_user_context_proxy[True]"
    test_id, structured = format_test_id(nodeid)

    assert (
        test_id
        == ":chromium-bidi!pytest:tests/browser/:test_create_user_context.py#test_browser_create_user_context_proxy[True]"
    )
    assert structured["caseNameComponents"] == [
        "test_browser_create_user_context_proxy[True]"
    ]


def test_format_test_id_nested_class():
    nodeid = "tests/session/test_session.py::TestSessionClass::test_session_create"
    test_id, structured = format_test_id(nodeid)

    assert (
        test_id
        == ":chromium-bidi!pytest:tests/session/:test_session.py#TestSessionClass:test_session_create"
    )
    assert structured["caseNameComponents"] == [
        "TestSessionClass",
        "test_session_create",
    ]


def test_parse_filter_tokens_structured():
    filter_str = (
        ":chromium-bidi!pytest:tests/a/:b.py#c:::chromium-bidi!pytest:tests/d/:e.py#f"
    )
    tokens = parse_filter_tokens(filter_str)
    assert tokens == [
        ":chromium-bidi!pytest:tests/a/:b.py#c",
        ":chromium-bidi!pytest:tests/d/:e.py#f",
    ]


@pytest.mark.parametrize(
    "case",
    [
        "test_download[Custom user context]",
        "test_download[Default user context-http]",
        r"test_download[https\://example.test/a\:\:b\#fragment]",
        r"test_download[space and \! punctuation \\]",
        "TestClass:test_download[Custom user context]",
        "__CANONICAL_0__",
    ],
)
def test_canonical_filters_preserve_complete_case_names(case):
    first = f":chromium-bidi!pytest:tests/browser/:test_download.py#{case}"
    second = ":chromium-bidi!pytest:tests/browser/:test_download.py#test_other"
    assert parse_filter_tokens(first) == [first]
    assert parse_filter_tokens(f"{first}::{second}") == [first, second]
    assert parse_filter_tokens(f"{second}::{first}") == [second, first]
    assert parse_filter_tokens(f"-{first}::{second}") == [f"-{first}", second]


def test_canonical_filters_do_not_interpret_legacy_names_as_placeholders():
    canonical = ":chromium-bidi!pytest:tests/:test_case.py#test_case[with spaces]"
    assert parse_filter_tokens(f"__CANONICAL_0__::{canonical}") == [
        "__CANONICAL_0__",
        canonical,
    ]


def test_canonical_filters_preserve_escaped_colon_at_separator():
    canonical = r":chromium-bidi!pytest:tests/:test_case.py#test_case[ends\:]"
    assert parse_filter_tokens(f"{canonical}::tests/other.py") == [
        canonical,
        "tests/other.py",
    ]


@pytest.mark.parametrize("separator", [":", "::"])
def test_canonical_filters_preserve_nested_node_names_and_legacy_boundaries(separator):
    canonical = (
        "ninja://third_party/chromium-bidi:webdriver_bidi_unittests/"
        ":chromium-bidi!mocha:src/utils/:assert.test.ts"
        r"#assert:nested suite:handles https\://example.test and \# fragments"
    )
    assert parse_filter_tokens(f"{canonical}{separator}-src/cdp/CdpClient.test.ts") == [
        canonical,
        "-src/cdp/CdpClient.test.ts",
    ]


@pytest.mark.parametrize("separator", [":", "::"])
@pytest.mark.parametrize(
    "wildcard, other_func", [("*", "z"), ("?", "z"), ("*z", "z"), ("?z", "az")]
)
def test_canonical_filters_preserve_wildcard_rule_boundaries(
    separator, wildcard, other_func
):
    canonical = ":chromium-bidi!pytest:tests/:test_case.py#test_case"
    other = f":chromium-bidi!pytest:tests/:test_case.py#{other_func}"

    for rules in ((canonical, wildcard), (wildcard, canonical)):
        tokens = parse_filter_tokens(separator.join(rules))
        assert tokens == list(rules)
        group = TestFilterGroup([TestFilter(token) for token in tokens])
        assert group.is_test_included(
            canonical,
            "tests/test_case.py::test_case",
            "tests/test_case.py",
            "test_case",
        )
        assert group.is_test_included(
            other,
            f"tests/test_case.py::{other_func}",
            "tests/test_case.py",
            other_func,
        )

    for rules in ((f"-{canonical}", wildcard), (wildcard, f"-{canonical}")):
        tokens = parse_filter_tokens(separator.join(rules))
        assert tokens == list(rules)
        group = TestFilterGroup([TestFilter(token) for token in tokens])
        assert not group.is_test_included(
            canonical,
            "tests/test_case.py::test_case",
            "tests/test_case.py",
            "test_case",
        )
        assert group.is_test_included(
            other,
            f"tests/test_case.py::{other_func}",
            "tests/test_case.py",
            other_func,
        )


@pytest.mark.parametrize("parse", [parse_filter_file, TestFilterGroup.from_filter_file])
def test_filter_files_preserve_parameters_and_only_remove_metadata(tmp_path, parse):
    canonical = (
        ":chromium-bidi!pytest:tests/browser/:test_download.py"
        r"#test_download[Custom user context with \# and [nested] brackets]"
    )
    legacy = "tests/browser/test_download.py::test_download[1-20]"
    path = tmp_path / "filters.txt"
    path.write_text(
        "# Comment\n"
        f"[ Debug ] Bug(12345) {canonical} [ Failure ] [ Skip ] # comment\n"
        f"{legacy}\n"
        "[ Release ] crbug.com/67890 tests/other.py [ Failure ]\n",
        encoding="utf-8",
    )
    parsed = parse(str(path))
    if isinstance(parsed, TestFilterGroup):
        parsed = [
            ("-" if item.is_exclusion else "") + item.filter_text
            for item in parsed.filters
        ]
    assert set(parsed) == {f"-{canonical}", legacy, "-tests/other.py"}


def test_legacy_filters_preserve_parameter_colons_and_spaces():
    first = "tests/a.py::test_case[https://example.test/a::b with spaces]"
    second = "tests/b.py::test_other"
    assert parse_filter_tokens(f"{first}::{second}") == [first, second]


@pytest.mark.parametrize(
    "first, following",
    [
        ("tests/a.py::test_a", "-test_b"),
        ("tests/a.py::test_a", "*"),
        ("tests/a.py::test_a", "test_b*"),
        ("src/a.test.ts::test one", "-test two"),
    ],
)
def test_legacy_filters_keep_following_rules_separate(first, following):
    expected_first = first.replace(".ts::", ".ts#")
    assert parse_filter_tokens(f"{first}::{following}") == [
        expected_first,
        following,
    ]


def test_legacy_filters_do_not_extract_canonical_text_from_parameters():
    first = "tests/a.py::test_case[example::chromium-bidi!pytest:tests/:b.py#case]"
    canonical = ":chromium-bidi!pytest:tests/:test_other.py#test_other"
    assert parse_filter_tokens(f"{first}::{canonical}") == [first, canonical]


@pytest.mark.parametrize("parameter", ["[", "x[y", "]", "ends\\", "nested[x:y]"])
def test_legacy_parameter_brackets_are_not_nesting_syntax(parameter):
    first = f"tests/a.py::test_case[{parameter}]"
    legacy = "tests/b.py::test_other"
    canonical = ":chromium-bidi!pytest:tests/:test_other.py#test_other"
    assert parse_filter_tokens(f"{first}::{legacy}") == [first, legacy]
    assert parse_filter_tokens(f"{first}::{canonical}") == [first, canonical]


def test_parse_filter_tokens_gtest():
    filter_str = "tests/a/test_a.py::test_func_a:tests/b/test_b.py::test_func_b:-tests/c/test_c.py"
    tokens = parse_filter_tokens(filter_str)
    assert tokens == [
        "tests/a/test_a.py::test_func_a",
        "tests/b/test_b.py::test_func_b",
        "-tests/c/test_c.py",
    ]


def test_parse_filter_tokens_test_ninja():
    filter_str = "tests/a.py::test_ninja:tests/b.py"
    tokens = parse_filter_tokens(filter_str)
    assert tokens == [
        "tests/a.py::test_ninja",
        "tests/b.py",
    ]


def test_parse_filter_tokens_mixed_ninja_and_regular():
    filter_str = (
        "ninja://third_party/chromium-bidi:webdriver_bidi_e2e_tests/:chromium-bidi!pytest:tests/a/:b.py#c"
        ":tests/b.py::test_ninja:-tests/c.py"
    )
    tokens = parse_filter_tokens(filter_str)
    assert tokens == [
        "ninja://third_party/chromium-bidi:webdriver_bidi_e2e_tests/:chromium-bidi!pytest:tests/a/:b.py#c",
        "tests/b.py::test_ninja",
        "-tests/c.py",
    ]


def test_parse_filter_tokens_legacy_joined():
    filter_str = "tests/bluetooth/test_a.py::test_func_a::tests/browser/test_b.py::test_func_b[True]"
    tokens = parse_filter_tokens(filter_str)
    assert tokens == [
        "tests/bluetooth/test_a.py::test_func_a",
        "tests/browser/test_b.py::test_func_b[True]",
    ]


def test_test_filter_group_ninja_and_wildcard():
    tokens = [
        "ninja://third_party/chromium-bidi:webdriver_bidi_e2e_tests/:chromium-bidi!pytest:tests/bluetooth/:test_characteristic_emulation.py#test_bluetooth_add_same_characteristic_uuid_twice",
        "*test_create_user_context*",
    ]
    group = TestFilterGroup([TestFilter(t) for t in tokens])

    # Should match ninja-prefixed structured test ID
    assert group.is_test_included(
        ":chromium-bidi!pytest:tests/bluetooth/:test_characteristic_emulation.py#test_bluetooth_add_same_characteristic_uuid_twice",
        "tests/bluetooth/test_characteristic_emulation.py::test_bluetooth_add_same_characteristic_uuid_twice",
        "tests/bluetooth/test_characteristic_emulation.py",
        "test_bluetooth_add_same_characteristic_uuid_twice",
    )

    # Should match wildcard
    assert group.is_test_included(
        ":chromium-bidi!pytest:tests/browser/:test_create_user_context.py#test_browser_create_user_context_legacy_proxy",
        "tests/browser/test_create_user_context.py::test_browser_create_user_context_legacy_proxy",
        "tests/browser/test_create_user_context.py",
        "test_browser_create_user_context_legacy_proxy",
    )


@pytest.mark.parametrize(
    "filter_text",
    [
        ":chromium-bidi!pytest:tests/:test_case.py#test_case[literal*]",
        ":chromium-bidi!pytest:tests/:test_case.py#test_case[literal?]",
        "ninja://third_party/chromium-bidi:webdriver_bidi_e2e_tests/"
        ":chromium-bidi!pytest:tests/:test_case.py#test_case[literal*]",
        "tests/test_case.py::test_case[literal*]",
        "test_case[literal?]",
    ],
    ids=["canonical-star", "canonical-question", "ninja", "nodeid", "function"],
)
def test_exact_filter_matches_literal_wildcards(filter_text):
    assert TestFilter(filter_text).matches_string(filter_text)


def test_literal_matching_preserves_wildcards_and_exclusions():
    canonical = ":chromium-bidi!pytest:tests/:test_case.py#test_case[literal*]"
    nodeid = "tests/test_case.py::test_case[literal*]"
    assert TestFilter("test_*").matches_string("test_case[literal*]")
    assert TestFilter("test_cas?").matches_string("test_case")
    assert not TestFilter("test_cas?").matches_string("test_other")
    group = TestFilterGroup([TestFilter("*"), TestFilter(f"-{canonical}")])
    assert not group.is_test_included(
        canonical, nodeid, "tests/test_case.py", "test_case[literal*]"
    )


def test_test_filter_ninja_prefix_with_wildcards():
    filter_rule = TestFilter(
        "ninja://other_target:other_test_name/:chromium-bidi!pytest:tests/bluetooth/*"
    )
    assert filter_rule.is_match(
        ":chromium-bidi!pytest:tests/bluetooth/:test_characteristic_emulation.py#test_bluetooth_add_same_characteristic_uuid_twice",
        "tests/bluetooth/test_characteristic_emulation.py::test_bluetooth_add_same_characteristic_uuid_twice",
        "tests/bluetooth/test_characteristic_emulation.py",
        "test_bluetooth_add_same_characteristic_uuid_twice",
    )
    assert not filter_rule.is_match(
        ":chromium-bidi!pytest:tests/browser/:test_create_user_context.py#test_browser_create_user_context_legacy_proxy",
        "tests/browser/test_create_user_context.py::test_browser_create_user_context_legacy_proxy",
        "tests/browser/test_create_user_context.py",
        "test_browser_create_user_context_legacy_proxy",
    )


def test_test_filter_group_matching():
    tokens = [
        ":chromium-bidi!pytest:tests/bluetooth/:test_characteristic_emulation.py#test_bluetooth_add_same_characteristic_uuid_twice",
        "tests/browser/test_create_user_context.py::test_browser_create_user_context_legacy_proxy",
    ]
    group = TestFilterGroup([TestFilter(t) for t in tokens])

    # Should match structured test ID
    assert group.is_test_included(
        ":chromium-bidi!pytest:tests/bluetooth/:test_characteristic_emulation.py#test_bluetooth_add_same_characteristic_uuid_twice",
        "tests/bluetooth/test_characteristic_emulation.py::test_bluetooth_add_same_characteristic_uuid_twice",
        "tests/bluetooth/test_characteristic_emulation.py",
        "test_bluetooth_add_same_characteristic_uuid_twice",
    )

    # Should match legacy node ID
    assert group.is_test_included(
        ":chromium-bidi!pytest:tests/browser/:test_create_user_context.py#test_browser_create_user_context_legacy_proxy",
        "tests/browser/test_create_user_context.py::test_browser_create_user_context_legacy_proxy",
        "tests/browser/test_create_user_context.py",
        "test_browser_create_user_context_legacy_proxy",
    )

    # Should not match other test
    assert not group.is_test_included(
        ":chromium-bidi!pytest:tests/network/:test_network.py#test_other",
        "tests/network/test_network.py::test_other",
        "tests/network/test_network.py",
        "test_other",
    )


def test_test_filter_group_exclusion():
    tokens = ["tests/bluetooth/*", "-test_bluetooth_add_same_characteristic_uuid_twice"]
    group = TestFilterGroup([TestFilter(t) for t in tokens])

    # Excluded test
    assert not group.is_test_included(
        ":chromium-bidi!pytest:tests/bluetooth/:test_characteristic_emulation.py#test_bluetooth_add_same_characteristic_uuid_twice",
        "tests/bluetooth/test_characteristic_emulation.py::test_bluetooth_add_same_characteristic_uuid_twice",
        "tests/bluetooth/test_characteristic_emulation.py",
        "test_bluetooth_add_same_characteristic_uuid_twice",
    )

    # Other test in the folder is included
    assert group.is_test_included(
        ":chromium-bidi!pytest:tests/bluetooth/:test_characteristic_emulation.py#test_bluetooth_simulateCharacteristic[notify]",
        "tests/bluetooth/test_characteristic_emulation.py::test_bluetooth_simulateCharacteristic[notify]",
        "tests/bluetooth/test_characteristic_emulation.py",
        "test_bluetooth_simulateCharacteristic[notify]",
    )


def test_test_filter_file_parsing():
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
        f.write("# Sample filter file\n")
        f.write("tests/bluetooth/*\n")
        f.write(
            "[ Skip ] tests/bluetooth/test_characteristic_emulation.py::test_bluetooth_add_same_characteristic_uuid_twice\n"
        )
        filepath = f.name

    try:
        group = TestFilterGroup.from_filter_file(filepath)
        assert len(group.filters) == 2

        # Included test
        assert group.is_test_included(
            ":chromium-bidi!pytest:tests/bluetooth/:test_characteristic_emulation.py#test_bluetooth_simulateCharacteristic[notify]",
            "tests/bluetooth/test_characteristic_emulation.py::test_bluetooth_simulateCharacteristic[notify]",
            "tests/bluetooth/test_characteristic_emulation.py",
            "test_bluetooth_simulateCharacteristic[notify]",
        )

        # Skipped test
        assert not group.is_test_included(
            ":chromium-bidi!pytest:tests/bluetooth/:test_characteristic_emulation.py#test_bluetooth_add_same_characteristic_uuid_twice",
            "tests/bluetooth/test_characteristic_emulation.py::test_bluetooth_add_same_characteristic_uuid_twice",
            "tests/bluetooth/test_characteristic_emulation.py",
            "test_bluetooth_add_same_characteristic_uuid_twice",
        )
    finally:
        os.remove(filepath)


def test_unit_test_filter_file_parsing():
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
        f.write("# WebKit style filter file\n")
        f.write("[ Debug ] Bug(12345) src/utils/assert.test.ts\n")
        f.write("[ Release ] crbug.com/67890 [ Skip ] src/utils/DefaultMap.test.ts\n")
        f.write(
            ":chromium-bidi!mocha:src/cdp/:CdpClient.test.ts#CdpClient:when some command is called\n"
        )
        filepath = f.name

    try:
        filters = parse_filter_file(filepath)
        assert filters == [
            "src/utils/assert.test.ts",
            "-src/utils/DefaultMap.test.ts",
            ":chromium-bidi!mocha:src/cdp/:CdpClient.test.ts#CdpClient:when some command is called",
        ]

        is_ex, f_pat, c_pat = parse_filter_pattern(filters[0])
        assert not is_ex
        assert f_pat == "src/utils/assert.test.ts"
        assert c_pat is None

        is_ex, f_pat, c_pat = parse_filter_pattern(filters[1])
        assert is_ex
        assert f_pat == "src/utils/DefaultMap.test.ts"
        assert c_pat is None

        is_ex, f_pat, c_pat = parse_filter_pattern(filters[2])
        assert not is_ex
        assert f_pat == "src/cdp/CdpClient.test.ts"
        assert c_pat == "when some command is called"

        assert matches_file(
            "out/Default/gen/third_party/chromium-bidi/src/utils/assert.test.js",
            "src/utils/assert.test.ts",
        )
        assert matches_file(
            "gen/third_party/chromium-bidi/src/cdp/CdpClient.test.js",
            "src/cdp/CdpClient.test.ts",
        )
        # Verify directory constraints prevent accidental mismatch
        assert not matches_file(
            "out/Default/gen/third_party/chromium-bidi/src/other/assert.test.js",
            "src/utils/assert.test.ts",
        )
        assert not matches_file(
            "out/Default/gen/third_party/chromium-bidi/x/y/c.test.js",
            "a/b/c.test.ts",
        )
    finally:
        os.remove(filepath)


def test_unit_test_filter_pattern_ninja_and_gtest():
    # GTest colon-delimited tokens with ninja prefix
    filter_str = (
        "ninja://third_party/chromium-bidi:webdriver_bidi_unittests/:chromium-bidi!mocha:src/utils/:assert.test.ts#assert:should not throw an error"
        ":ninja://third_party/chromium-bidi:webdriver_bidi_unittests/:chromium-bidi!mocha:src/utils/:DefaultMap.test.ts#DefaultMap:sets and gets properly"
        ":-src/cdp/CdpClient.test.ts"
    )
    tokens = parse_filter_tokens(filter_str)
    assert tokens == [
        "ninja://third_party/chromium-bidi:webdriver_bidi_unittests/:chromium-bidi!mocha:src/utils/:assert.test.ts#assert:should not throw an error",
        "ninja://third_party/chromium-bidi:webdriver_bidi_unittests/:chromium-bidi!mocha:src/utils/:DefaultMap.test.ts#DefaultMap:sets and gets properly",
        "-src/cdp/CdpClient.test.ts",
    ]

    is_ex, f_pat, c_pat = parse_filter_pattern(tokens[0])
    assert not is_ex
    assert f_pat == "src/utils/assert.test.ts"
    assert c_pat == "should not throw an error"

    is_ex, f_pat, c_pat = parse_filter_pattern(tokens[1])
    assert not is_ex
    assert f_pat == "src/utils/DefaultMap.test.ts"
    assert c_pat == "sets and gets properly"

    is_ex, f_pat, c_pat = parse_filter_pattern(tokens[2])
    assert is_ex
    assert f_pat == "src/cdp/CdpClient.test.ts"
    assert c_pat is None
