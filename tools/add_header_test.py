#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import random
import unittest

import add_header


class DecoratedFilenameTest(unittest.TestCase):
  def testCHeaderClassification(self):
    self.assertTrue(add_header.IsCSystemHeader('<stdlib.h>'))
    self.assertFalse(add_header.IsCSystemHeader('<type_traits>'))
    self.assertFalse(add_header.IsCSystemHeader('"moo.h"'))

  def testCXXHeaderClassification(self):
    self.assertFalse(add_header.IsCXXSystemHeader('<stdlib.h>'))
    self.assertTrue(add_header.IsCXXSystemHeader('<type_traits>'))
    self.assertFalse(add_header.IsCXXSystemHeader('"moo.h"'))

  def testUserHeaderClassification(self):
    self.assertFalse(add_header.IsUserHeader('<stdlib.h>'))
    self.assertFalse(add_header.IsUserHeader('<type_traits>'))
    self.assertTrue(add_header.IsUserHeader('"moo.h"'))

  def testClassifyHeader(self):
    self.assertEqual(add_header.ClassifyHeader('<stdlib.h>'),
                     add_header._HEADER_TYPE_C_SYSTEM)
    self.assertEqual(add_header.ClassifyHeader('<type_traits>'),
                     add_header._HEADER_TYPE_CXX_SYSTEM)
    self.assertEqual(add_header.ClassifyHeader('"moo.h"'),
                     add_header._HEADER_TYPE_USER)
    self.assertEqual(add_header.ClassifyHeader('invalid'),
                     add_header._HEADER_TYPE_INVALID)


class FindIncludesTest(unittest.TestCase):
  def testEmpty(self):
    begin, end = add_header.FindIncludes([])
    self.assertEqual(begin, -1)
    self.assertEqual(end, -1)

  def testNoIncludes(self):
    begin, end = add_header.FindIncludes(['a'])
    self.assertEqual(begin, -1)
    self.assertEqual(end, -1)

  def testOneInclude(self):
    begin, end = add_header.FindIncludes(['#include <algorithm>'])
    self.assertEqual(begin, 0)
    self.assertEqual(end, 1)

  def testIncludeWithInlineComment(self):
    begin, end = add_header.FindIncludes(
        ['#include "moo.h"  // TODO: Add more sounds.'])
    self.assertEqual(begin, 0)
    self.assertEqual(end, 1)

  def testNewlinesBetweenIncludes(self):
    begin, end = add_header.FindIncludes(
        ['#include <utility>', '', '#include "moo.h"'])
    self.assertEqual(begin, 0)
    self.assertEqual(end, 3)

  def testCommentsBetweenIncludes(self):
    begin, end = add_header.FindIncludes([
        '#include <utility>', '// TODO: Add goat support.', '#include "moo.h"'
    ])
    self.assertEqual(begin, 0)
    self.assertEqual(end, 3)

  def testEmptyLinesNotIncluded(self):
    begin, end = add_header.FindIncludes(
        ['', '#include <utility>', '', '#include "moo.h"', ''])
    self.assertEqual(begin, 1)
    self.assertEqual(end, 4)

  def testCommentsNotIncluded(self):
    begin, end = add_header.FindIncludes([
        '// Cow module.', '#include <utility>', '// For cow speech synthesis.',
        '#include "moo.h"', '// TODO: Add Linux audio support.'
    ])
    self.assertEqual(begin, 1)
    self.assertEqual(end, 4)

  def testNonIncludesLinesBeforeIncludesIgnored(self):
    begin, end = add_header.FindIncludes(
        ['#ifndef COW_H_', '#define COW_H_', '#include "moo.h"'])
    self.assertEqual(begin, 2)
    self.assertEqual(end, 3)

  def testNonIncludesLinesAfterIncludesTerminates(self):
    begin, end = add_header.FindIncludes([
        '#include "moo.h"', '#ifndef COW_MESSAGES_H_', '#define COW_MESSAGE_H_'
    ])
    self.assertEqual(begin, 0)
    self.assertEqual(end, 1)


