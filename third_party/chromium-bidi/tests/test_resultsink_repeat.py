# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import subprocess
import sys
import textwrap
from collections import Counter
from pathlib import Path

import pytest
import resultsink_reporter

# The nested run uses the real plugins, but never an inherited ResultSink or
# shard/filter configuration. Round-trip the reports through pytest's transport
# hooks before recording the actual ResultSink payloads in memory.
RECORDER = """
import json
from pathlib import Path

import pytest

results = []
reports = []
collected = []

class Transport:
    def __init__(self, config, reporter):
        self.config = config
        self.reporter = reporter

    def pytest_runtest_logreport(self, report):
        data = self.config.hook.pytest_report_to_serializable(
            config=self.config, report=report)
        restored = self.config.hook.pytest_report_from_serializable(
            config=self.config, data=json.loads(json.dumps(data)))
        reports.append([restored.nodeid, restored.when, restored.outcome])
        self.reporter.pytest_runtest_logreport(restored)

@pytest.hookimpl(trylast=True)
def pytest_configure(config):
    reporter = config.pluginmanager.get_plugin('resultsink_reporter_plugin')
    config.pluginmanager.unregister(reporter)
    # There is deliberately no URL or token, even if a future change bypasses
    # the recorder below. Synthetic cases must never reach a live ResultSink.
    reporter.sink_data = {'local_recorder': True}
    reporter._send_batch = results.extend
    config.pluginmanager.register(Transport(config, reporter))

def pytest_collection_finish(session):
    collected.extend(item.nodeid for item in session.items)

def pytest_sessionfinish(session, exitstatus):
    Path('captured.json').write_text(json.dumps({
        'collected': collected, 'results': results, 'reports': reports,
    }))
"""


@pytest.fixture
def run_nested(tmp_path):
    def run(source, *args, plugin=""):
        (tmp_path / "pytest.ini").write_text("[pytest]\n", encoding="utf-8")
        (tmp_path / "conftest.py").write_text(
            RECORDER + textwrap.dedent(plugin), encoding="utf-8"
        )
        (tmp_path / "test_cases.py").write_text(
            textwrap.dedent(source), encoding="utf-8"
        )
        env = {
            key: value
            for key, value in os.environ.items()
            if not key.startswith(("PYTEST_", "GTEST_", "ISOLATED_SCRIPT_"))
            and key
            not in {
                "LUCI_CONTEXT",
                "PYTHONPATH",
                "TEST_FILTER",
                "TEST_FILTER_FILE",
                "TEST_SHARD_INDEX",
                "TEST_TOTAL_SHARDS",
            }
        }
        env.update(
            {
                "PYTEST_DISABLE_PLUGIN_AUTOLOAD": "1",
                "PYTHONDONTWRITEBYTECODE": "1",
                "PYTHONPATH": str(Path(resultsink_reporter.__file__).parent),
            }
        )
        completed = subprocess.run(
            [
                sys.executable,
                "-m",
                "pytest",
                "-c",
                str(tmp_path / "pytest.ini"),
                "--confcutdir",
                str(tmp_path),
                "-p",
                "no:cacheprovider",
                "-p",
                "pytest_repeat",
                "-p",
                "resultsink_reporter",
                "-q",
                "--tb=short",
                *args,
                "test_cases.py",
            ],
            cwd=tmp_path,
            env=env,
            text=True,
            capture_output=True,
            check=False,
            timeout=30,
        )
        captured_path = tmp_path / "captured.json"
        captured = (
            json.loads(captured_path.read_text(encoding="utf-8"))
            if captured_path.exists()
            else None
        )
        return completed, captured

    return run


def case_counts(captured):
    return Counter(
        tuple(result["testIdStructured"]["caseNameComponents"])
        for result in captured["results"]
    )


