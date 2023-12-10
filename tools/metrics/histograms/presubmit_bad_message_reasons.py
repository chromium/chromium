# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Check to see if the various BadMessage enums in histograms.xml need to be
updated. This can be called from a chromium PRESUBMIT.py to ensure updates to
bad_message.h also include the generated changes to histograms.xml
"""

import update_histogram_enum


def PrecheckBadMessage(input_api,
                       output_api,
                       histogram_name,
                       end_marker='^BAD_MESSAGE_MAX',
                       strip_k_prefix=False):
  source_path = ''

  # This function is called once per bad_message.h-containing directory. Check
  # for the |bad_message.h| file, and if present, remember its path.
  for f in input_api.AffectedFiles():
    if f.LocalPath().endswith('bad_message.h'):
      source_path = f.LocalPath()
      break

  # If the |bad_message.h| wasn't found in this change, then there is nothing to
  # do and histogram.xml does not need to be updated.
  if source_path == '':
    return []

  START_MARKER='^enum (class )?BadMessageReason {'
  presubmit_error = update_histogram_enum.CheckPresubmitErrors(
      'tools/metrics/histograms/metadata/stability/enums.xml',
      histogram_enum_name=histogram_name,
      update_script_name='update_bad_message_reasons.py',
      source_enum_path=source_path,
      start_marker=START_MARKER,
      end_marker=end_marker,
      strip_k_prefix=strip_k_prefix)
  if presubmit_error:
    return [output_api.PresubmitPromptWarning(presubmit_error,
                                              items=[source_path])]
  return []
