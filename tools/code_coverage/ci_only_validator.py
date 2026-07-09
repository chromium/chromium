#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tool to check if a test suite is barred by ci_only = True configuration."""

import argparse
import json
import pathlib
import subprocess
import sys
from typing import Any, Callable, NamedTuple


class TrybotEntry(NamedTuple):
  """Holds trybot name and target group name.

  Attributes:
    trybot: Name of the trybot.
    group: Target group name (e.g. chromium.coverage).
  """
  trybot: str
  group: str


class BuilderConfig(NamedTuple):
  """Holds builder name and its ci_only status.

  Attributes:
    builder: Name of the builder.
    ci_only: Boolean status indicating if the builder is ci_only.
  """
  builder: str
  ci_only: bool


def discover_trybots_for_test_suite(
    src_root: pathlib.Path,
    test_suite: str,
    revision: str | None = None,
) -> list[TrybotEntry]:
  """Discovers all trybots and target groups configuring the test suite.

  Args:
    src_root: The root directory of the chromium source checkout.
    test_suite: Name of the test suite to search for.
    revision: Optional git revision (commit hash/branch) to search at.

  Returns:
    A list of TrybotEntry NamedTuples.
  """
  # Match exact JSON pattern for the test suite name.
  pattern = f'\\"name\\": \\"{test_suite}\\"'
  cmd = [
      'git',
      'grep',
      '-l',
      pattern,
  ]
  if revision:
    cmd.append(revision)
  cmd.extend(['--', 'infra/config/generated/builders/try/*/targets/*.json'])

  result = subprocess.run(cmd,
                          cwd=src_root,
                          capture_output=True,
                          text=True,
                          check=False)
  if result.returncode != 0:
    return []

  trybots_and_groups = []
  for line in result.stdout.splitlines():
    line = line.strip()
    if not line:
      continue
    if revision and ':' in line:
      line = line.split(':', 1)[1]

    path = pathlib.Path(line)
    parts = path.parts
    trybot = parts[5]
    group = path.stem
    trybots_and_groups.append(TrybotEntry(trybot=trybot, group=group))

  return trybots_and_groups


def read_local(src_root: pathlib.Path, rel_path: str) -> str | None:
  """Reads a configuration file from the local disk.

  Args:
    src_root: The root directory of the chromium source checkout.
    rel_path: Relative path to the file from the source root.

  Returns:
    The file contents as a string, or None if the file was missing/unread.
  """
  file_path = src_root / rel_path
  if not file_path.exists():
    return None
  try:
    return file_path.read_text(encoding='utf-8')
  except OSError as e:
    print(f'Error reading local file {file_path}: {e}', file=sys.stderr)
    return None


def read_git(src_root: pathlib.Path, revision: str,
             rel_path: str) -> str | None:
  """Reads a configuration file from a git revision.

  Args:
    src_root: The root directory of the chromium source checkout.
    revision: The git revision (commit hash/branch) to query.
    rel_path: Relative path to the file from the source root.

  Returns:
    The file contents as a string, or None if the git command failed.
  """
  cmd = ['git', 'show', f'{revision}:{rel_path}']
  result = subprocess.run(cmd,
                          cwd=src_root,
                          capture_output=True,
                          text=True,
                          check=False)
  if result.returncode != 0:
    return None
  return result.stdout


def check_coverage_enabled(read_fn: Callable[[str], str | None],
                           trybot: str) -> bool:
  """Checks if coverage is enabled in the trybot's GN args.

  Args:
    read_fn: Callable that takes a relative path and returns file content.
    trybot: Name of the trybot to inspect.

  Returns:
    True if code coverage is enabled for Clang or JaCoCo, False otherwise.
  """
  gn_args_path = f'infra/config/generated/builders/try/{trybot}/gn-args.json'
  gn_args_content = read_fn(gn_args_path)
  if not gn_args_content:
    return False
  try:
    gn_args = json.loads(gn_args_content).get('gn_args', {})
    return bool(
        gn_args.get('use_clang_coverage', False)
        or gn_args.get('use_jacoco_coverage', False))
  except json.JSONDecodeError:
    return False


