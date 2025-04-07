# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import sys
import tempfile
import unittest

# Force local directory to be in front of sys.path to avoid importing different
# version of PRESUBMIT.py which can be added in different files within python
# invocation.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import PRESUBMIT
from presubmit_caching_support import PresubmitCache

# Append chrome source root to import `PRESUBMIT_test_mocks.py`.
sys.path.append(
    os.path.dirname(
        os.path.dirname(
            os.path.dirname(os.path.dirname(os.path.abspath(__file__))))))
from PRESUBMIT_test_mocks import MockAffectedFile, MockInputApi, MockOutputApi

_BASE_DIR = os.path.dirname(os.path.abspath(__file__))
_TOP_LEVEL_ENUMS_PATH = (f'{os.path.dirname(__file__)}/enums.xml')


def _TempCacheFile():
  file_handle, file_path = tempfile.mkstemp(suffix='.json', text=True)
  os.close(file_handle)
  return file_path


def _MockInputFromTestFile(relative_path: str) -> (MockInputApi, str):
  """ Returns a MockInputApi that list a file relative to test_data/ as changed.

  The provided file is read and its contents are provided to the MockInputApi.

  Args:
    relative_path: The relative path to the file to mock.

  Returns:
    A MockInputApi that lists the provided file as only one changed and full
    path to that file.
  """
  full_path = f'{os.path.dirname(__file__)}/test_data/{relative_path}'
  with open(full_path, 'r') as f:
    contents = f.read()

  mock_input_api = MockInputApi()
  mock_input_api.presubmit_local_path = _BASE_DIR
  mock_input_api.files = [
      MockAffectedFile(full_path, [contents]),
  ]
  return (mock_input_api, full_path)


def _MockInputFromString(path: str, contents: str) -> MockInputApi:
  """ Returns a MockInputApi with single changed file with given contents.Api.

  Args:
    path: Fake path to the file to mock.
    contents: The contents of the file to mock.

  Returns:
    A MockInputApi that lists the provided file as only one changed.
  """
  mock_input_api = MockInputApi()
  mock_input_api.presubmit_local_path = _BASE_DIR
  mock_input_api.files = [
      MockAffectedFile(path, [contents]),
  ]
  return mock_input_api