def test_repeat_identity_preserves_cartesian_parameters_and_numeric_ids(run_nested):
    source = """
        import pytest

        @pytest.fixture(params=[0, 1], ids=['Custom user context', '1-20'])
        def context(request):
            return request.param

        class TestCases:
            @pytest.mark.parametrize(('a', 'b'), [(1, 2), (3, 4)], ids=['1-3', 'pair'])
            @pytest.mark.parametrize('kind', ['http', 'data'])
            def test_parameters(self, context, a, b, kind):
                pass

        def test_plain():
            pass
    """
    baseline, original = run_nested(source, "--count=1")
    repeated, actual = run_nested(source, "--count=3")
    assert baseline.returncode == repeated.returncode == 0, repeated.stdout
    assert len(original["results"]) == 9
    assert case_counts(actual) == Counter(
        {identity: count * 3 for identity, count in case_counts(original).items()}
    )

    def identities(results):
        return Counter(
            (result["testId"], json.dumps(result["testIdStructured"], sort_keys=True))
            for result in results
        )

    assert identities(actual["results"]) == Counter(
        {
            identity: count * 3
            for identity, count in identities(original["results"]).items()
        }
    )
    expected_order = []
    for nodeid in original["collected"]:
        for step in range(1, 4):
            expected_order.append(
                f"{nodeid[:-1]}-{step}-3]"
                if nodeid.endswith("]")
                else f"{nodeid}[{step}-3]"
            )
    assert actual["collected"] == expected_order


def test_repeat_identity_handles_later_grouped_parameters(run_nested):
    completed, captured = run_nested(
        """
        import pytest

        @pytest.fixture
        def extra_a(request):
            return request.param

        @pytest.fixture
        def extra_b(request):
            return request.param

        @pytest.mark.parametrize('first', [0, 1], ids=['left', 'right'])
        def test_middle(first, extra_a, extra_b):
            pass

        def test_first(extra_a, extra_b):
            pass
        """,
        "--count=3",
        plugin="""
        @pytest.hookimpl(hookwrapper=True, trylast=True)
        def pytest_generate_tests(metafunc):
            yield
            metafunc.parametrize(('extra_a', 'extra_b'), [(1, 3), (2, 3)],
                                 indirect=True, ids=['1-3', '2-3'])
        """,
    )
    assert completed.returncode == 0, completed.stdout
    assert case_counts(captured) == Counter(
        {
            ("test_middle[left-1-3]",): 3,
            ("test_middle[left-2-3]",): 3,
            ("test_middle[right-1-3]",): 3,
            ("test_middle[right-2-3]",): 3,
            ("test_first[1-3]",): 3,
            ("test_first[2-3]",): 3,
        }
    )


def test_unrepeated_numeric_id_is_not_removed(run_nested):
    completed, captured = run_nested(
        """
        import pytest

        @pytest.mark.parametrize('value', [0, 1], ids=['1-20', '2-20'])
        def test_numeric(value):
            pass
        """,
        "--count=1",
    )
    assert completed.returncode == 0, completed.stdout
    assert case_counts(captured) == Counter(
        {("test_numeric[1-20]",): 1, ("test_numeric[2-20]",): 1}
    )


def test_repeat_marker_and_all_reported_phases_keep_identity(run_nested):
    completed, captured = run_nested(
        """
        import pytest

        @pytest.fixture
        def outcome(request):
            if request.param == 'setup_skip':
                pytest.skip('setup skip')
            if request.param == 'setup_fail':
                pytest.fail('setup failure')
            return request.param

        @pytest.mark.repeat(2)
        @pytest.mark.parametrize('outcome',
            ['pass', 'call_fail', 'call_skip', 'setup_fail', 'setup_skip'],
            indirect=True)
        def test_outcome(outcome):
            if outcome == 'call_fail':
                pytest.fail('call failure')
            if outcome == 'call_skip':
                pytest.skip('call skip')
        """,
        "--count=3",
        "--repeat-scope=session",
    )
    assert completed.returncode == 1, completed.stdout
    assert case_counts(captured) == Counter(
        {
            (f"test_outcome[{outcome}]",): 2
            for outcome in (
                "pass",
                "call_fail",
                "call_skip",
                "setup_fail",
                "setup_skip",
            )
        }
    )
    assert Counter((r["status"], r["expected"]) for r in captured["results"]) == {
        ("PASS", True): 2,
        ("FAIL", False): 4,
        ("SKIP", True): 4,
    }
    assert len(captured["reports"]) == 26  # Setup failures/skips have no call.


