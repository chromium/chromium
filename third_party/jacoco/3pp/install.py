#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import os
import shutil
import subprocess
import sys


_JACOCO_BUILD_SUB_DIR = 'org.jacoco.build'


def install(output_prefix, deps_prefix):

  # Set up JAVA_HOME and PATH for the mvn command to find the JDK.
  env = os.environ.copy()
  env['JAVA_HOME'] = os.path.join(deps_prefix, 'current')
  java_home_bin = os.path.join(deps_prefix, 'current', 'bin')
  env['PATH'] = '%s:%s' % (env.get('PATH', ''), java_home_bin)

  # Building Jacoco following instructions from:
  # https://www.jacoco.org/jacoco/trunk/doc/build.html

  # Change working dir to jacoco build dir.
  os.chdir(os.path.join(os.getcwd(), _JACOCO_BUILD_SUB_DIR))

  # Build Jacoco.
  subprocess.run(['mvn', 'clean', 'verify', '-DskipTests'], env=env, check=True)

  # Find build ouput and move lib dir to output.
  build_output_dir = os.path.join(os.getcwd(), '..', 'jacoco', 'target')
  file_pattern = os.path.join(build_output_dir, 'jacoco-%s.*.zip' % env['_3PP_VERSION'])
  for item in glob.glob(file_pattern):
    shutil.unpack_archive(item, build_output_dir)
    break

  os.makedirs(output_prefix, exist_ok=True)
  shutil.move(os.path.join(build_output_dir, 'lib'), output_prefix)


if __name__ == '__main__':
  install(sys.argv[1], sys.argv[2])

