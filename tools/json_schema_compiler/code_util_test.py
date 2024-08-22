#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from code_util import Code
import unittest


class CodeTest(unittest.TestCase):

  def testAppend(self):
    c = Code()
    c.Append('line')
    self.assertEqual('line', c.Render())

  def testBlock(self):
    c = Code()
    (c.Append('line') \
      .Sblock('sblock') \
        .Append('inner') \
        .Append('moreinner') \
        .Sblock('moresblock') \
          .Append('inner') \
        .Eblock('out') \
        .Append('inner') \
      .Eblock('out')
    )
    self.assertEqual(
        'line\n'
        'sblock\n'
        '  inner\n'
        '  moreinner\n'
        '  moresblock\n'
        '    inner\n'
        '  out\n'
        '  inner\n'
        'out', c.Render())

  def testConcat(self):
    b = Code()
    (b.Sblock('2') \
        .Append('2') \
      .Eblock('2')
    )
    c = Code()
    (c.Sblock('1') \
        .Concat(b) \
        .Append('1') \
      .Eblock('1')
    )
    self.assertMultiLineEqual('1\n'
                              '  2\n'
                              '    2\n'
                              '  2\n'
                              '  1\n'
                              '1', c.Render())
    d = Code()
    a = Code()
    a.Concat(d)
    self.assertEqual('', a.Render())
    a.Concat(c)
    self.assertEqual('1\n'
                     '  2\n'
                     '    2\n'
                     '  2\n'
                     '  1\n'
                     '1', a.Render())

  def testConcatErrors(self):
    c = Code()
    d = Code()
    d.Append('%s')
    self.assertRaises(TypeError, c.Concat, d)
    d = Code()
    d.Append('%(classname)s')
    self.assertRaises(TypeError, c.Concat, d)
    d = 'line of code'
    self.assertRaises(TypeError, c.Concat, d)

  def testSubstitute(self):
    c = Code()
    c.Append('%(var1)s %(var2)s %(var1)s')
    c.Substitute({'var1': 'one', 'var2': 'two'})
    self.assertEqual('one two one', c.Render())
    c.Append('%(var1)s %(var2)s %(var3)s')
    c.Append('%(var2)s %(var1)s %(var3)s')
    c.Substitute({'var1': 'one', 'var2': 'two', 'var3': 'three'})
    self.assertEqual('one two one\n'
                     'one two three\n'
                     'two one three', c.Render())

  def testSubstituteErrors(self):
    # No unnamed placeholders allowed when substitute is run
    c = Code()
    c.Append('%s %s')
    self.assertRaises(TypeError, c.Substitute, ('var1', 'one'))
    c = Code()
    c.Append('%s %(var1)s')
    self.assertRaises(TypeError, c.Substitute, {'var1': 'one'})
    c = Code()
    c.Append('%s %(var1)s')
    self.assertRaises(TypeError, c.Substitute, {'var1': 'one'})
    c = Code()
    c.Append('%(var1)s')
    self.assertRaises(KeyError, c.Substitute, {'clearlynotvar1': 'one'})

  def testIsEmpty(self):
    c = Code()
    self.assertTrue(c.IsEmpty())
    c.Append('asdf')
    self.assertFalse(c.IsEmpty())

  def testComment(self):
    long_comment = ('This comment is ninety one characters in longness, '
                    'that is, using a different word, length.')
    c = Code()
    c.Comment(long_comment)
    self.assertEqual(
        '// This comment is ninety one characters '
        'in longness, that is, using a different\n'
        '// word, length.', c.Render())
    c = Code()
    c.Sblock('sblock')
    c.Comment(long_comment)
    c.Eblock('eblock')
    c.Comment(long_comment)
    self.assertEqual(
        'sblock\n'
        '  // This comment is ninety one characters '
        'in longness, that is, using a\n'
        '  // different word, length.\n'
        'eblock\n'
        '// This comment is ninety one characters in '
        'longness, that is, using a different\n'
        '// word, length.', c.Render())
    # Words that cannot be broken up are left as too long.
    long_word = 'x' * 100
    c = Code()
    c.Comment('xxx')
    c.Comment(long_word)
    c.Comment('xxx')
    self.assertEqual('// xxx\n'
                     '// ' + 'x' * 100 + '\n'
                     '// xxx', c.Render())
    c = Code(indent_size=2, comment_length=40)
    c.Comment(
        'Pretend this is a Closure Compiler style comment, which should '
        'both wrap and indent',
        comment_prefix=' * ',
        wrap_indent=4)
    self.assertEqual(
        ' * Pretend this is a Closure Compiler\n'
        ' *     style comment, which should both\n'
        ' *     wrap and indent', c.Render())

  def testCommentWithSpecialCharacters(self):
    c = Code()
    c.Comment('20% of 80%s')
    c.Substitute({})
    self.assertEqual('// 20% of 80%s', c.Render())
    d = Code()
    d.Append('90')
    d.Concat(c)
    self.assertEqual('90\n'
                     '// 20% of 80%s', d.Render())

  def testLinePrefixes(self):
    c = Code()
    c.Sblock(line='/**', line_prefix=' * ')
    c.Sblock('@typedef {{')
    c.Append('foo: bar,')
    c.Sblock('baz: {')
    c.Append('x: y')
    c.Eblock('}')
    c.Eblock('}}')
    c.Eblock(line=' */')
    output = c.Render()
    self.assertMultiLineEqual(
        '/**\n'
        ' * @typedef {{\n'
        ' *   foo: bar,\n'
        ' *   baz: {\n'
        ' *     x: y\n'
        ' *   }\n'
        ' * }}\n'
        ' */', output)

  def testSameLineAppendConcatComment(self):
    c = Code()
    c.Append('This is a line.')
    c.Append('This too.', new_line=False)
    d = Code()
    d.Append('And this.')
    c.Concat(d, new_line=False)
    self.assertEqual('This is a line.This too.And this.', c.Render())
    c = Code()
    c.Append('This is a')
    c.Comment(' spectacular 80-character line thingy ' +
              'that fits wonderfully everywhere.',
              comment_prefix='',
              new_line=False)
    self.assertEqual(
        'This is a spectacular 80-character line thingy that ' +
        'fits wonderfully everywhere.', c.Render())


if __name__ == '__main__':
  unittest.main()
