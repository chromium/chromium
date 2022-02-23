# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

USE_PYTHON3 = True


_IGNORE_FREEZE_FOOTER = 'Ignore-Freeze'

# The time module's handling of timezones is abysmal, so the boundaries are
# precomputed in UNIX time
_FREEZE_START = 1639641600  # 2021/12/16 00:00 -0800
_FREEZE_END = 1641196800  # 2022/01/03 00:00 -0800


def CheckFreeze(input_api, output_api):
  if _FREEZE_START <= input_api.time.time() < _FREEZE_END:
    footers = input_api.change.GitFootersFromDescription()
    if _IGNORE_FREEZE_FOOTER not in footers:

      def convert(t):
        ts = input_api.time.localtime(t)
        return input_api.time.strftime('%Y/%m/%d %H:%M %z', ts)

      return [
          output_api.PresubmitError(
              'There is a prod freeze in effect from {} until {},'
              ' files in //tools/mb cannot be modified'.format(
                  convert(_FREEZE_START), convert(_FREEZE_END)))
      ]

  return []


def CheckTests(input_api, output_api):
  glob = input_api.os_path.join(input_api.PresubmitLocalPath(), '*_test.py')
  tests = input_api.canned_checks.GetUnitTests(input_api,
                                               output_api,
                                               input_api.glob(glob),
                                               run_on_python2=False,
                                               run_on_python3=True,
                                               skip_shebang_check=True)
  return input_api.RunTests(tests)


def _CommonChecks(input_api, output_api):
  results = []

  # Run Pylint over the files in the directory.
  pylint_checks = input_api.canned_checks.GetPylint(
      input_api,
      output_api,
      version='2.7',
      # pylint complains about Checkfreeze not being defined, its probably
      # finding a different PRESUBMIT.py
      files_to_skip=['PRESUBMIT_test.py'],
      # Disabling certain python3-specific warnings until the conversion
      # is complete.
      disabled_warnings=[
          'super-with-arguments',
          'raise-missing-from',
          'useless-object-inheritance',
      ],
  )
  results.extend(input_api.RunTests(pylint_checks))

  # Run the MB unittests.
  results.extend(
      input_api.canned_checks.RunUnitTestsInDirectory(input_api,
                                                      output_api,
                                                      '.',
                                                      [r'^.+_unittest\.py$'],
                                                      skip_shebang_check=True))

  # Validate the format of the mb_config.pyl file.
  cmd = [input_api.python_executable, 'mb.py', 'validate']
  kwargs = {'cwd': input_api.PresubmitLocalPath()}
  results.extend(input_api.RunTests([
      input_api.Command(name='mb_validate',
                        cmd=cmd, kwargs=kwargs,
                        message=output_api.PresubmitError)]))

  results.extend(CheckFreeze(input_api, output_api))
  results.extend(CheckTests(input_api, output_api))

  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
