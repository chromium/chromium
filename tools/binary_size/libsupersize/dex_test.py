#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

import dex
import dex_deobfuscate
import dex_parser

_TEST_DEX_PATH = os.path.join(
  os.path.dirname(__file__), 'testdata', 'mock_dex', 'after', 'classes.dex'
)


class DexTest(unittest.TestCase):
  def assertNormalizedTo(self, class_path, expected_outer_class, expected_name):
    actual_outer_class, actual_name = dex.NormalizeLine(class_path, class_path)
    self.assertEqual(expected_outer_class, actual_outer_class)
    self.assertEqual(expected_name, actual_name)

  def testNoramlize_internalSyntheticLambda(self):
    self.assertNormalizedTo(
      class_path='pkg.Cls$$InternalSyntheticLambda$3$81073ff626$0',
      expected_outer_class='pkg.Cls',
      expected_name='pkg.Cls$$InternalSyntheticLambda$3',
    )

  def testNoramlize_externalSyntheticLambda(self):
    self.assertNormalizedTo(
      class_path='pkg.AnimatedProgressBar$$ExternalSyntheticLambda0',
      expected_outer_class='pkg.AnimatedProgressBar',
      expected_name=('pkg.AnimatedProgressBar$$ExternalSyntheticLambda0'),
    )

  # Google3 still uses this format.
  def testNoramlize_DesugarLambda(self):
    self.assertNormalizedTo(
      class_path='pkg.Cls$$Lambda$1',
      expected_outer_class='pkg.Cls',
      expected_name='pkg.Cls$$Lambda$1',
    )

  def testNoramlize_apiModelOutline(self):
    self.assertNormalizedTo(
      class_path='pkg.Cls$$ExternalSyntheticApiModelOutline0',
      expected_outer_class='pkg.Cls',
      expected_name='pkg.Cls$$ExternalSyntheticApiModelOutline0',
    )

  def testNoramlize_r8Outline(self):
    self.assertNormalizedTo(
      class_path='pkg.Cls$$ExternalSyntheticOutline0',
      expected_outer_class=None,
      expected_name='pkg.Cls$$ExternalSyntheticOutline0',
    )

  def testNoramlize_externalSyntheticCodegen(self):
    self.assertNormalizedTo(
      class_path='pkg.Cls$$ExternalSyntheticThrowCCEIfNotNull0',
      expected_outer_class=None,
      expected_name=('pkg.Cls$$ExternalSyntheticThrowCCEIfNotNull0'),
    )

    self.assertNormalizedTo(
      class_path='pkg.Cls$$ExternalSyntheticBackportWithForwarding0',
      expected_outer_class=None,
      expected_name=('pkg.Cls$$ExternalSyntheticBackportWithForwarding0'),
    )

  def testNoramlize_externalSyntheticOther(self):
    self.assertNormalizedTo(
      class_path='pkg.Cls$$ExternalSyntheticServiceLoad0',
      expected_outer_class='pkg.Cls',
      expected_name='pkg.Cls$$ExternalSyntheticServiceLoad0',
    )

  def testNoramlize_multiSameLine(self):
    name = (
      'pkg1.Cls$$InternalSyntheticLambda$3$81073ff626$0 '
      'pkg2.Cls$$InternalSyntheticLambda$0$81073ff626$0 bar'
    )
    outer_class = name.split(' ')[0]
    expected_name = (
      'pkg1.Cls$$InternalSyntheticLambda$3 '
      'pkg2.Cls$$InternalSyntheticLambda$0$81073ff626$0 bar'
    )
    self.assertEqual(
      ('pkg1.Cls', expected_name), dex.NormalizeLine(outer_class, name)
    )
    name = expected_name
    outer_class = name.split(' ')[1]
    expected_name = (
      'pkg1.Cls$$InternalSyntheticLambda$3 '
      'pkg2.Cls$$InternalSyntheticLambda$0 bar'
    )
    self.assertEqual(
      ('pkg2.Cls', expected_name), dex.NormalizeLine(outer_class, name)
    )

  def testCreateDexSymbol_normal(self):
    name = (
      'org.StackAnimation org.ChromeAnimation '
      'createReachTopAnimatorSet(org.StackTab[],float)'
    )
    size = 1
    source_map = {}
    symbol = dex.CreateDexSymbol(name, size, source_map)
    self.assertEqual('$APK/org/StackAnimation', symbol.object_path)

  def testCreateDexSymbol_classMerged_noSource(self):
    name = (
      'org.NewClass org.ChromeAnimation '
      'org.OldClass.createReachTopAnimatorSet(org.StackTab[],float)'
    )
    size = 1
    source_map = {}
    symbol = dex.CreateDexSymbol(name, size, source_map)
    self.assertEqual('$APK/org/OldClass', symbol.object_path)

  def testCreateDexSymbol_classMerged_withSource(self):
    name = (
      'org.NewClass org.ChromeAnimation '
      'org.OldClass.createReachTopAnimatorSet(org.StackTab[],float)'
    )
    size = 1
    source_map = {'org.OldClass': 'old_path.java'}
    symbol = dex.CreateDexSymbol(name, size, source_map)
    self.assertEqual('$APK/org/OldClass', symbol.object_path)
    self.assertEqual('old_path.java', symbol.source_path)

  def testCreateDexSymbol_classMerged_field(self):
    name = 'org.NewClass int org.OldClass.createReachTopAnimatorSet'
    size = 1
    source_map = {}
    symbol = dex.CreateDexSymbol(name, size, source_map)
    self.assertEqual('$APK/org/OldClass', symbol.object_path)

  def _CreateNodes(self, mapping_lines=(), supertypes=None):
    mapping = dex_deobfuscate.ParseMappingLines(mapping_lines)
    analyzer = dex._DexSizeAnalyzer(mapping)
    with open(_TEST_DEX_PATH, 'rb') as f:
      analyzer.AddDexFile(dex_parser.DexFile(f.read()))
    if supertypes:
      analyzer._supertypes.update(supertypes)
    return dict(analyzer.CreateNodes())

  def testDexSizeAnalyzer_matchesApkAnalyzer(self):
    # Expected values are from running apkanalyzer on the same dex file.
    expected = {
      'AuxClass': 40,
      'AuxClass <init>(int)': 42,
      'AuxClass int mRepeat': 10,
      'AuxClass int[] reverse(int[])': 62,
      'AuxClass java.lang.String repeatString(java.lang.String)': 90,
      'TestClass': 57,
      'TestClass <init>()': 38,
      'TestClass double quadHi(double,double,double)': 120,
      'TestClass int REPEAT': 12,
      'TestClass java.lang.String repeat(java.lang.String)': 50,
      'TestClass long gcd(long,long)': 62,
      'TestClass long lcm(long,long)': 58,
      'java.lang.IllegalArgumentException': 0,
      'java.lang.IllegalArgumentException <init>(java.lang.String)': 8,
      'java.lang.Math': 0,
      'java.lang.Math double sqrt(double)': 8,
      'java.lang.Object': 0,
      'java.lang.Object <init>()': 8,
      'java.lang.StringBuilder': 0,
      'java.lang.StringBuilder <init>()': 8,
      'java.lang.StringBuilder java.lang.String toString()': 8,
      'java.lang.StringBuilder java.lang.StringBuilder '
      'append(java.lang.String)': 8,
    }
    self.assertEqual(expected, self._CreateNodes())

  def testDexSizeAnalyzer_deobfuscation(self):
    mapping_lines = [
      '# {"id":"com.android.tools.r8.mapping","version":"2.2"}',
      'org.Original -> TestClass:',
      '    int ORIGINAL_REPEAT -> REPEAT',
      '    1:2:long originalGcd(long,long):10:11 -> gcd',
      # Inlined frame followed by the outer frame.
      '    1:2:long org.Other.inlined(long):20:21 -> lcm',
      '    1:2:long originalLcm(long,long,int):30 -> lcm',
      '      # {"id":"com.android.tools.r8.residualsignature",'
      '"signature":"(JJ)J"}',
      '    3:4:long originalLcm(long,long,int):31:32 -> lcm',
      # Moved from another class.
      '    double org.Moved.quad(double,double,double) -> quadHi',
      'org.OriginalAux -> AuxClass:',
    ]
    nodes = self._CreateNodes(mapping_lines)
    self.assertEqual(57, nodes['org.Original'])
    self.assertEqual(12, nodes['org.Original int ORIGINAL_REPEAT'])
    self.assertEqual(62, nodes['org.Original long originalGcd(long,long)'])
    self.assertEqual(58, nodes['org.Original long originalLcm(long,long,int)'])
    self.assertEqual(
      120, nodes['org.Original double org.Moved.quad(double,double,double)']
    )
    self.assertEqual(
      50, nodes['org.Original java.lang.String repeat(java.lang.String)']
    )
    self.assertEqual(42, nodes['org.OriginalAux <init>(int)'])

  def testDexSizeAnalyzer_inheritedReference(self):
    mapping_lines = [
      'org.Base -> Base:',
      '    java.lang.String originalAppend(java.lang.String) -> append',
    ]
    # Pretend that StringBuilder extends Base.
    nodes = self._CreateNodes(
      mapping_lines, {'Ljava/lang/StringBuilder;': ('LBase;',)}
    )
    self.assertEqual(
      8,
      nodes[
        'java.lang.StringBuilder java.lang.String '
        'originalAppend(java.lang.String)'
      ],
    )

  def testDexSizeAnalyzer_noSupertypeLookupWhenClassHasName(self):
    mapping_lines = [
      '# {"id":"com.android.tools.r8.mapping","version":"2.2"}',
      'org.Base -> Base:',
      '    long baseGcd(long,long) -> gcd',
      'org.Original -> TestClass:',
      '    long originalGcd(long,long,int) -> gcd',
      '      # {"id":"com.android.tools.r8.residualsignature",'
      '"signature":"(JJI)J"}',
    ]
    # TestClass's mapping has an entry for "gcd" that does not match, so the
    # supertype's entry should not be used.
    nodes = self._CreateNodes(mapping_lines, {'LTestClass;': ('LBase;',)})
    self.assertEqual(62, nodes['org.Original long gcd(long,long)'])


if __name__ == '__main__':
  unittest.main()
