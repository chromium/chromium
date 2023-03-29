# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Presubmit for Chromium HTML resources. See chrome/browser/PRESUBMIT.py.
"""

from . import regex_check


class HtmlChecker(object):
  def __init__(self, input_api, output_api, file_filter=None):
    self.input_api = input_api
    self.output_api = output_api
    self.file_filter = file_filter

  def ClassesUseDashFormCheck(self, line_number, line):
    msg = "Classes should use dash-form."
    re = self.input_api.re
    class_regex = re.compile("""
        (?:^|\s)                    # start of line or whitespace
        (class="[^"]*[A-Z_][^"]*")  # class contains caps or '_'
        """,
        re.VERBOSE)

    # $i18n{...} messes with highlighting. Special path for this.
    if "$i18n{" in line:
      match = re.search(class_regex, re.sub("\$i18n{[^}]+}", "", line))
      return "  line %d: %s" % (line_number, msg) if match else ""

    return regex_check.RegexCheck(re, line_number, line, class_regex, msg)

  def DoNotCloseSingleTagsCheck(self, line_number, line):
    regex = r"(/>)"
    return regex_check.RegexCheck(self.input_api.re, line_number, line, regex,
        "Do not close single tags.")

  def DoNotUseBrElementCheck(self, line_number, line):
    regex = r"(<br\b)"
    return regex_check.RegexCheck(self.input_api.re, line_number, line, regex,
        "Do not use <br>; place blocking elements (<div>) as appropriate.")

  def DoNotUseInputTypeButtonCheck(self, line_number, line):
    regex = self.input_api.re.compile("""
        (<input [^>]*  # "<input " followed by anything but ">"
        type="button"  # type="button"
        [^>]*>)        # anything but ">" then ">"
        """,
        self.input_api.re.VERBOSE)
    return regex_check.RegexCheck(self.input_api.re, line_number, line, regex,
        'Use the button element instead of <input type="button">')

  def DoNotUseSingleQuotesCheck(self, line_number, line):
    regex = self.input_api.re.compile("""
        <\S+                           # The tag name.
        (?:\s+\S+\$?="[^"]*"|\s+\S+)*  # Correctly quoted or non-value props.
        \s+(\S+\$?='[^']*')            # Find incorrectly quoted (foo='bar').
        [^>]*>                         # To the end of the tag.
        """,
        self.input_api.re.MULTILINE | self.input_api.re.VERBOSE)
    return regex_check.RegexCheck(self.input_api.re, line_number, line, regex,
        'Use double quotes rather than single quotes in HTML properties')

  def I18nContentJavaScriptCaseCheck(self, line_number, line):
    regex = self.input_api.re.compile("""
        (?:^|\s)                      # start of line or whitespace
        i18n-content="                # i18n-content="
        ([A-Z][^"]*|[^"]*[-_][^"]*)"  # starts with caps or contains '-' or '_'
        """,
        self.input_api.re.VERBOSE)
    return regex_check.RegexCheck(self.input_api.re, line_number, line, regex,
        "For i18n-content use javaScriptCase.")

  def LabelCheck(self, line_number, line):
    regex = self.input_api.re.compile("""
        (?:^|\s)     # start of line or whitespace
        <label[^>]+? # <label tag
        (for=)       # for=
        """,
        self.input_api.re.VERBOSE)
    return regex_check.RegexCheck(self.input_api.re, line_number, line, regex,
        "Avoid 'for' attribute on <label>. Place the input within the <label>, "
        "or use aria-labelledby for <select>.")

  def QuotePolymerBindings(self, line_number, line):
    regex = self.input_api.re.compile(r"=(\[\[|\{\{)")
    return regex_check.RegexCheck(self.input_api.re, line_number, line, regex,
        'Please use quotes around Polymer bindings (i.e. attr="[[prop]]")')

  def RunChecks(self):
    """Check for violations of the Chromium web development style guide. See
       https://chromium.googlesource.com/chromium/src/+/main/styleguide/web/web.md
    """
    results = []

    affected_files = self.input_api.AffectedFiles(file_filter=self.file_filter,
                                                  include_deletes=False)

    for f in affected_files:
      if not f.LocalPath().endswith('.html'):
        continue

      errors = []

      for line_number, line in f.ChangedContents():
        errors.extend([
            _f for _f in [
                self.ClassesUseDashFormCheck(line_number, line),
                self.DoNotCloseSingleTagsCheck(line_number, line),
                self.DoNotUseBrElementCheck(line_number, line),
                self.DoNotUseInputTypeButtonCheck(line_number, line),
                self.I18nContentJavaScriptCaseCheck(line_number, line),
                self.LabelCheck(line_number, line),
                self.QuotePolymerBindings(line_number, line),
            ] if _f
        ])

      if errors:
        abs_local_path = f.AbsoluteLocalPath()
        file_indicator = 'Found HTML style issues in %s' % abs_local_path
        prompt_msg = file_indicator + '\n\n' + '\n'.join(errors) + '\n'
        results.append(self.output_api.PresubmitPromptWarning(prompt_msg))

    return results
