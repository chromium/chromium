# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import os
import tempfile
import unittest
from unittest import mock
from typing import Tuple
import xml.etree.ElementTree as ET

import setup_modules  # pylint: disable=unused-import

import chromium_src.PRESUBMIT_test_mocks as PRESUBMIT_test_mocks
import chromium_src.tools.metrics.common.path_util as path_util
import chromium_src.tools.metrics.common.presubmit_util as presubmit_util
import chromium_src.tools.metrics.histograms.histogram_paths as histogram_paths
import chromium_src.tools.metrics.histograms.PRESUBMIT as PRESUBMIT

# Monkeypatch MockInputApi to have fallback ReadFile to avoid modifying
# PRESUBMIT_test_mocks.py.
_orig_ReadFile = PRESUBMIT_test_mocks.MockInputApi.ReadFile


def _fallback_ReadFile(self, filename, mode='r'):
  try:
    return _orig_ReadFile(self, filename, mode)
  except IOError:
    if hasattr(filename, 'AbsoluteLocalPath'):
      filename = filename.AbsoluteLocalPath()
    norm_filename = os.path.normpath(filename)
    if os.path.exists(norm_filename):
      with open(norm_filename, mode) as f:
        return f.read()
    raise


PRESUBMIT_test_mocks.MockInputApi.ReadFile = (  # type: ignore[method-assign]
  _fallback_ReadFile
)

_BASE_DIR = str(path_util.METRICS_TOOLS_PATH / 'histograms')
_TOP_LEVEL_ENUMS_PATH = str(
  path_util.METRICS_TOOLS_PATH / 'histograms' / 'enums.xml'
)

_INITIAL_HISTOGRAMS_CONTENT = '<histogram name="Foo" enum="Boolean" />'
_MODIFIED_HISTOGRAMS_CONTENT = '<histogram name="Foo" units="Boolean" />'
_HISTOGRAMS_WITH_VARIANT_TEMPLATE = """\
<histogram-configuration>
  <histograms>
    <variants name="TestVariant">
      <variant name="One" summary="{variant_summary}" />
    </variants>
    <histogram name="Segmentation.Test.{{Variant}}" units="count"
        expires_after="M200">
      <owner>owner@chromium.org</owner>
      <summary>Records {{Variant}} value.</summary>
      <token key="Variant" variants="TestVariant" />
    </histogram>
  </histograms>
</histogram-configuration>
"""


def _TempCacheDir():
  return tempfile.mkdtemp()


def _PrepareTestWorkingDirectory():
  test_dir = tempfile.mkdtemp()
  histograms_path = os.path.join(test_dir, 'histograms.xml')
  with open(histograms_path, 'w') as f:
    f.write(_INITIAL_HISTOGRAMS_CONTENT)
  return test_dir, histograms_path


def _MockInputFromTestFile(
  relative_path: str,
) -> Tuple[PRESUBMIT_test_mocks.MockInputApi, str]:
  """Returns a MockInputApi that list a file relative to test_data/ as changed.

  The provided file is read and its contents are provided to the MockInputApi.

  Args:
    relative_path: The relative path to the file to mock.

  Returns:
    A MockInputApi that lists the provided file as only one changed and full
    path to that file.
  """
  full_path = str(
    path_util.METRICS_TOOLS_PATH / 'histograms' / 'test_data' / relative_path
  )
  with open(full_path, 'r') as f:
    contents = f.read()

  mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
  mock_input_api.presubmit_local_path = _BASE_DIR
  mock_input_api.files = [
    PRESUBMIT_test_mocks.MockAffectedFile(full_path, [contents]),
  ]
  return (mock_input_api, full_path)


def _MockInputFromString(
  path: str, contents: str, test_directory_path: str = _BASE_DIR
) -> PRESUBMIT_test_mocks.MockInputApi:
  """Returns a MockInputApi with single changed file with given contents.Api.

  Args:
    path: Fake path to the file to mock.
    contents: The contents of the file to mock.

  Returns:
    A MockInputApi that lists the provided file as only one changed.
  """
  mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
  mock_input_api.presubmit_local_path = test_directory_path
  mock_input_api.files = [
    PRESUBMIT_test_mocks.MockAffectedFile(path, [contents]),
  ]
  return mock_input_api


