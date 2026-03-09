# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import os
import pathlib
import re
import platform

import shutil
import subprocess
import tempfile
import itertools
from typing import Dict, List, Generator

_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_ROOT = _THIS_DIR.parent.parent.parent

# This file lists all preexisting errors that were in the code when this
# check was added. Long term we want to clean that list.
# TODO(crbug.com/484881364): Fix this list.
_IGNORE_LIST_PATH = _SRC_ROOT.joinpath(
    'tools/metrics/python_support/mypy_ignore_list.txt')

# Specs of venv paths are applied in order of the list
_VPYTHON_SPEC_FILES: List[pathlib.Path] = [
    _SRC_ROOT.joinpath("tools/metrics/python_support/mypy_helpers_vpython_spec")
]

# In order to resolve imports coming from the setup_modules.py and its tooling
# stubs directory is created and symlinked to files using their "fake" paths,
# that way mypy can resolve them.
# The keys in the mapping are paths relative to stubs directory to create
# the values are the paths relative to src/ to symlink.
_STUB_MAPPINGS: Dict[str, str] = {
    'chromium_src': '.',
    'setup_modules_lib.py': 'tools/metrics/python_support/setup_modules_lib.py',
    'setup_modules.py': 'tools/metrics/setup_modules.py',
    'typ': 'third_party/catapult/third_party/typ',
}

# List of regex patterns for errors to ignore.
_IGNORED_ERRORS: List[str] = [
    # Ignore issues in chromium_src/tools/metrics as they are duplicates
    # of what we already have through analyzing "local" files.
    r"^.*chromium_mypy_stubs_.*chromium_src.tools.metrics.*$",

    # Ignore all third party errors.
    # TODO(crbug.com/484881364): Treat them as warning instead.
    r"^.*chromium_mypy_stubs_.*chromium_src.third_party.*$",

    # TODO(crbug.com/484881364): Fix this.
    r'^.*chromium_src.components.autofill.core.browser.data_model',
]

_IGNORED_ERRORS_PATTERNS = [re.compile(p) for p in _IGNORED_ERRORS]


def _convert_to_windows_ignore_line_if_needed(ignore_file_entry: str) -> str:
  """Converts the line from mypy_ignore_list.txt fixing paths on Windows."""
  if platform.system() != 'Windows':
    return ignore_file_entry

  # Replace slashes with backslashes in the first part of each line which
  # is a path.
  parts = ignore_file_entry.split(':', 1)
  parts[0] = parts[0].replace('/', '\\')
  return ':'.join(parts)



def _create_symlink(target: pathlib.Path, link_name: pathlib.Path) -> None:
  """Creates a symlink at link_name pointing to target."""
  if platform.system() == 'Windows':
    # Windows requires mklink to create junction or hard link to not require
    # admin rights.
    absolute_target = str(target.resolve())
    absolute_link = str(link_name.resolve())
    if target.is_dir():
      mklink = subprocess.run(
          ['cmd', '/c', 'mklink', '/j', absolute_link, absolute_target])
    else:
      mklink = subprocess.run(
          ['cmd', '/c', 'mklink', '/H', absolute_link, absolute_target])
    _ = mklink.returncode  # wait for it to finish
  else:
    os.symlink(target, link_name)


@contextlib.contextmanager
def _setup_stubs() -> Generator[pathlib.Path, None, None]:
  """Setups a stub directories required for proper analysis of tools/metrics

  It is a context manager that creates a temporary directory and populates it
  with symlinks (or copies) based on _STUB_MAPPINGS.

  Yields:
    pathlib.Path: The absolute path to the temporary stub directory.
  """
  temp_dir = tempfile.mkdtemp(prefix='chromium_mypy_stubs_')
  temp_path = pathlib.Path(temp_dir)

  try:
    for stub_name, rel_src_path in _STUB_MAPPINGS.items():
      src_target = _SRC_ROOT.joinpath(rel_src_path)
      stub_link = temp_path.joinpath(stub_name)

      if not src_target.exists():
        continue

      stub_link.parent.mkdir(parents=True, exist_ok=True)
      _create_symlink(src_target, stub_link)

    yield temp_path

  finally:
    shutil.rmtree(temp_dir, ignore_errors=True)


def run_mypy(stub_dir: pathlib.Path, dir_to_check: str) -> List[str]:
  """Runs mypy using vpython3 with tools/metrics specific config.

  Args:
    stub_dir: The directory containing the stubs/symlinks.
    dir_to_check: Directory that mypy will analyse

  Returns:
    List[str]: A list of error lines returned by mypy.
  """
  env = os.environ.copy()
  env["MYPYPATH"] = str(stub_dir.resolve())

  spec_flags = [["-vpython-spec", f"{str(p)}"] for p in _VPYTHON_SPEC_FILES]

  cmd = [
      "vpython3",
      *itertools.chain(*spec_flags),
      "-m",
      "mypy",
      "--explicit-package-bases",
      "--no-error-summary",  # Easier to parse without the summary
      "--hide-error-context",  # Cleaner line-by-line filtering
      "--",
      dir_to_check
  ]

  result = subprocess.run(cmd,
                          env=env,
                          cwd=_SRC_ROOT,
                          capture_output=True,
                          text=True)

  output = result.stdout + result.stderr
  return output.splitlines()


def run_mypy_and_filter_irrelevant(dir_to_check: str) -> List[str]:
  """Runs mypy and reports finding after filtering exceptions.

  Sets up stubs, runs mypy, and filters the output based on _IGNORED_ERRORS
  and _IGNORE_LIST_PATH content.

  Returns:
    List[str]: The filtered list of error messages.
  """
  with open(_IGNORE_LIST_PATH, 'r') as f:
    ignored_errors = {
        _convert_to_windows_ignore_line_if_needed(error.strip())
        for error in f.readlines()
    }

  with _setup_stubs() as stub_dir:
    raw_errors = run_mypy(stub_dir, dir_to_check)

    ignored_errors_cnt = 0
    ignored_patterns = 0
    empty_lines = 0

    filtered_errors = []
    for line in raw_errors:
      line = line.strip()
      if not line:
        empty_lines += 1
        continue

      if any(pattern.search(line) for pattern in _IGNORED_ERRORS_PATTERNS):
        ignored_patterns += 1
        continue

      if line in ignored_errors:
        ignored_errors_cnt += 1
        continue

      filtered_errors.append(line)

    assert empty_lines == 0
    print(f"MyPy found {len(filtered_errors)} issues + {ignored_patterns}"
          f" ignored by pattern + {ignored_errors_cnt} ignored by list")
    return filtered_errors
