#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Convert tf_version.bzl to tf_version.gni."""

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


if __name__ == '__main__':
  main()
