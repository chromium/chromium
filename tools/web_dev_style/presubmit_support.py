# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from . import html_checker
from . import js_checker
from . import resource_checker
from . import added_js_files_check


def IsResource(f):
  return f.LocalPath().endswith(('.html', '.css', '.js', '.ts'))


def CheckStyle(input_api, output_api, file_filter=lambda f: True):
  apis = input_api, output_api
  wrapped_filter = lambda f: file_filter(f) and IsResource(f)
  checkers = [
      html_checker.HtmlChecker(*apis, file_filter=wrapped_filter),
      js_checker.JSChecker(*apis, file_filter=wrapped_filter),
      resource_checker.ResourceChecker(*apis, file_filter=wrapped_filter),
  ]
  results = []
  for checker in checkers:
    results.extend(checker.RunChecks())
  return results


def CheckStyleESLint(input_api, output_api):
  should_check = lambda f: f.LocalPath().endswith(('.js', '.ts'))
  files_to_check = input_api.AffectedFiles(file_filter=should_check,
                                           include_deletes=False)
  if not files_to_check:
    return []
  return js_checker.JSChecker(input_api,
                              output_api).RunEsLintChecks(files_to_check)


def DisallowIncludes(input_api, output_api, msg):
  return resource_checker.ResourceChecker(
      input_api, output_api, file_filter=IsResource).DisallowIncludes(msg)


def DisallowNewJsFiles(input_api, output_api, file_filter=lambda f: True):
  return added_js_files_check.AddedJsFilesCheck(input_api,
                                                output_api,
                                                file_filter=file_filter)
