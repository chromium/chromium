# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script that is used by PRESUBMIT.py to run style checks on Java files."""

import collections
import os
import subprocess
import sys
import xml.dom.minidom


_SELF_DIR = os.path.dirname(__file__)
CHROMIUM_SRC = os.path.normpath(os.path.join(_SELF_DIR, '..', '..', '..'))
_CHECKSTYLE_ROOT = os.path.join(CHROMIUM_SRC, 'third_party', 'checkstyle',
                                'checkstyle-all.jar')
_JAVA_PATH = os.path.join(CHROMIUM_SRC, 'third_party', 'jdk', 'current', 'bin',
                          'java')
_STYLE_FILE = os.path.join(_SELF_DIR, 'chromium-style-5.0.xml')
_REMOVE_UNUSED_IMPORTS_PATH = os.path.join(_SELF_DIR,
                                           'remove_unused_imports.py')
_INCLUSIVE_WARNING_IDENTIFIER = 'Please use inclusive language'


class Violation(
        collections.namedtuple('Violation',
                               'file,line,column,message,severity')):
    def __str__(self):
        column = f'{self.column}:' if self.column else ''
        return f'{self.file}:{self.line}:{column} {self.message}'

    def is_warning(self):
        return self.severity == 'warning'

    def is_error(self):
        return self.severity == 'error'


def run_checkstyle(local_path, style_file, java_files):
    cmd = [
        _JAVA_PATH, '-cp', _CHECKSTYLE_ROOT,
        'com.puppycrawl.tools.checkstyle.Main', '-c', style_file, '-f', 'xml'
    ] + java_files
    check = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    stdout = check.communicate()[0].decode('utf-8')
    results = []

    lines = stdout.splitlines(keepends=True)
    if 'Checkstyle ends with' in lines[-1]:
        stdout = ''.join(lines[:-1])
    try:
        root = xml.dom.minidom.parseString(stdout)
    except Exception:
        print('Tried to parse:')
        print(stdout)
        raise

    inclusive_files = []
    inclusive_warning = ''
    for fileElement in root.getElementsByTagName('file'):
        filename = fileElement.attributes['name'].value
        if filename.startswith(local_path):
            filename = filename[len(local_path) + 1:]
        errors = fileElement.getElementsByTagName('error')
        for error in errors:
            severity = error.attributes['severity'].value
            if severity not in ('warning', 'error'):
                continue
            message = error.attributes['message'].value
            line = int(error.attributes['line'].value)
            column = None
            if error.hasAttribute('column'):
                column = int(error.attributes['column'].value)
            if _INCLUSIVE_WARNING_IDENTIFIER in message:
                inclusive_warning = message
                inclusive_files.append(f'{filename}:{str(line)}\n  ')
                continue
            results.append(Violation(filename, line, column, message,
                                     severity))

    if inclusive_files:
        results.append(
            Violation(
                ''.join(str(filename) for filename in inclusive_files) + '\n',
                '  ^^^ The above edited file(s) contain non-inclusive language (may be pre-existing). ^^^  ',
                '', inclusive_warning, 'warning'))

    return results


def run_presubmit(input_api, output_api, files_to_skip=None):
    # Android toolchain is only available on Linux.
    if not sys.platform.startswith('linux'):
        return []

    # Filter out non-Java files and files that were deleted.
    java_files = [
        x.AbsoluteLocalPath() for x in
        input_api.AffectedSourceFiles(lambda f: input_api.FilterSourceFile(
            f, files_to_skip=files_to_skip)) if x.LocalPath().endswith('.java')
    ]
    if not java_files:
        return []

    local_path = input_api.PresubmitLocalPath()
    violations = run_checkstyle(local_path, _STYLE_FILE, java_files)
    warnings = ['  ' + str(v) for v in violations if v.is_warning()]
    errors = ['  ' + str(v) for v in violations if v.is_error()]

    ret = []
    if warnings:
        ret.append(output_api.PresubmitPromptWarning('\n'.join(warnings)))
    if errors:
        msg = '\n'.join(errors)
        if 'Unused import:' in msg or 'Duplicate import' in msg:
            msg += """

To remove unused imports: """ + input_api.os_path.relpath(
                _REMOVE_UNUSED_IMPORTS_PATH, local_path)
        ret.append(output_api.PresubmitError(msg))
    return ret
