#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib
import shutil

NUM_CLASSES = 1000


def GenerateClassContents(classname):
  return f"""\
package org.jni_zero.benchmark;

import org.jni_zero.JNINamespace;
import org.jni_zero.CalledByNative;

@JNINamespace("jni_zero::benchmark")
public class {classname} {{
    @CalledByNative
    static void callMe() {{
    }}
}}
"""


def GenerateAllClasses(directory):
  for i in range(NUM_CLASSES):
    classname = 'Placeholder' + str(i + 1)
    output_directory = directory / 'src/org/jni_zero/benchmark'
    output_directory.mkdir(parents=True, exist_ok=True)
    output_path = output_directory / (classname + '.java')
    with open(output_path, 'wt') as output:
      output.write(GenerateClassContents(classname))


def GenerateBuildFile(buildfile_path):
  sb = []
  sb.append("""\
import("//build/config/android/rules.gni")
import("//third_party/jni_zero/jni_zero.gni")

generate_jni("generated_header") {
  sources = [
""")
  for i in range(NUM_CLASSES):
    classname = 'Placeholder' + str(i + 1)
    sb.append(f'"src/org/jni_zero/benchmark/{classname}.java",')
  sb.append("""
  ]
}

android_library("generated_java") {
  sources = [
""")
  for i in range(NUM_CLASSES):
    classname = 'Placeholder' + str(i + 1)
    sb.append(f'"src/org/jni_zero/benchmark/{classname}.java",')
  sb.append("""\
  ]

  deps = [
    ":generated_header_java",
    "//third_party/jni_zero:jni_zero_java",
  ]
}
""")
  with open(buildfile_path, 'wt') as output:
    output.write('\n'.join(sb))


def main():
  output_directory = pathlib.Path(__file__).parents[0]
  generated = output_directory / 'src'
  if generated.exists():
    shutil.rmtree(generated)
  GenerateAllClasses(output_directory)
  buildfile_path = output_directory / 'BUILD.gn'
  GenerateBuildFile(buildfile_path)


if __name__ == '__main__':
  main()
