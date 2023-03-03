# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Ignore the following files from SVG optimization checks.
BLOCKLIST = [
    # Ignore since it holds documentation comments.
    "components/dom_distiller/core/images/dom_distiller_material_spinner.svg",
]


def CheckOptimized(input_api, output_api):
  file_filter = lambda f: f.LocalPath().endswith('.svg') and \
      f.LocalPath().replace('\\', '/') not in BLOCKLIST
  svgs = input_api.AffectedFiles(file_filter=file_filter, include_deletes=False)

  if not svgs:
    return []

  from resources import svgo
  unoptimized = []

  def _ToBinary(s):
    if isinstance(s, bytes):
      return s
    if isinstance(s, str):
      return s.encode('utf-8')

  for f in svgs:
    original = b'\n'.join(_ToBinary(line) for line in f.NewContents()).strip()
    output = _ToBinary(
        svgo.Run(input_api.os_path,
                 ['-o', '-', '-i', f.AbsoluteLocalPath()]).strip())
    if original != output:
      unoptimized.append(f.LocalPath())

  if unoptimized:
    instructions = 'Run tools/resources/svgo.py on these files to optimize:'
    msg = '\n  '.join([instructions] + unoptimized)
    return [output_api.PresubmitNotifyResult(msg)]

  return []
