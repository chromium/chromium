# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Functions for getting paths to things."""

import abc
import logging
import os

_STATUS_DETECTED = 1
_STATUS_VERIFIED = 2

# Src root of SuperSize being run. Not to be confused with src root of the input
# binary being archived.
_TOOLS_SRC_ROOT = os.environ.get(
    'CHECKOUT_SOURCE_ROOT',
    os.path.abspath(
        os.path.join(os.path.dirname(__file__), os.pardir, os.pardir,
                     os.pardir)))


class _PathFinder:
  def __init__(self, name, value):
    self._status = _STATUS_DETECTED if value is not None else 0
    self._name = name
    self._value = value

  @abc.abstractmethod
  def Detect(self):
    pass

  @abc.abstractmethod
  def Verify(self):
    pass

  def Tentative(self):
    if self._status < _STATUS_DETECTED:
      self._value = self.Detect()
      logging.debug('Detected --%s=%s', self._name, self._value)
      self._status = _STATUS_DETECTED
    return self._value

  def Finalized(self):
    if self._status < _STATUS_VERIFIED:
      self.Tentative()
      self.Verify()
      logging.info('Using --%s=%s', self._name, self._value)
      self._status = _STATUS_VERIFIED
    return self._value


class OutputDirectoryFinder(_PathFinder):
  def __init__(self, value=None, any_path_within_output_directory=None):
    super().__init__(name='output-directory', value=value)
    self._any_path_within_output_directory = any_path_within_output_directory

  def Detect(self):
    # Try and find build.ninja.
    abs_path = os.path.abspath(self._any_path_within_output_directory)
    while True:
      if os.path.exists(os.path.join(abs_path, 'build.ninja')):
        return os.path.relpath(abs_path)
      parent_dir = os.path.dirname(abs_path)
      if parent_dir == abs_path:
        break
      abs_path = parent_dir

    # See if CWD=output directory.
    if os.path.exists('build.ninja'):
      return '.'
    return None

  def Verify(self):
    if not self._value or not os.path.isdir(self._value):
      raise Exception(
          'Invalid --output-directory. Path not found: {}\n'
          'Use --no-output-directory to disable features that rely on it.'.
          format(self._value))

def GetSrcRootFromOutputDirectory(output_directory):
  """Returns the source root directory from output directory.

  Typical case: '/.../chromium/src/out/Release' -> '/.../chromium/src/'.
  Heuristic: Look for .gn in the current and successive parent directories.

  Args:
    output_directory: Starting point of search. This may be relative to CWD.

  Returns:
    Source root directory.
  """
  if output_directory:
    cur_dir = os.path.abspath(output_directory)
    while True:
      gn_path = os.path.join(cur_dir, '.gn')
      if os.path.isfile(gn_path):
        return cur_dir
      cur_dir, prev_dir = os.path.dirname(cur_dir), cur_dir
      if cur_dir == prev_dir:  # Reached root.
        break
  logging.warning('Cannot deduce src root from output directory. Falling back '
                  'to tools src root.')
  return _TOOLS_SRC_ROOT


def FromToolsSrcRoot(*args):
  ret = os.path.relpath(os.path.join(_TOOLS_SRC_ROOT, *args))
  # Need to maintain a trailing /.
  if args[-1].endswith(os.path.sep):
    ret += os.path.sep
  return ret


def _LlvmTool(name):
  default = FromToolsSrcRoot('third_party', 'llvm-build', 'Release+Asserts',
                             'bin', 'llvm-')
  actual = os.environ.get('SUPERSIZE_TOOL_PREFIX', default)
  # abspath since some executions use cwd= argument.
  return os.path.abspath(actual + name)


def CheckLlvmToolsAvailable():
  test_path = _LlvmTool('objdump')
  if not os.path.isfile(test_path):
    raise Exception(
        ('File not found: {}\nProbably need to run: '
         'tools/clang/scripts/update.py --package=objdump').format(test_path))


def GetCppFiltPath():
  return _LlvmTool('cxxfilt')


def GetDwarfdumpPath():
  return _LlvmTool('dwarfdump')


def GetNmPath():
  return _LlvmTool('nm')


def GetReadElfPath():
  return _LlvmTool('readelf')


def GetBcAnalyzerPath():
  return _LlvmTool('bcanalyzer')


def GetObjDumpPath():
  return _LlvmTool('objdump')


def GetDisassembleObjDumpPath(arch):
  path = None
  if arch == 'arm':
    path = FromToolsSrcRoot('third_party', 'android_toolchain', 'ndk',
                            'toolchains', 'arm-linux-androideabi-4.9',
                            'prebuilt', 'linux-x86_64', 'bin',
                            'arm-linux-androideabi-objdump')
  elif arch == 'arm64':
    path = FromToolsSrcRoot('third_party', 'android_toolchain', 'ndk',
                            'toolchains', 'aarch64-linux-android-4.9',
                            'prebuilt', 'linux-x86_64', 'bin',
                            'aarch64-linux-android-objdump')
  if path and os.path.exists(path):
    return path

  logging.warning('Falling back to llvm-objdump for arch %s', arch)
  return GetObjDumpPath()


def GetStripPath():
  # Chromium's toolchain uses //buildtools/third_party/eu-strip, but first
  # look for the test-only "fakestrip" for the sake of tests.
  fake_strip = _LlvmTool('fakestrip')
  if os.path.exists(fake_strip):
    return fake_strip
  return FromToolsSrcRoot('buildtools', 'third_party', 'eu-strip', 'bin',
                          'eu-strip')


def GetApkAnalyzerPath():
  default_path = FromToolsSrcRoot('third_party', 'android_build_tools',
                                  'apkanalyzer', 'apkanalyzer')
  return os.environ.get('SUPERSIZE_APK_ANALYZER', default_path)


def GetAapt2Path():
  default_path = FromToolsSrcRoot('third_party', 'android_build_tools', 'aapt2',
                                  'cipd', 'aapt2')
  return os.environ.get('SUPERSIZE_AAPT2', default_path)


def GetJavaHome():
  return FromToolsSrcRoot('third_party', 'jdk', 'current')


def GetJavaExec():
  return os.path.join(GetJavaHome(), 'bin', 'java')


def GetDefaultJsonConfigPath():
  return FromToolsSrcRoot('tools', 'binary_size', 'supersize.json')


def GetR8Path():
  return FromToolsSrcRoot('third_party', 'r8', 'lib', 'r8.jar')