def test_repeat_filter_aliases_do_not_override_exclusions(run_nested):
    source = """
        import pytest

        @pytest.mark.parametrize('value', [0, 1], ids=['http', 'data'])
        def test_case(value):
            pass
    """
    prefix = ":chromium-bidi!pytest::test_cases.py#"
    selected, actual = run_nested(
        source, "--count=3", f"--test-filter={prefix}test_case[http]"
    )
    assert selected.returncode == 0, selected.stdout
    assert case_counts(actual) == Counter({("test_case[http]",): 3})

    excluded, remaining = run_nested(
        source, "--count=3", f"--test-filter=*:-{prefix}test_case[http]"
    )
    assert excluded.returncode == 0, excluded.stdout
    assert case_counts(remaining) == Counter({("test_case[data]",): 3})

    raw, one = run_nested(
        source, "--count=3", "--test-filter=test_cases.py::test_case[http-2-3]"
    )
    assert raw.returncode == 0, raw.stdout
    assert one["collected"] == ["test_cases.py::test_case[http-2-3]"]
    assert case_counts(one) == Counter({("test_case[http]",): 1})

    missing, none = run_nested(
        source, "--count=3", f"--test-filter={prefix}test_case[absent]"
    )
    assert missing.returncode == 5, missing.stdout
    assert none["collected"] == none["results"] == []


def test_twenty_repeats_select_only_the_exact_parameter(run_nested):
    completed, captured = run_nested(
        """
        import pytest

        @pytest.mark.parametrize('context', [0, 1, 2],
                                 ids=['Custom user context', 'Default user context', 'other'])
        def test_case(context):
            pass
        """,
        "--count=20",
        "--test-filter=:chromium-bidi!pytest::test_cases.py#test_case[Custom user context]",
    )
    assert completed.returncode == 0, completed.stdout
    assert len(captured["collected"]) == 20
    assert case_counts(captured) == Counter({("test_case[Custom user context]",): 20})


def test_normalized_function_filter_preserves_parameter_node_separators(run_nested):
    completed, captured = run_nested(
        """
        import pytest

        class TestCases:
            @pytest.mark.parametrize('value', [0, 1], ids=['literal::parameter', 'other'])
            def test_case(self, value):
                pass
        """,
        "--count=3",
        "--test-filter=test_case[literal::parameter]",
    )
    assert completed.returncode == 0, completed.stdout
    assert captured["collected"] == [
        f"test_cases.py::TestCases::test_case[literal::parameter-{step}-3]"
        for step in range(1, 4)
    ]
    assert len(captured["results"]) == 3


def test_repeated_exact_filters_preserve_literal_wildcards(run_nested):
    prefix = ":chromium-bidi!pytest::test_cases.py#test_case"
    completed, captured = run_nested(
        """
        import pytest

        @pytest.mark.parametrize('value', [0, 1, 2, 3],
                                 ids=['literal*', 'literal?', 'literalX', 'other'])
        def test_case(value):
            pass
        """,
        "--count=3",
        f"--test-filter={prefix}[literal*]::{prefix}[literal?]",
    )
    assert completed.returncode == 0, completed.stdout
    assert captured["collected"] == [
        f"test_cases.py::test_case[literal{symbol}-{step}-3]"
        for symbol in ("*", "?")
        for step in range(1, 4)
    ]
    assert case_counts(captured) == Counter(
        {("test_case[literal*]",): 3, ("test_case[literal?]",): 3}
    )


@pytest.mark.parametrize(
    "corruption",
    [
        # A duplicate matching dimension must not alias user identities.
        "for call in metafunc._calls:\n        call._idlist.append(call._idlist[0])",
        # A post-generation plugin cannot silently discard part of the product.
        "metafunc._calls.pop()",
        # Do not guess a numeric suffix if the repeat ID schema changes.
        "for call in metafunc._calls:\n        call._idlist[0] = 'unknown'",
        # Pruning to correlated numeric user parameters cannot turn that user
        # dimension into the apparent repeat dimension after an ID change.
        "metafunc.fixturenames.append('extra')\n"
        "    metafunc.parametrize('extra', range(3), indirect=True, ids=['1-3', '2-3', '3-3'])\n"
        "    metafunc._calls[:] = [c for c in metafunc._calls if c.params['extra'] == c.params['__pytest_repeat_step_number']]\n"
        "    for call in metafunc._calls:\n        call._idlist[0] = 'unknown'",
    ],
    ids=["ambiguous", "incomplete", "missing", "non-cartesian"],
)
def test_incompatible_repeat_metadata_fails_collection(run_nested, corruption):
    completed, captured = run_nested(
        """
        def test_plain():
            pass
        """,
        "--count=3",
        plugin=(
            "\n@pytest.hookimpl(hookwrapper=True, trylast=True)\n"
            "def pytest_generate_tests(metafunc):\n"
            "    yield\n"
            f"    {corruption}\n"
        ),
    )
    assert completed.returncode != 0
    assert "Cannot determine pytest-repeat identity" in (
        completed.stdout + completed.stderr
    )
    assert captured is None or captured["results"] == []
