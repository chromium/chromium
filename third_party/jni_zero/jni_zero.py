#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A bindings generator for JNI on Android."""

import argparse
import os
import posixpath
import shutil
import sys

import common
import jni_generator
import jni_registration_generator

# jni_zero.py requires Python 3.8+.
_MIN_PYTHON_MINOR = 8


def _add_io_args(parser, *, is_final=False, is_javap=False):
  inputs = parser.add_argument_group(title='Inputs')
  outputs = parser.add_argument_group(title='Outputs')
  if is_final:
    inputs.add_argument(
        '--java-sources-file',
        required=True,
        help='Newline-separated file containing paths to .java or .jni.pickle '
        'files, taken from Java dependency tree.')
    inputs.add_argument(
        '--priority-java-sources-file',
        help='Same format as java-sources-file, only used by multiplexing to '
        'pick certain methods to be the first N numbers in the switch table.')
    inputs.add_argument(
        '--never-omit-switch-num',
        action='store_true',
        help='Only used by multiplexing. Whether to disable optimization of '
        'omitting switch_num for unique signatures.')
    inputs.add_argument(
        '--native-sources-file',
        help='Newline-separated file containing paths to .java or .jni.pickle '
        'files, taken from Native dependency tree.')
  else:
    if is_javap:
      inputs.add_argument(
          '--jar-file',
          help='Extract the list of input files from a specified jar file. '
          'Uses javap to extract the methods from a pre-compiled class.')

      help_text = 'Paths within the .jar'
    else:
      help_text = 'Paths to .java files to parse.'
    inputs.add_argument('--input-file',
                        action='append',
                        required=True,
                        dest='input_files',
                        help=help_text)
    outputs.add_argument('--output-name',
                         action='append',
                         required=True,
                         dest='output_names',
                         help='Output filenames within output directory.')
    outputs.add_argument('--output-dir',
                         required=True,
                         help='Output directory. '
                         'Existing .h files in this directory will be assumed '
                         'stale and removed.')
    outputs.add_argument('--placeholder-srcjar-path',
                         help='Path to output srcjar with placeholders for '
                         'all referenced classes in |input_files|')

  outputs.add_argument('--header-path', help='Path to output header file.')

  if is_javap:
    inputs.add_argument('--javap', help='The path to javap command.')
  else:
    outputs.add_argument(
        '--srcjar-path',
        help='Path to output srcjar for GEN_JNI.java (and J/N.java if proxy'
        ' hash is enabled).')
    outputs.add_argument('--jni-pickle',
                         help='Path to write intermediate .jni.pickle file.')
  if is_final:
    outputs.add_argument(
        '--depfile', help='Path to depfile (for use with ninja build system)')


