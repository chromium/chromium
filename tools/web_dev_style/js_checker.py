# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for Chromium JS resources.

See chrome/browser/PRESUBMIT.py
"""

from . import regex_check


class JSChecker(object):
  def __init__(self, input_api, output_api, file_filter=None):
    self.input_api = input_api
    self.output_api = output_api
    self.file_filter = file_filter

  def RegexCheck(self, line_number, line, regex, message):
    return regex_check.RegexCheck(
        self.input_api.re, line_number, line, regex, message)

  def BindThisCheck(self, i, line):
    """Checks for usages of bind(this) with inlined functions."""
    return self.RegexCheck(i, line, r"\)(\.bind\(this)[^)]*\)",
                           "Prefer arrow (=>) functions over bind(this)")

  def ChromeSendCheck(self, i, line):
    """Checks for a particular misuse of "chrome.send"."""
    return self.RegexCheck(i, line, r"chrome\.send\('[^']+'\s*(, \[\])\)",
        "Passing an empty array to chrome.send is unnecessary")

  def CommentIfAndIncludeCheck(self, line_number, line):
    return self.RegexCheck(line_number, line, r"(?<!\/\/ )(<if|<include) ",
        "<if> or <include> should be in a single line comment with a space " +
        "after the slashes. Examples:\n" +
        '    // <include src="...">\n' +
        '    // <if expr="chromeos">\n' +
        "    // </if>\n")

  def EndJsDocCommentCheck(self, i, line):
    msg = "End JSDoc comments with */ instead of **/"
    def _check(regex):
      return self.RegexCheck(i, line, regex, msg)
    return _check(r"^\s*(\*\*/)\s*$") or _check(r"/\*\* @[a-zA-Z]+.* (\*\*/)")

  def ExtraDotInGenericCheck(self, i, line):
    return self.RegexCheck(i, line, r"((?:Array|Object|Promise)\.<)",
        "Don't use a dot after generics (Object.<T> should be Object<T>).")

  def InheritDocCheck(self, i, line):
    """Checks for use of "@inheritDoc" instead of "@override"."""
    return self.RegexCheck(i, line, r"\* (@inheritDoc)",
        "@inheritDoc is deprecated, use @override instead")

  def PolymerLocalIdCheck(self, i, line):
    """Checks for use of element.$.localId."""
    return self.RegexCheck(i, line, r"(?<!this)(\.\$)[\[\.](?![a-zA-Z]+\()",
        "Please only use this.$.localId, not element.$.localId")

  def RunEsLintChecks(self, affected_js_files, format="stylish"):
    """Runs lint checks using ESLint. The ESLint rules being applied are defined
       in the .eslintrc.js configuration file.
    """
    os_path = self.input_api.os_path

    # Extract paths to be passed to ESLint.
    affected_js_files_paths = []
    for f in affected_js_files:
      affected_js_files_paths.append(f.AbsoluteLocalPath())

    from os import isatty as os_isatty
    parameters = ["--color"] if os_isatty(
        self.input_api.sys.stdout.fileno()) else []
    parameters += ["--format", format, "--ignore-pattern", "!.eslintrc.js"]
    from . import eslint

    # When running git cl presubmit --all this presubmit may be asked to check
    # ~1,100 files, leading to a command line that is about 92,000 characters.
    # This goes past the Windows 8191 character cmd.exe limit and causes cryptic
    # failures. To avoid these we break the command up into smaller pieces. The
    # non-Windows limit is chosen so that the code that splits up commands will
    # get some exercise on other platforms.
    # Depending on how long the command is on Windows the error may be:
    #     The command line is too long.
    # Or it may be:
    #     OSError: Execution failed with error: [WinError 206] The filename or
    #     extension is too long.
    # The latter error comes from CreateProcess hitting its 32768 character
    # limit.
    files_per_command = 25 if self.input_api.is_windows else 1000
    results = []
    for i in range(0, len(affected_js_files_paths), files_per_command):
      args = parameters + affected_js_files_paths[i:i + files_per_command]

      try:
        output = eslint.Run(os_path=os_path, args=args)
      except RuntimeError as err:
        results.append(self.output_api.PresubmitError(str(err)))
    return results

  def VariableNameCheck(self, i, line):
    """See the style guide. http://goo.gl/eQiXVW"""
    return self.RegexCheck(i, line,
        r"(?:var|let|const) (?!g_\w+)(_?[a-z][a-zA-Z]*[_$][\w_$]*)(?<! \$)",
        "Please use variable namesLikeThis <https://goo.gl/eQiXVW>")

  def _GetErrorHighlight(self, start, length):
    """Takes a start position and a length, and produces a row of "^"s to
       highlight the corresponding part of a string.
    """
    return start * " " + length * "^"

  def RunChecks(self):
    """Check for violations of the Chromium JavaScript style guide. See
       https://chromium.googlesource.com/chromium/src/+/main/styleguide/web/web.md#JavaScript
    """
    results = []

    affected_files = self.input_api.AffectedFiles(file_filter=self.file_filter,
                                                  include_deletes=False)
    affected_js_files = [
        f for f in affected_files if f.LocalPath().endswith((".js", ".ts"))
    ]

    if affected_js_files:
      results += self.RunEsLintChecks(affected_js_files)

    for f in affected_js_files:
      error_lines = []

      for i, line in f.ChangedContents():
        error_lines += [
            _f for _f in [
                self.BindThisCheck(i, line),
                self.ChromeSendCheck(i, line),
                self.EndJsDocCommentCheck(i, line),
                self.ExtraDotInGenericCheck(i, line),
                self.InheritDocCheck(i, line),
                self.PolymerLocalIdCheck(i, line),
                self.VariableNameCheck(i, line),
            ] if _f
        ]

      if not f.LocalPath().endswith((".html.js", ".html.ts")):
        # Exclude JS/TS files holding HTML strings from
        # CommentIfAndIncludeCheck().
        for i, line in f.ChangedContents():
          error_lines += [
              _f for _f in [
                  self.CommentIfAndIncludeCheck(i, line),
              ] if _f
          ]

      if error_lines:
        error_lines = [
            "Found JavaScript style violations in %s:" %
            f.LocalPath()] + error_lines
        results.append(self.output_api.PresubmitError("\n".join(error_lines)))

    if results:
      results.append(self.output_api.PresubmitNotifyResult(
          "See the JavaScript style guide at "
          "https://chromium.googlesource.com/chromium/src/+/main/styleguide/web/web.md#JavaScript"))

    return results