class MetricsPresubmitTest(unittest.TestCase):
  def testCheckHistogramFormattingFailureIsDetected(self):
    (mock_input_api, malformed_histograms_path) = _MockInputFromTestFile(
      'tokens/token_errors_histograms.xml'
    )

    results = PRESUBMIT.ExecuteCheckHistogramFormatting(
      mock_input_api,
      PRESUBMIT_test_mocks.MockOutputApi(),
      allow_test_paths=True,
      xml_paths_override=[malformed_histograms_path, _TOP_LEVEL_ENUMS_PATH],
    )

    self.assertEqual(len(results), 2)

    self.assertEqual(results[0].type, 'error')
    self.assertRegex(
      results[0].message,
      '.*histograms.xml contains histogram.* using <variants> not defined in'
      ' the file, please run .*validate_token.py .*histograms.xml to fix.',
    )

    # validate_format.py also reports errors when the variants are not defined
    # in the file, hence there is a second error from the same check.
    self.assertEqual(results[1].type, 'error')
    self.assertRegex(
      results[1].message,
      'Histograms are not well-formatted; please run .*validate_format.py and'
      ' fix the reported errors.',
    )

  def testCheckWebViewHistogramsAllowlistOnUploadFailureIsDetected(self):
    valid_enums_path = _BASE_DIR + '/test_data/example_valid_enums.xml'
    example_allowlist_path = _BASE_DIR + '/test_data/AllowlistExample.java'

    (mock_input_api, missing_allow_list_entries_histograms_path) = (
      _MockInputFromTestFile('no_allowlist_entries_histograms.xml')
    )

    results = PRESUBMIT.ExecuteCheckWebViewHistogramsAllowlistOnUpload(
      mock_input_api,
      PRESUBMIT_test_mocks.MockOutputApi(),
      allowlist_path_override=example_allowlist_path,
      xml_paths_override=[
        missing_allow_list_entries_histograms_path,
        valid_enums_path,
        _TOP_LEVEL_ENUMS_PATH,
      ],
    )
    self.assertEqual(len(results), 1)
    self.assertRegex(
      results[0].message.replace('\n', ' '),
      'All histograms in .*AllowlistExample.java must be valid.',
    )
    self.assertEqual(results[0].type, 'error')

  def testCheckBooleansAreEnumsFailureIsDetected(self):
    mock_input_api = _MockInputFromString(
      'histograms.xml', '<histogram name="Foo" units="Boolean" />'
    )

    results = PRESUBMIT.ExecuteCheckBooleansAreEnums(
      mock_input_api, PRESUBMIT_test_mocks.MockOutputApi()
    )
    self.assertEqual(len(results), 1)
    self.assertRegex(
      results[0].message.replace('\n', ' '),
      '.*You are using .units. for a boolean histogram, but you should be'
      ' using\\s+.enum. instead\\.',
    )
    self.assertEqual(results[0].type, 'promptOrNotify')

  def testCheckHistogramFormattingPasses(self):
    valid_enums_path = _BASE_DIR + '/test_data/example_valid_enums.xml'

    (mock_input_api, valid_histograms_path) = _MockInputFromTestFile(
      'example_valid_histograms.xml'
    )

    results = PRESUBMIT.ExecuteCheckHistogramFormatting(
      mock_input_api,
      PRESUBMIT_test_mocks.MockOutputApi(),
      allow_test_paths=True,
      xml_paths_override=[
        valid_histograms_path,
        valid_enums_path,
        _TOP_LEVEL_ENUMS_PATH,
      ],
    )
    # Zero results mean that there were no errors reported.
    self.assertEqual(len(results), 0)

  def testCheckWebViewHistogramsAllowlistOnUploadPasses(self):
    valid_enums_path = _BASE_DIR + '/test_data/example_valid_enums.xml'
    example_allowlist_path = _BASE_DIR + '/test_data/AllowlistExample.java'

    (mock_input_api, valid_histograms_path) = _MockInputFromTestFile(
      'example_valid_histograms.xml'
    )

    results = PRESUBMIT.ExecuteCheckWebViewHistogramsAllowlistOnUpload(
      mock_input_api,
      PRESUBMIT_test_mocks.MockOutputApi(),
      allowlist_path_override=example_allowlist_path,
      xml_paths_override=[
        valid_histograms_path,
        valid_enums_path,
        _TOP_LEVEL_ENUMS_PATH,
      ],
    )
    # Zero results mean that there were no errors reported.
    self.assertEqual(len(results), 0)

  def testCheckBooleansAreEnumsPasses(self):
    mock_input_api = _MockInputFromString(
      'histograms.xml', '<histogram name="Foo" enum="Boolean" />'
    )

    results = PRESUBMIT.ExecuteCheckBooleansAreEnums(
      mock_input_api, PRESUBMIT_test_mocks.MockOutputApi()
    )
    # Zero results mean that there were no errors reported.
    self.assertEqual(len(results), 0)

  def _CacheSize(self, cache_file_path, observed_directory_path):
    cache = presubmit_util.PresubmitCache(
      cache_file_path, observed_directory_path
    )
    return len(cache.InspectCacheForTesting().data)

  def testSecondCheckOnTheSameDataReturnsSameResult(self):
    test_dir_path, _ = _PrepareTestWorkingDirectory()
    test_cache_file = _TempCacheDir()

    mock_input_api = _MockInputFromString(
      'histograms.xml',
      '<histogram name="Foo" units="Boolean" />',
      test_dir_path,
    )

    # The cache should be empty before we run any presubmit checks.
    self.assertEqual(self._CacheSize(test_cache_file, test_dir_path), 0)

    results = PRESUBMIT.CheckBooleansAreEnums(
      mock_input_api,
      PRESUBMIT_test_mocks.MockOutputApi(),
      cache_file_path=test_cache_file,
    )
    self.assertEqual(len(results), 1)
    self.assertRegex(
      results[0].message.replace('\n', ' '),
      '.*You are using .units. for a boolean histogram, but you should be'
      ' using\\s+.enum. instead\\.',
    )
    self.assertEqual(results[0].type, 'promptOrNotify')

    # The cache should now store a single entry for the check above.
    self.assertEqual(self._CacheSize(test_cache_file, test_dir_path), 1)

    second_results = PRESUBMIT.CheckBooleansAreEnums(
      mock_input_api,
      PRESUBMIT_test_mocks.MockOutputApi(),
      cache_file_path=test_cache_file,
    )
    self.assertEqual(len(second_results), 1)
    self.assertEqual(results[0].message, second_results[0].message)
    self.assertEqual(results[0].type, second_results[0].type)

    # The check result should be retrieved from the cache and the cache should
    # still have only one entry.
    self.assertEqual(self._CacheSize(test_cache_file, test_dir_path), 1)

  def testSecondCheckOnTheSameDataReturnsSameEmptyResult(self):
    test_dir_path, _ = _PrepareTestWorkingDirectory()
    test_cache_file = _TempCacheDir()

    mock_input_api = _MockInputFromString(
      'histograms.xml', '<histogram name="Foo" enum="Boolean" />', test_dir_path
    )

    # The cache should be empty before we run any presubmit checks.
    self.assertEqual(self._CacheSize(test_cache_file, test_dir_path), 0)

    results = PRESUBMIT.CheckBooleansAreEnums(
      mock_input_api,
      PRESUBMIT_test_mocks.MockOutputApi(),
      cache_file_path=test_cache_file,
    )
    # Zero results mean that there were no errors reported.
    self.assertEqual(len(results), 0)

    # The cache should now store a single entry for the check above.
    self.assertEqual(self._CacheSize(test_cache_file, test_dir_path), 1)

    second_results = PRESUBMIT.CheckBooleansAreEnums(
      mock_input_api,
      PRESUBMIT_test_mocks.MockOutputApi(),
      cache_file_path=test_cache_file,
    )
    # Zero results mean that there were no errors reported.
    self.assertEqual(len(second_results), 0)

    # The check result should be retrieved from the cache and the cache should
    # still have only one entry.
    self.assertEqual(self._CacheSize(test_cache_file, test_dir_path), 1)

  def testFailureInModifiedFileIsDetected(self):
    test_dir_path, histograms_path = _PrepareTestWorkingDirectory()

    test_cache_file = _TempCacheDir()
    mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    mock_input_api.presubmit_local_path = test_dir_path
    mock_input_api.files = [
      PRESUBMIT_test_mocks.MockAffectedFile(
        'histograms.xml', [_INITIAL_HISTOGRAMS_CONTENT]
      ),
    ]

    # The cache should be empty before we run any presubmit checks.
    self.assertEqual(self._CacheSize(test_cache_file, test_dir_path), 0)

    results = PRESUBMIT.CheckBooleansAreEnums(
      mock_input_api,
      PRESUBMIT_test_mocks.MockOutputApi(),
      cache_file_path=test_cache_file,
    )
    # Zero results mean that there were no errors reported.
    self.assertEqual(len(results), 0)

    # The cache should now store a single entry for the check above.
    self.assertEqual(self._CacheSize(test_cache_file, test_dir_path), 1)

    with open(histograms_path, 'w') as f:
      f.write(_MODIFIED_HISTOGRAMS_CONTENT)

    mock_input_api.files = [
      PRESUBMIT_test_mocks.MockAffectedFile(
        'histograms.xml', [_MODIFIED_HISTOGRAMS_CONTENT]
      ),
    ]

    second_results = PRESUBMIT.CheckBooleansAreEnums(
      mock_input_api,
      PRESUBMIT_test_mocks.MockOutputApi(),
      cache_file_path=test_cache_file,
    )

    self.assertEqual(len(second_results), 1)
    self.assertRegex(
      second_results[0].message.replace('\n', ' '),
      '.*You are using .units. for a boolean histogram, but you should be'
      ' using\\s+.enum. instead\\.',
    )
    self.assertEqual(second_results[0].type, 'promptOrNotify')

    # The cache should now have an extra entry as the second check was done on
    # a different version of the file.
    self.assertEqual(self._CacheSize(test_cache_file, test_dir_path), 2)

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
        PRESUBMIT_test_mocks.MockOutputApi(),
        allow_test_paths=True,
        xml_paths_override=[input_path],
      )

      self.assertEqual(len(results), 0)

  def testNonregisteredVariantsAreFailingValidation(self):
    (mock_input_api, input_path) = _MockInputFromTestFile(
      'tokens/variants_missing_histograms.xml'
    )

    results = PRESUBMIT.ExecuteCheckHistogramFormatting(
      mock_input_api,
      PRESUBMIT_test_mocks.MockOutputApi(),
      allow_test_paths=True,
      xml_paths_override=[input_path, _TOP_LEVEL_ENUMS_PATH],
    )

    self.assertEqual(len(results), 1)

    # validate_format.py also reports all errors as Histogram malformatted
    # errors, detailed errors are printed and the error itself refers back
    # to validate_format.py.
    self.assertEqual(results[0].type, 'error')
    self.assertRegex(
      results[0].message,
      'Histograms are not well-formatted; please run .*validate_format.py and'
      ' fix the reported errors.',
    )

  def testDeletedFileIsIgnoredByBooleansAreEnumsCheck(self):
    mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    mock_input_api.presubmit_local_path = _BASE_DIR
    mock_input_api.files = [
      PRESUBMIT_test_mocks.MockAffectedFile(
        'histograms.xml',
        ['<histogram name="Foo" units="Boolean" />'],
        action='D',
      ),
    ]

    results = PRESUBMIT.ExecuteCheckBooleansAreEnums(
      mock_input_api, PRESUBMIT_test_mocks.MockOutputApi()
    )

    # Zero results mean that there were no errors reported.
    self.assertEqual(len(results), 0)

  def testDeletedFileIsIgnoredByHistogramFormattingCheck(self):
    full_path = _BASE_DIR + '/test_data/non_existing_histograms.xml'

    mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    mock_input_api.presubmit_local_path = _BASE_DIR
    mock_input_api.files = [
      PRESUBMIT_test_mocks.MockAffectedFile(full_path, [], action='D'),
    ]

    results = PRESUBMIT.ExecuteCheckHistogramFormatting(
      mock_input_api,
      PRESUBMIT_test_mocks.MockOutputApi(),
      allow_test_paths=True,
      xml_paths_override=[_TOP_LEVEL_ENUMS_PATH],
    )

    # Zero results mean that there were no errors reported.
    self.assertEqual(len(results), 0)

  def testDeletedFileIsIgnoredByAllowlistCheck(self):
    non_existing_histograms_path = (
      _BASE_DIR + '/test_data/non_existing_histograms.xml'
    )
    valid_histograms_path = (
      _BASE_DIR + '/test_data/example_valid_histograms.xml'
    )
    valid_enums_path = _BASE_DIR + '/test_data/example_valid_enums.xml'
    example_allowlist_path = _BASE_DIR + '/test_data/AllowlistExample.java'

    with open(valid_histograms_path, 'r') as f:
      valid_histograms_contents = f.read()
    mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    mock_input_api.presubmit_local_path = _BASE_DIR
    mock_input_api.files = [
      PRESUBMIT_test_mocks.MockAffectedFile(
        non_existing_histograms_path, [], action='D'
      ),
      PRESUBMIT_test_mocks.MockAffectedFile(
        valid_histograms_path, [valid_histograms_contents]
      ),
    ]

    results = PRESUBMIT.ExecuteCheckWebViewHistogramsAllowlistOnUpload(
      mock_input_api,
      PRESUBMIT_test_mocks.MockOutputApi(),
      allowlist_path_override=example_allowlist_path,
      xml_paths_override=[
        valid_histograms_path,
        valid_enums_path,
        _TOP_LEVEL_ENUMS_PATH,
      ],
    )

    # Zero results mean that there were no errors reported.
    self.assertEqual(len(results), 0)

  def _run_check_histograms_changes(self, file_data, is_committing=True):
    mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    mock_input_api.presubmit_local_path = _BASE_DIR
    mock_input_api.is_committing = is_committing
    mock_input_api.files = [
      PRESUBMIT_test_mocks.MockAffectedFile(
        path, new_contents=new_xml, old_contents=old_xml, action='M'
      )
      for path, old_xml, new_xml in file_data
    ]
    return PRESUBMIT.CheckHistogramsChanges(
      mock_input_api, PRESUBMIT_test_mocks.MockOutputApi()
    )

  def _build_xml(self, tag, name=None, items=None, new_items=0, prefix='Bar'):
    xml = ['<histogram-configuration>']
    xml.append(f"<{tag} name='{name}'>" if name else f'<{tag}>')
    if items:
      xml.extend(items)
    for i in range(new_items):
      if tag == 'variants':
        xml.append(
          f"  <variant name='{prefix}{i + 1}' summary='{prefix}{i + 1}'/>"
        )
      else:
        xml.append(f"  <histogram name='{prefix}{i}' units='ms' />")
    xml.extend([f'</{tag}>', '</histogram-configuration>'])
    return xml

  @contextlib.contextmanager
  def _mock_histograms_xmls(
    self,
    xml_content,
    variants_relative_paths=('tools/metrics/histograms/variants.xml',),
  ):
    fd, temp_file_path = tempfile.mkstemp(suffix='_histograms.xml')
    orig_xmls = histogram_paths.HISTOGRAMS_XMLS
    orig_variants_relative = histogram_paths._VARIANTS_XML_RELATIVE
    try:
      with os.fdopen(fd, 'w') as f:
        f.write(xml_content)
      histogram_paths.HISTOGRAMS_XMLS = [temp_file_path]
      histogram_paths._VARIANTS_XML_RELATIVE = list(variants_relative_paths)
      yield
    finally:
      os.remove(temp_file_path)
      histogram_paths.HISTOGRAMS_XMLS = orig_xmls
      histogram_paths._VARIANTS_XML_RELATIVE = orig_variants_relative

  def testCheckHistogramsChangesPasses(self):
    old_xml = self._build_xml(
      'histograms', items=['  <histogram name="Foo" units="ms" />']
    )
    new_xml = self._build_xml(
      'histograms',
      items=[
        '  <histogram name="Foo" units="ms" />',
        '  <histogram name="Bar" units="ms" />',
      ],
    )
    results = self._run_check_histograms_changes(
      [(histogram_paths._HISTOGRAMS_XMLS_RELATIVE[0], old_xml, new_xml)]
    )
    self.assertEqual(len(results), 0)

  def testCheckHistogramsChangesFailureIsDetected(self):
    old_xml = self._build_xml(
      'histograms', items=['  <histogram name="Foo" units="ms" />']
    )
    new_xml = self._build_xml(
      'histograms',
      items=['  <histogram name="Foo" units="ms" />'],
      new_items=PRESUBMIT._NEW_HISTOGRAMS_THRESHOLD + 1,
    )
    results = self._run_check_histograms_changes(
      [(histogram_paths._HISTOGRAMS_XMLS_RELATIVE[0], old_xml, new_xml)]
    )
    self.assertEqual(len(results), 1)
    self.assertEqual(results[0].type, 'warning')
    self.assertRegex(
      results[0].message,
      r'More than \d+ new histograms are being introduced \(\d+\)\. '
      r'Are you sure you want to continue\?',
    )

  def testCheckHistogramsChangesUploadWarningIsDetected(self):
    old_xml = self._build_xml(
      'histograms', items=['  <histogram name="Foo" units="ms" />']
    )
    new_xml = self._build_xml(
      'histograms',
      items=['  <histogram name="Foo" units="ms" />'],
      new_items=PRESUBMIT._NEW_HISTOGRAMS_THRESHOLD + 1,
    )
    results = self._run_check_histograms_changes(
      [(histogram_paths._HISTOGRAMS_XMLS_RELATIVE[0], old_xml, new_xml)],
      is_committing=False,
    )
    self.assertEqual(len(results), 1)
    self.assertEqual(results[0].type, 'warning')
    self.assertRegex(
      results[0].message,
      r'More than \d+ new histograms are being introduced \(\d+\)\. '
      r'Are you sure you want to continue\?',
    )

  def testCheckHistogramsChanges_VariantsXmlChangedFailure(self):
    content = """<histogram-configuration>
<histograms>
  <histogram name='Test.{MockVariants}' enum='Boolean'
      expires_after='2025-12-31'>
    <owner>test@chromium.org</owner>
    <summary>Test</summary>
  </histogram>
</histograms>
</histogram-configuration>
"""
    with self._mock_histograms_xmls(content):
      old_xml = self._build_xml(
        'variants',
        name='MockVariants',
        items=['  <variant name="V0" summary="v0"/>'],
      )
      new_xml = self._build_xml(
        'variants',
        name='MockVariants',
        items=['  <variant name="V0" summary="v0"/>'],
        new_items=10,
        prefix='V',
      )

      orig_threshold = PRESUBMIT._NEW_HISTOGRAMS_THRESHOLD
      PRESUBMIT._NEW_HISTOGRAMS_THRESHOLD = 5
      try:
        results = self._run_check_histograms_changes(
          [('tools/metrics/histograms/variants.xml', old_xml, new_xml)]
        )
        self.assertEqual(len(results), 1)
        self.assertEqual(results[0].type, 'warning')
        self.assertRegex(
          results[0].message,
          r'More than 5 new histograms are being introduced \(10\)\.',
        )
      finally:
        PRESUBMIT._NEW_HISTOGRAMS_THRESHOLD = orig_threshold

  def testCheckHistogramsChanges_VariantsXmlChangedPasses(self):
    content = """<histogram-configuration>
<histograms>
  <histogram name='Test.{MockVariants}' enum='Boolean'
      expires_after='2025-12-31'>
    <owner>test@chromium.org</owner>
    <summary>Test histogram.</summary>
  </histogram>
</histograms>
</histogram-configuration>
"""
    with self._mock_histograms_xmls(content):
      old_xml = self._build_xml(
        'variants',
        name='MockVariants',
        items=['  <variant name="V0" summary="v0"/>'],
      )
      new_xml = self._build_xml(
        'variants',
        name='MockVariants',
        items=['  <variant name="V0" summary="v0"/>'],
        new_items=2,
        prefix='V',
      )

      orig_threshold = PRESUBMIT._NEW_HISTOGRAMS_THRESHOLD
      PRESUBMIT._NEW_HISTOGRAMS_THRESHOLD = 5
      try:
        results = self._run_check_histograms_changes(
          [('tools/metrics/histograms/variants.xml', old_xml, new_xml)]
        )
        self.assertEqual(len(results), 0)
      finally:
        PRESUBMIT._NEW_HISTOGRAMS_THRESHOLD = orig_threshold

  def testMalformedVariantsXmlFailureIsDetected(self):
    mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    mock_input_api.presubmit_local_path = _BASE_DIR
    mock_input_api.is_committing = True
    malformed_variants_xml = [
      '<histogram-configuration>',
      "<variants name='MockVariants'>",
      "  <variant name='V0' summary='v0'/>",
    ]
    mock_input_api.files = [
      PRESUBMIT_test_mocks.MockAffectedFile(
        'tools/metrics/histograms/variants.xml',
        new_contents=malformed_variants_xml,
        old_contents=['<histogram-configuration></histogram-configuration>'],
        action='M',
      ),
    ]
    with self.assertRaises((ValueError, ET.ParseError)):
      PRESUBMIT.CheckHistogramsChanges(
        mock_input_api, PRESUBMIT_test_mocks.MockOutputApi()
      )

  def testMalformedHistogramsXmlFailureIsDetected(self):
    mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    mock_input_api.presubmit_local_path = _BASE_DIR
    mock_input_api.is_committing = True
    malformed_histograms_xml = [
      '<histogram-configuration>',
      '<histograms>',
      "  <histogram name='Foo' units='ms' />",
    ]
    mock_input_api.files = [
      PRESUBMIT_test_mocks.MockAffectedFile(
        histogram_paths._HISTOGRAMS_XMLS_RELATIVE[0],
        new_contents=malformed_histograms_xml,
        old_contents=['<histogram-configuration></histogram-configuration>'],
        action='M',
      ),
    ]
    with self.assertRaises((ValueError, ET.ParseError)):
      PRESUBMIT.CheckHistogramsChanges(
        mock_input_api, PRESUBMIT_test_mocks.MockOutputApi()
      )

  def testCheckHistogramsChangesSegmentationFailureIsDetected(self):
    orig_get_names = PRESUBMIT.generate_histogram_list.GetActualHistogramNames
    PRESUBMIT.generate_histogram_list.GetActualHistogramNames = lambda: [
      'Segmentation.Test'
    ]
    try:
      old_xml = self._build_xml(
        'histograms',
        items=['  <histogram name="Segmentation.Test" units="ms" />'],
      )
      new_xml = self._build_xml('histograms')
      results = self._run_check_histograms_changes(
        [(histogram_paths._HISTOGRAMS_XMLS_RELATIVE[0], old_xml, new_xml)]
      )
      self.assertEqual(len(results), 1)
      self.assertEqual(results[0].type, 'error')
      self.assertRegex(
        results[0].message, r'segmentation platform and should not be removed'
      )
    finally:
      PRESUBMIT.generate_histogram_list.GetActualHistogramNames = orig_get_names

  def testCheckHistogramsChangesSegmentationPasses(self):
    orig_get_names = PRESUBMIT.generate_histogram_list.GetActualHistogramNames
    PRESUBMIT.generate_histogram_list.GetActualHistogramNames = lambda: [
      'Segmentation.Test'
    ]
    try:
      old_xml = self._build_xml(
        'histograms',
        items=['  <histogram name="Segmentation.Test" units="ms" />'],
      )
      new_xml = self._build_xml(
        'histograms',
        items=[
          '  <histogram name="Segmentation.Test" units="ms" />',
          '  <histogram name="Something.Else" units="ms" />',
        ],
      )
      results = self._run_check_histograms_changes(
        [(histogram_paths._HISTOGRAMS_XMLS_RELATIVE[0], old_xml, new_xml)]
      )
      self.assertEqual(len(results), 0)
    finally:
      PRESUBMIT.generate_histogram_list.GetActualHistogramNames = orig_get_names

  def testCheckHistogramsChanges_MultipleVariantsXmlChanged(self):
    content = """<histogram-configuration>
<histograms>
  <histogram name='Test.{MockVariants1}.{MockVariants2}' enum='Boolean'
      expires_after='2025-12-31'>
    <owner>test@chromium.org</owner>
    <summary>Test histogram.</summary>
  </histogram>
</histograms>
</histogram-configuration>
"""
    v1_rel = 'tools/metrics/histograms/variants1.xml'
    v2_rel = 'tools/metrics/histograms/variants2.xml'
    with self._mock_histograms_xmls(
      content, variants_relative_paths=[v1_rel, v2_rel]
    ):
      old_v1 = self._build_xml(
        'variants',
        name='MockVariants1',
        items=['  <variant name="V0" summary="v0"/>'],
      )
      new_v1 = self._build_xml(
        'variants',
        name='MockVariants1',
        items=[
          '  <variant name="V0" summary="v0"/>',
          '  <variant name="V1" summary="v1"/>',
        ],
      )
      old_v2 = self._build_xml(
        'variants',
        name='MockVariants2',
        items=['  <variant name="X0" summary="x0"/>'],
      )
      new_v2 = self._build_xml(
        'variants',
        name='MockVariants2',
        items=[
          '  <variant name="X0" summary="x0"/>',
          '  <variant name="X1" summary="x1"/>',
        ],
      )

      results = self._run_check_histograms_changes(
        [
          (v1_rel, old_v1, new_v1),
          (v2_rel, old_v2, new_v2),
        ]
      )
      self.assertEqual(len(results), 0)

      orig_threshold = PRESUBMIT._NEW_HISTOGRAMS_THRESHOLD
      PRESUBMIT._NEW_HISTOGRAMS_THRESHOLD = 2
      try:
        results = self._run_check_histograms_changes(
          [
            (v1_rel, old_v1, new_v1),
            (v2_rel, old_v2, new_v2),
          ]
        )
        self.assertEqual(len(results), 1)
        self.assertEqual(results[0].type, 'warning')
        self.assertRegex(
          results[0].message,
          r'More than 2 new histograms are being introduced \(3\)\.',
        )
      finally:
        PRESUBMIT._NEW_HISTOGRAMS_THRESHOLD = orig_threshold

  def testCheckHistogramsChanges_NewVariantsFileDoesNotCrash(self):
    new_xml = self._build_xml(
      'variants',
      name='MockVariants',
      items=['  <variant name="V0" summary="v0"/>'],
    )
    # Simulates adding a new variants file (old_contents is empty, action='A')
    mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    mock_input_api.presubmit_local_path = _BASE_DIR
    mock_input_api.is_committing = True
    mock_input_api.files = [
      PRESUBMIT_test_mocks.MockAffectedFile(
        'tools/metrics/histograms/variants.xml',
        new_contents=new_xml,
        old_contents=[],
        action='A',
      )
    ]
    # This should not raise an ExpatError or ValueError
    results = PRESUBMIT.CheckHistogramsChanges(
      mock_input_api, PRESUBMIT_test_mocks.MockOutputApi()
    )
    self.assertEqual(len(results), 0)

  def testCheckHistogramsChanges_DeletedVariantsFileDoesNotCrash(self):
    old_xml = self._build_xml(
      'variants',
      name='MockVariants',
      items=['  <variant name="V0" summary="v0"/>'],
    )
    # Simulates deleting a variants file (new_contents is empty, action='D')
    mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    mock_input_api.presubmit_local_path = _BASE_DIR
    mock_input_api.is_committing = True
    mock_input_api.files = [
      PRESUBMIT_test_mocks.MockAffectedFile(
        'tools/metrics/histograms/variants.xml',
        new_contents=[],
        old_contents=old_xml,
        action='D',
      )
    ]
    # This should not raise an ExpatError or ValueError
    results = PRESUBMIT.CheckHistogramsChanges(
      mock_input_api, PRESUBMIT_test_mocks.MockOutputApi()
    )
    self.assertEqual(len(results), 0)

  def testVariantMetadataChangeForSegmentationHistogramIsDetected(self):
    old_contents = _HISTOGRAMS_WITH_VARIANT_TEMPLATE.format(
      variant_summary='Old variant summary'
    )
    new_contents = _HISTOGRAMS_WITH_VARIANT_TEMPLATE.format(
      variant_summary='New variant summary'
    )
    mock_input_api = PRESUBMIT_test_mocks.MockInputApi()
    mock_input_api.presubmit_local_path = _BASE_DIR
    mock_input_api.is_committing = True
    mock_input_api.files = [
      PRESUBMIT_test_mocks.MockAffectedFile(
        histogram_paths._HISTOGRAMS_XMLS_RELATIVE[0],
        new_contents.splitlines(),
        old_contents.splitlines(),
        action='M',
      ),
    ]

    with mock.patch.object(
      PRESUBMIT.generate_histogram_list,
      'GetActualHistogramNames',
      return_value={'Segmentation.Test.One'},
    ):
      results = PRESUBMIT.CheckHistogramsChanges(
        mock_input_api, PRESUBMIT_test_mocks.MockOutputApi()
      )

    self.assertEqual(len(results), 1)
    self.assertEqual(results[0].type, 'error')
    self.assertRegex(results[0].message, 'metadata affected')
    self.assertEqual(results[0].items, ['Segmentation.Test.One'])

  def testSharedVariantsChangeAffectingOtherHistogramIsDetected(self):
    old_variants = self._build_xml(
      'variants',
      name='TestVariant',
      items=['  <variant name="One" summary="Old variant summary" />'],
    )
    new_variants = self._build_xml(
      'variants',
      name='TestVariant',
      items=['  <variant name="One" summary="New variant summary" />'],
    )
    histogram_contents = """\
<histogram-configuration>
  <histograms>
    <histogram name="Segmentation.Test.{TestVariant}" units="count"
        expires_after="M200">
      <owner>owner@chromium.org</owner>
      <summary>Records {TestVariant} value.</summary>
      <token key="TestVariant" variants="TestVariant" />
    </histogram>
  </histograms>
</histogram-configuration>
"""
    variants_fd, variants_path = tempfile.mkstemp(suffix='_variants.xml')
    histogram_fd, histogram_path = tempfile.mkstemp(suffix='_histograms.xml')
    try:
      with os.fdopen(variants_fd, 'w') as variants_file:
        variants_file.write('\n'.join(new_variants))
      with os.fdopen(histogram_fd, 'w') as histogram_file:
        histogram_file.write(histogram_contents)

      with self._mock_histograms_xmls(
        histogram_contents, variants_relative_paths=(variants_path,)
      ):
        histogram_paths.HISTOGRAMS_XMLS = [histogram_path]
        with mock.patch.object(
          PRESUBMIT.generate_histogram_list,
          'GetActualHistogramNames',
          return_value={'Segmentation.Test.One'},
        ):
          results = self._run_check_histograms_changes(
            [(variants_path, old_variants, new_variants)]
          )
    finally:
      os.remove(variants_path)
      os.remove(histogram_path)

    self.assertEqual(len(results), 1)
    self.assertEqual(results[0].type, 'error')
    self.assertEqual(results[0].items, ['Segmentation.Test.One'])

  def testNewSegmentationHistogramUsingNewVariantIsNotReported(self):
    old_contents = """\
<histogram-configuration>
  <histograms>
  </histograms>
</histogram-configuration>
"""
    new_contents = """\
<histogram-configuration>
  <histograms>
    <variants name="TestVariant">
      <variant name="One" summary="New variant summary" />
    </variants>
    <histogram name="Segmentation.Test.{Variant}" units="count"
        expires_after="M200">
      <owner>owner@chromium.org</owner>
      <summary>Records {Variant} value.</summary>
      <token key="Variant" variants="TestVariant" />
    </histogram>
  </histograms>
</histogram-configuration>
"""
    with mock.patch.object(
      PRESUBMIT.generate_histogram_list,
      'GetActualHistogramNames',
      return_value={'Segmentation.Test.One'},
    ):
      results = self._run_check_histograms_changes(
        [
          (
            histogram_paths._HISTOGRAMS_XMLS_RELATIVE[0],
            old_contents.splitlines(),
            new_contents.splitlines(),
          )
        ]
      )

    self.assertEqual(results, [])


if __name__ == '__main__':
  unittest.main()
