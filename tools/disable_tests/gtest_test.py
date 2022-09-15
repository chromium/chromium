# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for gtest.py"""

import unittest
from typing import Optional

import conditions
from conditions import Condition
import gtest


class GtestTest(unittest.TestCase):
  def disabler_test(self,
                    input_file: str,
                    test_name: str,
                    new_cond,
                    expected_result: str,
                    message: Optional[str] = None):
    """Helper function for testing gtest.disabler."""

    self.maxDiff = None

    if isinstance(new_cond, list):
      new_cond = conditions.parse(new_cond)
    else:
      assert isinstance(new_cond, conditions.BaseCondition)

    resulting_file = gtest.disabler(test_name, input_file, new_cond, message)

    self.assertEqual(expected_result.strip(), resulting_file.strip())

  def test_disable_enabled_test(self):
    self.disabler_test('TEST_F(Suite, Test) {}', 'Suite.Test',
                       conditions.ALWAYS, 'TEST_F(Suite, DISABLED_Test) {}')

  def test_disable_already_disabled_test(self):
    self.disabler_test('TEST(Suite, DISABLED_Test) {}', 'Suite.Test',
                       conditions.ALWAYS, 'TEST(Suite, DISABLED_Test) {}')

  def test_enable_disabled_test(self):
    self.disabler_test('TEST_F(Suite, DISABLED_Test) {}', 'Suite.Test',
                       conditions.NEVER, 'TEST_F(Suite, Test) {}')

  def test_enable_already_enabled_test(self):
    self.disabler_test('TEST(Suite, Test) {}', 'Suite.Test', conditions.NEVER,
                       'TEST(Suite, Test) {}')

  def test_conditionally_disable_test(self):
    # Include some bad formatting in the original TEST line, to make sure
    # the set of modified lines passed to clang-format includes it.
    self.disabler_test(
        'TEST ( Suite, Test) {}', 'Suite.Test', ['linux', 'mac'], '''
#include "build/build_config.h"
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif
TEST(Suite, MAYBE_Test) {}
''')

  def test_conditionally_disable_already_disabled_test(self):
    # Include some bad formatting in the original TEST line, to make sure
    # the set of modified lines passed to clang-format includes it.
    self.disabler_test(
        'TEST(Suite   , DISABLED_Test) {}', 'Suite.Test', ['linux', 'mac'], '''
#include "build/build_config.h"
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif
TEST(Suite, MAYBE_Test) {}
''')

  def test_extend_conditions_on_conditionally_disabled_test(self):
    self.disabler_test(
        '''
#include "build/build_config.h"
#if BUILDFLAG(IS_WIN)
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif
TEST(Suite, MAYBE_Test) {}
''', 'Suite.Test', ['linux', 'mac'], '''
#include "build/build_config.h"
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif
TEST(Suite, MAYBE_Test) {}
''')

  def test_enable_conditionally_disabled_test(self):
    self.disabler_test(
        '''
#if BUILDFLAG(IS_WIN)
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif
TEST(Suite, MAYBE_Test) {}
''', 'Suite.Test', conditions.NEVER, 'TEST(Suite, Test) {}')

  def test_simplify_redundant_conditions(self):
    self.disabler_test(
        '''
#include "build/build_config.h"
#if BUILDFLAG(IS_LINUX) && defined(ADDRESS_SANITIZER))
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif
TEST(Suite, MAYBE_Test) {}
''', 'Suite.Test', ['linux'], '''
#include "build/build_config.h"
#if BUILDFLAG(IS_LINUX)
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif
TEST(Suite, MAYBE_Test) {}
''')

  def test_handle_backslash_line_continuations(self):
    self.disabler_test(
        r'''
#if BUILDFLAG(IS_WIN)
#define MAYBE_Test \
    DISABLED_Test
#else
#define MAYBE_Test \
    Test
#endif
TEST(Suite, MAYBE_Test) {}
''', 'Suite.Test', conditions.ALWAYS, 'TEST(Suite, DISABLED_Test) {}')

  def test_disable_test_via_build_flag(self):
    self.disabler_test(
        'TEST(Suite, Test) {}', 'Suite.Test', ['lacros'], '''
#include "build/chromeos_buildflags.h"
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif
TEST(Suite, MAYBE_Test) {}
''')

  def test_split_long_lines_disabled(self):
    self.disabler_test(
        'TEST(Suite, ' +
        'ReallyReallyReallyReallyReallyReallyReallyReallyLongTestName) {}',
        'Suite.ReallyReallyReallyReallyReallyReallyReallyReallyLongTestName',
        conditions.ALWAYS, '''
TEST(Suite,
     DISABLED_ReallyReallyReallyReallyReallyReallyReallyReallyLongTestName) {}
     ''')

  def test_split_long_lines_conditionally_disabled(self):
    self.disabler_test(
        'TEST(Suite, ReallyReallyReallyReallyLongTestName) {}',
        'Suite.ReallyReallyReallyReallyLongTestName', ['win'], r'''
#include "build/build_config.h"
#if BUILDFLAG(IS_WIN)
#define MAYBE_ReallyReallyReallyReallyLongTestName \
  DISABLED_ReallyReallyReallyReallyLongTestName
#else
#define MAYBE_ReallyReallyReallyReallyLongTestName \
  ReallyReallyReallyReallyLongTestName
#endif
TEST(Suite, MAYBE_ReallyReallyReallyReallyLongTestName) {}
''')

  def test_with_name_ocurring_later_in_file(self):
    self.disabler_test(
        '''
TEST(Suite, Test) {}
void SomeFunctionWihTestInTheName() {}
''', 'Suite.Test', conditions.ALWAYS, '''
TEST(Suite, DISABLED_Test) {}
void SomeFunctionWihTestInTheName() {}
''')

  def test_ignore_comments_in_preprocessor_directive(self):
    self.disabler_test(
        '''
#if /* something */ BUILDFLAG( /* something else */ IS_WIN) // comment ||
#define MAYBE_Test DISABLED_Test // another one &&
#else /* and another... */
#define MAYBE_Test Test // you know the drill(
#endif // one more!
TEST(Suite, MAYBE_Test) {}
''', 'Suite.Test', conditions.NEVER, 'TEST(Suite, Test) {}')

  def test_disable_unconditionally_with_message(self):
    # Also include some formatting that should be fixed up, to test that the
    # correct line number is passed to clang-format.
    self.disabler_test('''
// existing comment
TEST(Suite,   Test) {}
// another comment''',
                       'Suite.Test',
                       conditions.ALWAYS,
                       '''
// existing comment
// here is the message
TEST(Suite, DISABLED_Test) {}
// another comment''',
                       message='here is the message')

  def test_conditionally_disable_test_with_message(self):
    # Also include some formatting that should be fixed up, to test that the
    # correct line number is passed to clang-format.
    self.disabler_test('TEST(Suite,     Test) {}',
                       'Suite.Test', ['linux', 'mac'],
                       '''
#include "build/build_config.h"
// we should really fix this
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif
TEST(Suite, MAYBE_Test) {}
''',
                       message='we should really fix this')

  def test_ignores_test_names_in_other_contexts(self):
    self.disabler_test(
        '''
IN_PROC_BROWSER_TEST_P(Suite, Test) {
  // This is a very important Test.
  auto s = "Test";
  auto t = "some other Test stuff";
}''', 'Suite.Test', conditions.ALWAYS, '''
IN_PROC_BROWSER_TEST_P(Suite, DISABLED_Test) {
  // This is a very important Test.
  auto s = "Test";
  auto t = "some other Test stuff";
}''')


if __name__ == '__main__':
  unittest.main()
