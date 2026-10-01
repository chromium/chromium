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

import fnmatch
import html
import json
import os
import re
import sys
import urllib.request

import pytest

sys.path.insert(
    0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "tools"))
)
from test_runner_utils import parse_filter_file, parse_filter_tokens

_REPEAT_PARAMETER = "__pytest_repeat_step_number"
_REPEAT_IDENTITIES = pytest.StashKey[dict[str, str]]()
_NODE_ID_PROPERTY = "chromium-bidi:nodeid"


@pytest.hookimpl(hookwrapper=True, tryfirst=True)
def pytest_generate_tests(metafunc):
    outcome = yield
    if outcome.excinfo is not None:
        return

    # pytest-repeat adds an ID component through parametrize(). Inspect the
    # complete Cartesian product here: later pytest versions/fixture processing
    # may change callspec indices, and later generators can append parameters.
    # _calls and _idlist are pinned pytest internals, covered by real-plugin
    # tests. Do not infer a repeat dimension from a numeric-looking user ID.
    calls = metafunc._calls
    if not any(_REPEAT_PARAMETER in call.params for call in calls):
        return

    def incompatible():
        outcome.force_exception(
            pytest.UsageError(
                "Cannot determine pytest-repeat identity for "
                f"{metafunc.definition.nodeid}; incompatible parametrization metadata"
            )
        )

    steps = [call.params.get(_REPEAT_PARAMETER) for call in calls]
    if any(type(step) is not int for step in steps):
        incompatible()
        return
    repeat_steps = set(steps)
    count = len(repeat_steps)
    dimensions = len(calls[0]._idlist)
    if (
        count < 2
        or repeat_steps != set(range(count))
        or any(len(call._idlist) != dimensions for call in calls)
    ):
        incompatible()
        return
    parameter_groups = {}
    for call, step in zip(calls, steps):
        parameters = tuple(
            (name, index)
            for name, index in call.indices.items()
            if name != _REPEAT_PARAMETER
        )
        parameter_groups.setdefault(parameters, set()).add(step)
    if any(group != repeat_steps for group in parameter_groups.values()):
        incompatible()
        return
    positions = [
        position
        for position in range(dimensions)
        if all(
            call._idlist[position] == f"{step + 1}-{count}"
            for call, step in zip(calls, steps)
        )
    ]
    # Across the full Cartesian product, a user parameter cannot track every
    # repeat step. A missing or non-unique dimension must not silently alias
    # separate tests or produce a successful zero-test flakiness run.
    if len(positions) != 1:
        incompatible()
        return
    position = positions[0]
    identities = metafunc.config.stash.setdefault(_REPEAT_IDENTITIES, {})
    for call in calls:
        raw_nodeid = f"{metafunc.definition.nodeid}[{call.id}]"
        parameter_ids = call._idlist[:position] + call._idlist[position + 1 :]
        nodeid = metafunc.definition.nodeid
        if parameter_ids:
            nodeid += f"[{'-'.join(parameter_ids)}]"
        if identities.setdefault(raw_nodeid, nodeid) != nodeid:
            incompatible()
            return


def format_test_id(nodeid: str):
    """Formats a pytest nodeid into a ResultDB canonical structured testId and dict.

    Canonical flat format: :chromium-bidi!pytest:${coarseName}:${fineName}#${caseName}
    """
    parts = nodeid.split("::")
    file_path = parts[0]
    case_components = parts[1:] if len(parts) > 1 else [os.path.basename(file_path)]

    coarse_name = os.path.dirname(file_path)
    if coarse_name and not coarse_name.endswith("/"):
        coarse_name += "/"
    fine_name = os.path.basename(file_path)

    def flat_escape(s: str) -> str:
        return re.sub(r"([!#:\\])", r"\\\1", s)

    coarse_flat = flat_escape(coarse_name)
    fine_flat = flat_escape(fine_name)
    case_flat = ":".join(flat_escape(c) for c in case_components)[:512]

    test_id = f":chromium-bidi!pytest:{coarse_flat}:{fine_flat}#{case_flat}"
    test_id_structured = {
        "moduleName": "chromium-bidi",
        "moduleScheme": "pytest",
        "coarseName": coarse_name,
        "fineName": fine_name,
        "caseNameComponents": case_components,
    }
    return test_id, test_id_structured


