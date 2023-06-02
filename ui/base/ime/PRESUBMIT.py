# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for ui/base/ime

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import filecmp
import os

CHARACTER_COMPOSER_DATA_SOURCES=['character_composer_sequences.txt']
CHARACTER_COMPOSER_DATA_HEADER='character_composer_data.h'
CHARACTER_COMPOSER_DATA_GENERATOR='generate_character_composer_data.py'

def CheckCharacterComposerData(input_api, output_api):
  results = []
  whereami = input_api.PresubmitLocalPath()
  files = [input_api.os_path.join(whereami, x) for x in
           CHARACTER_COMPOSER_DATA_SOURCES +
           [CHARACTER_COMPOSER_DATA_HEADER, CHARACTER_COMPOSER_DATA_GENERATOR]]

  if not input_api.AffectedFiles(
    file_filter=lambda x: x.AbsoluteLocalPath() in files):
    return results

  # Generate a copy of the data header and compare it with the current file,
  # to ensure that it is not hand-editied and stays in sync with the sources.
  (tempfd, tempname) = input_api.tempfile.mkstemp()
  os.close(tempfd)
  generator = [input_api.python3_executable,
               CHARACTER_COMPOSER_DATA_GENERATOR,
               '--output',
               tempname,
               '--guard',
               CHARACTER_COMPOSER_DATA_HEADER] + CHARACTER_COMPOSER_DATA_SOURCES
  print(generator)
  generate = input_api.subprocess.Popen(generator,
                                        cwd=whereami,
                                        stdout=input_api.subprocess.PIPE)
  errors = generate.communicate()[0].strip()
  if errors:
    results += [output_api.PresubmitPromptOrNotify(
      'compose data generator failed.',
      errors.splitlines())]
  elif not filecmp.cmp(tempname, CHARACTER_COMPOSER_DATA_HEADER):
    results += [output_api.PresubmitPromptOrNotify(
      'Generated compose data does not match "' +
      CHARACTER_COMPOSER_DATA_HEADER + '".')]
    print(tempname)
  else:
    os.remove(tempname)
  return results

def CheckChangeOnCommit(input_api, output_api):
  return CheckCharacterComposerData(input_api, output_api)

def CheckChangeOnUpload(input_api, output_api):
  return CheckCharacterComposerData(input_api, output_api)