class IncludeTest(unittest.TestCase):
  def testToSource(self):
    self.assertEqual(
        add_header.Include('<moo.h>', 'include', [], None).ToSource(),
        ['#include <moo.h>'])

  def testIncludeWithPreambleToSource(self):
    self.assertEqual(
        add_header.Include('"moo.h"', 'include', ['// preamble'],
                           None).ToSource(),
        ['// preamble', '#include "moo.h"'])

  def testIncludeWithInlineCommentToSource(self):
    self.assertEqual(
        add_header.Include('"moo.h"', 'include', [],
                           ' inline comment').ToSource(),
        ['#include "moo.h"  // inline comment'])

  def testIncludeWithPreambleAndInlineCommentToSource(self):
    # Make sure whitespace is vaguely normalized too.
    self.assertEqual(
        add_header.Include('"moo.h"', 'include', [
            '// preamble with trailing space ',
        ], ' inline comment with trailing space ').ToSource(), [
            '// preamble with trailing space',
            '#include "moo.h"  // inline comment with trailing space'
        ])

  def testImportToSource(self):
    self.assertEqual(
        add_header.Include('"moo.h"', 'import', [], None).ToSource(),
        ['#import "moo.h"'])


class ParseIncludesTest(unittest.TestCase):
  def testInvalid(self):
    self.assertIsNone(add_header.ParseIncludes(['invalid']))

  def testInclude(self):
    includes = add_header.ParseIncludes(['#include "moo.h"'])
    self.assertEqual(len(includes), 1)
    self.assertEqual(includes[0].decorated_name, '"moo.h"')
    self.assertEqual(includes[0].directive, 'include')
    self.assertEqual(includes[0].preamble, [])
    self.assertIsNone(includes[0].inline_comment)
    self.assertEqual(includes[0].header_type, add_header._HEADER_TYPE_USER)
    self.assertFalse(includes[0].is_primary_header)

  def testIncludeSurroundedByWhitespace(self):
    includes = add_header.ParseIncludes([' #include "moo.h" '])
    self.assertEqual(len(includes), 1)
    self.assertEqual(includes[0].decorated_name, '"moo.h"')
    self.assertEqual(includes[0].directive, 'include')
    self.assertEqual(includes[0].preamble, [])
    self.assertIsNone(includes[0].inline_comment)
    self.assertEqual(includes[0].header_type, add_header._HEADER_TYPE_USER)
    self.assertFalse(includes[0].is_primary_header)

  def testImport(self):
    includes = add_header.ParseIncludes(['#import "moo.h"'])
    self.assertEqual(len(includes), 1)
    self.assertEqual(includes[0].decorated_name, '"moo.h"')
    self.assertEqual(includes[0].directive, 'import')
    self.assertEqual(includes[0].preamble, [])
    self.assertIsNone(includes[0].inline_comment)
    self.assertEqual(includes[0].header_type, add_header._HEADER_TYPE_USER)
    self.assertFalse(includes[0].is_primary_header)

  def testIncludeWithPreamble(self):
    includes = add_header.ParseIncludes(
        ['// preamble comment ', '#include "moo.h"'])
    self.assertEqual(len(includes), 1)
    self.assertEqual(includes[0].decorated_name, '"moo.h"')
    self.assertEqual(includes[0].directive, 'include')
    self.assertEqual(includes[0].preamble, ['// preamble comment '])
    self.assertIsNone(includes[0].inline_comment)
    self.assertEqual(includes[0].header_type, add_header._HEADER_TYPE_USER)
    self.assertFalse(includes[0].is_primary_header)

  def testIncludeWithInvalidPreamble(self):
    self.assertIsNone(
        add_header.ParseIncludes(['// orphan comment', '', '#include "moo.h"']))

  def testIncludeWIthInlineComment(self):
    includes = add_header.ParseIncludes(['#include "moo.h"// For SFX '])
    self.assertEqual(len(includes), 1)
    self.assertEqual(includes[0].decorated_name, '"moo.h"')
    self.assertEqual(includes[0].directive, 'include')
    self.assertEqual(includes[0].preamble, [])
    self.assertEqual(includes[0].inline_comment, ' For SFX ')
    self.assertEqual(includes[0].header_type, add_header._HEADER_TYPE_USER)
    self.assertFalse(includes[0].is_primary_header)

  def testIncludeWithInlineCommentAndPreamble(self):
    includes = add_header.ParseIncludes(
        ['// preamble comment ', '#include "moo.h"  // For SFX '])
    self.assertEqual(len(includes), 1)
    self.assertEqual(includes[0].decorated_name, '"moo.h"')
    self.assertEqual(includes[0].directive, 'include')
    self.assertEqual(includes[0].preamble, ['// preamble comment '])
    self.assertEqual(includes[0].inline_comment, ' For SFX ')
    self.assertEqual(includes[0].header_type, add_header._HEADER_TYPE_USER)
    self.assertFalse(includes[0].is_primary_header)

  def testMultipleIncludes(self):
    includes = add_header.ParseIncludes([
        '#include <time.h>', '', '#include "moo.h"  // For SFX ',
        '// TODO: Implement death ray.', '#import "goat.h"'
    ])
    self.assertEqual(len(includes), 3)
    self.assertEqual(includes[0].decorated_name, '<time.h>')
    self.assertEqual(includes[0].directive, 'include')
    self.assertEqual(includes[0].preamble, [])
    self.assertIsNone(includes[0].inline_comment)
    self.assertEqual(includes[0].header_type, add_header._HEADER_TYPE_C_SYSTEM)
    self.assertFalse(includes[0].is_primary_header)
    self.assertEqual(includes[1].decorated_name, '"moo.h"')
    self.assertEqual(includes[1].directive, 'include')
    self.assertEqual(includes[1].preamble, [])
    self.assertEqual(includes[1].inline_comment, ' For SFX ')
    self.assertEqual(includes[1].header_type, add_header._HEADER_TYPE_USER)
    self.assertFalse(includes[1].is_primary_header)
    self.assertEqual(includes[2].decorated_name, '"goat.h"')
    self.assertEqual(includes[2].directive, 'import')
    self.assertEqual(includes[2].preamble, ['// TODO: Implement death ray.'])
    self.assertIsNone(includes[2].inline_comment)
    self.assertEqual(includes[2].header_type, add_header._HEADER_TYPE_USER)
    self.assertFalse(includes[2].is_primary_header)