class MetricsPresubmitTest(unittest.TestCase):

  def testCheckHistogramFormattingFailureIsDetected(self):
    (mock_input_api, malformed_histograms_path
     ) = _MockInputFromTestFile('tokens/token_errors_histograms.xml')

    results = PRESUBMIT.ExecuteCheckHistogramFormatting(
        mock_input_api,
        MockOutputApi(),
        allow_test_paths=True,
        xml_paths_override=[malformed_histograms_path, _TOP_LEVEL_ENUMS_PATH])

    self.assertEqual(len(results), 2)

    self.assertEqual(results[0].type, 'error')
    self.assertRegex(
        results[0].message,
        '.*histograms.xml contains histogram.* using <variants> not defined in'
        ' the file, please run validate_token.py .*histograms.xml to fix.')

    # validate_format.py also reports errors when the variants are not defined
    # in the file, hence there is a second error from the same check.
    self.assertEqual(results[1].type, 'error')
    self.assertRegex(
        results[1].message,
        'Histograms are not well-formatted; please run .*validate_format.py and'
        ' fix the reported errors.')

  def testCheckWebViewHistogramsAllowlistOnUploadFailureIsDetected(self):
    valid_enums_path = (f'{os.path.dirname(__file__)}'
                        '/test_data'
                        '/example_valid_enums.xml')
    example_allowlist_path = (f'{os.path.dirname(__file__)}'
                              '/test_data'
                              '/AllowlistExample.java')

    (mock_input_api, missing_allow_list_entries_histograms_path
     ) = _MockInputFromTestFile('no_allowlist_entries_histograms.xml')

    results = PRESUBMIT.ExecuteCheckWebViewHistogramsAllowlistOnUpload(
        mock_input_api,
        MockOutputApi(),
        allowlist_path_override=example_allowlist_path,
        xml_paths_override=[
            missing_allow_list_entries_histograms_path, valid_enums_path,
            _TOP_LEVEL_ENUMS_PATH
        ],
    )
    self.assertEqual(len(results), 1)
    self.assertRegex(
        results[0].message.replace('\n', ' '), 'All histograms in'
        ' .*AllowlistExample.java must be valid.')
    self.assertEqual(results[0].type, 'error')

  def testCheckBooleansAreEnumsFailureIsDetected(self):
    mock_input_api = _MockInputFromString(
        'histograms.xml', '<histogram name="Foo" units="Boolean" />')

    results = PRESUBMIT.ExecuteCheckBooleansAreEnums(mock_input_api,
                                                     MockOutputApi())
    self.assertEqual(len(results), 1)
    self.assertRegex(
        results[0].message.replace('\n', ' '),
        '.*You are using .units. for a boolean histogram, but you should be'
        ' using\\s+.enum. instead\\.')
    self.assertEqual(results[0].type, 'promptOrNotify')

  def testCheckHistogramFormattingPasses(self):
    valid_enums_path = (f'{os.path.dirname(__file__)}'
                        '/test_data'
                        '/example_valid_enums.xml')

    (mock_input_api, valid_histograms_path
     ) = _MockInputFromTestFile('example_valid_histograms.xml')

    results = PRESUBMIT.ExecuteCheckHistogramFormatting(
        mock_input_api,
        MockOutputApi(),
        allow_test_paths=True,
        xml_paths_override=[
            valid_histograms_path, valid_enums_path, _TOP_LEVEL_ENUMS_PATH
        ])
    # Zero results mean that there were no errors reported.
    self.assertEqual(len(results), 0)

  def testCheckWebViewHistogramsAllowlistOnUploadPasses(self):
    valid_enums_path = (f'{os.path.dirname(__file__)}'
                        '/test_data'
                        '/example_valid_enums.xml')
    example_allowlist_path = (f'{os.path.dirname(__file__)}'
                              '/test_data'
                              '/AllowlistExample.java')

    (mock_input_api, valid_histograms_path
     ) = _MockInputFromTestFile('example_valid_histograms.xml')

    results = PRESUBMIT.ExecuteCheckWebViewHistogramsAllowlistOnUpload(
        mock_input_api,
        MockOutputApi(),
        allowlist_path_override=example_allowlist_path,
        xml_paths_override=[
            valid_histograms_path, valid_enums_path, _TOP_LEVEL_ENUMS_PATH
        ])
    # Zero results mean that there were no errors reported.
    self.assertEqual(len(results), 0)

  def testCheckBooleansAreEnumsPasses(self):
    mock_input_api = _MockInputFromString(
        'histograms.xml', '<histogram name="Foo" enum="Boolean" />')

    results = PRESUBMIT.ExecuteCheckBooleansAreEnums(mock_input_api,
                                                     MockOutputApi())
    # Zero results mean that there were no errors reported.
    self.assertEqual(len(results), 0)

  def _CacheSize(self, cache_file_path, observed_directory_path):
    cache = PresubmitCache(cache_file_path, observed_directory_path)
    return len(cache.InspectCacheForTesting().data)

  def testSecondCheckOnTheSameDataReturnsSameResult(self):
    test_cache_file = _TempCacheFile()
    mock_input_api = _MockInputFromString(
        'histograms.xml', '<histogram name="Foo" units="Boolean" />')

    # The cache should be empty before we run any presubmit checks.
    self.assertEqual(self._CacheSize(test_cache_file, _BASE_DIR), 0)

    results = PRESUBMIT.CheckBooleansAreEnums(mock_input_api,
                                              MockOutputApi(),
                                              cache_file_path=test_cache_file)
    self.assertEqual(len(results), 1)
    self.assertRegex(
        results[0].message.replace('\n', ' '),
        '.*You are using .units. for a boolean histogram, but you should be'
        ' using\\s+.enum. instead\\.')
    self.assertEqual(results[0].type, 'promptOrNotify')

    # The cache should now store a single entry for the check above.
    self.assertEqual(self._CacheSize(test_cache_file, _BASE_DIR), 1)

    second_results = PRESUBMIT.CheckBooleansAreEnums(
        mock_input_api, MockOutputApi(), cache_file_path=test_cache_file)
    self.assertEqual(len(second_results), 1)
    self.assertEqual(results[0].message, second_results[0].message)
    self.assertEqual(results[0].type, second_results[0].type)

    # The check result should be retrieved from the cache and the cache should
    # still have only one entry.
    self.assertEqual(self._CacheSize(test_cache_file, _BASE_DIR), 1)

  def testSecondCheckOnTheSameDataReturnsSameEmptyResult(self):
    test_cache_file = _TempCacheFile()
    mock_input_api = _MockInputFromString(
        'histograms.xml', '<histogram name="Foo" enum="Boolean" />')

    # The cache should be empty before we run any presubmit checks.
    self.assertEqual(self._CacheSize(test_cache_file, _BASE_DIR), 0)

    results = PRESUBMIT.CheckBooleansAreEnums(mock_input_api,
                                              MockOutputApi(),
                                              cache_file_path=test_cache_file)
    # Zero results mean that there were no errors reported.
    self.assertEqual(len(results), 0)

    # The cache should now store a single entry for the check above.
    self.assertEqual(self._CacheSize(test_cache_file, _BASE_DIR), 1)

    second_results = PRESUBMIT.CheckBooleansAreEnums(
        mock_input_api, MockOutputApi(), cache_file_path=test_cache_file)
    # Zero results mean that there were no errors reported.
    self.assertEqual(len(second_results), 0)

    # The check result should be retrieved from the cache and the cache should
    # still have only one entry.
    self.assertEqual(self._CacheSize(test_cache_file, _BASE_DIR), 1)

  def testFailureInModifiedFileIsDetected(self):
    test_dir = tempfile.mkdtemp()
    initial_histograms_content = '<histogram name="Foo" enum="Boolean" />'
    modified_histograms_content = '<histogram name="Foo" units="Boolean" />'
    histograms_path = os.path.join(test_dir, 'histograms.xml')

    with open(histograms_path, 'w') as f:
      f.write(initial_histograms_content)

    test_cache_file = _TempCacheFile()
    mock_input_api = MockInputApi()
    mock_input_api.presubmit_local_path = test_dir
    mock_input_api.files = [
        MockAffectedFile('histograms.xml', [initial_histograms_content]),
    ]

    # The cache should be empty before we run any presubmit checks.
    self.assertEqual(self._CacheSize(test_cache_file, test_dir), 0)

    results = PRESUBMIT.CheckBooleansAreEnums(mock_input_api,
                                              MockOutputApi(),
                                              cache_file_path=test_cache_file)
    # Zero results mean that there were no errors reported.
    self.assertEqual(len(results), 0)

    # The cache should now store a single entry for the check above.
    self.assertEqual(self._CacheSize(test_cache_file, test_dir), 1)

    with open(histograms_path, 'w') as f:
      f.write(modified_histograms_content)

    mock_input_api.files = [
        MockAffectedFile('histograms.xml', [modified_histograms_content]),
    ]

    second_results = PRESUBMIT.CheckBooleansAreEnums(
        mock_input_api, MockOutputApi(), cache_file_path=test_cache_file)

    self.assertEqual(len(second_results), 1)
    self.assertRegex(
        second_results[0].message.replace('\n', ' '),
        '.*You are using .units. for a boolean histogram, but you should be'
        ' using\\s+.enum. instead\\.')
    self.assertEqual(second_results[0].type, 'promptOrNotify')

    # The cache should now have an extra entry as the second check was done on
    # a different version of the file.
    self.assertEqual(self._CacheSize(test_cache_file, test_dir), 2)

  def testRegisteredVariantsArePassingValidation(self):
    valid_tokens_histograms_relative_paths = [
        'tokens/variants_inline_histograms.xml',
        'tokens/variants_out_of_line_explicit_histograms.xml',
        'tokens/variants_out_of_line_implicit_histograms.xml',
    ]
    for relative_path in valid_tokens_histograms_relative_paths:
      (mock_input_api, input_path) = _MockInputFromTestFile(relative_path)

      results = PRESUBMIT.ExecuteCheckHistogramFormatting(
          mock_input_api,
          MockOutputApi(),
          allow_test_paths=True,
          xml_paths_override=[input_path])

      self.assertEqual(len(results), 0)

  def testNonregisteredVariantsAreFailingValidation(self):
    (mock_input_api, input_path
     ) = _MockInputFromTestFile('tokens/variants_missing_histograms.xml')

    results = PRESUBMIT.ExecuteCheckHistogramFormatting(
        mock_input_api,
        MockOutputApi(),
        allow_test_paths=True,
        xml_paths_override=[input_path, _TOP_LEVEL_ENUMS_PATH])

    self.assertEqual(len(results), 1)

    # validate_format.py also reports all errors as Histogram malformatted
    # errors, detailed errors are printed and the error itself refers back
    # to validate_format.py.
    self.assertEqual(results[0].type, 'error')
    self.assertRegex(
        results[0].message,
        'Histograms are not well-formatted; please run .*validate_format.py and'
        ' fix the reported errors.')

  def testDeletedFileIsIgnoredByBooleansAreEnumsCheck(self):
    mock_input_api = MockInputApi()
    mock_input_api.presubmit_local_path = _BASE_DIR
    mock_input_api.files = [
        MockAffectedFile('histograms.xml',
                         ['<histogram name="Foo" units="Boolean" />'],
                         action='D'),
    ]

    results = PRESUBMIT.ExecuteCheckBooleansAreEnums(mock_input_api,
                                                     MockOutputApi())

    # Zero results mean that there were no errors reported.
    self.assertEqual(len(results), 0)

  def testDeletedFileIsIgnoredByHistogramFormattingCheck(self):
    full_path = (f'{os.path.dirname(__file__)}'
                 '/test_data/non_existing_histograms.xml')

    mock_input_api = MockInputApi()
    mock_input_api.presubmit_local_path = _BASE_DIR
    mock_input_api.files = [
        MockAffectedFile(full_path, [], action='D'),
    ]

    results = PRESUBMIT.ExecuteCheckHistogramFormatting(
        mock_input_api,
        MockOutputApi(),
        allow_test_paths=True,
        xml_paths_override=[_TOP_LEVEL_ENUMS_PATH])

    # Zero results mean that there were no errors reported.
    self.assertEqual(len(results), 0)

  def testDeletedFileIsIgnoredByAllowlistCheck(self):
    non_existing_histograms_path = (f'{os.path.dirname(__file__)}'
                                    '/test_data/non_existing_histograms.xml')
    valid_histograms_path = (f'{os.path.dirname(__file__)}'
                             '/test_data'
                             '/example_valid_histograms.xml')
    valid_enums_path = (f'{os.path.dirname(__file__)}'
                        '/test_data'
                        '/example_valid_enums.xml')
    example_allowlist_path = (f'{os.path.dirname(__file__)}'
                              '/test_data'
                              '/AllowlistExample.java')

    with open(valid_histograms_path, 'r') as f:
      valid_histograms_contents = f.read()
    mock_input_api = MockInputApi()
    mock_input_api.presubmit_local_path = _BASE_DIR
    mock_input_api.files = [
        MockAffectedFile(non_existing_histograms_path, [], action='D'),
        MockAffectedFile(valid_histograms_path, [valid_histograms_contents]),
    ]

    results = PRESUBMIT.ExecuteCheckWebViewHistogramsAllowlistOnUpload(
        mock_input_api,
        MockOutputApi(),
        allowlist_path_override=example_allowlist_path,
        xml_paths_override=[
            valid_histograms_path, valid_enums_path, _TOP_LEVEL_ENUMS_PATH
        ])

    # Zero results mean that there were no errors reported.
    self.assertEqual(len(results), 0)


if __name__ == '__main__':
  unittest.main()
