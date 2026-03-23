# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from dataclasses import dataclass, field
import json
import os

# TODO: b/494592883 - Upload results to rdb instead of telemetry.
# This can be done once RDB has wipeout support.


@dataclass
class TestSummary:
  test_count: int = 0
  failed_tests: list[tuple[str, str]] = field(default_factory=list)
  passed_tests: list[str] = field(default_factory=list)
  parse_error: str = ""

  def __str__(self) -> str:
    error_str = f"Parse error: {self.parse_error}\n\n" if self.parse_error else ""

    failed_lines = []
    m = len(self.failed_tests)

    passed_lines = []
    p = len(self.passed_tests)
    for i, name in enumerate(sorted(self.passed_tests)):
      passed_lines.append(f"[{i + 1}/{p}] {name}")

    passed = '\n'.join(passed_lines)

    for i, (name, log) in enumerate(sorted(self.failed_tests)):
      clean_log = (log or "").strip()
      indented_log = '    ' + clean_log.replace('\n', '\n    ')

      failed_lines.append(f"[{i + 1}/{m}] {name}\n{indented_log}")

    failed = '\n\n'.join(failed_lines)

    return (f"{error_str}"
            f"Test count: \n{self.test_count}\n\n"
            f"Passed tests:\n{passed}\n\n"
            f"Failed tests:\n{failed}")


def ParseTests(file_path: str) -> TestSummary:
  if not os.path.exists(file_path) or os.path.getsize(file_path) == 0:
    return TestSummary(parse_error="Summary file is missing or empty")

  try:
    with open(file_path, 'r') as f:
      data = json.load(f)
  except json.JSONDecodeError as e:
    return TestSummary(parse_error=f"Failed to parse summary file as JSON: {e}")

  failed_tests: list[tuple[str, str]] = list()
  passed_tests: list[str] = list()
  test_names: set[str] = set()
  count: int = 0

  for iteration in data.get('per_iteration_data', []):
    for test_name, results in iteration.items():
      for result in results:
        status = result.get('status')

        if (status in ('SUCCESS', 'PASS', 'SKIPPED')
            and test_name not in test_names):
          test_names.add(test_name)
          passed_tests.append(test_name)
        elif test_name not in test_names:
          test_names.add(test_name)
          logs: str = result.get('output_snippet')
          failed_tests.append((test_name, logs))
      count += 1

  return TestSummary(test_count=count,
                     failed_tests=failed_tests,
                     passed_tests=passed_tests)