class MarkPrimaryIncludeTest(unittest.TestCase):
  def _extract_primary_name(self, includes):
    for include in includes:
      if include.is_primary_header:
        return include.decorated_name

  def testNoOpOnHeader(self):
    includes = [add_header.Include('"cow.h"', 'include', [], None)]
    add_header.MarkPrimaryInclude(includes, 'cow.h')
    self.assertIsNone(self._extract_primary_name(includes))

  def testSystemHeaderNotMatched(self):
    includes = [add_header.Include('<cow.h>', 'include', [], None)]
    add_header.MarkPrimaryInclude(includes, 'cow.cc')
    self.assertIsNone(self._extract_primary_name(includes))

  def testExactMatch(self):
    includes = [
        add_header.Include('"cow.h"', 'include', [], None),
        add_header.Include('"cow_posix.h"', 'include', [], None),
    ]
    add_header.MarkPrimaryInclude(includes, 'cow.cc')
    self.assertEqual(self._extract_primary_name(includes), '"cow.h"')

  def testFuzzyMatch(self):
    includes = [add_header.Include('"cow.h"', 'include', [], None)]
    add_header.MarkPrimaryInclude(includes, 'cow_linux_unittest.cc')
    self.assertEqual(self._extract_primary_name(includes), '"cow.h"')

  def testFuzzymatchInReverse(self):
    includes = [add_header.Include('"cow.h"', 'include', [], None)]
    add_header.MarkPrimaryInclude(includes, 'cow_uitest_aura.cc')
    self.assertEqual(self._extract_primary_name(includes), '"cow.h"')

  def testFuzzyMatchDoesntMatchDifferentSuffixes(self):
    includes = [add_header.Include('"cow_posix.h"', 'include', [], None)]
    add_header.MarkPrimaryInclude(includes, 'cow_windows.cc')
    self.assertIsNone(self._extract_primary_name(includes))

  def testMarksMostSpecific(self):
    includes = [
        add_header.Include('"cow.h"', 'include', [], None),
        add_header.Include('"cow_posix.h"', 'include', [], None),
    ]
    add_header.MarkPrimaryInclude(includes, 'cow_posix.cc')
    self.assertEqual(self._extract_primary_name(includes), '"cow_posix.h"')

  def testFullPathMatch(self):
    includes = [add_header.Include('"zfs/impl/cow.h"', 'include', [], None)]
    add_header.MarkPrimaryInclude(includes, 'zfs/impl/cow.cc')
    self.assertEqual(self._extract_primary_name(includes), '"zfs/impl/cow.h"')

  def testTopmostDirectoryDoesNotMatch(self):
    includes = [add_header.Include('"animal/impl/cow.h"', 'include', [], None)]
    add_header.MarkPrimaryInclude(includes, 'zfs/impl/cow.cc')
    self.assertIsNone(self._extract_primary_name(includes))

  def testSubstantiallySimilarPaths(self):
    includes = [
        add_header.Include('"farm/public/animal/cow.h"', 'include', [], None)
    ]
    add_header.MarkPrimaryInclude(includes, 'farm/animal/cow.cc')
    self.assertEqual(self._extract_primary_name(includes),
                     '"farm/public/animal/cow.h"')

  def testSubstantiallySimilarPathsAndExactMatch(self):
    includes = [
        add_header.Include('"ui/gfx/ipc/geometry/gfx_param_traits.h"',
                           'include', [], None),
        add_header.Include('"ui/gfx/ipc/gfx_param_traits.h"', 'include', [],
                           None),
    ]
    add_header.MarkPrimaryInclude(includes, 'ui/gfx/ipc/gfx_param_traits.cc')
    self.assertEqual(self._extract_primary_name(includes),
                     '"ui/gfx/ipc/gfx_param_traits.h"')

  def testNoMatchingSubdirectories(self):
    includes = [add_header.Include('"base/zfs/cow.h"', 'include', [], None)]
    add_header.MarkPrimaryInclude(includes, 'base/animal/cow.cc')
    self.assertIsNone(self._extract_primary_name(includes))


