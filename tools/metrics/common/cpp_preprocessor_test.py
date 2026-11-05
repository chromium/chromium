# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for cpp_preprocessor."""

import textwrap
import unittest

import setup_modules  # pylint: disable=unused-import
import chromium_src.tools.metrics.common.cpp_preprocessor as cpp_preprocessor

_ALL_TEST_SYMBOLS = frozenset(
  {
    'IS_WIN',
    'IS_LINUX',
    'IS_CHROMEOS',
    'IS_MAC',
    'IS_ANDROID',
    'IS_FUCHSIA',
  }
)

_PREPROCESSOR_TEST_CASES = [
  {
    'testcase_name': 'simple_if_defined',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_WIN)
          BASE_FEATURE(
              kMyFeature, "MyFeature",
              base::FEATURE_ENABLED_BY_DEFAULT);
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_in': ['kMyFeature'],
  },
  {
    'testcase_name': 'simple_if_not_defined',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_WIN)
          BASE_FEATURE(
              kMyFeature, "MyFeature",
              base::FEATURE_ENABLED_BY_DEFAULT);
        #endif
    """),
    'defines': [],
    'expected_not_in': ['kMyFeature'],
  },
  {
    'testcase_name': 'if_else_defined',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_LINUX)
          BASE_FEATURE(
              kFeatureOne, "FeatureOne",
              base::FEATURE_ENABLED_BY_DEFAULT);
        #else
          BASE_FEATURE(
              kFeatureTwo, "FeatureTwo",
              base::FEATURE_ENABLED_BY_DEFAULT);
        #endif
    """),
    'defines': ['IS_LINUX'],
    'expected_in': ['kFeatureOne'],
    'expected_not_in': ['kFeatureTwo'],
  },
  {
    'testcase_name': 'if_else_not_defined',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_LINUX)
          BASE_FEATURE(
              kFeatureOne, "FeatureOne",
              base::FEATURE_ENABLED_BY_DEFAULT);
        #else
          BASE_FEATURE(
              kFeatureTwo, "FeatureTwo",
              base::FEATURE_ENABLED_BY_DEFAULT);
        #endif
    """),
    'defines': [],
    'expected_in': ['kFeatureTwo'],
    'expected_not_in': ['kFeatureOne'],
  },
  {
    'testcase_name': 'nested_both_defined',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_CHROMEOS)
          #if BUILDFLAG(IS_WIN)
            BASE_FEATURE(
                kFeatureOne, "FeatureOne",
                base::FEATURE_ENABLED_BY_DEFAULT);
          #else
            BASE_FEATURE(
                kFeatureTwo, "FeatureTwo",
                base::FEATURE_ENABLED_BY_DEFAULT);
          #endif
        #else
          BASE_FEATURE(
              kFeatureThree, "FeatureThree",
              base::FEATURE_ENABLED_BY_DEFAULT);
        #endif
    """),
    'defines': ['IS_CHROMEOS', 'IS_WIN'],
    'expected_in': ['kFeatureOne'],
    'expected_not_in': ['kFeatureTwo'],
  },
  {
    'testcase_name': 'elif_elif_defined',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_MAC)
          BASE_FEATURE(
              kFeatureOne, "FeatureOne",
              base::FEATURE_ENABLED_BY_DEFAULT);
        #elif BUILDFLAG(IS_LINUX)
          BASE_FEATURE(
              kFeatureTwo, "FeatureTwo",
              base::FEATURE_ENABLED_BY_DEFAULT);
        #else
          BASE_FEATURE(
              kFeatureThree, "FeatureThree",
              base::FEATURE_ENABLED_BY_DEFAULT);
        #endif
    """),
    'defines': ['IS_LINUX'],
    'expected_in': ['kFeatureTwo'],
    'expected_not_in': ['kFeatureOne'],
  },
  {
    'testcase_name': 'multiline_defined',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
            BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
          BASE_FEATURE(
              kFeatureOne, "FeatureOne",
              base::FEATURE_ENABLED_BY_DEFAULT);
        #else
          BASE_FEATURE(
              kFeatureTwo, "FeatureTwo",
              base::FEATURE_ENABLED_BY_DEFAULT);
        #endif
    """),
    'defines': ['IS_LINUX'],
    'expected_in': ['kFeatureOne'],
    'expected_not_in': ['kFeatureTwo'],
  },
  {
    'testcase_name': 'complex_multiline_defined',
    'code': textwrap.dedent("""\
        BASE_FEATURE(
            kAsyncDns,
            "AsyncDns",
            #if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
                BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN) || \
                BUILDFLAG(IS_LINUX)
            base::FEATURE_ENABLED_BY_DEFAULT
            #else
            base::FEATURE_DISABLED_BY_DEFAULT
            #endif
        );
    """),
    'defines': ['IS_WIN'],
    'expected_in': ['base::FEATURE_ENABLED_BY_DEFAULT'],
    'expected_not_in': ['base::FEATURE_DISABLED_BY_DEFAULT'],
  },
  {
    'testcase_name': 'ifndef_defined',
    'code': textwrap.dedent("""\
        #ifndef BUILDFLAG(IS_WIN)
          A
        #else
          B
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_in': ['B'],
    'expected_not_in': ['A'],
  },
  {
    'testcase_name': 'ifndef_not_defined',
    'code': textwrap.dedent("""\
        #ifndef BUILDFLAG(IS_WIN)
          A
        #else
          B
        #endif
    """),
    'defines': [],
    'expected_in': ['A'],
    'expected_not_in': ['B'],
  },
  {
    'testcase_name': 'multiline_backslash',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_WIN) || \\
            BUILDFLAG(IS_MAC)
          A
        #endif
    """),
    'defines': ['IS_MAC'],
    'expected_in': ['A'],
  },
  {
    'testcase_name': 'if_raw_identifier_defined',
    'code': textwrap.dedent("""\
        #if IS_WIN
          A
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_in': ['A'],
  },
  {
    'testcase_name': 'simple_ifdef_defined',
    'code': textwrap.dedent("""\
        #ifdef BUILDFLAG(IS_WIN)
          A
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_in': ['A'],
  },
  {
    'testcase_name': 'multiline_backslash_at_eof',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_WIN) || \\
            BUILDFLAG(IS_MAC)
          A
        #endif\\
    """),
    'defines': ['IS_MAC'],
    'expected_in': ['A'],
  },
  {
    'testcase_name': 'ifdef_no_buildflag',
    'code': textwrap.dedent("""\
        #ifdef IS_WIN
          A
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_in': ['A'],
  },
  {
    'testcase_name': 'invalid_expression',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_WIN) &&
          A
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_not_in': ['A'],
  },
  {
    'testcase_name': 'if_0',
    'code': textwrap.dedent("""\
        #if 0
          A
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_not_in': ['A'],
  },
  {
    'testcase_name': 'if_1',
    'code': textwrap.dedent("""\
        #if 1
          A
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_in': ['A'],
  },
  {
    'testcase_name': 'if_not_0',
    'code': textwrap.dedent("""\
        #if !0
          A
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_in': ['A'],
  },
  {
    'testcase_name': 'non_boolean_constant_expression',
    # Integer literals other than 0 and 1 are unsupported in boolean AST
    # evaluation and fall back to False.
    'code': textwrap.dedent("""\
        #if 2
          A
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_not_in': ['A'],
  },
  {
    'testcase_name': 'operator_and_true',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_WIN) && BUILDFLAG(IS_MAC)
          A
        #else
          B
        #endif
    """),
    'defines': ['IS_MAC', 'IS_WIN'],
    'expected_in': ['A'],
    'expected_not_in': ['B'],
  },
  {
    'testcase_name': 'operator_and_false',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_WIN) && BUILDFLAG(IS_MAC)
          A
        #else
          B
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_in': ['B'],
    'expected_not_in': ['A'],
  },
  {
    'testcase_name': 'operator_or_true',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
          A
        #else
          B
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_in': ['A'],
    'expected_not_in': ['B'],
  },
  {
    'testcase_name': 'operator_or_false',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
          A
        #else
          B
        #endif
    """),
    'defines': [],
    'expected_in': ['B'],
    'expected_not_in': ['A'],
  },
  {
    'testcase_name': 'operator_not_true',
    'code': textwrap.dedent("""\
        #if !BUILDFLAG(IS_WIN)
          A
        #else
          B
        #endif
    """),
    'defines': [],
    'expected_in': ['A'],
    'expected_not_in': ['B'],
  },
  {
    'testcase_name': 'operator_not_false',
    'code': textwrap.dedent("""\
        #if !BUILDFLAG(IS_WIN)
          A
        #else
          B
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_in': ['B'],
    'expected_not_in': ['A'],
  },
  {
    'testcase_name': 'operator_parens_and_or_true',
    'code': textwrap.dedent("""\
        #if (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)) && BUILDFLAG(IS_CHROMEOS)
          A
        #else
          B
        #endif
    """),
    'defines': ['IS_CHROMEOS', 'IS_WIN'],
    'expected_in': ['A'],
    'expected_not_in': ['B'],
  },
  {
    'testcase_name': 'operator_parens_and_or_false',
    'code': textwrap.dedent("""\
        #if (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)) && BUILDFLAG(IS_CHROMEOS)
          A
        #else
          B
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_in': ['B'],
    'expected_not_in': ['A'],
  },
  {
    'testcase_name': 'operator_parens_or_and_true',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_MAC) && BUILDFLAG(IS_CHROMEOS))
          A
        #else
          B
        #endif
    """),
    'defines': ['IS_CHROMEOS', 'IS_MAC'],
    'expected_in': ['A'],
    'expected_not_in': ['B'],
  },
  {
    'testcase_name': 'operator_parens_or_and_false',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_MAC) && BUILDFLAG(IS_CHROMEOS))
          A
        #else
          B
        #endif
    """),
    'defines': ['IS_MAC'],
    'expected_in': ['B'],
    'expected_not_in': ['A'],
  },
  {
    'testcase_name': 'comments_after_if_directives',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_WIN) // comment1
          A
        #elif BUILDFLAG(IS_MAC) /* comment2 */
          B
        #else // comment3
          C
        #endif // comment4
    """),
    'defines': ['IS_MAC'],
    'expected_in': ['B'],
    'expected_not_in': ['A'],
  },
  {
    'testcase_name': 'comments_after_ifdef_directives',
    'code': textwrap.dedent("""\
        #ifdef IS_WIN // comment1
          A
        #else /* comment2 */
          B
        #endif // comment3
    """),
    'defines': ['IS_WIN'],
    'expected_in': ['A'],
    'expected_not_in': ['B'],
  },
  {
    'testcase_name': 'multiline_comments',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_WIN) /* multiline
        comment */
          A
        #else
          B
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_in': ['A'],
    'expected_not_in': ['B'],
  },
  {
    'testcase_name': 'multiline_comments_in_directive',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_WIN) || /* multiline
        comment */ BUILDFLAG(IS_MAC)
          A
        #endif
    """),
    'defines': ['IS_MAC'],
    'expected_in': ['A'],
  },
  {
    'testcase_name': 'multiline_comment_with_stars',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_WIN)
        /*
         * Thing 1
         * Thing 2
        */
        A
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_in': ['A'],
  },
  {
    'testcase_name': 'multiline_comment_with_slashes',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_WIN)
        /*
         // Thing 1
        */
        A
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_in': ['A'],
  },
  {
    'testcase_name': 'line_continuation_in_line_comment',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_WIN)
        // comment start \
        still comment
        A
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_in': ['A'],
    'expected_not_in': ['still comment'],
  },
  {
    'testcase_name': 'comments_in_string_literals',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_WIN)
        const char* s1 = "foo // bar";
        const char* s2 = "foo /* bar */";
        const char* s3 = R"(foo // bar)";
        const char* s4 = R"(foo /* bar */)";
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_in': [
      '"foo // bar"',
      '"foo /* bar */"',
      'R"(foo // bar)"',
      'R"(foo /* bar */)"',
    ],
  },
  {
    'testcase_name': 'if_unknown_symbol',
    'code': textwrap.dedent("""\
        #if MY_UNKNOWN_SYMBOL
          A
        #else
          B
        #endif
    """),
    'defines': [],
    'known_symbols': [],
    'expected_in': ['A'],
    'expected_not_in': ['B'],
  },
  {
    'testcase_name': 'if_buildflag_unknown_symbol',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(UNKNOWN_BUILD_FLAG)
          A
        #else
          B
        #endif
    """),
    'defines': [],
    'known_symbols': [],
    'expected_in': ['A'],
    'expected_not_in': ['B'],
  },
  {
    'testcase_name': 'ifdef_unknown_symbol',
    'code': textwrap.dedent("""\
        #ifdef UNKNOWN_IFDEF_FLAG
          A
        #else
          B
        #endif
    """),
    'defines': [],
    'known_symbols': [],
    'expected_in': ['A'],
    'expected_not_in': ['B'],
  },
  {
    'testcase_name': 'if_known_symbol',
    'code': textwrap.dedent("""\
        #if KNOWN_SYMBOL
          A
        #else
          B
        #endif
    """),
    'defines': [],
    'known_symbols': ['KNOWN_SYMBOL'],
    'expected_in': ['B'],
    'expected_not_in': ['A'],
  },
  {
    'testcase_name': 'elifdef_defined',
    'code': textwrap.dedent("""\
        #if IS_MAC
          A
        #elifdef IS_WIN
          B
        #else
          C
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_in': ['B'],
    'expected_not_in': ['A', 'C'],
  },
  {
    'testcase_name': 'elifdef_not_defined',
    'code': textwrap.dedent("""\
        #if IS_MAC
          A
        #elifdef IS_WIN
          B
        #else
          C
        #endif
    """),
    'defines': [],
    'expected_in': ['C'],
    'expected_not_in': ['A', 'B'],
  },
  {
    'testcase_name': 'elifndef_defined',
    'code': textwrap.dedent("""\
        #if IS_MAC
          A
        #elifndef IS_WIN
          B
        #else
          C
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_in': ['C'],
    'expected_not_in': ['A', 'B'],
  },
  {
    'testcase_name': 'elifndef_not_defined',
    'code': textwrap.dedent("""\
        #if IS_MAC
          A
        #elifndef IS_WIN
          B
        #else
          C
        #endif
    """),
    'defines': [],
    'expected_in': ['B'],
    'expected_not_in': ['A', 'C'],
  },
  {
    'testcase_name': 'elifdef_after_true_if',
    'code': textwrap.dedent("""\
        #if IS_MAC
          A
        #elifdef IS_WIN
          B
        #else
          C
        #endif
    """),
    'defines': ['IS_MAC', 'IS_WIN'],
    'expected_in': ['A'],
    'expected_not_in': ['B', 'C'],
  },
  {
    'testcase_name': 'elifndef_after_true_if',
    'code': textwrap.dedent("""\
        #if IS_MAC
          A
        #elifndef IS_WIN
          B
        #else
          C
        #endif
    """),
    'defines': ['IS_MAC'],
    'expected_in': ['A'],
    'expected_not_in': ['B', 'C'],
  },
  {
    'testcase_name': 'elifdef_buildflag',
    'code': textwrap.dedent("""\
        #if IS_MAC
          A
        #elifdef BUILDFLAG(IS_WIN)
          B
        #else
          C
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_in': ['B'],
    'expected_not_in': ['A', 'C'],
  },
  {
    'testcase_name': 'elifndef_buildflag',
    'code': textwrap.dedent("""\
        #if IS_MAC
          A
        #elifndef BUILDFLAG(IS_WIN)
          B
        #else
          C
        #endif
    """),
    'defines': [],
    'expected_in': ['B'],
    'expected_not_in': ['A', 'C'],
  },
]

