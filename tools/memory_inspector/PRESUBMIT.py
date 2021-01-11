# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for memory_inspector.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into depot_tools.
"""


def _CommonChecks(input_api, output_api):
  output = []
  files_to_skip = [r'classification_rules.*']
  output.extend(
      input_api.canned_checks.RunPylint(input_api,
                                        output_api,
                                        files_to_skip=files_to_skip,
                                        extra_paths_list=[
                                            input_api.os_path.join(
                                                input_api.PresubmitLocalPath(),
                                                '..', '..', 'build', 'android')
                                        ]))
  output.extend(input_api.canned_checks.RunUnitTests(
      input_api,
      output_api,
      [input_api.os_path.join(input_api.PresubmitLocalPath(), 'run_tests')]))

  if input_api.is_committing:
    output.extend(input_api.canned_checks.PanProjectChecks(input_api,
                                                           output_api,
                                                           owners_check=False))
  return output


def _CheckPrebuiltsAreUploaded(input_api, output_api):
  """Checks that the SHA1 files in prebuilts/ reference existing objects on GCS.

  This is to avoid that somebody accidentally checks in some new XXX.sha1 files
  into prebuilts/ without having previously uploaded the corresponding binaries
  to the cloud storage bucket. This can happen if the developer has a consistent
  local copy of the binary. This check verifies (through a HTTP HEAD request)
  that the GCS bucket has an object for each .sha1 file in prebuilts and raises
  a presubmit error, listing the missing files, if not.
  """
  import sys
  import urllib2
  old_sys_path = sys.path
  try:
    sys.path = [input_api.PresubmitLocalPath()] + sys.path
    from memory_inspector import constants
  finally:
    sys.path = old_sys_path
  missing_files = []
  for f in input_api.os_listdir(constants.PREBUILTS_PATH):
    if not f.endswith('.sha1'):
      continue
    prebuilt_sha_path = input_api.os_path.join(constants.PREBUILTS_PATH, f)
    with open(prebuilt_sha_path) as sha_file:
      sha = sha_file.read().strip()
    url = constants.PREBUILTS_BASE_URL + sha
    request = urllib2.Request(url)
    request.get_method = lambda : 'HEAD'
    try:
      urllib2.urlopen(request, timeout=5)
    except Exception as e:
      if isinstance(e, urllib2.HTTPError) and e.code == 404:
        missing_files += [prebuilt_sha_path]
      else:
        return [output_api.PresubmitError('HTTP Error while checking %s' % url,
                                          long_text=str(e))]
  if missing_files:
    return [output_api.PresubmitError(
        'Some prebuilts have not been uploaded. Perhaps you forgot to '
        'upload_to_google_storage.py?', missing_files)]
  return []


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  results.extend(_CheckPrebuiltsAreUploaded(input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
