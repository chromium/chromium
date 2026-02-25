# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import sys

import utils.command_util as command
import utils.constants as const


# A persistent cache to avoid running gn on repeated runs of autotest.
class TargetCache:

  def __init__(self, out_dir: str) -> None:
    self.out_dir = out_dir
    self.path: str = os.path.join(out_dir, 'autotest_cache')
    self.gold_mtime: float = self.GetBuildNinjaMtime()
    self.cache: dict[str, list[str]] = {}
    try:
      mtime, cache = json.load(open(self.path, 'r'))
      if mtime == self.gold_mtime:
        self.cache = cache
    except Exception:
      pass

  def Save(self) -> None:
    with open(self.path, 'w') as f:
      json.dump([self.gold_mtime, self.cache], f)

  def Find(self, test_paths: list[str]) -> list[str] | None:
    key: str = ' '.join(test_paths)
    return self.cache.get(key, None)

  def Store(self, test_paths: list[str], test_targets: list[str]) -> None:
    key: str = ' '.join(test_paths)
    self.cache[key] = test_targets

  def GetBuildNinjaMtime(self) -> float:
    return os.path.getmtime(os.path.join(self.out_dir, 'build.ninja'))

  def IsStillValid(self) -> bool:
    return self.GetBuildNinjaMtime() == self.gold_mtime


def _TestTargetsFromGnRefs(targets: list[str]) -> list[str]:
  # Prevent repeated targets.
  all_test_targets: set[str] = set()

  # Find "standard" targets (e.g., GTests).
  standard_targets: list[str] = [t for t in targets if '__' not in t]
  standard_targets = [
      t for t in standard_targets if t.endswith(const.TEST_TARGET_SUFFIXES)
      or t in const.TEST_TARGET_ALLOWLIST
  ]
  all_test_targets.update(standard_targets)

  # Find targets using internal GN suffixes (e.g., Java APKs).
  _SUBTARGET_SUFFIXES = (
      '__java_binary',  # robolectric_binary()
      '__test_runner_script',  # test() targets
      '__test_apk',  # instrumentation_test_apk() targets
  )
  for suffix in _SUBTARGET_SUFFIXES:
    all_test_targets.update(t[:-len(suffix)] for t in targets
                            if t.endswith(suffix))

  return sorted(list(all_test_targets))


def _ParseRefsOutput(output: str) -> list[str]:
  targets: list[str] = output.splitlines()
  # Filter out any warnings messages. E.g. those about unused GN args.
  # https://crbug.com/444024516
  targets = [t for t in targets if t.startswith('//')]
  return targets


def FindTestTargets(target_cache: TargetCache,
                    out_dir: str,
                    paths: list[str],
                    run_all: bool = False,
                    run_changed: bool = False,
                    target_index: int | None = None) -> tuple[list[str], bool]:
  run_all: bool = run_all or run_changed

  # Normalize paths, so they can be cached.
  paths = [os.path.realpath(p) for p in paths]
  test_targets: list[str] | None = target_cache.Find(paths)
  used_cache: bool = True
  if not test_targets:
    used_cache = False

    # Use gn refs to recursively find all targets that depend on |path|, filter
    # internal gn targets, and match against well-known test suffixes, falling
    # back to a list of known test targets if that fails.
    gn_path: str = os.path.join(str(const.DEPOT_TOOLS_DIR), 'gn.py')

    cmd: list[str] = [
        sys.executable,
        gn_path,
        'refs',
        out_dir,
        '--all',
        '--relation=source',
        '--relation=input',
    ] + paths
    targets: list[str] = _ParseRefsOutput(command.RunCommand(cmd))
    test_targets = _TestTargetsFromGnRefs(targets)

    # If no targets were identified as tests by looking at their names, ask GN
    # if any are executables.
    if not test_targets and targets:
      test_targets = _ParseRefsOutput(
          command.RunCommand(cmd + ['--type=executable']))

  if not test_targets:
    command.ExitWithMessage(
        f'"{paths}" did not match any test targets. Consider adding'
        f' one of the following targets to _TEST_TARGET_ALLOWLIST within '
        f'{__file__}: \n' + '\n'.join(targets))

  test_targets.sort()
  target_cache.Store(paths, test_targets)
  target_cache.Save()

  if len(test_targets) > 1:
    if run_all:
      print(f'Warning, found {len(test_targets)} test targets.',
            file=sys.stderr)
      if len(test_targets) > 10:
        command.ExitWithMessage('Your query likely involves non-test sources.')
      print('Trying to run all of them!', file=sys.stderr)
    elif target_index is not None and 0 <= target_index < len(test_targets):
      test_targets = [test_targets[target_index]]
    else:
      test_targets = [command.HaveUserPickTarget(paths, test_targets)]

  # Remove the // prefix to turn GN label into ninja target.
  test_targets_gn: list[str] = [t[2:] for t in test_targets]

  return (test_targets_gn, used_cache)
