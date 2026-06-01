# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import dataclasses
import difflib
import hashlib
import logging
import os
import pathlib
import pickle
import shutil
import sys
from typing import Any, Callable, Dict, List, Optional

import setup_modules  # pylint: disable=unused-import

import chromium_src.tools.metrics.common.diff_util as diff_util
import chromium_src.tools.python.google.path_utils as path_utils


def DoPresubmit(argv,
                original_filename,
                backup_filename,
                prettyFn,
                script_name='git cl format'):
  """Execute presubmit/pretty printing for the target file.

  Args:
    argv: command line arguments
    original_filename: The path to the file to read from.
    backup_filename: When pretty printing, move the old file contents here.
    prettyFn: A function which takes the original xml content and produces
        pretty printed xml.
    script_name: The name of the script to run for pretty printing.

  Returns:
    An exit status.  Non-zero indicates errors.
  """
  # interactive: Print log info messages and prompt user to accept the diff.
  interactive = ('--non-interactive' not in argv)
  # presubmit: Simply print a message if the input is not formatted correctly.
  presubmit = ('--presubmit' in argv)
  # diff: Print diff to stdout rather than modifying files.
  diff = ('--diff' in argv)
  # cleanup: Remove the backup file after at the end, if created.
  cleanup = ('--cleanup' in argv)

  if interactive:
    logging.basicConfig(level=logging.INFO)
  else:
    logging.basicConfig(level=logging.ERROR)

  # If there is a description xml in the current working directory, use that.
  # Otherwise, use the one residing in the same directory as this script.
  xml_dir = os.getcwd()
  if not os.path.isfile(os.path.join(xml_dir, original_filename)):
    xml_dir = path_utils.ScriptDir()

  xml_path = os.path.join(xml_dir, original_filename)

  # Save the original file content.
  logging.info('Loading %s...', os.path.relpath(xml_path))
  with open(xml_path, 'rb') as f:
    original_xml = f.read()

  # Check there are no CR ('\r') characters in the file.
  if b'\r' in original_xml:
    logging.error('DOS-style line endings (CR characters) detected - these are '
                  'not allowed. Please run dos2unix %s', original_filename)
    return 1

  original_xml = original_xml.decode('utf-8')

  try:
    pretty = prettyFn(original_xml)
  except Exception:
    logging.exception('Aborting parsing due to fatal errors:')
    return 1

  if original_xml == pretty:
    logging.info('%s is correctly pretty-printed.', original_filename)
    return 0

  if presubmit:
    if interactive:
      logging.error('%s is not formatted correctly; run `%s` to fix.',
                    original_filename, script_name)
    return 1

  # Prompt user to consent on the change.
  if interactive and not diff_util.PromptUserToAcceptDiff(
      original_xml, pretty, 'Is the new version acceptable?'):
    logging.error('Diff not accepted. Aborting.')
    return 1

  if diff:
    for line in difflib.unified_diff(original_xml.splitlines(),
                                     pretty.splitlines()):
      print(line)
    return 0

  logging.info('Creating backup file: %s', backup_filename)
  backup_path = os.path.join(xml_dir, backup_filename)
  shutil.move(xml_path, backup_path)

  pretty = pretty.encode('utf-8')
  with open(xml_path, 'wb') as f:
    f.write(pretty)
  logging.info('Updated %s. Don\'t forget to add it to your changelist',
               xml_path)

  # Remove backup file if created, if prompted by user.
  if cleanup and backup_path:
    logging.info('Cleaning up backup file: %s' % backup_filename)
    os.remove(backup_path)

  return 0


def DoPresubmitMain(*args, **kwargs):
  sys.exit(DoPresubmit(*args, **kwargs))


def CheckChange(xml_file, input_api, output_api):
  """Checks that xml is pretty-printed and well-formatted."""
  absolute_paths_of_affected_files = [
      f.AbsoluteLocalPath() for f in input_api.AffectedFiles()
  ]
  xml_file_changed = any([
      input_api.basename(p) == xml_file
      and input_api.os_path.dirname(p) == input_api.PresubmitLocalPath()
      for p in absolute_paths_of_affected_files
  ])

  if not xml_file_changed:
    return []

  cwd = input_api.PresubmitLocalPath()
  pretty_print_args = [
      input_api.python3_executable, 'pretty_print.py', '--presubmit', xml_file
  ]

  exit_code = input_api.subprocess.call(pretty_print_args, cwd=cwd)
  if exit_code != 0:
    return [
        output_api.PresubmitError(
            '%s is not prettified; run `git cl format` to fix.' % xml_file),
    ]

  validate_format_args = [
      input_api.python3_executable, 'validate_format.py', '--presubmit',
      xml_file
  ]
  exit_code = input_api.subprocess.call(validate_format_args, cwd=cwd)
  if exit_code != 0:
    return [
        output_api.PresubmitError(
            '%s does not pass format validation; run %s/validate_format.py '
            'and fix the reported error(s) or warning(s).' %
            (xml_file, input_api.PresubmitLocalPath())),
    ]

  return []


