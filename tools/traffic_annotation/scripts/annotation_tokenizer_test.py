#!/usr/bin/env vpython3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Unit tests for annotation_tokenizer.py.
"""

import unittest

from annotation_tokenizer import Tokenizer, SourceCodeParsingError


class AnnotationTokenizerTest(unittest.TestCase):
  def testRealAnnotationDefinition(self):
    real_definition = """
        DefineNetworkTrafficAnnotation("foobar_fetcher", R"(
          semantics {
            sender: "Foobar Component"
            description: "Fetches Foobars for the user."
            trigger: "The user requests a new Foobar."
            data: "The type of Foobar the user requested."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting: "Privacy and Security > Enable Foobars"
            chrome_policy {
              FoobarsEnabled {
                FoobarsEnabled: false
              }
            }
          })");"""
    tokenizer = Tokenizer(real_definition,
                          'components/foobar/foobar_request_handler.cc', 42)
    self.assertEqual('DefineNetworkTrafficAnnotation',
                     tokenizer.advance('symbol'))
    self.assertEqual('(', tokenizer.advance('left_paren'))
    self.assertEqual('foobar_fetcher', tokenizer.advance('string_literal'))
    self.assertEqual(',', tokenizer.advance('comma'))
    self.assertTrue(bool(tokenizer.advance('string_literal')))
    self.assertEqual(')', tokenizer.advance('right_paren'))

  def testAdvanceHappyPath(self):
    tokenizer = Tokenizer('"hello", R"(world)", function_name())));',
                          'foo.txt', 33)
    self.assertEqual('hello', tokenizer.advance('string_literal'))
    self.assertEqual(',', tokenizer.advance('comma'))
    self.assertEqual('world', tokenizer.advance('string_literal'))
    self.assertEqual(',', tokenizer.advance('comma'))
    self.assertEqual('function_name', tokenizer.advance('symbol'))
    self.assertEqual('(', tokenizer.advance('left_paren'))
    self.assertEqual(')', tokenizer.advance('right_paren'))
    self.assertEqual(')', tokenizer.advance('right_paren'))

  def testAdvanceMultiline(self):
    tokenizer = Tokenizer('\n\tR"(the quick\nbrown\nfox)"', 'foo.txt', 33)
    self.assertEqual(
        'the quick\nbrown\nfox', tokenizer.advance('string_literal'))

  def testAdvanceTextBlock(self):
    tokenizer = Tokenizer('\n """\n  the quick\n  red\n  fox"""', 'foo.txt', 2)
    self.assertEqual('the quick\nred\nfox', tokenizer.advance('string_literal'))

  def testAdvanceErrorPaths(self):
    tokenizer = Tokenizer('  hello , ', 'foo.txt', 33)
    tokenizer.advance('symbol')
    with self.assertRaisesRegex(SourceCodeParsingError,
                                'Expected symbol.+at foo.txt:33'):
      # There are no more tokens.
      tokenizer.advance('symbol')

    tokenizer = Tokenizer('"hello"', 'foo.txt', 33)
    with self.assertRaisesRegex(SourceCodeParsingError,
                                'Expected comma.+at foo.txt:33'):
      # The type doesn't match.
      tokenizer.advance('comma')

    tokenizer = Tokenizer('{', 'foo.txt', 33)
    with self.assertRaisesRegex(SourceCodeParsingError,
                                'Expected string_literal.+at foo.txt:33'):
      # Not a valid token at all.
      tokenizer.advance('string_literal')

  def testMaybeAdvance(self):
    tokenizer = Tokenizer('"hello", world', 'foo.txt', 33)
    self.assertEqual(None, tokenizer.maybe_advance('symbol'))
    self.assertEqual('hello', tokenizer.maybe_advance('string_literal'))
    self.assertEqual(',', tokenizer.maybe_advance('comma'))
    self.assertEqual(None, tokenizer.maybe_advance('left_paren'))
    self.assertEqual('world', tokenizer.maybe_advance('symbol'))
    self.assertEqual(None, tokenizer.maybe_advance('right_paren'))

  def testEscaping(self):
    tokenizer = Tokenizer(
        '''
      "\\"ab\\nc \\\\\\" def \\\\\\""
      "string ends here:\\\\" this is not part of the string"
    ''', 'foo.txt', 33)
    self.assertEqual('"ab\nc \\" def \\"', tokenizer.advance('string_literal'))
    self.assertEqual('string ends here:\\', tokenizer.advance('string_literal'))


if __name__ == '__main__':
  unittest.main()
