#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for //third_party/protobuf/compile_size_probe.py."""

from __future__ import annotations

import os
import sys
import tempfile
import unittest

_THIS_DIRECTORY = os.path.abspath(os.path.dirname(__file__))
if _THIS_DIRECTORY not in sys.path:
    sys.path.insert(0, _THIS_DIRECTORY)

import compile_size_probe


class CompileSizeProbeTest(unittest.TestCase):
    """Tests path normalization and Ninja compile-rule discovery."""

    def testUnescapeNinjaValueReplacesEscapedColonsSpacesAndDollarSigns(self):
        raw_value = 'path$:with$ spaces/and$$dollars'
        unescaped_value = compile_size_probe.UnescapeNinjaValue(raw_value)
        self.assertEqual(
            'path:with spaces/and$dollars',
            unescaped_value,
            msg=(
                'Expected UnescapeNinjaValue to convert "$:", "$ ", and "$$" '
                'into literal colons, spaces, and dollar signs.'),
        )

    def testNormalizeTranslationUnitPathMapsGeneratedPathToGenRelative(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_directory = os.path.join(
                temporary_directory, 'out', 'Default')
            os.makedirs(build_directory)

            normalized_generated = (
                compile_size_probe.NormalizeTranslationUnitPath(
                    'out/Default/gen/net/cert/duration.pb.cc',
                    build_directory,
                    temporary_directory,
                ))

        self.assertEqual(
            'gen/net/cert/duration.pb.cc',
            normalized_generated,
            msg=(
                'Expected generated paths inside build_directory to normalize '
                'to gen/....'),
        )

    def testNormalizeTranslationUnitPathMapsDoubleSlashToRepositoryRelative(
            self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_directory = os.path.join(
                temporary_directory, 'out', 'Default')
            os.makedirs(build_directory)

            normalized_source = (
                compile_size_probe.NormalizeTranslationUnitPath(
                    '//components/sync/model/processor_entity.cc',
                    build_directory,
                    temporary_directory,
                ))

        self.assertEqual(
            'components/sync/model/processor_entity.cc',
            normalized_source,
            msg=(
                'Expected leading-// source paths to normalize to '
                'repository-relative paths.'),
        )

    def testMergeTranslationUnitsPreservesOrderAndDeduplicatesExtraPaths(self):
        merged_units = compile_size_probe.MergeTranslationUnits(
            default_units=('gen/a.pb.cc', 'gen/b.pb.cc'),
            extra_units=('gen/b.pb.cc', 'components/c.cc', 'gen/a.pb.cc'),
        )
        self.assertEqual(
            ['gen/a.pb.cc', 'gen/b.pb.cc', 'components/c.cc'],
            merged_units,
            msg=(
                'Expected MergeTranslationUnits to preserve default ordering '
                'and append only unseen extra translation units.'),
        )

    def testParseNinjaCompileRuleExtractsBuildEdgeAndTargetVariableOverrides(
            self):
        ninja_text = (
            'defines = -DFOO=1\n'
            'include_dirs = -I../.. -Igen\n'
            'cflags = -O2\n'
            'cflags_cc = -std=c++20\n'
            'build obj/net/proto.o: cxx gen/net/duration.pb.cc | ../../a.h\n'
            '  cflags_cc = -std=c++20 -fno-exceptions\n'
        )
        compile_rule = compile_size_probe.ParseNinjaCompileRuleFromText(
            ninja_text=ninja_text,
            ninja_file_path='/out/obj/net/proto.ninja',
            translation_unit='gen/net/duration.pb.cc',
            expected_ninja_source='gen/net/duration.pb.cc',
        )

        self.assertEqual(
            compile_size_probe.NinjaCompileRule(
                translation_unit='gen/net/duration.pb.cc',
                ninja_source_path='gen/net/duration.pb.cc',
                object_target='obj/net/proto.o',
                ninja_file_path='/out/obj/net/proto.ninja',
                variables={
                    'defines': '-DFOO=1',
                    'include_dirs': '-I../.. -Igen',
                    'cflags': '-O2',
                    'cflags_cc': '-std=c++20 -fno-exceptions',
                },
                prerequisite_targets=('gen/net/duration.pb.cc', '../../a.h'),
            ),
            compile_rule,
            msg=(
                'Expected ParseNinjaCompileRuleFromText to capture top-level '
                'variables, indented per-edge overrides, and prerequisite '
                'targets.'),
        )

    def testFindNinjaCompileRuleLocatesTargetInAncestorObjectDirectory(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_directory = os.path.join(
                temporary_directory, 'out', 'Default')
            ninja_directory = os.path.join(
                build_directory, 'obj', 'net', 'cert')
            os.makedirs(ninja_directory)
            ninja_file_path = os.path.join(
                ninja_directory, 'root_store_proto_lite.ninja')
            with open(ninja_file_path, 'w', encoding='utf-8') as file_handle:
                file_handle.write(
                    'include_dirs = -I../..\n'
                    'build obj/net/cert/duration.pb.o: cxx '
                    'gen/net/cert/root_store_proto_lite/duration.pb.cc\n')

            compile_rule = compile_size_probe.FindNinjaCompileRule(
                build_directory=build_directory,
                repository_root=temporary_directory,
                translation_unit=(
                    'gen/net/cert/root_store_proto_lite/duration.pb.cc'),
            )

        self.assertEqual(
            'obj/net/cert/duration.pb.o',
            compile_rule.object_target,
            msg=(
                'Expected FindNinjaCompileRule to discover the .ninja file '
                'in the parent ancestor directory obj/net/cert/.'),
        )

    def testFindNinjaCompileRuleFallsBackToWalkingFullObjectTreeWhenMoved(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_directory = os.path.join(
                temporary_directory, 'out', 'Default')
            unrelated_directory = os.path.join(
                build_directory, 'obj', 'custom_umbrella', 'subtarget')
            os.makedirs(unrelated_directory)
            ninja_file_path = os.path.join(
                unrelated_directory, 'umbrella.ninja')
            with open(ninja_file_path, 'w', encoding='utf-8') as file_handle:
                file_handle.write(
                    'include_dirs = -I../..\n'
                    'build obj/custom_umbrella/duration.pb.o: cxx '
                    'gen/net/cert/root_store_proto_lite/duration.pb.cc\n')

            compile_rule = compile_size_probe.FindNinjaCompileRule(
                build_directory=build_directory,
                repository_root=temporary_directory,
                translation_unit=(
                    'gen/net/cert/root_store_proto_lite/duration.pb.cc'),
            )

        self.assertEqual(
            'obj/custom_umbrella/duration.pb.o',
            compile_rule.object_target,
            msg=(
                'Expected FindNinjaCompileRule to fall back to walking the '
                'entire obj/ tree when the .ninja file is outside ancestor '
                'directories.'),
        )

    def testFindNinjaCompileRuleRaisesRuntimeErrorWhenTargetIsNotInBuild(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_directory = os.path.join(
                temporary_directory, 'out', 'Default')
            os.makedirs(os.path.join(build_directory, 'obj'))

            with self.assertRaisesRegex(
                    RuntimeError,
                    r'Could not find a Ninja cxx build rule',
                    msg=(
                        'Expected FindNinjaCompileRule to raise RuntimeError '
                        'when no .ninja file compiles the translation unit.')):
                compile_size_probe.FindNinjaCompileRule(
                    build_directory=build_directory,
                    repository_root=temporary_directory,
                    translation_unit='gen/net/missing.pb.cc',
                )

    def testFindNinjaCompileRuleSkipsNinjaFileContainingNonUtf8Bytes(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_directory = os.path.join(
                temporary_directory, 'out', 'Default')
            ninja_directory = os.path.join(
                build_directory, 'obj', 'net', 'cert')
            os.makedirs(ninja_directory)
            non_utf8_ninja = os.path.join(
                ninja_directory, '00_non_utf8_rust.ninja')
            with open(non_utf8_ninja, 'wb') as binary_handle:
                binary_handle.write(b'# non-utf8 byte: \xff\xfe\n')

            valid_ninja = os.path.join(
                ninja_directory, 'root_store_proto_lite.ninja')
            with open(valid_ninja, 'w', encoding='utf-8') as file_handle:
                file_handle.write(
                    'include_dirs = -I../..\n'
                    'build obj/net/cert/duration.pb.o: cxx '
                    'gen/net/cert/root_store_proto_lite/duration.pb.cc\n')

            compile_rule = compile_size_probe.FindNinjaCompileRule(
                build_directory=build_directory,
                repository_root=temporary_directory,
                translation_unit=(
                    'gen/net/cert/root_store_proto_lite/duration.pb.cc'),
            )

        self.assertEqual(
            'obj/net/cert/duration.pb.o',
            compile_rule.object_target,
            msg=(
                'Expected FindNinjaCompileRule to tolerate non-UTF-8 bytes in '
                'unrelated .ninja files without raising UnicodeDecodeError.'),
        )

    def testFindNinjaCompileRuleFindsTargetWhenSourceDirectoryHasEscapedColon(
            self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_directory = os.path.join(
                temporary_directory, 'out', 'Default')
            ninja_directory = os.path.join(build_directory, 'obj')
            os.makedirs(ninja_directory)
            ninja_file_path = os.path.join(ninja_directory, 'proto.ninja')
            with open(ninja_file_path, 'w', encoding='utf-8') as file_handle:
                file_handle.write(
                    'include_dirs = -I../..\n'
                    'build obj/weird$:dir/f.pb.o: cxx '
                    'gen/weird$:dir/f.pb.cc\n')

            compile_rule = compile_size_probe.FindNinjaCompileRule(
                build_directory=build_directory,
                repository_root=temporary_directory,
                translation_unit='gen/weird:dir/f.pb.cc',
            )

        self.assertEqual(
            'obj/weird:dir/f.pb.o',
            compile_rule.object_target,
            msg=(
                'Expected FindNinjaCompileRule to locate the .ninja file '
                'when the source directory contains a Ninja-escaped "$:" '
                'colon.'),
        )


if __name__ == '__main__':
    unittest.main()
