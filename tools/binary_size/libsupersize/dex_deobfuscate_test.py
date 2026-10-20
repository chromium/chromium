#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import dex_deobfuscate

_MAPPING = [
  '# compiler: R8',
  '# {"id":"com.android.tools.r8.mapping","version":"2.2"}',
  'org.chromium.Foo -> a.b:',
  '# {"id":"sourceFile","fileName":"Foo.java"}',
  '    int mCount -> a',
  '    org.chromium.Bar mBar -> b',
  '      # {"id":"com.android.tools.r8.residualsignature","signature":"La/c;"}',
  '    1:1:void <init>():10:10 -> <init>',
  '    1:3:void doThing(int):20:22 -> c',
  '    1:2:int org.chromium.Inlined.helper(int):5:6 -> d',
  '    1:2:void overload(int,java.lang.String):30 -> d',
  '      # {"id":"com.android.tools.r8.residualsignature","signature":"(I)V"}',
  '    3:4:void overload(int,java.lang.String):31:32 -> d',
  '    5:6:void overload(org.chromium.Bar):40:41 -> d',
  '    void org.chromium.Moved.moved() -> e',
  '    3:6:org.chromium.Bar create(org.chromium.Bar):50:50 -> f',
  '    3:6:Bar Foo.create(Bar,java.lang.String):0 -> f',
  '      # {"id":"com.android.tools.r8.synthesized"}',
  '      # {"id":"com.android.tools.r8.residualsignature",'
  '"signature":"(La/c;Ljava/lang/String;)La/c;"}',
  '    7:8:org.chromium.Bar create(org.chromium.Bar):51:51 -> f',
  '    7:8:Bar Foo.create(Bar,java.lang.String):0 -> f',
  '    1:2:Bar Foo.synthetic(Bar):0:0 -> g',
  '      # {"id":"com.android.tools.r8.synthesized"}',
  '    byte Foo.$r8$classId -> h',
  '      # {"id":"com.android.tools.r8.synthesized"}',
  'org.chromium.Bar -> a.c:',
]


