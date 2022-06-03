# Copyright 2010 The Closure Library Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS-IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Utility to use the Closure Compiler CLI from Python."""

import logging
import os
import re
import subprocess
import tempfile

# Pulls just the major and minor version numbers from the first line of
# 'java -version'. Versions are in the format of [0-9]+(\.[0-9]+)? See:
# http://openjdk.java.net/jeps/223
_VERSION_REGEX = re.compile(r'"([0-9]+)(?:\.([0-9]+))?')


class JsCompilerError(Exception):
  """Raised if there's an error in calling the compiler."""
  pass


def _GetJavaVersionString():
  """Get the version string from the Java VM."""
  return subprocess.check_output(['java', '-version'], stderr=subprocess.STDOUT)


def _ParseJavaVersion(version_string):
  """Returns a 2-tuple for the current version of Java installed.

  Args:
    version_string: String of the Java version (e.g. '1.7.2-ea').

  Returns:
    The major and minor versions, as a 2-tuple (e.g. (1, 7)).
  """
  match = _VERSION_REGEX.search(version_string)
  if match:
    version = tuple(int(x or 0) for x in match.groups())
    assert len(version) == 2
    return version


def _JavaSupports32BitMode():
  """Determines whether the JVM supports 32-bit mode on the platform."""
  # Suppresses process output to stderr and stdout from showing up in the
  # console as we're only trying to determine 32-bit JVM support.
  supported = False
  try:
    devnull = open(os.devnull, 'wb')
    return subprocess.call(
        ['java', '-d32', '-version'], stdout=devnull, stderr=devnull) == 0
  except IOError:
    pass
  else:
    devnull.close()
  return supported


def _GetJsCompilerArgs(compiler_jar_path, java_version, jvm_flags):
  """Assembles arguments for call to JsCompiler."""

  if java_version < (1, 7):
    raise JsCompilerError('Closure Compiler requires Java 1.7 or higher. '
                          'Please visit http://www.java.com/getjava')

  args = ['java']

  # Add JVM flags we believe will produce the best performance.  See
  # https://groups.google.com/forum/#!topic/closure-library-discuss/7w_O9-vzlj4

  # Attempt 32-bit mode if available (Java 7 on Mac OS X does not support 32-bit
  # mode, for example).
  if _JavaSupports32BitMode():
    args += ['-d32']

  # Prefer the "client" VM.
  args += ['-client']

  # Add JVM flags, if any
  if jvm_flags:
    args += jvm_flags

  # Add the application JAR.
  args += ['-jar', compiler_jar_path]

  return args


def _GetFlagFile(source_paths, compiler_flags):
  """Writes given source paths and compiler flags to a --flagfile.

  The given source_paths will be written as '--js' flags and the compiler_flags
  are written as-is.

  Args:
    source_paths: List of string js source paths.
    compiler_flags: List of string compiler flags.

  Returns:
    The file to which the flags were written.
  """
  args = []
  for path in source_paths:
    args += ['--js', path]

  # Add compiler flags, if any.
  if compiler_flags:
    args += compiler_flags

  flags_file = tempfile.NamedTemporaryFile(mode='w+t', delete=False)
  flags_file.write(' '.join(args))
  flags_file.close()

  return flags_file


def Compile(compiler_jar_path,
            source_paths,
            jvm_flags=None,
            compiler_flags=None):
  """Prepares command-line call to Closure Compiler.

  Args:
    compiler_jar_path: Path to the Closure compiler .jar file.
    source_paths: Source paths to build, in order.
    jvm_flags: A list of additional flags to pass on to JVM.
    compiler_flags: A list of additional flags to pass on to Closure Compiler.

  Returns:
    The compiled source, as a string, or None if compilation failed.
  """

  java_version = _ParseJavaVersion(str(_GetJavaVersionString()))

  args = _GetJsCompilerArgs(compiler_jar_path, java_version, jvm_flags)

  # Write source path arguments to flag file for avoiding "The filename or
  # extension is too long" error in big projects. See
  # https://github.com/google/closure-library/pull/678
  flags_file = _GetFlagFile(source_paths, compiler_flags)
  args += ['--flagfile', flags_file.name]

  logging.info('Compiling with the following command: %s', ' '.join(args))

  try:
    return subprocess.check_output(args)
  except subprocess.CalledProcessError:
    raise JsCompilerError('JavaScript compilation failed.')
  finally:
    os.remove(flags_file.name)
