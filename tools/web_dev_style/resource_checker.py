# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Presubmit for Chromium HTML/CSS/JS resources. See chrome/browser/PRESUBMIT.py.
"""

from . import regex_check


class ResourceChecker(object):
  def __init__(self, input_api, output_api, file_filter=None):
    self.input_api = input_api
    self.output_api = output_api
    self.file_filter = file_filter

  def DeprecatedMojoBindingsCheck(self, line_number, line):
    return regex_check.RegexCheck(self.input_api.re, line_number, line,
        '(mojo_bindings\.js)', 'Please use mojo_bindings_lite.js in new code')

  def DisallowIncludeCheck(self, msg, line_number, line):
    return regex_check.RegexCheck(self.input_api.re, line_number, line,
        '^\s*(?:\/[\*\/])?\s*(<include)\s*src=', msg)

  # This is intentionally not included in RunChecks(). It's an optional check
  # that can be used from a PRESUBMIT.py in a directory that does not wish to
  # use <include> (i.e. uses a different bundling mechanism, does not grit
  # process, etc.).
  def DisallowIncludes(self, msg):
    check = lambda *args: self.DisallowIncludeCheck(msg, *args)
    return self._RunCheckOnAffectedFiles(check, 'Found resource errors in %s',
                                         is_error=True)

  def SelfClosingIncludeCheck(self, line_number, line):
    return regex_check.RegexCheck(self.input_api.re, line_number, line,
        '(</include>|<include.*/>)', 'Closing <include> tags is unnecessary.')

  def RunChecks(self):
    msg = 'Found resources style issues in %s'
    # TODO(crbug.com/40613816): is_error for Mojo check when -lite is majority?
    return self._RunCheckOnAffectedFiles(self.DeprecatedMojoBindingsCheck,
        msg, only_changed_lines=True) + \
        self._RunCheckOnAffectedFiles(self.SelfClosingIncludeCheck, msg)

  def _RunCheckOnAffectedFiles(self, check, msg_template, is_error=False,
                               only_changed_lines=False):
    """Check for violations of the Chromium web development style guide. See
       https://chromium.googlesource.com/chromium/src/+/main/styleguide/web/web.md
    """
    results = []

    affected_files= self.input_api.AffectedFiles(file_filter=self.file_filter,
                                                 include_deletes=False)
    for f in affected_files:
      errors = []
      if only_changed_lines:
        contents = f.ChangedContents()
      else:
        contents = enumerate(f.NewContents(), start=1)
      for line_number, line in contents:
        error = check(line_number, line)
        if error:
          errors.append(error)

      if errors:
        abs_local_path = f.AbsoluteLocalPath()
        msg = msg_template % abs_local_path + '\n\n' + '\n'.join(errors) + '\n'
        if is_error:
          results.append(self.output_api.PresubmitError(msg))
        else:
          results.append(self.output_api.PresubmitPromptWarning(msg))

    return results
