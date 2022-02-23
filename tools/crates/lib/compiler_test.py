# python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import unittest

from lib import compiler


class CompilerTestCase(unittest.TestCase):
    def assertListSortedEqual(self, a, b, msg=None):
        a.sort()
        b.sort()
        if msg:
            self.assertListEqual(a, b, msg=msg)
        else:
            self.assertListEqual(a, b)

    def matching_archs(self, matching: str) -> set[str]:
        return {
            arch
            for arch in compiler._RUSTC_ARCH_TO_BUILD_CONDITION
            if re.search(matching, arch)
        }

    def not_matching_archs(self, matching: str) -> set[str]:
        """The inverse of matching_archs()."""
        return {
            arch
            for arch in compiler._RUSTC_ARCH_TO_BUILD_CONDITION
            if not re.search(matching, arch)
        }


class TestGnConditions(CompilerTestCase):
    def test_all_platforms(self):
        s = compiler.BuildConditionSet(compiler.ArchSet.ALL())
        self.assertListSortedEqual([], s.get_gn_conditions())

    def test_one_platform(self):
        for a in compiler.ArchSet.ALL().as_strings():
            s = compiler.BuildConditionSet(compiler.ArchSet(initial={a}))
            mode: compiler.BuildCondition = \
                compiler._RUSTC_ARCH_TO_BUILD_CONDITION[a]
            self.assertListSortedEqual([mode.gn_condition()],
                                       s.get_gn_conditions())

    def test_os(self):
        # One OS.
        for (matching, mode) in [
            (compiler._RUSTC_ARCH_MATCH_ANDROID,
             compiler.BuildCondition.ALL_ANDROID),
            (compiler._RUSTC_ARCH_MATCH_FUCHSIA,
             compiler.BuildCondition.ALL_FUCHSIA),
            (compiler._RUSTC_ARCH_MATCH_IOS, compiler.BuildCondition.ALL_IOS),
            (compiler._RUSTC_ARCH_MATCH_WINDOWS,
             compiler.BuildCondition.ALL_WINDOWS),
            (compiler._RUSTC_ARCH_MATCH_LINUX,
             compiler.BuildCondition.ALL_LINUX),
            (compiler._RUSTC_ARCH_MATCH_MAC, compiler.BuildCondition.ALL_MAC),
        ]:
            archs = self.matching_archs(matching)
            s = compiler.BuildConditionSet(compiler.ArchSet(initial=archs))
            cond = mode.gn_condition()
            self.assertListSortedEqual([cond],
                                       s.get_gn_conditions(),
                                       msg=repr(archs))

        # Two OSs.
        archs = self.matching_archs(compiler._RUSTC_ARCH_MATCH_WINDOWS + r"|" +
                                    compiler._RUSTC_ARCH_MATCH_MAC)
        s = compiler.BuildConditionSet(compiler.ArchSet(initial=archs))
        cond1 = compiler.BuildCondition.ALL_WINDOWS.gn_condition()
        cond2 = compiler.BuildCondition.ALL_MAC.gn_condition()
        self.assertListSortedEqual([cond1, cond2], s.get_gn_conditions())

        # All but one OS.
        for (matching_os, mode) in [
            (compiler._RUSTC_ARCH_MATCH_ANDROID,
             compiler.BuildCondition.NOT_ANDROID),
            (compiler._RUSTC_ARCH_MATCH_FUCHSIA,
             compiler.BuildCondition.NOT_FUCHSIA),
            (compiler._RUSTC_ARCH_MATCH_IOS, compiler.BuildCondition.NOT_IOS),
            (compiler._RUSTC_ARCH_MATCH_WINDOWS,
             compiler.BuildCondition.NOT_WINDOWS),
            (compiler._RUSTC_ARCH_MATCH_LINUX,
             compiler.BuildCondition.NOT_LINUX),
            (compiler._RUSTC_ARCH_MATCH_MAC, compiler.BuildCondition.NOT_MAC),
        ]:
            s = compiler.BuildConditionSet(
                compiler.ArchSet(initial=self.not_matching_archs(matching_os)))
            cond = mode.gn_condition()
            self.assertListSortedEqual([cond], s.get_gn_conditions())

    def test_one_cpu(self):
        for (matching, mode) in [
            (compiler._RUSTC_ARCH_MATCH_X86, compiler.BuildCondition.ALL_X86),
            (compiler._RUSTC_ARCH_MATCH_X64, compiler.BuildCondition.ALL_X64),
            (compiler._RUSTC_ARCH_MATCH_ARM32,
             compiler.BuildCondition.ALL_ARM32),
            (compiler._RUSTC_ARCH_MATCH_ARM64,
             compiler.BuildCondition.ALL_ARM64)
        ]:
            s = compiler.BuildConditionSet(
                compiler.ArchSet(initial=self.matching_archs(matching)))
            cond = mode.gn_condition()
            self.assertListSortedEqual([cond], s.get_gn_conditions())

    def test_combining_os_and_cpu(self):
        # One Cpu and one OS (with overlap).
        archs = self.matching_archs(compiler._RUSTC_ARCH_MATCH_LINUX + r"|" +
                                    compiler._RUSTC_ARCH_MATCH_X86)
        s = compiler.BuildConditionSet(compiler.ArchSet(initial=archs))
        cond1 = compiler.BuildCondition.ALL_LINUX.gn_condition()
        cond2 = compiler.BuildCondition.ALL_X86.gn_condition()
        self.assertListSortedEqual([cond1, cond2], s.get_gn_conditions())

        # One Cpu and one OS (without overlap).
        archs = self.matching_archs(compiler._RUSTC_ARCH_MATCH_MAC + r"|" +
                                    compiler._RUSTC_ARCH_MATCH_X86)
        s = compiler.BuildConditionSet(compiler.ArchSet(initial=archs))
        cond1 = compiler.BuildCondition.ALL_MAC.gn_condition()
        cond2 = compiler.BuildCondition.ALL_X86.gn_condition()
        self.assertListSortedEqual([cond1, cond2], s.get_gn_conditions())

    def test_invert(self):
        all = compiler.BuildConditionSet.ALL()
        none = compiler.BuildConditionSet.EMPTY()
        self.assertEqual(none, all.inverted())
        self.assertEqual(all, none.inverted())

        one = compiler.BuildConditionSet(
            compiler.ArchSet(
                initial=self.matching_archs(compiler._RUSTC_ARCH_MATCH_MAC)))
        the_rest = compiler.BuildConditionSet(
            compiler.ArchSet(initial=self.not_matching_archs(
                compiler._RUSTC_ARCH_MATCH_MAC)))
        self.assertListSortedEqual(one.inverted().get_gn_conditions(),
                                   the_rest.get_gn_conditions())


