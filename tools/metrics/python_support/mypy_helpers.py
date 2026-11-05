# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import os
import pathlib
import platform
import sys

import shutil
import subprocess
import tempfile
import itertools
from typing import Dict, List, Generator

_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_ROOT = _THIS_DIR.parent.parent.parent

# Decides which findings are relevant for tools/metrics, e.g. it silences
# errors inside vendored code.
_CONFIG_PATH = _SRC_ROOT.joinpath('tools/metrics/python_support/mypy.ini')

# Specs of venv paths are applied in order of the list
_VPYTHON_SPEC_FILES: List[pathlib.Path] = [
  _SRC_ROOT.joinpath('tools/metrics/python_support/vpython.toml')
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


def _create_symlink(target: pathlib.Path, link_name: pathlib.Path) -> None:
  """Creates a symlink at link_name pointing to target."""
  if platform.system() == 'Windows':
    # Windows requires mklink to create junction or hard link to not require
    # admin rights.
    absolute_target = str(target.resolve())
    absolute_link = str(link_name.resolve())
    if target.is_dir():
      mklink = subprocess.run(
        ['cmd', '/c', 'mklink', '/j', absolute_link, absolute_target]
      )
    else:
      mklink = subprocess.run(
        ['cmd', '/c', 'mklink', '/H', absolute_link, absolute_target]
      )
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
  env['MYPYPATH'] = str(stub_dir.resolve())

  spec_flags = [['-vpython-spec', f'{str(p)}'] for p in _VPYTHON_SPEC_FILES]

  cmd = [
    'vpython3',
    *itertools.chain(*spec_flags),
    '-m',
    'mypy',
    '--config-file',
    str(_CONFIG_PATH),
    '--explicit-package-bases',
    '--no-error-summary',  # Easier to parse without the summary
    '--hide-error-context',  # Cleaner line-by-line filtering
    '--',
    dir_to_check,
  ]

  result = subprocess.run(
    cmd,
    env=env,
    cwd=_SRC_ROOT,
    capture_output=True,
    shell=sys.platform == 'win32',
    text=True,
  )

  output = result.stdout + result.stderr
  return output.splitlines()


def run_mypy_and_filter_irrelevant(dir_to_check: str) -> List[str]:
  """Runs mypy and reports the findings relevant for tools/metrics.

  Sets up stubs and runs mypy, which filters out irrelevant findings according
  to _CONFIG_PATH.

  Returns:
    List[str]: The list of error messages.
  """
  with _setup_stubs() as stub_dir:
    errors = [line.strip() for line in run_mypy(stub_dir, dir_to_check)]

  assert all(errors)
  print(f'MyPy found {len(errors)} issues')
  return errors