@dataclasses.dataclass(frozen=True)
class _PresubmitCheckContext:
  observed_hash: str
  check_id: int

  def key(self):
    return f"{self.observed_hash}:{self.check_id}"


_CURRENT_CACHE_FILE_SCHEMA_VERSION = "v2"


@dataclasses.dataclass(frozen=True)
class CacheFileSchema:
  data: Dict[str, Any]


def _CalculateCombinedDirectoryHash(directory_path):
  hasher = hashlib.sha256()
  for root, dirs, files in os.walk(directory_path):
    dirs[:] = [d for d in dirs if d != '__pycache__' and not d.startswith('.')]
    dirs.sort()
    for file in sorted(files):
      file_path = os.path.join(root, file)
      with open(file_path, "rb") as f:
        chunk = f.read(4096)
        while chunk:
          hasher.update(chunk)
          chunk = f.read(4096)
  return hasher.hexdigest()


class PresubmitCache:

  def __init__(self, storage_directory_path: str, observed_directory_path: str):
    base_dir_path = pathlib.Path(storage_directory_path)
    versioned_path = base_dir_path.joinpath(_CURRENT_CACHE_FILE_SCHEMA_VERSION)
    versioned_path.mkdir(parents=True, exist_ok=True)

    self._storage_file_path = str(versioned_path.joinpath("cache.json"))
    self._observed_directory = observed_directory_path
    self._cache_contents = CacheFileSchema(data={})

    if not os.path.exists(self._storage_file_path) or os.path.getsize(
        self._storage_file_path) == 0:
      return

    with open(self._storage_file_path, "rb") as f:
      try:
        self._cache_contents = pickle.load(f)
        cache_needs_invalidation = False
      except Exception as e:
        print(f"Cache is being cleared as it failed to finish reading: {e}")
        cache_needs_invalidation = True

      if not cache_needs_invalidation:
        return

    try:
      os.remove(self._storage_file_path)
    except Exception as e:
      print(f"Failed to delete the cache file ({e}). To invalidate cache,"
            f" please try to remove {self._storage_file_path} manually.")

  def _GetForContext(self, context: _PresubmitCheckContext) -> Optional[str]:
    if context.key() not in self._cache_contents.data:
      return None
    return self._cache_contents.data[context.key()]

  def _StoreForContext(self, context: _PresubmitCheckContext,
                       check_result: Any):
    self._cache_contents.data[context.key()] = check_result
    self._SaveCurrentCache()

  def _SaveCurrentCache(self):
    with open(self._storage_file_path, "wb") as f:
      pickle.dump(self._cache_contents, f)

  def InspectCacheForTesting(self) -> CacheFileSchema:
    return self._cache_contents

  def RetrieveResultFromCache(self, check_id: int) -> Optional[Any]:
    return self._GetForContext(
        _PresubmitCheckContext(
            observed_hash=_CalculateCombinedDirectoryHash(
                self._observed_directory),
            check_id=check_id,
        ))

  def StoreResultInCache(self, check_id: int, check_result: Any):
    self._StoreForContext(
        _PresubmitCheckContext(
            observed_hash=_CalculateCombinedDirectoryHash(
                self._observed_directory),
            check_id=check_id,
        ), check_result)


PresubmitCheckMethod = Callable[..., List[Any]]


def RunCheckWithCache(check_method: PresubmitCheckMethod, check_id: int,
                      input_api: Any, output_api: Any, cache_file_path: str,
                      *args, **kwargs) -> List[Any]:
  """Runs a check method with caching support.

  Args:
    check_method: The method that executes actual checks, must accept input_api
      and output_api as first two arguments and will get past the rest generic
      arguments (args, kwargs).
    check_id: Unique identifier for the check used as a key for the cache. The
      same type of check must always use the same id.
    input_api: The input api type, generally provided by the PRESUBMIT system.
    output_api: An output_api instance to create results of the PRESUBMIT check.
    cache_file_path: The path of the cache file to be used.
    *args: The extra args to pass to the check method (see: check_method).
    **kwargs: The extra kwargs to pass to the check method (see: check_method).
  """
  cache = PresubmitCache(cache_file_path, input_api.PresubmitLocalPath())
  cached_result = cache.RetrieveResultFromCache(check_id)

  if cached_result is not None:
    print(f'Using cached result for {check_id}\n')
    return cached_result

  new_result = check_method(input_api, output_api, *args, **kwargs)
  cache.StoreResultInCache(check_id, new_result)
  return new_result