class TestFilter:
    """Represents a single test filter rule."""

    __test__ = False

    def __init__(self, filter_text: str):
        self.is_exclusion = filter_text.startswith("-")
        if self.is_exclusion:
            filter_text = filter_text[1:]
        self.filter_text = filter_text

    def matches_string(self, s: str) -> bool:
        if not s:
            return False
        # Canonical/parameterized IDs can contain literal glob punctuation.
        if s == self.filter_text:
            return True
        if "*" in self.filter_text or "?" in self.filter_text:
            return fnmatch.fnmatchcase(s, self.filter_text)
        return False

    def is_match(
        self, structured_id: str, nodeid: str, file_path: str, func_name: str
    ) -> bool:
        clean_filter = re.sub(r"^ninja://\S+?:[^\s/]+/", "", self.filter_text)
        return (
            self.matches_string(structured_id)
            or self.matches_string(
                f"ninja://third_party/chromium-bidi:webdriver_bidi_e2e_tests/{structured_id}"
            )
            or TestFilter(clean_filter).matches_string(structured_id)
            or self.matches_string(nodeid)
            or self.matches_string(file_path)
            or self.matches_string(func_name)
        )

    def specificity_key(self):
        return len(self.filter_text)


class TestFilterGroup:
    """Represents a group of filters (e.g. from a single CLI flag or file)."""

    __test__ = False

    def __init__(self, filters: list[TestFilter]):
        self.filters = sorted(filters, key=lambda f: f.specificity_key(), reverse=True)
        if self.filters and all(f.is_exclusion for f in self.filters):
            self.filters.append(TestFilter("*"))

    @classmethod
    def from_filter_file(cls, filepath: str):
        return cls([TestFilter(token) for token in parse_filter_file(filepath)])

    def is_test_included(
        self,
        structured_id: str,
        nodeid: str,
        file_path: str,
        func_name: str,
        *aliases: tuple[str, str, str, str],
    ) -> bool:
        if not self.filters:
            return True
        identifiers = ((structured_id, nodeid, file_path, func_name), *aliases)
        for f in self.filters:
            # Decide once per ordered rule, across all aliases. Otherwise a
            # wildcard matching the raw repeat can override a base-ID exclusion.
            if any(f.is_match(*identifier) for identifier in identifiers):
                return not f.is_exclusion
        return False


class ResultSinkReporter:
    def __init__(self):
        self.sink_data = self._get_sink_data()
        self.pending_results = []
        self.batch_size = 1

    def _get_sink_data(self):
        luci_context = os.environ.get("LUCI_CONTEXT")
        if not luci_context or not os.path.exists(luci_context):
            return None
        try:
            with open(luci_context) as f:
                config = json.load(f)
                sink = config.get("result_sink")
                if not sink:
                    return None
                return {
                    "url": f"http://{sink['address']}/prpc/luci.resultsink.v1.Sink/ReportTestResults",
                    "auth_token": sink["auth_token"],
                }
        except Exception as e:
            print(f"Failed to read LUCI_CONTEXT: {e}")
            return None

    def _send_batch(self, batch):
        if not self.sink_data or not batch:
            return

        data = json.dumps({"testResults": batch}).encode("utf-8")
        req = urllib.request.Request(
            self.sink_data["url"],
            data=data,
            headers={
                "Content-Type": "application/json",
                "Accept": "application/json",
                "Authorization": f"ResultSink {self.sink_data['auth_token']}",
            },
        )
        try:
            with urllib.request.urlopen(req) as response:
                response.read()
        except Exception as e:
            print(f"Failed to post to ResultSink: {e}")

    def pytest_runtest_logreport(self, report):
        if not self.sink_data:
            return

        # We only care about the actual call, unless it failed/skipped in setup
        if report.when == "setup" and report.outcome == "skipped":
            status = "SKIP"
            expected = True
        elif report.when == "setup" and report.outcome == "failed":
            status = "FAIL"
            expected = False
        elif report.when == "call":
            if report.outcome == "passed":
                status = "PASS"
                expected = True
            elif report.outcome == "failed":
                status = "FAIL"
                expected = False
            elif report.outcome == "skipped":
                status = "SKIP"
                expected = True
            else:
                return
        else:
            return

        nodeid = dict(report.user_properties).get(_NODE_ID_PROPERTY, report.nodeid)
        test_id, test_id_structured = format_test_id(nodeid)
        test_result = {
            "testId": test_id,
            "testIdStructured": test_id_structured,
            "status": status,
            "expected": expected,
            "duration": f"{report.duration:.3f}s",
        }

        if status == "FAIL" and report.longrepr:
            # report.longreprtext contains the string representation of the failure
            error_info = html.escape(report.longreprtext)
            test_result["summaryHtml"] = f"<pre>{error_info}</pre>"

        self.pending_results.append(test_result)

        if len(self.pending_results) >= self.batch_size:
            self._send_batch(self.pending_results)
            self.pending_results = []

    def pytest_sessionfinish(self, session, exitstatus):
        if self.pending_results:
            self._send_batch(self.pending_results)
            self.pending_results = []


