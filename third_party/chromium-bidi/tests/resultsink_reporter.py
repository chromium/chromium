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

import html
import json
import os
import urllib.request


class ResultSinkReporter:
    def __init__(self):
        self.sink_data = self._get_sink_data()
        self.pending_results = []
        self.batch_size = 50

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

        test_result = {
            "testId": report.nodeid.replace("\n", " ")[:512],
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


def pytest_configure(config):
    config.pluginmanager.register(ResultSinkReporter(), "resultsink_reporter_plugin")
