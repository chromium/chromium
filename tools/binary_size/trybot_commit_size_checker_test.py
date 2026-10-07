#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from unittest import mock

import trybot_commit_size_checker


class Tests(unittest.TestCase):
  def testIterForTestingSymbolsFromMapping(self):
    def test_case(snippet, expected):
      actual = list(
        trybot_commit_size_checker.IterForTestingSymbolsFromMapping(snippet)
      )
      self.assertEqual(expected, actual)

    # Test ignored comments and non-ForTest symbols
    test_case(
      """
# pkg.CommentForTesting > o:
pkg.NormalClass -> p:
    android.os.IBinder mRemote -> o
    1:3:android.os.IBinder asBinder():168:168 -> asBinder
    3:6:Bundle foo(java.lang.StringForTest):250:250 -> b
        # {"id":"residualsignature","signature":"()LBundleForTest;"}
""",
      [],
    )

    # Test when class has ForTest in it.
    test_case(
      """
pkg.ClzForTest -> Ja1:
    java.lang.String DESCRIPTOR -> b
    7:13:void <clinit>():290:290 -> <clinit>
    7:13:FooForTest pkg2.ForTesting.someMethod():290:290 -> b
    7:13:void pkg2.Clz.setForTests():290:290 -> b
""",
      [
        'pkg.ClzForTest',
        'pkg.ClzForTest#DESCRIPTOR',
        'pkg.ClzForTest#<clinit>',
        'pkg2.ForTesting#someMethod',
        'pkg2.Clz#setForTests',
      ],
    )

    # Test when class does not have ForTest in it.
    test_case(
      """
pkg.Clz -> Ja1:
    java.lang.String fieldForTest -> b
    java.lang.String FIELD_FOR_TEST -> b
    7:13:void <clinit>():290:290 -> <clinit>
    7:13:FooForTest pkg2.ForTesting.someMethod():290:290 -> b
    7:13:void pkg2.Clz.setForTests():290:290 -> b
""",
      [
        'pkg.Clz#fieldForTest',
        'pkg.Clz#FIELD_FOR_TEST',
        'pkg2.ForTesting#someMethod',
        'pkg2.Clz#setForTests',
      ],
    )

  def testCreateMutableConstantsDelta(self):
    Symbol = trybot_commit_size_checker.models.Symbol
    DeltaSymbol = trybot_commit_size_checker.models.DeltaSymbol
    DeltaSymbolGroup = trybot_commit_size_checker.models.DeltaSymbolGroup

    # Create dummy symbols (Added status: before_symbol=None)
    rust_lazy = DeltaSymbol(
      None,
      Symbol(
        '.data',
        8,
        full_name='<>::deref::__stability::LAZY',
        name='LAZY',
        source_path='',
      ),
    )
    rust_once = DeltaSymbol(
      None,
      Symbol(
        '.data',
        8,
        full_name='once_cell::race::OnceBox',
        name='OnceBox',
        source_path='',
      ),
    )
    rust_lazy_static = DeltaSymbol(
      None,
      Symbol(
        '.data',
        8,
        full_name='lazy_static::lazy::Lazy',
        name='Lazy',
        source_path='',
      ),
    )
    rust_with_path = DeltaSymbol(
      None,
      Symbol(
        '.data',
        8,
        full_name='some_rust_crate::kConst',
        name='kConst',
        source_path='lib.rs',
      ),
    )

    cpp_mutable_k = DeltaSymbol(
      None,
      Symbol(
        '.data',
        8,
        full_name='net::kMaxHeaderSize',
        name='kMaxHeaderSize',
        source_path='net.cc',
      ),
    )
    cpp_mutable_upper = DeltaSymbol(
      None,
      Symbol(
        '.data',
        8,
        full_name='MY_MUTABLE_GLOBAL',
        name='MY_MUTABLE_GLOBAL',
        source_path='main.cc',
      ),
    )

    symbols = DeltaSymbolGroup(
      [
        rust_lazy,
        rust_once,
        rust_lazy_static,
        rust_with_path,
        cpp_mutable_k,
        cpp_mutable_upper,
      ]
    )

    lines, delta = trybot_commit_size_checker._CreateMutableConstantsDelta(
      symbols
    )

    self.assertEqual(2, delta.actual)

    output_text = '\n'.join(lines)
    self.assertIn('kMaxHeaderSize', output_text)
    self.assertIn('MY_MUTABLE_GLOBAL', output_text)
    self.assertNotIn('LAZY', output_text)
    self.assertNotIn('OnceBox', output_text)
    self.assertNotIn('Lazy', output_text)
    self.assertNotIn('kConst', output_text)

  def testCreateMutableConstantsDeltaWithFeatures(self):
    Symbol = trybot_commit_size_checker.models.Symbol
    DeltaSymbol = trybot_commit_size_checker.models.DeltaSymbol
    DeltaSymbolGroup = trybot_commit_size_checker.models.DeltaSymbolGroup

    cpp_feature_added = DeltaSymbol(
      None,
      Symbol(
        '.data',
        8,
        full_name='kMyFeatureAdded',
        name='kMyFeatureAdded',
        source_path='chrome/browser/my_feature.cc',
        flags=trybot_commit_size_checker.models.FLAG_FEATURE,
      ),
    )

    cpp_feature_removed = DeltaSymbol(
      Symbol(
        '.data',
        8,
        full_name='kMyFeatureRemoved',
        name='kMyFeatureRemoved',
        source_path='chrome/browser/my_feature.cc',
        flags=trybot_commit_size_checker.models.FLAG_FEATURE,
      ),
      None,
    )

    cpp_normal_const = DeltaSymbol(
      None,
      Symbol(
        '.data',
        8,
        full_name='kMyNormalConstant',
        name='kMyNormalConstant',
        source_path='chrome/browser/my_feature.cc',
      ),
    )

    symbols = DeltaSymbolGroup(
      [
        cpp_feature_added,
        cpp_feature_removed,
        cpp_normal_const,
      ]
    )

    lines, delta = trybot_commit_size_checker._CreateMutableConstantsDelta(
      symbols
    )

    # Since cpp_features have FLAG_FEATURE, they should be filtered out.
    # cpp_normal_const is not a feature, so it remains.
    self.assertEqual(1, delta.actual)

    output_text = '\n'.join(lines)
    self.assertIn('kMyNormalConstant', output_text)
    self.assertNotIn('kMyFeatureAdded', output_text)
    self.assertNotIn('kMyFeatureRemoved', output_text)

  def testUseAlternativeIfMissing(self):
    # The reference build is from main and only stages the legacy names, while
    # the patched build asks for the renamed ones.
    def test_case(staged, existing, expected):
      with mock.patch('os.path.isfile') as isfile_mock:
        isfile_mock.side_effect = lambda path: path == existing
        self.assertEqual(
          expected,
          trybot_commit_size_checker._UseAlternativeIfMissing(staged),
          staged,
        )

    # Already present: returned untouched.
    test_case(
      '/dir/ChromeAndWebView32.ssargs.size',
      '/dir/ChromeAndWebView32.ssargs.size',
      '/dir/ChromeAndWebView32.ssargs.size',
    )

    for staged, legacy in [
      ('ChromeAndWebView32.ssargs.size', 'Trichrome32.ssargs.size'),
      ('ChromeAndWebViewGoogle32.ssargs.size', 'TrichromeGoogle32.ssargs.size'),
      ('ChromePublic32.aab.mapping', 'TrichromeChrome32.aab.mapping'),
      ('SystemWebView32.aab.mapping', 'TrichromeWebView32.aab.mapping'),
      ('Chrome32.aab.mapping', 'TrichromeChromeGoogle32.aab.mapping'),
      (
        'SystemWebViewGoogle32.aab.mapping',
        'TrichromeWebViewGoogle32.aab.mapping',
      ),
    ]:
      test_case('/dir/' + staged, '/dir/' + legacy, '/dir/' + legacy)

    # x32y -> xy still works, and is still the result when nothing matches.
    test_case(
      '/dir/SystemWebView32.aab.R.txt',
      '/dir/SystemWebView.aab.R.txt',
      '/dir/SystemWebView.aab.R.txt',
    )
    test_case('/dir/Absent32.mapping', '/dir/nothing', '/dir/Absent.mapping')

  def testResourceSizesDeltaExceptionFallback(self):
    with mock.patch(
      'diagnose_bloat.ResourceSizesDiff'
    ) as resource_sizes_diff_mock:
      instance = resource_sizes_diff_mock.return_value
      instance.Summary.return_value = ['summary line']
      instance.DetailedResults.return_value = ['detail line']
      type(instance).summary_stat = mock.PropertyMock(
        side_effect=Exception('Could not find canonical "normalized"')
      )
      instance.CombinedSizeChangeForSection.side_effect = Exception(
        'Could not find "Combined"'
      )

      _, delta_64 = trybot_commit_size_checker._CreateResourceSizes64Delta(
        'before', 'after', 1000
      )
      self.assertEqual(0, delta_64.actual)
      self.assertEqual('Normalized APK Size (arm64)', delta_64.name)

      _, delta_32 = trybot_commit_size_checker._CreateResourceSizesDelta(
        'before', 'after', 1000
      )
      self.assertEqual(0, delta_32.actual)
      self.assertEqual('Normalized APK Size', delta_32.name)

      _, delta_base = (
        trybot_commit_size_checker._CreateBaseModuleResourceSizesDelta(
          'before', 'after', 1000
        )
      )
      self.assertEqual(0, delta_base.actual)
      self.assertEqual('Base Module Size', delta_base.name)


if __name__ == '__main__':
  unittest.main()
