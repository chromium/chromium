# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'

from enum import Enum
import os
from pathlib import Path
import sys
import tempfile
from typing import Any, Callable, List, Type


# Cannot be called CheckType because by convention PRESUBMIT will try to call
# anything with a Check prefix as a function.
class HistogramsPresubmitCheckType(Enum):
  """Unique identifiers for the checks in this files.

  As this file contains multiple checks, we need to have unique identifiers for
  each of them to identify proper result set in the cache. This enum defines
  all the unique identifiers for the checks in this file.
  """
  BOOLS_ARE_ENUMS = 1
  ALL_ALLOWLIST_HISTOGRAMS_PRESENT = 2
  FORMATTING_VALIDATION = 3


_CACHE_FILE_PATH = os.path.join(tempfile.gettempdir(),
                                'histograms_presubmit_cache.json')


def _RunCheckWithCache(check_method: Callable[[Type, Type, Any], List[Any]],
                       check_id: int, input_api: type, output_api: type,
                       cache_file_path: str, *args, **kwargs):
  """Runs a check method with caching support.

  Args:
    check_method: The method that executes actual checks, must accept input_api
      and output_api as first two arguments and will get past the rest generic
      arguments (args, kwargs).
    check_id: Unique identifier for the check used as a key for the cache. The
      same type of check must always use the same id.
    input_api: The input api type, generally provided by the PRESUBMIT system.
    output_api: The output api type, generally provided by the PRESUBMIT system.
    cache_file_path: The path of the cache file to be used.
    *args: The extra args to pass to the check method (see: check_method).
    **kwargs: The extra kwargs to pass to the check method (see: check_method).
  """
  # As the path for import is relative to InputApi importing is done within
  # the function that already has a reference to the InputApi.
  sys.path.append(input_api.PresubmitLocalPath())
  import presubmit_caching_support
  cache = presubmit_caching_support.PresubmitCache(
      cache_file_path, input_api.PresubmitLocalPath())
  cached_result = cache.RetrieveResultFromCache(check_id)

  if cached_result is not None:
    sys.stdout.write(f'Using cached result for {check_id}\n')
    return cached_result

  new_result = check_method(input_api, output_api, *args, **kwargs)
  cache.StoreResultInCache(check_id, new_result)
  return new_result


def GetPrettyPrintErrors(input_api, output_api, cwd, rel_path, results):
  """Runs pretty-print command for specified file."""
  args = [
      input_api.python3_executable,
      os.path.join(input_api.PresubmitLocalPath(), 'pretty_print.py'), rel_path,
      '--presubmit', '--non-interactive'
  ]
  exit_code = input_api.subprocess.call(args, cwd=cwd)

  if exit_code != 0:
    error_msg = ('%s is not formatted correctly; run `git cl format` to fix.' %
                 rel_path)
    results.append(output_api.PresubmitError(error_msg))


def GetTokenErrors(input_api, output_api, cwd, rel_path, results):
  """Validates histogram tokens in specified file."""
  exit_code = input_api.subprocess.call([
      input_api.python3_executable,
      os.path.join(input_api.PresubmitLocalPath(), 'validate_token.py'),
      rel_path
  ],
                                        cwd=cwd)

  if exit_code != 0:
    error_msg = (
        '%s contains histogram(s) using <variants> not defined in the file, '
        'please run validate_token.py %s to fix.' % (rel_path, rel_path))
    results.append(output_api.PresubmitError(error_msg))


def GetValidateHistogramsError(input_api: Type, output_api: Type, cwd: str,
                               xml_paths_override: List[str],
                               results: List[Any]):
  """Validates histograms format using validate_format.py tool.

  This validates things like:
  - Histograms files are valid XMLs.
  - Histograms namespaces only span one file
  - Tokens used in histograms are registered.

  Args:
    input_api: An input_api instance that contains information about changes.
    output_api: An output_api instance to create results of the PRESUBMIT check.
    cwd: Work directory to run the python process in.
    xml_paths: A list of paths to the xml files to validate or None to use the
      default set of production xml files.
    results: The list of output_api objects to append the check warnings to.
  """
  validate_format_argv = [
      input_api.python3_executable,
      os.path.join(input_api.PresubmitLocalPath(), 'validate_format.py'),
  ]

  if xml_paths_override is not None:
    validate_format_argv.append('--xml_paths')
    validate_format_argv.extend(xml_paths_override)

  exit_code = input_api.subprocess.call(validate_format_argv, cwd=cwd)
  if exit_code != 0:
    error_msg = (
        'Histograms are not well-formatted; please run %s/validate_format.py '
        'and fix the reported errors.' % cwd)
    results.append(output_api.PresubmitError(error_msg))


