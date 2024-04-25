# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Extracts metadata information from proto traces.
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), os.pardir, 'perf'))

from core.tbmv3 import trace_processor

VERSION_NUM_QUERY = (
    'select str_value from metadata where name="cr-product-version"')
OS_NAME_QUERY = 'select str_value from metadata where name="cr-os-name"'
ARCH_QUERY = 'select str_value from metadata where name="cr-os-arch"'
BITNESS_QUERY = (
    'select int_value from metadata where name="cr-chrome-bitness"')
VERSION_CODE_QUERY = (
    'select int_value from metadata where name="cr-playstore_version_code"')
MODULES_QUERY = 'select name, build_id from stack_profile_mapping'


class OSName():
  ANDROID = 'Android'
  LINUX = 'Linux'
  MAC = 'Mac OS X'
  WINDOWS = 'Windows NT'
  CROS = 'CrOS'
  FUSCHIA = 'Fuschia'


class MetadataExtractor:
  """Extracts and stores metadata from a perfetto trace.

  Attributes:
    _initialized: boolean of whether the class has been
      initialized or not by calling the Initialize function.
    _trace_processor_path: path to the trace_processor executable.
    _trace_file: path to a perfetto system trace file.
    version_number: chrome version number (eg: 93.0.4537.0).
    os_name: platform of the trace writer of type OSName
      (eg. OSName.Android).
    architecture: OS arch of the trace writer, as returned by
      base::SysInfo::OperatingSystemArchitecture() (eg: 'x86_64).
    bitness: integer of architecture bitness (eg. 32, 64).
    version_code: version code of chrome used by Android play store.
    modules: map from module name to module debug ID, for all
      modules that need symbolization.
  """

  def __init__(self, trace_processor_path, trace_file):
    self._initialized = False
    self._trace_processor_path = trace_processor_path
    self._trace_file = trace_file
    self.version_number = None
    self.os_name = None
    self.architecture = None
    self.bitness = None
    self.version_code = None
    self.modules = None

  def __str__(self):
    return ('Initialized: {initialized}\n'
            'Trace Processor Path: {trace_processor_path}\n'
            'Trace File: {trace_file}\n'
            'Version Number: {version_number}\n'
            'OS Name: {os_name}\n'
            'Architecture: {architecture}\n'
            'Bitness: {bitness}\n'
            'Version Code: {version_code}\n'
            'Modules: {modules}\n'.format(
                initialized=self._initialized,
                trace_processor_path=self._trace_processor_path,
                trace_file=self._trace_file,
                version_number=self.version_number,
                os_name=self.os_name,
                architecture=self.architecture,
                bitness=self.bitness,
                version_code=self.version_code,
                modules=self.modules))

  @property
  def trace_file(self):
    return self._trace_file

  def GetModuleIds(self):
    """Returns set of all module IDs in |modules| field.
    """
    self.Initialize()
    if self.modules is None:
      return None
    return set(self.modules.values())

  def Initialize(self):
    """Extracts metadata from perfetto system trace.
    """
    # TODO(crbug.com/40193968): Implement Trace Processor method to run multiple
    # SQL queries without processing trace for every query.

    if self._initialized:
      return
    self._initialized = True

    # Version Number query returns the name and number (Chrome/93.0.4537.0).
    # Parse the result to only get the version number.
    version_number = self._GetStringValueFromQuery(VERSION_NUM_QUERY)
    if version_number is None:
      self.version_number = None
    elif version_number.count('/') == 1:
      self.version_number = version_number.split('/')[1]
    else:
      self.version_number = version_number
    # Mac 64 traces add '-64' after the version number.
    if self.version_number is not None and self.version_number.endswith('-64'):
      self.version_number = self.version_number[:-3]

    raw_os_name = self._GetStringValueFromQuery(OS_NAME_QUERY)
    self.os_name = self._ParseOSName(raw_os_name)

    self.architecture = self._GetStringValueFromQuery(ARCH_QUERY)
    self.bitness = self._GetIntValueFromQuery(BITNESS_QUERY)
    self.version_code = self._GetIntValueFromQuery(VERSION_CODE_QUERY)

    # Parse module to be a mapping between module name and debug id
    self.modules = self._ExtractValidModuleMap()

  def _ParseOSName(self, raw_os_name):
    """Parsed OS name string into an enum.

    Args:
      raw_os_name: An OS name string returned from
        base::SysInfo::OperatingSystemName().

    Returns:
      An enum of type OSName.

    Raises:
      Exception: If OS name string is not recognized.
    """
    if raw_os_name is None:
      return None

    if raw_os_name == 'Android':
      return OSName.ANDROID
    if raw_os_name == 'Linux':
      return OSName.LINUX
    if raw_os_name == 'Mac OS X':
      return OSName.MAC
    if raw_os_name == 'Windows NT':
      return OSName.WINDOWS
    if raw_os_name == 'CrOS':
      return OSName.CROS
    if raw_os_name == 'Fuschia':
      return OSName.FUSCHIA
    raise Exception('OS name "%s" not recognized: %s' %
                    (raw_os_name, self._trace_file))

  def InitializeForTesting(self,
                           version_number=None,
                           os_name=None,
                           architecture=None,
                           bitness=None,
                           version_code=None,
                           modules=None):
    """Sets class parameter values for test cases.

    The |trace_processor_path| and |trace_file| parameters should
    be specified in the constructor.
    """
    self._initialized = True
    self.version_number = version_number
    self.os_name = os_name
    self.architecture = architecture
    self.bitness = bitness
    self.version_code = version_code
    self.modules = modules

  def _GetStringValueFromQuery(self, sql):
    """Runs SQL query on trace processor and returns 'str_value' result.
    """
    try:
      return trace_processor.RunQuery(self._trace_processor_path,
                                      self._trace_file, sql)[0]['str_value']
    except Exception:
      return None

  def _GetIntValueFromQuery(self, sql):
    """Runs SQL query on trace processor and returns 'int_value' result.
    """
    try:
      return trace_processor.RunQuery(self._trace_processor_path,
                                      self._trace_file, sql)[0]['int_value']
    except Exception:
      return None

  def _ExtractValidModuleMap(self):
    """Extracts valid module name to module debug ID map/dict from trace.
    """
    try:
      query_result = trace_processor.RunQuery(self._trace_processor_path,
                                              self._trace_file, MODULES_QUERY)
      module_map = {}
      for row in query_result:
        row_name = row['name']
        row_debug_id = row['build_id']
        # Discard invalid key, value pairs
        if ((row_name is None or row_name == '/missing')
            or (row_debug_id is None or row_debug_id == '/missing')):
          continue
        module_map[row_name] = row_debug_id.upper()

      if not module_map:
        return None
      return module_map

    except Exception:
      return None
