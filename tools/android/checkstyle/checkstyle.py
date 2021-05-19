# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script that is used by PRESUBMIT.py to run style checks on Java files."""

import os
import subprocess
import sys
import xml.dom.minidom


CHROMIUM_SRC = os.path.normpath(
    os.path.join(os.path.dirname(__file__),
                 os.pardir, os.pardir, os.pardir))
CHECKSTYLE_ROOT = os.path.join(CHROMIUM_SRC, 'third_party', 'checkstyle',
                               'checkstyle-all.jar')
JAVA_PATH = os.path.join(CHROMIUM_SRC, 'third_party', 'jdk', 'current', 'bin',
                         'java')


def FormatCheckstyleOutput(checkstyle_output):
  lines = checkstyle_output.splitlines(True)
  if 'Checkstyle ends with' in lines[-1]:
    return ''.join(lines[:-1])
  else:
    return checkstyle_output


def RunCheckstyle(input_api, output_api, style_file, files_to_skip=None):
  # Android toolchain is only available on Linux.
  if not sys.platform.startswith('linux'):
    return []

  if not os.path.exists(style_file):
    file_error = ('  Java checkstyle configuration file is missing: '
                  + style_file)
    return [output_api.PresubmitError(file_error)]

  # Filter out non-Java files and files that were deleted.
  java_files = [
      x.AbsoluteLocalPath() for x in input_api.AffectedSourceFiles(
          lambda f: input_api.FilterSourceFile(f, files_to_skip=files_to_skip))
      if os.path.splitext(x.LocalPath())[1] == '.java'
  ]
  if not java_files:
    return []

  # Run checkstyle
  check = subprocess.Popen([JAVA_PATH, '-cp',
                            CHECKSTYLE_ROOT,
                            'com.puppycrawl.tools.checkstyle.Main', '-c',
                            style_file, '-f', 'xml'] + java_files,
                            stdout=subprocess.PIPE)
  stdout = check.communicate()[0].decode('utf-8')

  result_errors = []
  result_warnings = []

  formatted_checkstyle_output = FormatCheckstyleOutput(stdout)

  local_path = input_api.PresubmitLocalPath()
  root = xml.dom.minidom.parseString(formatted_checkstyle_output)
  for fileElement in root.getElementsByTagName('file'):
    fileName = fileElement.attributes['name'].value
    fileName = os.path.relpath(fileName, local_path)
    errors = fileElement.getElementsByTagName('error')
    for error in errors:
      line = error.attributes['line'].value
      column = ''
      if error.hasAttribute('column'):
        column = '%s:' % (error.attributes['column'].value)
      message = error.attributes['message'].value
      result = '  %s:%s:%s %s' % (fileName, line, column, message)

      severity = error.attributes['severity'].value
      if severity == 'error':
        result_errors.append(result)
      elif severity == 'warning':
        result_warnings.append(result)

  result = []
  if result_warnings:
    result.append(output_api.PresubmitPromptWarning('\n'.join(result_warnings)))
  if result_errors:
    result.append(output_api.PresubmitError('\n'.join(result_errors)))
  return result
