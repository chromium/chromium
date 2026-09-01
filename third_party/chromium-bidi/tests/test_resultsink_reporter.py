# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import tempfile

from resultsink_reporter import (
    TestFilter,
    TestFilterGroup,
    format_test_id,
    parse_filter_tokens,
)

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools"))
from run_unittests import (
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
