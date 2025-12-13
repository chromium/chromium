#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Integration tests for licenses.py."""

import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest

_THIS_DIR = pathlib.Path(__file__).parent
_LICENSES_PY = _THIS_DIR.parent / 'licenses.py'
_GOLDENS_DIR = _THIS_DIR / 'goldens'


class LicensesIntegrationTest(unittest.TestCase):
  maxDiff = None  # Causes assertEquals() to display full diff.

  @classmethod
  def setUpClass(cls):
    # Remove stale .golden files.
    if os.environ.get('REBASELINE') == '1':
      for f in _GOLDENS_DIR.glob('*.golden'):
        f.unlink()

    # Mirror mock_root into a tempdir so that we can remove the ".test"
    # extension on README.chromium.test.
    # Use cwd to avoid relpath failures on windows when drive letter changes.
    # Prefix needed for deterministic sort order in depfile.
    cls._temp_dir = pathlib.Path(tempfile.mkdtemp(dir=os.getcwd(), prefix='Z'))
    out_dir = cls._temp_dir / 'out'
    out_dir.mkdir()
    (out_dir / 'args.gn').touch()

    cls._mock_root_dir = cls._temp_dir / 'mock_root'
    shutil.copytree(_THIS_DIR / 'mock_root', cls._mock_root_dir)
    for root, _, files in os.walk(cls._mock_root_dir):
      for f in files:
        if f.endswith('.chromium.test'):
          p = pathlib.Path(root) / f
          p.rename(p.with_suffix(''))

  @classmethod
  def tearDownClass(cls):
    shutil.rmtree(cls._temp_dir)

  def _run_test(self,
                command,
                gn_target=None,
                use_depfile=False,
                extra_args=None):
    """Runs a test and compares the output to a golden file."""
    if command in ('scan', 'list'):
      out_path = None
    else:
      out_path = self._temp_dir / 'out' / 'test_output.txt'
      out_path.unlink(missing_ok=True)

    env = os.environ.copy()

    args = [
        sys.executable,
        str(_LICENSES_PY),
        command,
        '--scan-root',
        str(self._mock_root_dir),
        '--extra-third-party-dirs=["test_dir_invalid_metadata","extra_dir"]',
    ]

    if gn_target:
      # The gn binary is faked out.
      env['LICENSES_GN_PATH'] = str(_THIS_DIR / 'mock_gn.py')
      args += [
          '--gn-target',
          gn_target,
          '--gn-out-dir',
          str(self._temp_dir / 'out'),
      ]

    if use_depfile:
      depfile_path = self._temp_dir / 'out' / 'depfile.d'
      args += ['--depfile', str(depfile_path)]

    if extra_args:
      args.extend(extra_args)

    # The `scan` and `list` commands do not take an output file argument.
    if out_path:
      args.append(str(out_path))

    result = subprocess.run(args,
                            check=False,
                            env=env,
                            capture_output=True,
                            encoding='utf-8')
    output = f"""\
exit code: {result.returncode}
stdout:
{result.stdout}\
stderr:
{result.stderr}"""

    if out_path:
      if out_path.exists():
        output += 'output:\n' + out_path.read_text(encoding='utf-8')
      else:
        output += 'OUTPUT FILE WAS MISSING\n'
    if use_depfile:
      if depfile_path.exists():
        output += 'depfile:\n' + depfile_path.read_text(encoding='utf-8')
      else:
        output += 'DEPFILE FILE WAS MISSING\n'

    # The temp dir changes between runs, so replace it with a stable
    # placeholder (happens in depfile).
    output = output.replace(str(os.path.relpath(self._temp_dir)), '{REL_TMP}')
    # Normalize slashes so goldens work Windows hosts.
    output = output.replace('\\\\', '/')  # Escaped slashes
    output = output.replace('\\', '/')  # Normal slashes
    # Undo slash translation where it's obvious it should be a \.
    output = output.replace('/n"', '\\n"').replace('/\n', '\\\n')

    golden_path = _GOLDENS_DIR / f'{self._testMethodName}.golden'
    if os.environ.get('REBASELINE') == '1':
      golden_path.write_text(output, 'utf-8')
      return

    golden_data = golden_path.read_text(encoding='utf-8')
    self.assertEqual(golden_data, output)

  def test_list(self):
    self._run_test('list')

  def test_list_verbose(self):
    self._run_test('list', extra_args=['--verbose'])

  def test_list_verbose_shipped_only(self):
    self._run_test('list', extra_args=['--verbose', '--shipped-only'])

  def test_list_with_gn_target(self):
    self._run_test('list', gn_target='//foo')

  def test_license_file_txt(self):
    self._run_test(
        'license_file',
        extra_args=['--format=spdx', '--spdx-root',
                    str(self._mock_root_dir)])

  def test_license_file_spdx(self):
    self._run_test(
        'license_file',
        extra_args=['--format=spdx', '--spdx-root=' + str(self._mock_root_dir)])

  def test_license_file_csv(self):
    self._run_test('license_file', extra_args=['--format=csv'])

  def test_license_file_notice(self):
    self._run_test('license_file', extra_args=['--format=notice'])

  def test_scan(self):
    self._run_test('scan')

  def test_credits(self):
    self._run_test('credits', use_depfile=True)

  def test_credits_with_warnings(self):
    self._run_test('credits', extra_args=['--enable-warnings'])


if __name__ == '__main__':
  unittest.main()