class TestCompiler(unittest.TestCase):
    def test_all_and_one(self):
        self.assertEqual(len(compiler.ArchSet.ALL().as_strings()),
                         len(compiler._RUSTC_ARCH_TO_BUILD_CONDITION))
        self.assertEqual(len(compiler.ArchSet.ONE()), 1)


class TestArchSet(CompilerTestCase):
    def test_construct(self):
        a = compiler.ArchSet(
            initial=self.matching_archs(compiler._RUSTC_ARCH_MATCH_ARM32))
        self.assertSetEqual({
            "armv7-linux-androideabi",
            "armv7-apple-ios",
        }, a.as_strings())

        a = compiler.ArchSet.EMPTY()
        self.assertSetEqual(set(), a.as_strings())

        a = compiler.ArchSet(
            initial=self.matching_archs(compiler._RUSTC_ARCH_MATCH_ARM32))
        self.assertSetEqual({
            "armv7-linux-androideabi",
            "armv7-apple-ios",
        }, a.as_strings())

        a = compiler.ArchSet(
            initial=self.matching_archs(compiler._RUSTC_ARCH_MATCH_ARM32))
        self.assertTrue(a.has_arch("armv7-linux-androideabi"))
        self.assertFalse(a.has_arch("i686-pc-windows-msvc"))

    def test_bool(self):
        a = compiler.ArchSet.EMPTY()
        self.assertFalse(bool(a))

        a = compiler.ArchSet.ONE()
        self.assertTrue(bool(a))

        a = compiler.ArchSet.ALL()
        self.assertTrue(bool(a))

    def test_eq(self):
        self.assertEqual(compiler.ArchSet.EMPTY(), compiler.ArchSet.EMPTY())
        self.assertEqual(compiler.ArchSet.ONE(), compiler.ArchSet.ONE())
        self.assertEqual(compiler.ArchSet.ALL(), compiler.ArchSet.ALL())

    def test_len(self):
        self.assertEqual(len(compiler.ArchSet.EMPTY()), 0)
        self.assertEqual(len(compiler.ArchSet.ONE()), 1)
        self.assertEqual(len(compiler.ArchSet.ALL()),
                         len(compiler._RUSTC_ARCH_TO_BUILD_CONDITION))

    def test_add_archset(self):
        a = compiler.ArchSet.EMPTY()
        a.add_archset(compiler.ArchSet.ALL())
        self.assertEqual(a, compiler.ArchSet.ALL())

        a = compiler.ArchSet.ONE()
        a.add_archset(compiler.ArchSet.ALL())
        self.assertEqual(a, compiler.ArchSet.ALL())

        a = compiler.ArchSet.ALL()
        a.add_archset(compiler.ArchSet.ALL())
        self.assertEqual(a, compiler.ArchSet.ALL())

    def test_and(self):
        a = compiler.ArchSet.EMPTY()
        a = a & compiler.ArchSet.ALL()
        self.assertSetEqual(set(), a.as_strings())

        a = compiler.ArchSet.EMPTY()
        a &= compiler.ArchSet.ALL()
        self.assertSetEqual(set(), a.as_strings())

        a = compiler.ArchSet.EMPTY()
        a = a & compiler.ArchSet.EMPTY()
        self.assertSetEqual(set(), a.as_strings())

        a = compiler.ArchSet.EMPTY()
        a &= compiler.ArchSet.EMPTY()
        self.assertSetEqual(set(), a.as_strings())

        a = compiler.ArchSet.ALL()
        a = a & compiler.ArchSet.ALL()
        self.assertSetEqual(compiler.ArchSet.ALL().as_strings(), a.as_strings())

        a = compiler.ArchSet.ALL()
        a &= compiler.ArchSet.ALL()
        self.assertSetEqual(compiler.ArchSet.ALL().as_strings(), a.as_strings())

        a = compiler.ArchSet.ALL()
        a = a & compiler.ArchSet.ONE()
        self.assertSetEqual(compiler.ArchSet.ONE().as_strings(), a.as_strings())

        a = compiler.ArchSet.ALL()
        a &= compiler.ArchSet.ONE()
        self.assertSetEqual(compiler.ArchSet.ONE().as_strings(), a.as_strings())

        a = compiler.ArchSet.ALL()
        a = a & compiler.ArchSet.EMPTY()
        self.assertSetEqual(set(), a.as_strings())

        a = compiler.ArchSet.ALL()
        a &= compiler.ArchSet.EMPTY()
        self.assertSetEqual(set(), a.as_strings())
