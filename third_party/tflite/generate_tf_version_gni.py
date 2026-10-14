#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Convert tf_version.bzl to tf_version.gni and update models.proto."""

import os
import re
import sys

_TMPL = '''
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#
#     THIS FILE IS AUTO-GENERATED. DO NOT EDIT.
#
#     See //third_party/tflite/generate_tf_version_gni.py
#
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

tf_version_major = %TF_VERSION_MAJOR%
tf_version_minor = %TF_VERSION_MINOR%
tf_version_patch = %TF_VERSION_PATCH%
'''.lstrip()


def _tflite_dir() -> str:
    """Returns the absolute path of //third_party/tflite/."""
    return os.path.dirname(os.path.realpath(__file__))


def _tensorflow_dir() -> str:
    """Returns the absolute path of //third_party/tflite/src/tensorflow/."""
    return os.path.join(_tflite_dir(), "src", "tensorflow")


def _update_models_proto(new_major: str, new_minor: str) -> None:
  models_proto_path = os.path.normpath(
      os.path.join(
          _tflite_dir(),
          '..',
          '..',
          'components',
          'optimization_guide',
          'proto',
          'models.proto',
      ))
  with open(models_proto_path, 'r') as f:
    proto_content = f.read()

  enum_match = re.search(
      r'(enum\s+ModelEngineVersion\s*\{)(.*?)(\n\})',
      proto_content,
      re.DOTALL,
  )
  if not enum_match:
    print('Error: Could not find enum ModelEngineVersion in models.proto')
    sys.exit(1)

  enum_body = enum_match.group(2)
  entries = re.findall(
      r'MODEL_ENGINE_VERSION_TFLITE_(\d+)_(\d+)(?:_\d+)*\s*=\s*(\d+);',
      enum_body,
  )
  if not entries:
    print('Error: Could not find MODEL_ENGINE_VERSION_TFLITE_ entries in '
          'models.proto')
    sys.exit(1)

  parsed_entries = [
      (int(tag), int(major), int(minor)) for major, minor, tag in entries
  ]
  max_tag, cur_major, cur_minor = max(parsed_entries, key=lambda x: x[0])

  if (int(new_major), int(new_minor)) > (cur_major, cur_minor):
    enum_name = f'MODEL_ENGINE_VERSION_TFLITE_{new_major}_{new_minor}_0'
    new_entry = (
        f'\n  // A model using only operations that are supported by '
        f'{new_major}.{new_minor}.0.\n'
        f'  {enum_name} = {max_tag + 1};'
    )
    updated_proto = (
        proto_content[:enum_match.end(2)] + new_entry +
        proto_content[enum_match.start(3):]
    )
    with open(models_proto_path, 'w') as f:
      f.write(updated_proto)
    print(
        f'Added {enum_name} = {max_tag + 1} to '
        'components/optimization_guide/proto/models.proto.\n'
        'NOTE: Please also:\n'
        '  1. Regenerate '
        'components/optimization_guide/internal/testing/proto/components/'
        'optimization_guide/proto/models_pb2.py in src-internal (per '
        'components/optimization_guide/internal/testing/proto/README.md).\n'
        '  2. Update ModelEngineVersion in optimizationguide.proto and '
        'tflite_versions.md in google3.'
    )


def main():
  with open(os.path.join(_tensorflow_dir(), 'tf_version.bzl'), 'r') as f:
    content = f.read()

  match = re.search(r'TF_VERSION = "(\d+).(\d+).(\d+)"', content)
  if not match:
    print("Error: Could not find TF_VERSION in tf_version.bzl")
    sys.exit(1)

  with open(os.path.join(_tflite_dir(), 'tf_version.gni'), 'w') as f:
    f.write(_TMPL.replace('%TF_VERSION_MAJOR%', match.group(1)) \
                 .replace('%TF_VERSION_MINOR%', match.group(2)) \
                 .replace('%TF_VERSION_PATCH%', match.group(3)))

  _update_models_proto(match.group(1), match.group(2))


if __name__ == '__main__':
  main()
