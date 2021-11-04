# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for gtest.py"""

import unittest

import conditions
from conditions import Condition
import gtest


class GtestTest(unittest.TestCase):
  def disabler_test(self, input_file: str, test_name: str, new_cond,
                    expected_result: str):
    """Helper function for testing gtest.disabler."""

    if isinstance(new_cond, list):
      new_cond = conditions.parse(new_cond)
    else:
      assert isinstance(new_cond, conditions.BaseCondition)

    resulting_file = gtest.disabler(test_name, input_file, new_cond)

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
    self.disabler_test(
        'TEST(Suite, Test) {}', 'Suite.Test', ['linux', 'mac'], '''
#include "build/build_config.h"
#if defined(OS_LINUX) || defined(OS_MAC)
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif
TEST(Suite, MAYBE_Test) {}
''')

  def test_conditionally_disable_already_disabled_test(self):
    self.disabler_test(
        'TEST(Suite, DISABLED_Test) {}', 'Suite.Test', ['linux', 'mac'], '''
#include "build/build_config.h"
#if defined(OS_LINUX) || defined(OS_MAC)
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
#if defined(OS_WIN)
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif
TEST(Suite, MAYBE_Test) {}
''', 'Suite.Test', ['linux', 'mac'], '''
#include "build/build_config.h"
#if defined(OS_LINUX) || defined(OS_MAC) || defined(OS_WIN)
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif
TEST(Suite, MAYBE_Test) {}
''')

  def test_enable_conditionally_disabled_test(self):
    self.disabler_test(
        '''
#if defined(OS_WIN)
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
#if defined(OS_LINUX) && defined(ADDRESS_SANITIZER))
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif
TEST(Suite, MAYBE_Test) {}
''', 'Suite.Test', ['linux'], '''
#include "build/build_config.h"
#if defined(OS_LINUX)
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif
TEST(Suite, MAYBE_Test) {}
''')

  def test_handle_backslash_line_continuations(self):
    self.disabler_test(
        r'''
#if defined(OS_WIN)
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


if __name__ == '__main__':
  unittest.main()
