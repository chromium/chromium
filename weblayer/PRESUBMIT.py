# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit tests for weblayer.

Runs various style checks before upload.
"""

import re

WEBLAYER_VERSION_PATH = (
    'weblayer/browser/java/org/chromium/weblayer_private/interfaces/' +
    'WebLayerVersion.java')


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CheckAIDLVersionBump(input_api, output_api))
  return results


def _Canonicalize(lines):
  """Strip comments, and convert all whitespace to single spaces."""

  def _CanonicalizeLine(line):
    line = re.sub(r'//,*', '', line)
    line = re.sub(r'\s+', ' ', line)
    return line

  return re.sub(r'\s*/\*.*?\*/\s*', ' ', ''.join(map(_CanonicalizeLine, lines)))


def _CheckAIDLVersionBump(input_api, output_api):
  """Any change to an AIDL file must be accompanied by a version code bump."""

  def AIDLFiles(affected_file):
    return input_api.FilterSourceFile(affected_file, white_list=(r'.*\.aidl$',))

  aidl_changes = []
  for f in input_api.AffectedSourceFiles(AIDLFiles):
    old_contents = _Canonicalize(f.OldContents())
    new_contents = _Canonicalize(f.NewContents())
    if old_contents != new_contents:
      aidl_changes.append((f.LocalPath(), f.Action()))

  if not aidl_changes:
    return []

  aidl_changes = '\n'.join(
      '  {1} {0}'.format(path, action) for path, action in aidl_changes)

  def VersionFile(affected_file):
    return input_api.FilterSourceFile(
        affected_file, white_list=(WEBLAYER_VERSION_PATH,))

  changed_version_file = list(input_api.AffectedSourceFiles(VersionFile))

  if not changed_version_file:
    return [
        output_api.PresubmitPromptWarning(
            'Commit contains AIDL changes,' +
            ' but does not change WebLayerVersion.java\n' + aidl_changes)
    ]

  assert len(changed_version_file) == 1

  old_contents = _Canonicalize(changed_version_file[0].OldContents())
  new_contents = _Canonicalize(changed_version_file[0].NewContents())

  m_old = re.search(r'sVersionNumber\s*=\s*(.*?);', old_contents)
  m_new = re.search(r'sVersionNumber\s*=\s*(.*?);', new_contents)
  if m_old and m_new and m_old.group(1) == m_new.group(1):
    return [
        output_api.PresubmitPromptWarning(
            'Commit contains AIDL changes,' +
            ' but does not change WebLayerVersion.sVersionNumber\n' +
            aidl_changes)
    ]

  return []
