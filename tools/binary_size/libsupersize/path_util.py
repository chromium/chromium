# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Functions for dealing with determining --tool-prefix."""

import abc
import distutils.spawn
import json
import logging
import os

_STATUS_DETECTED = 1
_STATUS_VERIFIED = 2

# Src root of SuperSize being run. Not to be confused with src root of the input
# binary being archived.
TOOLS_SRC_ROOT = os.environ.get(
    'CHECKOUT_SOURCE_ROOT',
    os.path.abspath(
        os.path.join(
            os.path.dirname(__file__), os.pardir, os.pardir, os.pardir)))

_SAMPLE_TOOL_SUFFIX = 'readelf'


class _PathFinder(object):
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
    super(OutputDirectoryFinder, self).__init__(
        name='output-directory', value=value)
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


class ToolPrefixFinder(_PathFinder):
  def __init__(self, value=None, output_directory=None, linker_name=None):
    super(ToolPrefixFinder, self).__init__(
        name='tool-prefix', value=value)
    self._output_directory = output_directory
    self._linker_name = linker_name;

  def IsLld(self):
    return self._linker_name.startswith('lld') if self._linker_name else True

  def Detect(self):
    output_directory = self._output_directory
    if output_directory:
      ret = None
      if self.IsLld():
        ret = os.path.join(TOOLS_SRC_ROOT, 'third_party', 'llvm-build',
                           'Release+Asserts', 'bin', 'llvm-')
      else:
        # Auto-detect from build_vars.json
        build_vars = _LoadBuildVars(output_directory)
        tool_prefix = build_vars.get('android_tool_prefix')
        if tool_prefix:
          ret = os.path.normpath(os.path.join(output_directory, tool_prefix))
          # Maintain a trailing '/' if needed.
          if tool_prefix.endswith(os.path.sep):
            ret += os.path.sep
      if ret:
        # Check for output directories that have a stale build_vars.json
        if os.path.isfile(ret + _SAMPLE_TOOL_SUFFIX):
          return ret
        else:
          err_lines = ['tool-prefix not found: %s' % ret]
          if ret.endswith('llvm-'):
            err_lines.append('Probably need to run: '
                             'tools/clang/scripts/update.py --package=objdump')
          raise Exception('\n'.join(err_lines))
    from_path = distutils.spawn.find_executable(_SAMPLE_TOOL_SUFFIX)
    if from_path:
      return from_path[:-7]
    return None

  def Verify(self):
    if os.path.sep not in self._value:
      full_path = distutils.spawn.find_executable(
          self._value + _SAMPLE_TOOL_SUFFIX)
    else:
      full_path = self._value + _SAMPLE_TOOL_SUFFIX
    if not full_path or not os.path.isfile(full_path):
      raise Exception('Bad --%s. Path not found: %s' % (self._name, full_path))


def _LoadBuildVars(output_directory):
  build_vars_path = os.path.join(output_directory, 'build_vars.json')
  if os.path.exists(build_vars_path):
    with open(build_vars_path) as f:
      return json.load(f)
  return {}


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
  return TOOLS_SRC_ROOT


def FromToolsSrcRootRelative(path):
  ret = os.path.relpath(os.path.join(TOOLS_SRC_ROOT, path))
  # Need to maintain a trailing /.
  if path.endswith(os.path.sep):
    ret += os.path.sep
  return ret


def ToToolsSrcRootRelative(path):
  ret = os.path.relpath(path, TOOLS_SRC_ROOT)
  # Need to maintain a trailing /.
  if path.endswith(os.path.sep):
    ret += os.path.sep
  return ret


def GetCppFiltPath(tool_prefix):
  if tool_prefix[-5:] == 'llvm-':
    return tool_prefix + 'cxxfilt'
  return tool_prefix + 'c++filt'


def GetStripPath(tool_prefix):
  # Chromium's toolchain uses //buildtools/third_party/eu-strip, but first
  # look for the test-only "fakestrip" for the sake of tests.
  fake_strip = tool_prefix + 'fakestrip'
  if os.path.exists(fake_strip):
    return fake_strip
  return FromToolsSrcRootRelative(
      os.path.join('buildtools', 'third_party', 'eu-strip', 'bin', 'eu-strip'))


def GetNmPath(tool_prefix):
  return tool_prefix + 'nm'


def GetApkAnalyzerPath():
  default_path = FromToolsSrcRootRelative(
      os.path.join('third_party', 'android_sdk', 'public', 'cmdline-tools',
                   'latest', 'bin', 'apkanalyzer'))
  return os.environ.get('APK_ANALYZER', default_path)


def GetJavaHome():
  return FromToolsSrcRootRelative(os.path.join('third_party', 'jdk', 'current'))


def GetObjDumpPath(tool_prefix):
  return tool_prefix + 'objdump'


def GetReadElfPath(tool_prefix):
  return tool_prefix + 'readelf'


def GetBcAnalyzerPath(tool_prefix):
  if tool_prefix[-5:] != 'llvm-':
    raise ValueError('BC analyzer is only supported in LLVM.')
  return tool_prefix + 'bcanalyzer'
