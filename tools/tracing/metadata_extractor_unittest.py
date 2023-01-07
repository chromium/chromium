#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), os.pardir, 'perf'))

from core import path_util
path_util.AddPyUtilsToPath()
path_util.AddTracingToPath()

import metadata_extractor
from metadata_extractor import OSName
from core.tbmv3 import trace_processor

import mock


class ExtractMetadataTestCase(unittest.TestCase):
  def setUp(self):
    self.trace_processor_path = 'trace_processor_shell'
    self.trace_file = 'trace_file.proto'

  def _RunQueryParams(self, query):
    """Returns tuple of RunQuery function parameters.

    Args:
      query: sql query to extract metadata from proto trace.
    """
    return (self.trace_processor_path, self.trace_file, query)

  def _CreateRunQueryResults(self,
                             version_number_results=frozenset([]),
                             os_name_results=frozenset([]),
                             architecture_results=frozenset([]),
                             bitness_results=frozenset([]),
                             version_code_results=frozenset([]),
                             modules_results=frozenset([])):
    """Mock return of RunQuery calls.

    See trace_processor.RunQuery for the format of the query results.
    Each parameter is a result dictionary for corresponding SQL query
    defined in metadata_extractor.py. For example, valid value for
    os_name_results = [{'str_value' : 'pineapple'}]

    Returns:
      A dictionary mapping of RunQuery parameters to their mocked
      RunQuery function return values.
    """
    return {
        self._RunQueryParams(metadata_extractor.VERSION_NUM_QUERY):
        version_number_results,
        self._RunQueryParams(metadata_extractor.OS_NAME_QUERY): os_name_results,
        self._RunQueryParams(metadata_extractor.ARCH_QUERY):
        architecture_results,
        self._RunQueryParams(metadata_extractor.BITNESS_QUERY): bitness_results,
        self._RunQueryParams(metadata_extractor.VERSION_CODE_QUERY):
        version_code_results,
        self._RunQueryParams(metadata_extractor.MODULES_QUERY): modules_results
    }

  def _CreateRunQueryResultsFromValues(self,
                                       version_number=None,
                                       os_name=None,
                                       architecture=None,
                                       bitness=None,
                                       version_code=None,
                                       modules=None):
    """Mock return of RunQuery calls by values, except for modules.

    Args:
      version_number: string containing chrome version number
        (eg: 'Chrome/93.0.4537.0').
      os_name: string of platform of the trace writer (eg. 'Android').
      architecture: string of OS arch of the trace writer, as returned by
        base::SysInfo::OperatingSystemArchitecture() (eg: 'x86_64).
      bitness: string of architecture bitness (eg. '32', '64').
      version_code: string of version code of chrome used by Android
        play store.
      modules: list of dictionaries mock return value of RunQuery
        when its called with sql query metadata_extractor.MODULES_QUERY
        See _CreateRunQueryResults function for more information.
        (eg: [{'name': '/libmonochrome.so, 'build_id': '3284389AB83CD'}]).

    Returns:
      A dictionary mapping of RunQuery parameters to their mocked
      RunQuery function return values.
    """
    return self._CreateRunQueryResults(version_number_results=[{
        'str_value':
        version_number
    }],
                                       os_name_results=[{
                                           'str_value': os_name
                                       }],
                                       architecture_results=[{
                                           'str_value':
                                           architecture
                                       }],
                                       bitness_results=[{
                                           'int_value': bitness
                                       }],
                                       version_code_results=[{
                                           'int_value':
                                           version_code
                                       }],
                                       modules_results=modules)

  def testExtractMetadata(self):
    def side_effect(*args):
      params = self._CreateRunQueryResultsFromValues(
          version_number='Chrome/36.9.7934.4',
          os_name='Android',
          architecture='x86_64',
          bitness='64',
          version_code='857854',
          modules=[{
              'name': '/libmonochrome.so',
              'build_id': '3284389AB83CD'
          }, {
              'name': '/missing',
              'build_id': 'AB3288CDE3283'
          }, {
              'name': '/chrome.so',
              'build_id': 'abcdef'
          }])
      return params[args]

    extractor = metadata_extractor.MetadataExtractor(self.trace_processor_path,
                                                     self.trace_file)
    trace_processor.RunQuery = mock.MagicMock(side_effect=side_effect)
    extractor.Initialize()

    self.assertEqual(extractor.version_number, '36.9.7934.4')
    self.assertEqual(extractor.os_name, OSName.ANDROID)
    self.assertEqual(extractor.architecture, 'x86_64')
    self.assertEqual(extractor.bitness, '64')
    self.assertEqual(extractor.version_code, '857854')
    self.assertEqual(extractor.modules, {
        '/libmonochrome.so': '3284389AB83CD',
        '/chrome.so': 'ABCDEF'
    })

  def testExtractMetadataEmptyList(self):
    def side_effect(*args):
      params = self._CreateRunQueryResults()
      return params[args]

    extractor = metadata_extractor.MetadataExtractor(self.trace_processor_path,
                                                     self.trace_file)
    trace_processor.RunQuery = mock.MagicMock(side_effect=side_effect)
    extractor.Initialize()

    self.assertEqual(extractor.version_number, None)
    self.assertEqual(extractor.os_name, None)
    self.assertEqual(extractor.architecture, None)
    self.assertEqual(extractor.bitness, None)
    self.assertEqual(extractor.version_code, None)
    self.assertEqual(extractor.modules, None)

  def testExtractMetadataValuesNull(self):
    def side_effect(*args):
      params = self._CreateRunQueryResultsFromValues(modules=[{
          'name': None,
          'build_id': None
      }, {
          'name': None,
          'build_id': None
      }])
      return params[args]

    extractor = metadata_extractor.MetadataExtractor(self.trace_processor_path,
                                                     self.trace_file)
    trace_processor.RunQuery = mock.MagicMock(side_effect=side_effect)
    extractor.Initialize()

    self.assertEqual(extractor.version_number, None)
    self.assertEqual(extractor.os_name, None)
    self.assertEqual(extractor.architecture, None)
    self.assertEqual(extractor.bitness, None)
    self.assertEqual(extractor.version_code, None)
    self.assertEqual(extractor.modules, None)

  def testExtractMetadataVersionNumberParsed(self):
    def side_effect(*args):
      params = self._CreateRunQueryResultsFromValues(
          version_number='36.9.7934.4')
      return params[args]

    extractor = metadata_extractor.MetadataExtractor(self.trace_processor_path,
                                                     self.trace_file)
    trace_processor.RunQuery = mock.MagicMock(side_effect=side_effect)
    extractor.Initialize()

    self.assertEqual(extractor.version_number, '36.9.7934.4')

  def testParseOSNameAndroid(self):
    def side_effect(*args):
      params = self._CreateRunQueryResultsFromValues(os_name='Android')
      return params[args]

    extractor = metadata_extractor.MetadataExtractor(self.trace_processor_path,
                                                     self.trace_file)
    trace_processor.RunQuery = mock.MagicMock(side_effect=side_effect)
    extractor.Initialize()

    self.assertEqual(extractor.os_name, OSName.ANDROID)

  def testParseOSNameLinux(self):
    def side_effect(*args):
      params = self._CreateRunQueryResultsFromValues(os_name='Linux')
      return params[args]

    extractor = metadata_extractor.MetadataExtractor(self.trace_processor_path,
                                                     self.trace_file)
    trace_processor.RunQuery = mock.MagicMock(side_effect=side_effect)
    extractor.Initialize()

    self.assertEqual(extractor.os_name, OSName.LINUX)

  def testParseMac64(self):
    def side_effect(*args):
      params = self._CreateRunQueryResultsFromValues(
          os_name='Mac OS X', version_number='Chrome/28.9.9364.32-64')
      return params[args]

    extractor = metadata_extractor.MetadataExtractor(self.trace_processor_path,
                                                     self.trace_file)
    trace_processor.RunQuery = mock.MagicMock(side_effect=side_effect)
    extractor.Initialize()

    self.assertEqual(extractor.version_number, '28.9.9364.32')
    self.assertEqual(extractor.os_name, OSName.MAC)

  def testParseOSNameWindows(self):
    def side_effect(*args):
      params = self._CreateRunQueryResultsFromValues(os_name='Windows NT')
      return params[args]

    extractor = metadata_extractor.MetadataExtractor(self.trace_processor_path,
                                                     self.trace_file)
    trace_processor.RunQuery = mock.MagicMock(side_effect=side_effect)
    extractor.Initialize()

    self.assertEqual(extractor.os_name, OSName.WINDOWS)

  def testParseOSNameCrOS(self):
    def side_effect(*args):
      params = self._CreateRunQueryResultsFromValues(os_name='CrOS')
      return params[args]

    extractor = metadata_extractor.MetadataExtractor(self.trace_processor_path,
                                                     self.trace_file)
    trace_processor.RunQuery = mock.MagicMock(side_effect=side_effect)
    extractor.Initialize()

    self.assertEqual(extractor.os_name, OSName.CROS)

  def testParseOSNameFuschia(self):
    def side_effect(*args):
      params = self._CreateRunQueryResultsFromValues(os_name='Fuschia')
      return params[args]

    extractor = metadata_extractor.MetadataExtractor(self.trace_processor_path,
                                                     self.trace_file)
    trace_processor.RunQuery = mock.MagicMock(side_effect=side_effect)
    extractor.Initialize()

    self.assertEqual(extractor.os_name, OSName.FUSCHIA)

  def testParseOSNameNotRecognized(self):
    def side_effect(*args):
      params = self._CreateRunQueryResultsFromValues(os_name='blah')
      return params[args]

    extractor = metadata_extractor.MetadataExtractor(self.trace_processor_path,
                                                     self.trace_file)
    trace_processor.RunQuery = mock.MagicMock(side_effect=side_effect)

    exception_msg = 'OS name "blah" not recognized: ' + self.trace_file
    with self.assertRaises(Exception) as context:
      extractor.Initialize()
    self.assertEqual(exception_msg, str(context.exception))

  def testGetModuleIds(self):
    extractor = metadata_extractor.MetadataExtractor(self.trace_processor_path,
                                                     self.trace_file)
    extractor.InitializeForTesting(modules={
        'name': '13423EDFAB2',
        'name2': '321468945',
        'name3': '4093492737482'
    })
    self.assertEqual(extractor.GetModuleIds(),
                     {'13423EDFAB2', '321468945', '4093492737482'})

  def testGetModuleIdsEmpty(self):
    extractor = metadata_extractor.MetadataExtractor(self.trace_processor_path,
                                                     self.trace_file)
    extractor.InitializeForTesting(modules={})
    self.assertEqual(extractor.GetModuleIds(), set())

  def testGetModuleIdsNone(self):
    extractor = metadata_extractor.MetadataExtractor(self.trace_processor_path,
                                                     self.trace_file)
    extractor.InitializeForTesting(modules=None)
    self.assertEqual(extractor.GetModuleIds(), None)


if __name__ == '__main__':
  unittest.main()
