#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for //third_party/protobuf/compile_size_probe.py."""

from __future__ import annotations

import json
import os
import sys
import tempfile
import unittest
from unittest import mock

_THIS_DIRECTORY = os.path.abspath(os.path.dirname(__file__))
if _THIS_DIRECTORY not in sys.path:
    sys.path.insert(0, _THIS_DIRECTORY)

import compile_size_probe


def _WriteFileWithByteSize(file_path: str, byte_size: int) -> None:
    """Creates `file_path` (and parent directories) with `byte_size` bytes."""
    os.makedirs(os.path.dirname(file_path), exist_ok=True)
    with open(file_path, 'wb') as file_handle:
        file_handle.write(b'x' * byte_size)


class CompileSizeProbeTest(unittest.TestCase):
    """Tests path normalization, Ninja parsing, and clang -M -H measurement."""

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

    def testTranslationUnitSnapshotRoundTripsThroughJson(self):
        snapshot = compile_size_probe.TranslationUnitSnapshot(
            translation_unit='gen/net/duration.pb.cc',
            total_bytes=300,
            included_files={
                'gen/net/duration.pb.cc': 100,
                'gen/net/duration.pb.h': 200,
            },
            include_chains={
                'gen/net/duration.pb.h': (
                    'gen/net/duration.pb.cc',
                    'gen/net/duration.pb.h',
                ),
            },
        )
        round_tripped = compile_size_probe.TranslationUnitSnapshot.FromDict(
            json.loads(json.dumps(snapshot.ToDict())))
        self.assertEqual(
            snapshot,
            round_tripped,
            msg=(
                'Expected FromDict(ToDict()) to reproduce the snapshot, '
                'including include chains as tuples.'),
        )

    def testBuildClangProbeCommandRaisesRuntimeErrorWhenPcmArtifactIsMissing(
            self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            compile_rule = compile_size_probe.NinjaCompileRule(
                translation_unit='gen/net/duration.pb.cc',
                ninja_source_path='gen/net/duration.pb.cc',
                object_target='obj/net/duration.pb.o',
                ninja_file_path='obj/net/proto.ninja',
                variables={
                    'defines': '-DPROTOBUF_INLINE_NOT_IN_HEADERS=0',
                    'include_dirs': '-I../.. -Igen',
                    'cflags': '-O2',
                    'cflags_cc': '-std=c++20',
                    'module_deps': (
                        '-fmodule-file=//build/modules:system='
                        'obj/build/modules/system/module.pcm'
                    ),
                },
            )
            with self.assertRaises(
                    RuntimeError,
                    msg=(
                        'Expected BuildClangProbeCommand to raise '
                        'RuntimeError when a required .pcm module artifact is '
                        'missing on disk.')):
                compile_size_probe.BuildClangProbeCommand(
                    compile_rule=compile_rule,
                    build_directory=temporary_directory,
                    clang_binary='/bin/clang++',
                )

    def testBuildClangProbeCommandRetainsModuleDepsWhenPcmArtifactExists(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            pcm_path = os.path.join(
                temporary_directory,
                'obj',
                'build',
                'modules',
                'system',
                'module.pcm',
            )
            _WriteFileWithByteSize(pcm_path, 64)
            compile_rule = compile_size_probe.NinjaCompileRule(
                translation_unit='gen/net/duration.pb.cc',
                ninja_source_path='gen/net/duration.pb.cc',
                object_target='obj/net/duration.pb.o',
                ninja_file_path='obj/net/proto.ninja',
                variables={
                    'include_dirs': '-I../..',
                    'module_deps': (
                        '-fmodule-file=//build/modules:system='
                        'obj/build/modules/system/module.pcm'
                    ),
                },
            )
            command = compile_size_probe.BuildClangProbeCommand(
                compile_rule=compile_rule,
                build_directory=temporary_directory,
                clang_binary='/bin/clang++',
            )

        self.assertIn(
            '-fmodule-file=//build/modules:system='
            'obj/build/modules/system/module.pcm',
            command,
            msg=(
                'Expected BuildClangProbeCommand to retain module_deps when '
                'all referenced .pcm artifacts exist on disk.'),
        )

    def testBuildClangProbeCommandAppendsPrivateModuleNameFlag(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            compile_rule = compile_size_probe.NinjaCompileRule(
                translation_unit='gen/net/duration.pb.cc',
                ninja_source_path='gen/net/duration.pb.cc',
                object_target='obj/net/duration.pb.o',
                ninja_file_path='obj/net/proto.ninja',
                variables={
                    'include_dirs': '-I../..',
                    'cc_module_name': '//net/cert:root_store_proto_lite',
                },
            )
            command = compile_size_probe.BuildClangProbeCommand(
                compile_rule=compile_rule,
                build_directory=temporary_directory,
                clang_binary='/bin/clang++',
            )

        self.assertEqual(
            [
                '/bin/clang++',
                '-I../..',
                '-fmodule-name=//net/cert:root_store_proto_lite_Private',
                '-M',
                '-H',
                'gen/net/duration.pb.cc',
            ],
            command,
            msg=(
                'Expected BuildClangProbeCommand to append '
                '-fmodule-name=<cc_module_name>_Private.'),
        )

    def testParseMakefileDependenciesOutputSkipsPrecompiledModuleFiles(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_directory = os.path.join(
                temporary_directory, 'out', 'Default')
            source_file = os.path.join(
                build_directory, 'gen', 'net', 'duration.pb.cc')
            header_file = os.path.join(
                temporary_directory,
                'third_party',
                'protobuf',
                'src',
                'port.h',
            )
            module_map = os.path.join(
                build_directory, 'gen', 'build', 'modules', 'module.modulemap')
            pcm_file = os.path.join(
                build_directory, 'obj', 'build', 'modules', 'std.pcm')

            _WriteFileWithByteSize(source_file, 100)
            _WriteFileWithByteSize(header_file, 200)
            _WriteFileWithByteSize(module_map, 300)
            _WriteFileWithByteSize(pcm_file, 4000)

            makefile_stdout = (
                'duration.pb.o: \\\r\n'
                '  gen/net/duration.pb.cc \\\n'
                '  ../../third_party/protobuf/src/port.h \\\n'
                '  gen/build/modules/module.modulemap \\\n'
                '  obj/build/modules/std.pcm\n'
            )
            dependencies = compile_size_probe.ParseMakefileDependenciesOutput(
                makefile_stdout=makefile_stdout,
                build_directory=build_directory,
                repository_root=temporary_directory,
            )

        self.assertEqual(
            {
                'gen/net/duration.pb.cc',
                'third_party/protobuf/src/port.h',
                'gen/build/modules/module.modulemap',
            },
            set(dependencies.keys()),
            msg=(
                'Expected ParseMakefileDependenciesOutput to include source '
                'files, headers, and module.modulemap while skipping .pcm '
                'files to match compiler_inputs_size.py.'),
        )

    def testParseMakefileDependenciesOutputMatchesCompilerInputsSizeMetric(
            self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_directory = os.path.join(
                temporary_directory, 'out', 'Default')
            source_file = os.path.join(
                build_directory, 'gen', 'net', 'duration.pb.cc')
            generated_header = os.path.join(
                build_directory, 'gen', 'net', 'duration.pb.h')
            runtime_header = os.path.join(
                temporary_directory,
                'third_party',
                'protobuf',
                'src',
                'arena.h',
            )
            _WriteFileWithByteSize(source_file, 1234)
            _WriteFileWithByteSize(generated_header, 2345)
            _WriteFileWithByteSize(runtime_header, 3456)

            makefile_stdout = (
                'duration.pb.o: gen/net/duration.pb.cc '
                'gen/net/duration.pb.h '
                '../../third_party/protobuf/src/arena.h\n'
            )
            dependencies = compile_size_probe.ParseMakefileDependenciesOutput(
                makefile_stdout=makefile_stdout,
                build_directory=build_directory,
                repository_root=temporary_directory,
            )
            total_bytes = sum(
                os.path.getsize(disk_path)
                for disk_path in dependencies.values()
            )

        self.assertEqual(
            1234 + 2345 + 3456,
            total_bytes,
            msg=(
                'Expected total compiler input bytes to equal the exact sum '
                'of the source file and all unique #included headers.'),
        )

    def testParseMakefileDependenciesOutputRaisesValueErrorWhenColonIsMissing(
            self):
        with self.assertRaisesRegex(
                ValueError,
                r'missing ":"',
                msg=(
                    'Expected ParseMakefileDependenciesOutput to raise '
                    'ValueError when clang -M stdout lacks a target colon.')):
            compile_size_probe.ParseMakefileDependenciesOutput(
                makefile_stdout='malformed output without colon\n',
                build_directory='/out/Default',
                repository_root='/src',
            )

    def testNormalizeDependencyFilePathMapsRepositoryHeaderToRepositoryRelative(
            self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_directory = os.path.join(
                temporary_directory, 'out', 'Default')
            repository_header = os.path.join(
                temporary_directory,
                'third_party',
                'protobuf',
                'src',
                'google',
                'protobuf',
                'extension_set.h',
            )
            os.makedirs(build_directory)
            _WriteFileWithByteSize(repository_header, 900)

            normalized_path, _ = (
                compile_size_probe.NormalizeDependencyFilePath(
                    raw_dependency_path=(
                        '../../third_party/protobuf/src/google/protobuf/'
                        'extension_set.h'),
                    build_directory=build_directory,
                    repository_root=temporary_directory,
                ))

        self.assertEqual(
            'third_party/protobuf/src/google/protobuf/extension_set.h',
            normalized_path,
            msg=(
                'Expected headers inside repository_root to normalize to '
                'repository-relative paths.'),
        )

    def testParseHeaderIncludeChainsReconstructsFirstIncludePathPerHeader(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_directory = os.path.join(
                temporary_directory, 'out', 'Default')
            os.makedirs(build_directory)
            header_trace_stderr = (
                '. gen/net/duration.pb.h\n'
                '.. ../../third_party/protobuf/src/google/protobuf/'
                'extension_set.h\n'
                '... ../../third_party/abseil-cpp/absl/container/btree_map.h\n'
                '.. ../../third_party/protobuf/src/google/protobuf/map.h\n'
                '... ../../third_party/abseil-cpp/absl/container/btree_map.h\n'
            )
            include_chains = compile_size_probe.ParseHeaderIncludeChains(
                header_trace_stderr=header_trace_stderr,
                translation_unit='gen/net/duration.pb.cc',
                build_directory=build_directory,
                repository_root=temporary_directory,
            )

        self.assertEqual(
            (
                'gen/net/duration.pb.cc',
                'gen/net/duration.pb.h',
                'third_party/protobuf/src/google/protobuf/extension_set.h',
                'third_party/abseil-cpp/absl/container/btree_map.h',
            ),
            include_chains['third_party/abseil-cpp/absl/container/btree_map.h'],
            msg=(
                'Expected ParseHeaderIncludeChains to record the first '
                'include chain that reached btree_map.h via extension_set.h.'),
        )
        self.assertEqual(
            (
                'gen/net/duration.pb.cc',
                'gen/net/duration.pb.h',
                'third_party/protobuf/src/google/protobuf/map.h',
            ),
            include_chains['third_party/protobuf/src/google/protobuf/map.h'],
            msg=(
                'Expected a shallower ".. map.h" line to pop extension_set.h '
                'and btree_map.h off the include stack.'),
        )

    def testMeasureTranslationUnitSumsDependencySizesAndRecordsIncludeChains(
            self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_directory = os.path.join(
                temporary_directory, 'out', 'Default')
            _WriteFileWithByteSize(
                os.path.join(build_directory, 'gen', 'net', 'duration.pb.cc'),
                100,
            )
            _WriteFileWithByteSize(
                os.path.join(build_directory, 'gen', 'net', 'duration.pb.h'),
                200,
            )
            compile_rule = compile_size_probe.NinjaCompileRule(
                translation_unit='gen/net/duration.pb.cc',
                ninja_source_path='gen/net/duration.pb.cc',
                object_target='obj/net/duration.pb.o',
                ninja_file_path='obj/net/proto.ninja',
                variables={
                    'defines': '-DFOO=1',
                    'include_dirs': '-I../..',
                    'cflags': '-O2',
                    'cflags_cc': '-std=c++20',
                },
            )
            completed_process = mock.Mock(
                returncode=0,
                stdout=(
                    'duration.pb.o: gen/net/duration.pb.cc '
                    'gen/net/duration.pb.h\n'
                ),
                stderr='. gen/net/duration.pb.h\n',
            )
            with mock.patch(
                    'subprocess.run',
                    return_value=completed_process) as mock_run:
                snapshot = compile_size_probe.MeasureTranslationUnit(
                    compile_rule=compile_rule,
                    build_directory=build_directory,
                    repository_root=temporary_directory,
                    clang_binary='/bin/clang++',
                )

        mock_run.assert_called_once_with(
            [
                '/bin/clang++',
                '-DFOO=1',
                '-I../..',
                '-O2',
                '-std=c++20',
                '-M',
                '-H',
                'gen/net/duration.pb.cc',
            ],
            cwd=build_directory,
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(
            compile_size_probe.TranslationUnitSnapshot(
                translation_unit='gen/net/duration.pb.cc',
                total_bytes=300,
                included_files={
                    'gen/net/duration.pb.cc': 100,
                    'gen/net/duration.pb.h': 200,
                },
                include_chains={
                    'gen/net/duration.pb.cc': ('gen/net/duration.pb.cc',),
                    'gen/net/duration.pb.h': (
                        'gen/net/duration.pb.cc',
                        'gen/net/duration.pb.h',
                    ),
                },
            ),
            snapshot,
            msg=(
                'Expected MeasureTranslationUnit to size every -M dependency '
                'from stdout and build include chains from the -H trace on '
                'stderr.'),
        )

    def testMeasureTranslationUnitRaisesRuntimeErrorWhenClangExitsNonZero(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            compile_rule = compile_size_probe.NinjaCompileRule(
                translation_unit='gen/net/duration.pb.cc',
                ninja_source_path='gen/net/duration.pb.cc',
                object_target='obj/net/duration.pb.o',
                ninja_file_path='obj/net/proto.ninja',
                variables={},
            )
            failed_process = mock.Mock(
                returncode=1,
                stdout='',
                stderr='fatal error: file not found\n',
            )
            with mock.patch('subprocess.run', return_value=failed_process):
                with self.assertRaisesRegex(
                        RuntimeError,
                        r'(?s)clang\+\+ -M -H failed.*fatal error: file not '
                        r'found',
                        msg=(
                            'Expected MeasureTranslationUnit to raise '
                            'RuntimeError including clang stderr on failure.')):
                    compile_size_probe.MeasureTranslationUnit(
                        compile_rule=compile_rule,
                        build_directory=temporary_directory,
                        repository_root=temporary_directory,
                        clang_binary='/bin/clang++',
                    )


if __name__ == '__main__':
    unittest.main()