def _add_codegen_args(parser, *, is_final=False, is_javap=False):
  group = parser.add_argument_group(title='Codegen Options')
  mode_group = parser.add_mutually_exclusive_group()
  group.add_argument(
      '--module-name',
      help='Only look at natives annotated with a specific module name.')
  this_dir = posixpath.abspath(posixpath.dirname(__file__))
  root_dir = posixpath.dirname(posixpath.dirname(this_dir))
  default_path_prefix = os.path.relpath(this_dir, root_dir) + '/'
  group.add_argument('--include-path-prefix',
                     help='Value for #include "${PREFIX}jni_zero.h" '
                     f'(default={default_path_prefix})',
                     default=default_path_prefix)
  group.add_argument('--extra-include',
                     action='append',
                     dest='extra_includes',
                     help='Header file to #include in the generated header.')
  group.add_argument(
      '--enable-legacy-natives',
      action='store_true',
      help='Whether to generate code from "native" java methods.')
  if is_final:
    group.add_argument(
        '--add-stubs-for-missing-native',
        action='store_true',
        help='Adds stub methods for any --java-sources-file which are missing '
        'from --native-sources-files. If not passed, we will assert that none '
        'of these exist.')
    group.add_argument(
        '--remove-uncalled-methods',
        action='store_true',
        help='Removes --native-sources-files which are not in '
        '--java-sources-file. If not passed, we will assert that none of these '
        'exist.')
    group.add_argument(
        '--namespace',
        help='Native namespace to wrap the registration functions into.')
    group.add_argument('--manual-jni-registration',
                       action='store_true',
                       help='Generate a call to RegisterNatives()')
    group.add_argument('--include-test-only',
                       action='store_true',
                       help='Whether to maintain ForTesting JNI methods.')
  else:
    group.add_argument(
        '--split-name',
        help='Split name that the Java classes should be loaded from.')
    mode_group.add_argument(
        '--per-file-natives',
        action='store_true',
        help='Generate .srcjar and .h such that a final generate-final '
        'step is not necessary')
    if not is_javap:
      group.add_argument(
          '--enable-definition-macros',
          action='store_true',
          help='Generate JNI glue code in DEFINE_JNI_FOR_MyClass() macros')
    group.add_argument('--allow-private-called-by-natives',
                       action='store_true',
                       help='Whether to allow private @CalledByNative symbols.')

  if is_javap:
    group.add_argument('--unchecked-exceptions',
                       action='store_true',
                       help='Do not check that no exceptions were thrown.')
  else:
    mode_group.add_argument(
        '--use-proxy-hash',
        action='store_true',
        help='Enables hashing of the native declaration for methods in '
        'a @NativeMethods interface')
    mode_group.add_argument(
        '--enable-jni-multiplexing',
        action='store_true',
        help='Enables JNI multiplexing for Java native methods')
    group.add_argument(
        '--package-prefix',
        help='Adds a prefix to the classes fully qualified-name. Effectively '
        'changing a class name from foo.bar -> prefix.foo.bar')
    group.add_argument(
        '--package-prefix-filter',
        help=
        ': separated list of java packages to apply the --package-prefix to.')

  if not is_final:
    if is_javap:
      instead_msg = 'instead of the javap class name.'
    else:
      instead_msg = 'when there is no @JNINamespace set'

    group.add_argument('--namespace',
                       help='Uses as a namespace in the generated header ' +
                       instead_msg)


def _maybe_relaunch_with_newer_python():
  # If "python3" is < python3.8, but a newer version is available, then use
  # that.
  py_version = sys.version_info
  if py_version < (3, _MIN_PYTHON_MINOR):
    if os.environ.get('JNI_ZERO_RELAUNCHED'):
      sys.stderr.write('JNI_ZERO_RELAUNCHED failure.\n')
      sys.exit(1)
    for i in range(_MIN_PYTHON_MINOR, 30):
      name = f'python3.{i}'
      if shutil.which(name):
        cmd = [name] + sys.argv
        env = os.environ.copy()
        env['JNI_ZERO_RELAUNCHED'] = '1'
        os.execvpe(cmd[0], cmd, env)
    sys.stderr.write(
        f'jni_zero requires Python 3.{_MIN_PYTHON_MINOR} or greater.\n')
    sys.exit(1)


def _add_args(parser, *, is_final=False, is_javap=False):
  _add_io_args(parser, is_final=is_final, is_javap=is_javap)
  _add_codegen_args(parser, is_final=is_final, is_javap=is_javap)


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  subparsers = parser.add_subparsers(required=True)

  subp = subparsers.add_parser(
      'from-source', help='Generates files for a set of .java sources.')
  _add_args(subp)
  subp.set_defaults(func=jni_generator.GenerateFromSource)

  subp = subparsers.add_parser(
      'from-jar', help='Generates files from a .jar of .class files.')
  _add_args(subp, is_javap=True)
  subp.set_defaults(func=jni_generator.GenerateFromJar)

  subp = subparsers.add_parser(
      'generate-final',
      help='Generates files that require knowledge of all intermediates.')
  _add_args(subp, is_final=True)
  subp.set_defaults(func=jni_registration_generator.main)

  # Default to showing full help text when no args are passed.
  if len(sys.argv) == 1:
    parser.print_help()
  elif len(sys.argv) == 2 and sys.argv[1] in subparsers.choices:
    parser.parse_args(sys.argv[1:] + ['-h'])
  else:
    args = parser.parse_args()

    bool_arg = lambda name: getattr(args, name, False)
    jni_mode = common.JniMode(is_hashing=bool_arg('use_proxy_hash'),
                              is_muxing=bool_arg('enable_jni_multiplexing'),
                              is_per_file=bool_arg('per_file_natives'))
    args.func(parser, args, jni_mode)

if __name__ == '__main__':
  _maybe_relaunch_with_newer_python()
  main()