def check_trybot_coverage_config(
    read_fn: Callable[[str], str | None],
    trybot: str,
    group: str,
    test_suite: str,
) -> list[BuilderConfig]:
  """Checks target specs for a trybot to see if a test suite is ci_only.

  Args:
    read_fn: Callable that takes a relative path and returns file content.
    trybot: Name of the trybot to check.
    group: Target group name (e.g. chromium.coverage).
    test_suite: Name of the test suite to verify.

  Returns:
    A list of BuilderConfig NamedTuples.
  """
  target_spec_rel_path = (
      f'infra/config/generated/builders/try/{trybot}/targets/{group}.json')
  target_spec_content = read_fn(target_spec_rel_path)
  if not target_spec_content:
    return []

  try:
    target_spec = json.loads(target_spec_content)
  except json.JSONDecodeError as e:
    print(
        f'Error parsing target spec {target_spec_rel_path}: {e}',
        file=sys.stderr,
    )
    return []

  results = []
  for builder_name, builder_specs in target_spec.items():
    suite_found = False
    is_ci_only = False
    for test_type in [
        'gtest_tests',
        'isolated_scripts',
        'junit_tests',
        'scripts',
    ]:
      tests = builder_specs.get(test_type, [])
      for test in tests:
        if test.get('name') == test_suite:
          suite_found = True
          is_ci_only = test.get('ci_only', False)
          break
      if suite_found:
        break

    if suite_found:
      results.append(BuilderConfig(builder=builder_name, ci_only=is_ci_only))
  return results


def main() -> None:
  parser = argparse.ArgumentParser(
      description=('Check if a test suite is barred by ci_only = True on'
                   ' trybots.'))
  parser.add_argument(
      'test_suite',
      help='Name of the test suite (e.g. chrome_junit_tests)',
  )
  parser.add_argument(
      '--revision',
      '-r',
      help=('Git revision to check the configuration at (e.g., commit hash,'
            ' branch).'),
  )

  args = parser.parse_args()

  script_dir = pathlib.Path(__file__).resolve().parent
  src_root = script_dir.parents[1]

  if not src_root.exists():
    print(f'Error: Src path {src_root} does not exist', file=sys.stderr)
    sys.exit(1)

  if args.revision:
    # Check if the revision is valid
    res = subprocess.run(
        ['git', 'rev-parse', '--verify', args.revision],
        cwd=src_root,
        capture_output=True,
        text=True,
        check=False,
    )
    if res.returncode != 0:
      print(f'Error: Invalid revision: {args.revision}', file=sys.stderr)
      sys.exit(1)

  trybots_and_groups = discover_trybots_for_test_suite(src_root,
                                                       args.test_suite,
                                                       args.revision)
  if not trybots_and_groups:
    print(
        f"Warning: Test suite '{args.test_suite}' was not found on any"
        ' trybots.',
        file=sys.stderr,
    )
    sys.exit(0)

  if args.revision:
    read_fn = lambda path: read_git(src_root, args.revision, path)
  else:
    read_fn = lambda path: read_local(src_root, path)

  revision_msg = f' at revision {args.revision}' if args.revision else ''
  print(f"Checking test suite '{args.test_suite}' across"
        f' {len(trybots_and_groups)} configured trybots{revision_msg}...')

  for entry in sorted(trybots_and_groups, key=lambda e: e.trybot):
    trybot = entry.trybot
    group = entry.group
    configs = check_trybot_coverage_config(read_fn, trybot, group,
                                           args.test_suite)
    coverage_enabled = check_coverage_enabled(read_fn, trybot)

    for config in configs:
      status = 'ci_only = True' if config.ci_only else 'ci_only = False'
      coverage_status = ('Coverage: Enabled'
                         if coverage_enabled else 'Coverage: Disabled')
      print(f'  Trybot: {trybot:<30} | Builder: {group}:{config.builder:<30}'
            f' | {status:<15} | {coverage_status}')


if __name__ == '__main__':
  main()