def pytest_addoption(parser):
    group = parser.getgroup("test_filtering", "Test filtering options")
    group.addoption(
        "--test-filter",
        "--isolated-script-test-filter",
        "--gtest_filter",
        "--gtest-filter",
        action="append",
        default=[],
        dest="test_filters",
        help="Test filters",
    )
    group.addoption(
        "--test-filter-file",
        "--isolated-script-test-filter-file",
        action="append",
        default=[],
        dest="test_filter_files",
        help="Path to test filter file",
    )


def pytest_collection_modifyitems(session, config, items):
    identities = config.stash.get(_REPEAT_IDENTITIES, {})
    for item in items:
        callspec = getattr(item, "callspec", None)
        if (
            callspec is not None
            and _REPEAT_PARAMETER in callspec.params
            and item.nodeid not in identities
        ):
            raise pytest.UsageError(
                f"Cannot determine pytest-repeat identity for {item.nodeid}; "
                "collected ID differs from parametrization metadata"
            )
        # Public pytest properties travel into setup/call reports, including
        # serialized reports. Keep raw node IDs unchanged for pytest consumers.
        item.user_properties.append(
            (_NODE_ID_PROPERTY, identities.get(item.nodeid, item.nodeid))
        )

    test_filters = list(config.getoption("test_filters") or [])
    test_filter_files = list(config.getoption("test_filter_files") or [])

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

    if not test_filters and not test_filter_files:
        return

    filter_groups = []
    for f_str in test_filters:
        tokens = parse_filter_tokens(f_str)
        if tokens:
            filter_groups.append(TestFilterGroup([TestFilter(t) for t in tokens]))

    for f_path in test_filter_files:
        if os.path.exists(f_path):
            group = TestFilterGroup.from_filter_file(f_path)
            if group.filters:
                filter_groups.append(group)

    if not filter_groups:
        return

    kept_items = []
    for item in items:
        clean_nodeid = identities.get(item.nodeid, item.nodeid)
        structured_id, _ = format_test_id(clean_nodeid)
        raw_structured_id, _ = format_test_id(item.nodeid)
        file_path = clean_nodeid.split("::")[0]
        func_name = item.name
        if clean_nodeid != item.nodeid:
            # A parameter ID itself may contain "::"; it is not a node boundary.
            func_name = clean_nodeid.removeprefix(f"{item.parent.nodeid}::")

        included = all(
            group.is_test_included(
                structured_id,
                clean_nodeid,
                file_path,
                func_name,
                (raw_structured_id, item.nodeid, file_path, item.name),
            )
            for group in filter_groups
        )
        if included:
            kept_items.append(item)

    items[:] = kept_items


def pytest_configure(config):
    config.pluginmanager.register(ResultSinkReporter(), "resultsink_reporter_plugin")