def _GetValidateHistogramsIndexError(input_api: Type, output_api: Type,
                                     cwd: str, results: List[Any]):
  """Validates if index file is up-to-date with current state of the tree using
  validate_histograms_index.py tool.

  Args:
    input_api: An input_api instance that contains information about changes.
    output_api: An output_api instance to create results of the PRESUBMIT check.
    cwd: Work directory to run the python process in.
    results: The list of output_api objects to append the check warnings to.
  """
  exit_code = input_api.subprocess.call([
      input_api.python3_executable,
      os.path.join(input_api.PresubmitLocalPath(),
                   'validate_histograms_index.py')
  ],
                                        cwd=cwd)

  if exit_code != 0:
    error_msg = ('Histograms index file is not up-to-date. Please run '
                 '%s/histogram_paths.py to update it' % cwd)
    results.append(output_api.PresubmitError(error_msg))


def ValidateSingleFile(input_api, output_api, file_obj, cwd, results,
                       allow_test_paths):
  """Does corresponding validations if histograms.xml or enums.xml is changed.

  Args:
    input_api: An input_api instance that contains information about changes.
    output_api: An output_api instance to create results of the PRESUBMIT check.
    file_obj: A file object of one of the changed files.
    cwd: Path to current working directory.
    results: The returned variable which is a list of output_api results.
    allow_testing_paths: A boolean that determines if the test_data directory
      changes should be validated. If it's False, all the files under
      `test_data` directory will be ignored. This is needed as the `test_data`
      xmls contains intentional errors to trip the presubmit checks and we want
      to trip those presubmits in checks, but a the same time due to the nature
      of how the input_api gives all changed files in the directory, we don't
      want "production" checks to trip over those mistakes.

  Returns:
    A boolean that True if a histograms.xml or enums.xml file is changed.
  """
  p = file_obj.AbsoluteLocalPath()
  # Only do PRESUBMIT checks when |p| is under |cwd|.
  if input_api.os_path.commonprefix([p, cwd]) != cwd:
    return False
  filepath = input_api.os_path.relpath(p, cwd)

  if not allow_test_paths and 'test_data' in filepath:
    return False

  # If the changed file is histograms.xml or histogram_suffixes_list.xml,
  # pretty-print it.
  elif ('histograms.xml' in filepath
        or 'histogram_suffixes_list.xml' in filepath):
    GetPrettyPrintErrors(input_api, output_api, cwd, filepath, results)
    GetTokenErrors(input_api, output_api, cwd, filepath, results)
    return True

  # If the changed file is enums.xml, pretty-print it.
  elif 'enums.xml' in filepath:
    GetPrettyPrintErrors(input_api, output_api, cwd, filepath, results)
    return True

  return False


def CheckHistogramFormatting(input_api,
                             output_api,
                             cache_file_path=_CACHE_FILE_PATH,
                             allow_test_paths=False,
                             xml_paths_override=None):
  """Checks that histograms.xml is pretty-printed and well-formatted.

  This function is a wrapper around
  ExecuteCheckHistogramFormatting that adds caching support.
  """
  return _RunCheckWithCache(ExecuteCheckHistogramFormatting,
                            HistogramsPresubmitCheckType.FORMATTING_VALIDATION,
                            input_api, output_api, cache_file_path,
                            allow_test_paths, xml_paths_override)


# Note: Execute convention in this file comes from the fact that PRESUBMIT
# will try to call anything with a Check prefix as a function. As we want to
# avoid this and at the same we want to add a caching support, we are using
# Execute prefix for executing the checks on cache miss.
def ExecuteCheckHistogramFormatting(input_api, output_api, allow_test_paths,
                                    xml_paths_override):
  """Checks that histograms.xml is pretty-printed and well-formatted.

  This is a method that is called by the PRESUBMIT system and those it
  represents a production check rather then a test one. This is why we
  set allow_test_paths to False by default.
  """
  results = []
  cwd = input_api.PresubmitLocalPath()
  xml_changed = False

  # Only for changed files, do corresponding checks if the file is
  # histograms.xml or enums.xml.
  for file_obj in input_api.AffectedFiles(include_deletes=False):
    is_changed = ValidateSingleFile(input_api, output_api, file_obj, cwd,
                                    results, allow_test_paths)
    xml_changed = xml_changed or is_changed

  # Run validate_format.py if there were modified xml files.
  if xml_changed:
    GetValidateHistogramsError(input_api, output_api, cwd, xml_paths_override,
                               results)

  # Always run validate_histograms_index.py the condiditon when we need it is
  # relatively complex and given that this is a fast check (<100ms) it's easier
  # to just always make that check.
  _GetValidateHistogramsIndexError(input_api, output_api, cwd, results)

  return results


