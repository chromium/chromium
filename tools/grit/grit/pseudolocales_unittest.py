#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Unit tests for grit.pseudolocales'''

import sys
import os.path
import unittest

if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

import grit.pseudolocales as pl

from grit import tclib
import grit.extern.tclib

PLACEHOLDER_NODE = pl.BasicVariable(pl.PLACEHOLDER_STRING)
VAR_NODE = pl.BasicVariable('{VAR}')


class TclibUnittest(unittest.TestCase):
  def assertBuildTree(self, text, tree):
    self.assertTreesEqual(pl.BuildTree(text), tree)

  def assertTreesEqual(self, lhs, rhs):
    try:
      self.assertSubtreesEqual(lhs, rhs)
    except AssertionError as e:
      print('Actual:', lhs, 'Expected:', rhs, sep='\n')
      raise e

  def assertSubtreesEqual(self, lhs, rhs):
    try:
      self.assertIsInstance(lhs, rhs.__class__)
      self.assertIsInstance(rhs, lhs.__class__)
      self.assertEqual(lhs.text, rhs.text)
      self.assertEqual(lhs.after, rhs.after)
      self.assertEqual(len(lhs.children), len(rhs.children))
    except AssertionError as e:
      print('Failing subtree actual:',
            lhs,
            'Failing subtree expected:',
            rhs,
            sep='\n')
      raise e
    for lhs_child, rhs_child in zip(lhs.children, rhs.children):
      self.assertSubtreesEqual(lhs_child, rhs_child)

  def testSplitTextBasic(self):
    self.assertBuildTree('foo bar baz', pl.RawText('foo bar baz'))

  def testSplitTextTags(self):
    self.assertBuildTree(
        'foo<a href="url">bar</a>baz',
        pl.NodeSequence([
            pl.RawText('foo'),
            pl.HtmlTag('<a href="url">'),
            pl.RawText('bar'),
            pl.HtmlTag('</a>'),
            pl.RawText('baz'),
        ]))

  def testSplitTextVariables(self):
    self.assertBuildTree(
        'Downloaded by <a href="{PLACEHOLDER}">{PLACEHOLDER}</a>${foo}{bar}',
        pl.NodeSequence([
            pl.RawText('Downloaded by '),
            pl.HtmlTag('<a href="{PLACEHOLDER}">'),
            pl.BasicVariable('{PLACEHOLDER}'),
            pl.HtmlTag('</a>'),
            pl.BasicVariable('${foo}'),
            pl.BasicVariable('{bar}'),
        ]))

  def testSplitTextWithCurlies(self):
    self.assertBuildTree(
        '''{COUNT, plural,
  =0  {Open all in &incognito window}
  =1 {Open in &incognito window}
  other {Open all ({COUNT}) in &incognito window}}''',
        pl.Plural('{COUNT, plural,\n  ', [
            pl.PluralOption('=0  {',
                            [pl.RawText('Open all in &incognito window')]),
            pl.PluralOption('=1 {', [pl.RawText('Open in &incognito window')]),
            pl.PluralOption('other {', [
                pl.RawText('Open all ('),
                pl.BasicVariable('{COUNT}'),
                pl.RawText(') in &incognito window'),
            ]),
        ]))

    self.assertBuildTree(
        '''{ATTEMPTS_LEFT, plural,
  =1 {{0} attempt left}
  other {{0}<a>attempts</a> left}}''',
        pl.Plural('{ATTEMPTS_LEFT, plural,\n  ', [
            pl.PluralOption(
                '=1 {', [pl.BasicVariable('{0}'),
                         pl.RawText(' attempt left')]),
            pl.PluralOption('other {', [
                pl.BasicVariable('{0}'),
                pl.HtmlTag('<a>'),
                pl.RawText('attempts'),
                pl.HtmlTag('</a>'),
                pl.RawText(' left')
            ])
        ]))

  def testRawTextNumWords(self):
    self.assertEqual(pl.RawText('hello').GetNumWords(), 1)
    self.assertEqual(pl.RawText('hello world').GetNumWords(), 2)
    self.assertEqual(pl.RawText('  ').GetNumWords(), 0)
    self.assertEqual(pl.RawText('hello \n world').GetNumWords(), 2)
    self.assertEqual(pl.RawText('that\'s nice, right').GetNumWords(), 3)
    self.assertEqual(pl.RawText('1 2 3 4 5').GetNumWords(), 5)

  def testTreeNumWords(self):
    self.assertEqual(pl.HtmlTag('<a href="blah">').GetNumWords(), 0)
    self.assertEqual(pl.BasicVariable('{COUNT}').GetNumWords(), 1)

    self.assertEqual(
        pl.NodeSequence([pl.RawText('hi'),
                         pl.RawText('World')]).GetNumWords(), 2)
    self.assertEqual(
        pl.PluralOption(
            'other {',
            [pl.RawText('hello'), pl.RawText('World')]).GetNumWords(), 2)
    self.assertEqual(
        pl.Plural('{COUNT, plural, {', [
            pl.PluralOption('=0 {', [pl.RawText('1 2')]),
            pl.PluralOption('=0 {', [pl.RawText('1 2 3')]),
            pl.PluralOption('=0 {', [pl.RawText('1 2 3 4 5 6 7 8 9 10')]),
        ]).GetNumWords(), 10)

  def assertTransformsInto(self, initial, expected):
    initial.Transform(lambda x: x.title())
    self.assertTreesEqual(initial, expected)

  def testTransform(self):
    self.assertTransformsInto(pl.RawText('HI WORLD'), pl.RawText('Hi World'))
    self.assertTransformsInto(pl.BasicVariable('{HELLO}'),
                              pl.BasicVariable('{HELLO}'))
    self.assertTransformsInto(pl.HtmlTag('<a>'), pl.HtmlTag('<a>'))
    self.assertTransformsInto(
        pl.NodeSequence(
            [pl.RawText('HELLO'),
             pl.HtmlTag('<a>'),
             pl.RawText('WORLD')]),
        pl.NodeSequence(
            [pl.RawText('Hello'),
             pl.HtmlTag('<a>'),
             pl.RawText('World')]))
    self.assertTransformsInto(
        pl.Plural('{ATTEMPTS_LEFT, plural,\n  ', [
            pl.PluralOption('=1 {', [pl.RawText('hello')]),
            pl.PluralOption('other {', [pl.RawText('world')])
        ]),
        pl.Plural('{ATTEMPTS_LEFT, plural,\n  ', [
            pl.PluralOption('=1 {', [pl.RawText('Hello')]),
            pl.PluralOption('other {', [pl.RawText('World')])
        ]))

  def testToString(self):
    self.assertEqual(pl.RawText('Hello world').ToString(), 'Hello world')
    self.assertEqual(pl.BasicVariable('{0}').ToString(), '{0}')
    self.assertEqual(pl.HtmlTag('<a>').ToString(), '<a>')
    self.assertEqual(
        pl.NodeSequence([pl.RawText('Hello'),
                         pl.RawText('World')]).ToString(), 'HelloWorld')
    self.assertEqual(
        pl.Plural('{ATTEMPTS_LEFT, plural,\n  ', [
            pl.PluralOption('=1 {', [pl.RawText('hello')]),
            pl.PluralOption('other {', [pl.RawText('world')])
        ]).ToString(),
        '{ATTEMPTS_LEFT, plural,\n  =1 {hello}\nother {world}\n}')

  def testBuildAndUnbuildTree(self):
    p1 = tclib.Placeholder('USERNAME', '%s', 'foo')
    p2 = tclib.Placeholder('EMAIL', '%s', 'bar')

    msg = tclib.Message()
    msg.AppendText('hello')
    msg.AppendPlaceholder(p1)
    msg.AppendPlaceholder(p2)
    msg.AppendText('world')

    tree, placeholders = pl.BuildTreeFromMessage(msg)
    self.assertTreesEqual(
        tree,
        pl.NodeSequence([
            pl.RawText('hello'), PLACEHOLDER_NODE, PLACEHOLDER_NODE,
            pl.RawText('world')
        ]))
    self.assertEqual(placeholders, [p1, p2])

    transl = pl.ToTranslation(tree, placeholders)
    self.assertEqual(transl.GetContent(), ['hello', p1, p2, 'world'])

  def testPseudolocales(self):
    p1 = tclib.Placeholder('USERNAME', '%s', 'foo')
    p2 = tclib.Placeholder('EMAIL', '%s', 'bar')
    msg = tclib.Message()
    msg.AppendText('h_')
    msg.AppendPlaceholder(p1)
    msg.AppendPlaceholder(p2)
    msg.AppendText('w')

    self.assertEqual(
        pl.PseudoLongStringMessage(msg).GetContent(),
        ['\u0125_', p1, p2, '\u0175', ' - one two three four'])

    msg.AppendText('hello world')
    self.assertEqual(
        pl.PseudoRTLMessage(msg).GetContent(),
        ['\u202eh\u202c_', p1, p2, '\u202ewhello\u202c \u202eworld\u202c'])

  # If it fails to translate with prod messages, add the failure to here to
  # make sure it doesn't happen again.
  def testProdFailures(self):
    p1 = tclib.Placeholder('USERNAME', '%s', 'foo')

    msg = tclib.Message()
    msg.AppendText('{LINE_COUNT, plural,\n      =1 {<1 line not shown>}\n'
                   '      other {<')
    msg.AppendPlaceholder(p1)
    msg.AppendText(' lines not shown>}\n}')
    tree, _ = pl.BuildTreeFromMessage(msg)
    self.assertTreesEqual(
        tree,
        pl.Plural('{LINE_COUNT, plural,\n      ', [
            pl.PluralOption('=1 {', [pl.RawText('<1 line not shown>')]),
            pl.PluralOption('other {', [
                pl.RawText('<'), PLACEHOLDER_NODE,
                pl.RawText(' lines not shown>')
            ])
        ]))

    msg = tclib.Message()
    msg.AppendText('{1, plural,\n   \n             =1 {Rated ')
    msg.AppendPlaceholder(p1)
    msg.AppendText(' by one user.}\n      other{Rated ')
    msg.AppendPlaceholder(p1)
    msg.AppendText(' by # users.}}')
    tree, _ = pl.BuildTreeFromMessage(msg)
    self.assertTreesEqual(
        tree,
        pl.Plural('{1, plural,\n   \n             ', [
            pl.PluralOption('=1 {', [
                pl.RawText('Rated '), PLACEHOLDER_NODE,
                pl.RawText(' by one user.')
            ]),
            pl.PluralOption('other{', [
                pl.RawText('Rated '), PLACEHOLDER_NODE,
                pl.RawText(' by # users.')
            ]),
        ]))

    self.assertBuildTree(
        '{count, plural, offset:2\n'
        '        =1 {{VAR}}\n'
        '        =2 {{VAR}, {VAR}}\n'
        '        other {{VAR}, {VAR}, and # more}\n'
        '      }',
        pl.Plural('{count, plural, offset:2\n        ', [
            pl.PluralOption('=1 {', [VAR_NODE]),
            pl.PluralOption('=2 {',
                            [VAR_NODE, pl.RawText(', '), VAR_NODE]),
            pl.PluralOption('other {', [
                VAR_NODE,
                pl.RawText(', '), VAR_NODE,
                pl.RawText(', and # more')
            ]),
        ]))

    self.assertBuildTree(
        '{NUM_POPUPS,plural,=1{Pop-up blocked} other{# pop-ups blocked}}',
        pl.Plural('{NUM_POPUPS,plural,', [
            pl.PluralOption('=1{', [pl.RawText('Pop-up blocked')]),
            pl.PluralOption('other{', [pl.RawText('# pop-ups blocked')])
        ]))

    self.assertBuildTree(
        'Open ${url}',
        pl.NodeSequence([pl.RawText('Open '),
                         pl.BasicVariable('${url}')]))


if __name__ == '__main__':
  unittest.main()