class DexDeobfuscateTest(unittest.TestCase):
  def setUp(self):
    self.mapping = dex_deobfuscate.ParseMappingLines(_MAPPING)

  def testDescriptorToJavaName(self):
    self.assertEqual('int', dex_deobfuscate.DescriptorToJavaName('I'))
    self.assertEqual('int[][]', dex_deobfuscate.DescriptorToJavaName('[[I'))
    self.assertEqual('a.b', dex_deobfuscate.DescriptorToJavaName('La/b;'))
    self.assertEqual('a.b[]', dex_deobfuscate.DescriptorToJavaName('[La/b;'))

  def testDeobfuscateClassName(self):
    self.assertEqual(
      'org.chromium.Foo', self.mapping.DeobfuscateClassName('a.b')
    )
    self.assertEqual('x.y', self.mapping.DeobfuscateClassName('x.y'))

  def testDeobfuscateDescriptor(self):
    self.assertEqual(
      'org.chromium.Foo[]', self.mapping.DeobfuscateDescriptor('[La/b;')
    )
    self.assertEqual('boolean', self.mapping.DeobfuscateDescriptor('Z'))

  def testLookupField(self):
    info = self.mapping.LookupField('a.b', 'a', 'I')
    self.assertEqual(('mCount', 'int'), (info.name, info.return_type))
    # Residual signature differs from the original type.
    info = self.mapping.LookupField('a.b', 'b', 'La/c;')
    self.assertEqual(
      ('mBar', 'org.chromium.Bar'), (info.name, info.return_type)
    )
    self.assertIsNone(self.mapping.LookupField('a.b', 'z', 'I'))
    # Synthesized fields have their package-less qualifier removed.
    info = self.mapping.LookupField('a.b', 'h', 'B')
    self.assertEqual('$r8$classId', info.name)
    self.assertIsNone(self.mapping.LookupField('x.y', 'a', 'I'))

  def testLookupMethod_simple(self):
    info = self.mapping.LookupMethod('a.b', 'c', 'V', ['I'])
    self.assertEqual(
      ('doThing', 'void', ('int',)), (info.name, info.return_type, info.params)
    )
    info = self.mapping.LookupMethod('a.b', '<init>', 'V', [])
    self.assertEqual('<init>', info.name)

  def testLookupMethod_overloadsAndInlining(self):
    # Uses the outer-most frame, and matches via residual signature.
    info = self.mapping.LookupMethod('a.b', 'd', 'V', ['I'])
    self.assertEqual(
      ('overload', ('int', 'java.lang.String')), (info.name, info.params)
    )
    # Matches via deobfuscated signature.
    info = self.mapping.LookupMethod('a.b', 'd', 'V', ['La/c;'])
    self.assertEqual(
      ('overload', ('org.chromium.Bar',)), (info.name, info.params)
    )
    # Does not match the inlined frame.
    self.assertIsNone(self.mapping.LookupMethod('a.b', 'd', 'I', ['I']))

  def testLookupMethod_moved(self):
    info = self.mapping.LookupMethod('a.b', 'e', 'V', [])
    self.assertEqual('org.chromium.Moved.moved', info.name)

  def testLookupMethod_syntheticFrame(self):
    # Uses the frame below the synthetic one.
    info = self.mapping.LookupMethod(
      'a.b', 'f', 'La/c;', ['La/c;', 'Ljava/lang/String;']
    )
    self.assertEqual(
      ('create', 'org.chromium.Bar', ('org.chromium.Bar',)),
      (info.name, info.return_type, info.params),
    )
    # When there is no frame below the synthetic one, the class qualifier is
    # removed and package names are added to types.
    info = self.mapping.LookupMethod('a.b', 'g', 'La/c;', ['La/c;'])
    self.assertEqual(
      ('synthetic', 'org.chromium.Bar', ('org.chromium.Bar',)),
      (info.name, info.return_type, info.params),
    )

  def testLookupMethod_noResidualSignatureSupport(self):
    mapping = dex_deobfuscate.ParseMappingLines(
      [
        'org.chromium.Foo -> a.b:',
        '    1:3:void doThing(int):20:22 -> c',
      ]
    )
    # Without residual signatures, signature changes are assumed when there is
    # only one candidate.
    info = mapping.LookupMethod('a.b', 'c', 'V', [])
    self.assertEqual('doThing', info.name)

  def testHasMemberNamed(self):
    self.assertTrue(self.mapping.HasMemberNamed('a.b', 'c', True))
    self.assertFalse(self.mapping.HasMemberNamed('a.b', 'c', False))
    self.assertTrue(self.mapping.HasMemberNamed('a.b', 'a', False))
    self.assertFalse(self.mapping.HasMemberNamed('a.b', 'z', True))
    self.assertFalse(self.mapping.HasMemberNamed('x.y', 'c', True))

  def testLookupMethod_residualSignatureAfterInlineFrame(self):
    mapping = dex_deobfuscate.ParseMappingLines(
      [
        '# {"id":"com.android.tools.r8.mapping","version":"2.2"}',
        'org.chromium.Foo -> a.b:',
        '    1:2:int org.chromium.Inlined.helper(int):5:6 -> c',
        '      # {"id":"com.android.tools.r8.residualsignature",'
        '"signature":"()V"}',
        '    1:2:void outer(int):30 -> c',
      ]
    )
    info = mapping.LookupMethod('a.b', 'c', 'V', [])
    self.assertEqual(('outer', ('int',)), (info.name, info.params))

  def testLookupMethod_loneSyntheticFrame(self):
    mapping = dex_deobfuscate.ParseMappingLines(
      [
        '# {"id":"com.android.tools.r8.mapping","version":"2.2"}',
        'org.chromium.Foo -> a.b:',
        '    1:2:void orig(org.chromium.Bar,int):10:10 -> c',
        '    1:2:void Foo.orig(int,Bar):0 -> c',
        # Belongs to the same method as above, so should not create a new entry.
        '    3:4:void Foo.orig(int,Bar):0 -> c',
        'org.chromium.Bar -> a.c:',
      ]
    )
    self.assertEqual(1, len(mapping._GetCandidates('a.b', 'c', is_method=True)))
    # Matches using the synthetic frame's (residual) signature.
    info = mapping.LookupMethod('a.b', 'c', 'V', ['I', 'La/c;'])
    self.assertEqual(
      ('orig', ('org.chromium.Bar', 'int')), (info.name, info.params)
    )

  def testLookupMethod_loneSyntheticFrameWithSameOriginalSignature(self):
    mapping = dex_deobfuscate.ParseMappingLines(
      [
        '# {"id":"com.android.tools.r8.mapping","version":"2.2"}',
        'org.chromium.Foo -> a.b:',
        '    6:9:void <init>(int):879:879 -> <init>',
        '      # {"id":"com.android.tools.r8.residualsignature",'
        '"signature":"()V"}',
        '    10:10:void Foo.<init>(int):0:0 -> <init>',
        '      # {"id":"com.android.tools.r8.synthesized"}',
      ]
    )
    self.assertEqual(
      '<init>', mapping.LookupMethod('a.b', '<init>', 'V', []).name
    )
    info = mapping.LookupMethod('a.b', '<init>', 'V', ['I'])
    self.assertEqual(('<init>', ('int',)), (info.name, info.params))

  def testLookupField_multipleCandidates(self):
    mapping = dex_deobfuscate.ParseMappingLines(
      [
        '# {"id":"com.android.tools.r8.mapping","version":"2.2"}',
        'org.chromium.Foo -> a.b:',
        '    int mInt -> x',
        '    org.chromium.Bar mBar -> x',
        '      # {"id":"com.android.tools.r8.residualsignature",'
        '"signature":"La/c;"}',
        'org.chromium.Bar -> a.c:',
      ]
    )
    self.assertEqual('mInt', mapping.LookupField('a.b', 'x', 'I').name)
    self.assertEqual('mBar', mapping.LookupField('a.b', 'x', 'La/c;').name)
    self.assertIsNone(mapping.LookupField('a.b', 'x', 'J'))

  def testParseMappingLines_nonNumericVersion(self):
    mapping = dex_deobfuscate.ParseMappingLines(
      [
        '# {"id":"com.android.tools.r8.mapping","version":"experimental"}',
        'org.chromium.Foo -> a.b:',
      ]
    )
    self.assertEqual('org.chromium.Foo', mapping.DeobfuscateClassName('a.b'))
    self.assertTrue(mapping._has_residual_signatures)

  def testParseMappingFile_none(self):
    mapping = dex_deobfuscate.ParseMappingFile(None)
    self.assertEqual({}, mapping.class_map)
    self.assertIsNone(mapping.LookupMethod('a.b', 'c', 'V', []))


if __name__ == '__main__':
  unittest.main()