class SerializeIncludesTest(unittest.TestCase):
  def testSystemHeaders(self):
    source = add_header.SerializeIncludes([
        add_header.Include('<stdlib.h>', 'include', [], None),
        add_header.Include('<map>', 'include', [], None),
    ])
    self.assertEqual(source, ['#include <stdlib.h>', '', '#include <map>'])

  def testUserHeaders(self):
    source = add_header.SerializeIncludes([
        add_header.Include('"goat.h"', 'include', [], None),
        add_header.Include('"moo.h"', 'include', [], None),
    ])
    self.assertEqual(source, ['#include "goat.h"', '#include "moo.h"'])

  def testSystemAndUserHeaders(self):
    source = add_header.SerializeIncludes([
        add_header.Include('<stdlib.h>', 'include', [], None),
        add_header.Include('<map>', 'include', [], None),
        add_header.Include('"moo.h"', 'include', [], None),
    ])
    self.assertEqual(
        source,
        ['#include <stdlib.h>', '', '#include <map>', '', '#include "moo.h"'])

  def testPrimaryAndSystemHeaders(self):
    primary_header = add_header.Include('"cow.h"', 'include', [], None)
    primary_header.is_primary_header = True
    source = add_header.SerializeIncludes([
        primary_header,
        add_header.Include('<stdlib.h>', 'include', [], None),
        add_header.Include('<map>', 'include', [], None),
    ])
    self.assertEqual(
        source,
        ['#include "cow.h"', '', '#include <stdlib.h>', '', '#include <map>'])

  def testPrimaryAndUserHeaders(self):
    primary_header = add_header.Include('"cow.h"', 'include', [], None)
    primary_header.is_primary_header = True
    source = add_header.SerializeIncludes([
        primary_header,
        add_header.Include('"moo.h"', 'include', [], None),
    ])
    self.assertEqual(source, ['#include "cow.h"', '', '#include "moo.h"'])

  def testPrimarySystemAndUserHeaders(self):
    primary_header = add_header.Include('"cow.h"', 'include', [], None)
    primary_header.is_primary_header = True
    source = add_header.SerializeIncludes([
        primary_header,
        add_header.Include('<stdlib.h>', 'include', [], None),
        add_header.Include('<map>', 'include', [], None),
        add_header.Include('"moo.h"', 'include', [], None),
    ])
    self.assertEqual(source, [
        '#include "cow.h"', '', '#include <stdlib.h>', '', '#include <map>', '',
        '#include "moo.h"'
    ])

  def testSpecialHeaders(self):
    includes = []
    primary_header = add_header.Include('"cow.h"', 'include', [], None)
    primary_header.is_primary_header = True
    includes.append(primary_header)
    includes.append(add_header.Include('<objbase.h>', 'include', [], None))
    includes.append(add_header.Include('<atlbase.h>', 'include', [], None))
    includes.append(add_header.Include('<ole2.h>', 'include', [], None))
    includes.append(add_header.Include('<shobjidl.h>', 'include', [], None))
    includes.append(add_header.Include('<tchar.h>', 'include', [], None))
    includes.append(add_header.Include('<unknwn.h>', 'include', [], None))
    includes.append(add_header.Include('<winsock2.h>', 'include', [], None))
    includes.append(add_header.Include('<windows.h>', 'include', [], None))
    includes.append(add_header.Include('<ws2tcpip.h>', 'include', [], None))
    includes.append(add_header.Include('<stddef.h>', 'include', [], None))
    includes.append(add_header.Include('<stdio.h>', 'include', [], None))
    includes.append(add_header.Include('<string.h>', 'include', [], None))
    includes.append(add_header.Include('"moo.h"', 'include', [], None))
    random.shuffle(includes)
    source = add_header.SerializeIncludes(includes)
    self.assertEqual(source, [
        '#include "cow.h"', '', '#include <objbase.h>', '#include <atlbase.h>',
        '#include <ole2.h>', '#include <shobjidl.h>', '#include <tchar.h>',
        '#include <unknwn.h>', '#include <windows.h>', '#include <winsock2.h>',
        '#include <ws2tcpip.h>', '#include <stddef.h>', '#include <stdio.h>',
        '#include <string.h>', '', '#include "moo.h"'
    ])