_ERROR_TEST_CASES = [
  {
    'testcase_name': 'unexpected_elif',
    'code': textwrap.dedent("""\
        #elif BUILDFLAG(IS_WIN)
          A
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_error': 'Unexpected #elif',
  },
  {
    'testcase_name': 'unexpected_else',
    'code': textwrap.dedent("""\
        #else
          A
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_error': 'Unexpected #else',
  },
  {
    'testcase_name': 'unexpected_endif',
    'code': textwrap.dedent("""\
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_error': 'Unexpected #endif',
  },
  {
    'testcase_name': 'unterminated_if',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_WIN)
          A
    """),
    'defines': ['IS_WIN'],
    'expected_error': 'Unterminated #if block',
  },
  {
    'testcase_name': 'unterminated_if_else',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_WIN)
          A
        #else
          B
    """),
    'defines': ['IS_WIN'],
    'expected_error': 'Unterminated #if block',
  },
  {
    'testcase_name': 'unterminated_if_elif',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_WIN)
          A
        #elif BUILDFLAG(IS_MAC)
          B
    """),
    'defines': ['IS_WIN'],
    'expected_error': 'Unterminated #if block',
  },
  {
    'testcase_name': 'multiline_backslash_at_eof_no_newline',
    'code': textwrap.dedent("""\
        #if BUILDFLAG(IS_WIN) || \\
            BUILDFLAG(IS_MAC)
        #endif\\"""),
    'defines': ['IS_MAC'],
    'expected_error': 'Unterminated #if block',
  },
  {
    'testcase_name': 'unexpected_elifdef',
    'code': textwrap.dedent("""\
        #elifdef IS_WIN
          A
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_error': 'Unexpected #elifdef',
  },
  {
    'testcase_name': 'unexpected_elifndef',
    'code': textwrap.dedent("""\
        #elifndef IS_WIN
          A
        #endif
    """),
    'defines': ['IS_WIN'],
    'expected_error': 'Unexpected #elifndef',
  },
]


class CppPreprocessorTest(unittest.TestCase):
  def test_preprocessor(self):
    for case in _PREPROCESSOR_TEST_CASES:
      with self.subTest(case['testcase_name']):
        known_symbols = case.get('known_symbols', _ALL_TEST_SYMBOLS)
        preprocessor = cpp_preprocessor.CppPreprocessor(
          defines=case['defines'], known_symbols=known_symbols
        )
        processed_code = preprocessor.preprocess(case['code'])
        for s in case.get('expected_in', []):
          self.assertIn(s, processed_code)
        for s in case.get('expected_not_in', []):
          self.assertNotIn(s, processed_code)

  def test_preprocessor_errors(self):
    for case in _ERROR_TEST_CASES:
      with self.subTest(case['testcase_name']):
        preprocessor = cpp_preprocessor.CppPreprocessor(
          defines=case['defines'], known_symbols=_ALL_TEST_SYMBOLS
        )
        with self.assertRaisesRegex(ValueError, case['expected_error']):
          preprocessor.preprocess(case['code'])

  def test_remove_comments(self):
    code = 'A // line comment\nB /* line1\nline2\nline3 */ C'
    self.assertEqual(
      cpp_preprocessor.remove_comments(code),
      'A \nB  C',
    )
    self.assertEqual(
      cpp_preprocessor.remove_comments(code, preserve_newlines=False),
      'A \nB  C',
    )
    self.assertEqual(
      cpp_preprocessor.remove_comments(code, preserve_newlines=True),
      'A \nB \n\n C',
    )
    self.assertEqual(
      cpp_preprocessor.RemoveComments(code),
      'A \nB  C',
    )
    self.assertEqual(
      cpp_preprocessor.RemoveComments(code, preserve_newlines=False),
      'A \nB  C',
    )
    self.assertEqual(
      cpp_preprocessor.RemoveComments(code, preserve_newlines=True),
      'A \nB \n\n C',
    )

  def test_evaluate_condition(self):
    known = {'IS_WIN', 'IS_ANDROID', 'IS_IOS'}
    self.assertTrue(
      cpp_preprocessor.evaluate_condition(
        '!IS_IOS && (IS_ANDROID || BUILDFLAG(IS_WIN))',
        true_symbols={'IS_WIN'},
        known_symbols=known,
      )
    )
    self.assertFalse(
      cpp_preprocessor.evaluate_condition(
        '!IS_IOS && (IS_ANDROID || BUILDFLAG(IS_WIN))',
        true_symbols={'IS_IOS'},
        known_symbols=known,
      )
    )
    self.assertIsNone(
      cpp_preprocessor.evaluate_condition(
        'IS_WIN || UNKNOWN_SYMBOL',
        true_symbols={'IS_WIN'},
        known_symbols=known,
      )
    )
    self.assertIsNone(
      cpp_preprocessor.evaluate_condition(
        'IS_WIN == 1',
        true_symbols={'IS_WIN'},
        known_symbols=known,
      )
    )
    self.assertTrue(
      cpp_preprocessor.EvaluateCondition(
        'BUILDFLAG(IS_WIN)',
        true_symbols={'IS_WIN'},
        known_symbols=known,
      )
    )
    self.assertTrue(
      cpp_preprocessor.evaluate_condition(
        'defined(IS_WIN) && !defined IS_IOS',
        true_symbols={'IS_WIN'},
        known_symbols=known,
      )
    )

  def test_preprocessor_defined(self):
    code = textwrap.dedent("""\
        #if defined(IS_WIN)
          A
        #endif
        #if !defined(IS_WIN)
          B
        #endif
        #if defined IS_MAC && !defined IS_WIN
          C
        #endif
        #if defined(MY_UNKNOWN_SYMBOL)
          D
        #endif
    """)
    win = cpp_preprocessor.CppPreprocessor(
      defines={'IS_WIN'}, known_symbols=_ALL_TEST_SYMBOLS
    ).preprocess(code)
    self.assertIn('A', win)
    self.assertNotIn('B', win)
    self.assertNotIn('C', win)
    self.assertIn('D', win)
    mac = cpp_preprocessor.CppPreprocessor(
      defines={'IS_MAC'}, known_symbols=_ALL_TEST_SYMBOLS
    ).preprocess(code)
    self.assertNotIn('A', mac)
    self.assertIn('B', mac)
    self.assertIn('C', mac)


if __name__ == '__main__':
  unittest.main()