def CheckWebViewHistogramsAllowlistOnUpload(input_api,
                                            output_api,
                                            cache_file_path=_CACHE_FILE_PATH,
                                            allowlist_path_override=None,
                                            xml_paths_override=None):
  """Checks that HistogramsAllowlist.java contains valid histograms.

  This function is a wrapper around
  ExecuteCheckWebViewHistogramsAllowlistOnUpload that adds caching support.
  """
  return _RunCheckWithCache(
      ExecuteCheckWebViewHistogramsAllowlistOnUpload,
      HistogramsPresubmitCheckType.ALL_ALLOWLIST_HISTOGRAMS_PRESENT, input_api,
      output_api, cache_file_path, allowlist_path_override, xml_paths_override)


# Note: Execute convention in this file comes from the fact that PRESUBMIT
# will try to call anything with a Check prefix as a function. As we want to
# avoid this and at the same we want to add a caching support, we are using
# Execute prefix for executing the checks on cache miss.
def ExecuteCheckWebViewHistogramsAllowlistOnUpload(input_api, output_api,
                                                   allowlist_path_override,
                                                   xml_paths_override):
  """Checks that HistogramsAllowlist.java contains valid histograms."""
  xml_filter = lambda f: Path(f.LocalPath()).suffix == '.xml'
  xml_files = input_api.AffectedFiles(include_deletes=False,
                                      file_filter=xml_filter)
  if not xml_files:
    return []

  sys.path.append(input_api.PresubmitLocalPath())
  from histogram_paths import ALL_XMLS
  from histograms_allowlist_check import check_histograms_allowlist
  from histograms_allowlist_check import WellKnownAllowlistPath

  xml_files_paths = ALL_XMLS
  if xml_paths_override is not None:
    xml_files_paths = xml_paths_override

  xml_files = [open(f, encoding='utf-8') for f in xml_files_paths]
  src_path = os.path.join(input_api.PresubmitLocalPath(), '..', '..', '..')

  allowlist_path = os.path.join(
      src_path, WellKnownAllowlistPath.ANDROID_WEBVIEW.relative_path())

  if allowlist_path_override is not None:
    allowlist_path = allowlist_path_override

  result = check_histograms_allowlist(output_api, allowlist_path, xml_files)
  for f in xml_files:
    f.close()
  return result


def CheckBooleansAreEnums(input_api,
                          output_api,
                          cache_file_path=_CACHE_FILE_PATH):
  """Checks that histograms that use Booleans do not use units.

  This function is a wrapper around ExecuteCheckBooleansAreEnums that adds
  caching support.
  """
  return _RunCheckWithCache(ExecuteCheckBooleansAreEnums,
                            HistogramsPresubmitCheckType.BOOLS_ARE_ENUMS,
                            input_api, output_api, cache_file_path)


# Note: Execute convention in this file comes from the fact that PRESUBMIT
# will try to call anything with a Check prefix as a function. As we want to
# avoid this and at the same we want to add a caching support, we are using
# Execute prefix for executing the checks on cache miss.
def ExecuteCheckBooleansAreEnums(input_api, output_api):
  """Checks that histograms that use Booleans do not use units."""
  results = []
  cwd = input_api.PresubmitLocalPath()
  inclusion_pattern = input_api.re.compile(r'units="[Bb]oolean')
  units_warning = """
  You are using 'units' for a boolean histogram, but you should be using
  'enum' instead."""

  # Only for changed files, do corresponding checks if the file is
  # histograms.xml or enums.xml.
  for affected_file in input_api.AffectedFiles(include_deletes=False):
    filepath = input_api.os_path.relpath(affected_file.AbsoluteLocalPath(), cwd)
    if 'histograms.xml' in filepath:
      for line_number, line in affected_file.ChangedContents():
        if inclusion_pattern.search(line):
          results.append('%s:%s\n\t%s' % (filepath, line_number, line.strip()))

  # If a histograms.xml file was changed, check for units="[Bb]oolean".
  if results:
    return [output_api.PresubmitPromptOrNotify(units_warning, results)]
  return results