class AddHeaderToSourceTest(unittest.TestCase):
  def testAddInclude(self):
    source = add_header.AddHeaderToSource(
        'cow.cc', '\n'.join([
            '// Copyright info here.', '', '#include <utility>',
            '// For cow speech synthesis.',
            '#include "moo.h"  // TODO: Add Linux audio support.',
            '#include <time.h>', '#include "cow.h"', 'namespace bovine {', '',
            '// TODO: Implement.', '}  // namespace bovine'
        ]), '<memory>')
    self.assertEqual(
        source, '\n'.join([
            '// Copyright info here.', '', '#include "cow.h"', '',
            '#include <time.h>', '', '#include <memory>', '#include <utility>',
            '', '// For cow speech synthesis.',
            '#include "moo.h"  // TODO: Add Linux audio support.',
            'namespace bovine {', '', '// TODO: Implement.',
            '}  // namespace bovine', ''
        ]))

  def testAlreadyIncluded(self):
    # To make sure the original source is returned unmodified, the input source
    # intentionally scrambles the #include order.
    source = '\n'.join([
        '// Copyright info here.', '', '#include "moo.h"', '#include <utility>',
        '#include <memory>', '#include "cow.h"', 'namespace bovine {', '',
        '// TODO: Implement.', '}  // namespace bovine'
    ])
    self.assertEqual(add_header.AddHeaderToSource('cow.cc', source, '<memory>'),
                     None)

  def testConditionalIncludesLeftALone(self):
    # TODO(dcheng): Conditional header handling could probably be more clever.
    # But for the moment, this is probably Good Enough.
    source = add_header.AddHeaderToSource(
        'cow.cc', '\n'.join([
            '// Copyright info here.', '', '#include "cow.h"',
            '#include <utility>', '// For cow speech synthesis.',
            '#include "moo.h"  // TODO: Add Linux audio support.',
            '#if defined(USE_AURA)', '#include <memory>',
            '#endif  // defined(USE_AURA)'
        ]), '<memory>')
    self.assertEqual(
        source, '\n'.join([
            '// Copyright info here.', '', '#include "cow.h"', '',
            '#include <memory>', '#include <utility>', '',
            '// For cow speech synthesis.',
            '#include "moo.h"  // TODO: Add Linux audio support.',
            '#if defined(USE_AURA)', '#include <memory>',
            '#endif  // defined(USE_AURA)', ''
        ]))

  def testRemoveInclude(self):
    source = add_header.AddHeaderToSource(
        'cow.cc',
        '\n'.join([
            '// Copyright info here.', '', '#include <memory>',
            '#include <utility>', '// For cow speech synthesis.',
            '#include "moo.h"  // TODO: Add Linux audio support.',
            '#include <time.h>', '#include "cow.h"', 'namespace bovine {', '',
            '// TODO: Implement.', '}  // namespace bovine'
        ]),
        '<utility>',
        remove=True)
    self.assertEqual(
        source, '\n'.join([
            '// Copyright info here.', '', '#include "cow.h"', '',
            '#include <time.h>', '', '#include <memory>', '',
            '// For cow speech synthesis.',
            '#include "moo.h"  // TODO: Add Linux audio support.',
            'namespace bovine {', '', '// TODO: Implement.',
            '}  // namespace bovine', ''
        ]))


if __name__ == '__main__':
  unittest.main()
