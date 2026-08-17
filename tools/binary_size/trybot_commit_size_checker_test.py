#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import unittest.mock as mock

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

    cpp_feature = DeltaSymbol(
      None,
      Symbol(
        '.data',
        8,
        full_name='kMyFeatureName',
        name='kMyFeatureName',
        source_path='chrome/browser/my_feature.cc',
      ),
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
        cpp_feature,
        cpp_normal_const,
      ]
    )

    # Clear any lru_cache to make sure mock works reliably
    trybot_commit_size_checker._GetFeatureSymbolNamesCached.cache_clear()

    with (
      mock.patch('pathlib.Path.exists', return_value=True),
      mock.patch(
        'find_features.FindFeatureSymbolNamesInFile',
        return_value=['kMyFeatureName'],
      ),
    ):
      lines, delta = trybot_commit_size_checker._CreateMutableConstantsDelta(
        symbols
      )

    # Since cpp_feature is identified as a feature, it should be filtered out.
    # cpp_normal_const is not a feature, so it remains.
    self.assertEqual(1, delta.actual)

    output_text = '\n'.join(lines)
    self.assertIn('kMyNormalConstant', output_text)
    self.assertNotIn('kMyFeatureName', output_text)

  def testCreateMutableConstantsDeltaWithGeneratedFeatures(self):
    Symbol = trybot_commit_size_checker.models.Symbol
    DeltaSymbol = trybot_commit_size_checker.models.DeltaSymbol
    DeltaSymbolGroup = trybot_commit_size_checker.models.DeltaSymbolGroup

    cpp_generated_feature = Symbol(
      '.data',
      8,
      full_name='kMyGenFeatureName',
      name='kMyGenFeatureName',
      source_path='third_party/blink/common/features_generated.cc',
    )
    cpp_generated_feature.generated_source = True
    cpp_feature = DeltaSymbol(None, cpp_generated_feature)

    symbols = DeltaSymbolGroup([cpp_feature])

    trybot_commit_size_checker._GetFeatureSymbolNamesCached.cache_clear()

    exists_mock = mock.MagicMock(return_value=True)

    with (
      mock.patch('pathlib.Path.exists', exists_mock),
      mock.patch(
        'find_features.FindFeatureSymbolNamesInFile',
        return_value=['kMyGenFeatureName'],
      ),
    ):
      _, delta = trybot_commit_size_checker._CreateMutableConstantsDelta(
        symbols, out_directory='out/Release'
      )

    self.assertEqual(0, delta.actual)


if __name__ == '__main__':
  unittest.main()
