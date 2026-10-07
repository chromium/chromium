#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for //third_party/protobuf/compile_size_probe.py."""

from __future__ import annotations

from collections.abc import Sequence
import io
import json
import os
import stat
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


def _CreateSyntheticNinjaWorkspace(
        repository_root: str,
        build_directory: str,
        translation_units: Sequence[str]) -> None:
    """Populates `.ninja` rules and source files for `translation_units`."""
    for index, translation_unit in enumerate(translation_units):
        if translation_unit.startswith('gen/'):
            source_path = os.path.join(build_directory, translation_unit)
            ninja_source = translation_unit
        else:
            source_path = os.path.join(repository_root, translation_unit)
            ninja_source = os.path.relpath(
                source_path, build_directory).replace(os.sep, '/')
        _WriteFileWithByteSize(source_path, 500 + index * 100)

        relative_without_gen = translation_unit.removeprefix('gen/')
        target_parent = os.path.dirname(relative_without_gen)
        ninja_directory = os.path.join(build_directory, 'obj', target_parent)
        os.makedirs(ninja_directory, exist_ok=True)
        ninja_file_path = os.path.join(ninja_directory, f'target_{index}.ninja')
        object_target = (
            f'obj/{target_parent}/target_{index}/'
            f'{os.path.basename(translation_unit)}.o')
        ninja_contents = (
            'defines = -DDUMMY\n'
            'include_dirs = -I../.. -Igen\n'
            'cflags = -O2\n'
            'cflags_cc = -std=c++20\n'
            f'build {object_target}: cxx {ninja_source}\n'
        )
        with open(ninja_file_path, 'w', encoding='utf-8') as file_handle:
            file_handle.write(ninja_contents)


_SKIP_ON_WINDOWS = unittest.skipIf(
    sys.platform == 'win32',
    'compile_size_probe.py drives clang++ -M -H, which the clang-cl Windows '
    'toolchain does not use.',
)


def _WriteFakeClangScript(
        script_path: str, mode_file_path: str | None = None) -> str:
    """Writes an executable fake `clang++` script that mimics `-M -H`."""
    script_contents = f"""#!/usr/bin/env python3
import sys

mode = "before"
mode_file_path = {mode_file_path!r}
if mode_file_path:
    with open(mode_file_path, "r", encoding="utf-8") as handle:
        mode = handle.read().strip()

source_path = sys.argv[-1]
dependencies = [
    source_path,
    "../../third_party/protobuf/src/google/protobuf/extension_set.h",
]
trace_lines = [
    ". ../../third_party/protobuf/src/google/protobuf/extension_set.h",
]
if mode == "after":
    dependencies.append(
        "../../third_party/abseil-cpp/absl/container/btree_map.h")
    trace_lines.append(
        ".. ../../third_party/abseil-cpp/absl/container/btree_map.h")

sys.stdout.write("target.o: " + " \\\\\\n  ".join(dependencies) + "\\n")
sys.stderr.write("\\n".join(trace_lines) + "\\n")
"""
    with open(script_path, 'w', encoding='utf-8') as file_handle:
        file_handle.write(script_contents)
    os.chmod(script_path, stat.S_IRWXU)
    return script_path


class CompileSizeProbeTest(unittest.TestCase):
    """Tests Ninja parsing, dependency measurement, and baseline storage."""

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

    def testBuildObjectTargetsRedirectsAutoninjaStdoutToStderr(self):
        with mock.patch(
                'compile_size_probe.ResolveAutoninjaBinary',
                return_value='/bin/autoninja'):
            with mock.patch('subprocess.check_call') as mock_check_call:
                compile_size_probe.BuildObjectTargets(
                    build_directory='/out/Release',
                    repository_root='/src',
                    object_targets=['gen/net/duration.pb.cc'],
                )

        mock_check_call.assert_called_once_with(
            ['/bin/autoninja', '-C', '/out/Release', 'gen/net/duration.pb.cc'],
            stdout=sys.stderr,
        )

    def testMeasureProbeBaselineBuildsPrerequisiteTargetsByDefault(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_directory = os.path.join(
                temporary_directory, 'out', 'Default')
            _CreateSyntheticNinjaWorkspace(
                temporary_directory,
                build_directory,
                ('gen/net/cert/root_store_proto_lite/duration.pb.cc',),
            )
            fake_snapshot = compile_size_probe.TranslationUnitSnapshot(
                translation_unit=(
                    'gen/net/cert/root_store_proto_lite/duration.pb.cc'),
                total_bytes=500,
                included_files={
                    'gen/net/cert/root_store_proto_lite/duration.pb.cc': 500,
                },
                include_chains={},
            )

            with (
                mock.patch(
                    'compile_size_probe.BuildObjectTargets') as mock_build,
                mock.patch(
                    'compile_size_probe.MeasureTranslationUnit',
                    return_value=fake_snapshot),
            ):
                compile_size_probe.MeasureProbeBaseline(
                    build_directory=build_directory,
                    repository_root=temporary_directory,
                    translation_units=(
                        'gen/net/cert/root_store_proto_lite/duration.pb.cc',
                    ),
                    clang_binary='/bin/clang++',
                )

        mock_build.assert_called_once_with(
            build_directory=build_directory,
            repository_root=temporary_directory,
            object_targets=[
                'gen/net/cert/root_store_proto_lite/duration.pb.cc',
            ],
        )

    def testMeasureProbeBaselineSkipsBuildWhenShouldBuildTargetsIsFalse(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_directory = os.path.join(
                temporary_directory, 'out', 'Default')
            _CreateSyntheticNinjaWorkspace(
                temporary_directory,
                build_directory,
                ('gen/net/cert/root_store_proto_lite/duration.pb.cc',),
            )
            fake_snapshot = compile_size_probe.TranslationUnitSnapshot(
                translation_unit=(
                    'gen/net/cert/root_store_proto_lite/duration.pb.cc'),
                total_bytes=500,
                included_files={
                    'gen/net/cert/root_store_proto_lite/duration.pb.cc': 500,
                },
                include_chains={},
            )

            with (
                mock.patch(
                    'compile_size_probe.BuildObjectTargets') as mock_build,
                mock.patch(
                    'compile_size_probe.MeasureTranslationUnit',
                    return_value=fake_snapshot),
            ):
                compile_size_probe.MeasureProbeBaseline(
                    build_directory=build_directory,
                    repository_root=temporary_directory,
                    translation_units=(
                        'gen/net/cert/root_store_proto_lite/duration.pb.cc',
                    ),
                    clang_binary='/bin/clang++',
                    should_build_targets=False,
                )

        self.assertEqual(
            0,
            mock_build.call_count,
            msg=(
                'Expected MeasureProbeBaseline to skip BuildObjectTargets '
                'when should_build_targets is False.'),
        )

    def testSaveAndLoadProbeBaselineRoundTripsThroughBuildDirectory(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            baseline_directory = (
                compile_size_probe.ResolveBaselineStorageDirectory(
                    temporary_directory))
            original_baseline = compile_size_probe.ProbeBaseline(
                translation_units=(
                    compile_size_probe.TranslationUnitSnapshot(
                        translation_unit='gen/net/cert/duration.pb.cc',
                        total_bytes=1500,
                        included_files={
                            'gen/net/cert/duration.pb.cc': 500,
                            'gen/net/cert/duration.pb.h': 1000,
                        },
                        include_chains={
                            'gen/net/cert/duration.pb.h': (
                                'gen/net/cert/duration.pb.cc',
                                'gen/net/cert/duration.pb.h',
                            ),
                        },
                    ),
                ))

            compile_size_probe.SaveProbeBaseline(
                baseline=original_baseline,
                baseline_directory=baseline_directory,
                baseline_name='before_roll',
            )
            loaded_baseline = compile_size_probe.LoadProbeBaseline(
                baseline_directory=baseline_directory,
                baseline_name='before_roll',
            )

        self.assertEqual(
            original_baseline,
            loaded_baseline,
            msg=(
                'Expected SaveProbeBaseline and LoadProbeBaseline to '
                'round-trip snapshots and include chains losslessly under '
                '<build_directory>/.compile_size_probe.'),
        )

    def testSaveProbeBaselineRaisesFileExistsErrorWithoutOverwriteFlag(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            baseline_directory = (
                compile_size_probe.ResolveBaselineStorageDirectory(
                    temporary_directory))
            baseline = compile_size_probe.ProbeBaseline(translation_units=())
            compile_size_probe.SaveProbeBaseline(
                baseline=baseline,
                baseline_directory=baseline_directory,
                baseline_name='before_roll',
            )

            with self.assertRaises(
                    FileExistsError,
                    msg=(
                        'Expected SaveProbeBaseline to raise FileExistsError '
                        'when overwriting an existing baseline without '
                        'should_overwrite=True.')):
                compile_size_probe.SaveProbeBaseline(
                    baseline=baseline,
                    baseline_directory=baseline_directory,
                    baseline_name='before_roll',
                )

    def testResolveBaselineStorageDirectoryPlacesBaselinesUnderBuildDirectory(
            self):
        build_directory = '/workspace/src/out_linux/Release'
        storage_directory = compile_size_probe.ResolveBaselineStorageDirectory(
            build_directory)
        self.assertEqual(
            os.path.join(
                os.path.abspath(build_directory), '.compile_size_probe'),
            storage_directory,
            msg=(
                'Expected ResolveBaselineStorageDirectory to place saved '
                'baselines under <build_directory>/.compile_size_probe.'),
        )

    def testBaselineFilePathRejectsPathTraversalNames(self):
        with self.assertRaises(
                ValueError,
                msg=(
                    'Expected BaselineFilePath to reject names containing '
                    'path separators or parent-directory traversal.')):
            compile_size_probe.BaselineFilePath('/tmp/baselines', '../escape')

    def testBaselineFilePathRejectsSingleDotBaselineName(self):
        with self.assertRaises(
                ValueError,
                msg='Expected BaselineFilePath to reject "." as a name.'):
            compile_size_probe.BaselineFilePath('/tmp/baselines', '.')

    def testBaselineFilePathRejectsConsecutiveDotsInBaselineName(self):
        with self.assertRaises(
                ValueError,
                msg=(
                    'Expected BaselineFilePath to reject names containing '
                    '".." even without path separators.')):
            compile_size_probe.BaselineFilePath(
                '/tmp/baselines', 'before..after')

    def testLoadProbeBaselineRaisesValueErrorOnUnsupportedFormatVersion(
            self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            target_path = compile_size_probe.BaselineFilePath(
                temporary_directory, 'future')
            with open(target_path, 'w', encoding='utf-8') as file_handle:
                json.dump(
                    {'format_version': 99, 'translation_units': []},
                    file_handle,
                )
            with self.assertRaisesRegex(
                    ValueError,
                    r'Unsupported baseline format_version 99',
                    msg=(
                        'Expected LoadProbeBaseline to reject baselines with '
                        'an unsupported format_version.')):
                compile_size_probe.LoadProbeBaseline(
                    temporary_directory, 'future')

    def testLoadProbeBaselineRaisesValueErrorOnMalformedPayload(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            target_path = compile_size_probe.BaselineFilePath(
                temporary_directory, 'malformed')
            with open(target_path, 'w', encoding='utf-8') as file_handle:
                json.dump(
                    {
                        'format_version': 1,
                        'translation_units': [{'translation_unit': 'a.cc'}],
                    },
                    file_handle,
                )
            with self.assertRaisesRegex(
                    ValueError,
                    r'Malformed baseline JSON',
                    msg=(
                        'Expected LoadProbeBaseline to convert missing keys '
                        'or wrong types in baseline JSON into ValueError.')):
                compile_size_probe.LoadProbeBaseline(
                    temporary_directory, 'malformed')

    def testBaselineFilePathRejectsTrailingNewlineInBaselineName(self):
        with self.assertRaises(
                ValueError,
                msg=(
                    'Expected BaselineFilePath to reject baseline names '
                    'with a trailing newline via fullmatch.')):
            compile_size_probe.BaselineFilePath('/tmp/baselines', 'valid\n')

    def testMeasureProbeBaselineRaisesFileNotFoundErrorForMissingBuildDirectory(
            self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            missing_build_directory = os.path.join(
                temporary_directory, 'no_such_build_directory')
            with self.assertRaises(
                    FileNotFoundError,
                    msg=(
                        'Expected MeasureProbeBaseline to raise '
                        'FileNotFoundError when build_directory is missing.')):
                compile_size_probe.MeasureProbeBaseline(
                    build_directory=missing_build_directory,
                    repository_root=temporary_directory,
                    translation_units=(
                        compile_size_probe.DEFAULT_TRANSLATION_UNITS),
                    clang_binary='clang++',
                    should_build_targets=False,
                )

    def testFormatHumanByteSizeFormatsMebibytesForOneMebibyteAndAbove(self):
        formatted = compile_size_probe.FormatHumanByteSize(1024 * 1024)
        self.assertEqual(
            '1.00 MiB (1,048,576 B)',
            formatted,
            msg=(
                'Expected FormatHumanByteSize to format 1,048,576 bytes as '
                '"1.00 MiB (1,048,576 B)".'),
        )

    def testFormatHumanByteSizeFormatsNegativeKilobytesWithLeadingMinusSign(
            self):
        formatted = compile_size_probe.FormatHumanByteSize(-2048)
        self.assertEqual(
            '-2.00 KiB (-2,048 B)',
            formatted,
            msg=(
                'Expected FormatHumanByteSize to format -2,048 bytes as '
                '"-2.00 KiB (-2,048 B)".'),
        )

    def testFormatBaselineSummaryRendersSavedPathAndUnitCounts(self):
        baseline = compile_size_probe.ProbeBaseline(
            translation_units=(
                compile_size_probe.TranslationUnitSnapshot(
                    translation_unit='gen/net/duration.pb.cc',
                    total_bytes=1536,
                    included_files={
                        'gen/net/duration.pb.cc': 512,
                        'gen/net/duration.pb.h': 1024,
                    },
                    include_chains={},
                ),
            ))

        summary = compile_size_probe.FormatBaselineSummary(
            baseline, saved_path='/out/Default/.compile_size_probe/base.json')

        self.assertEqual(
            (
                'Saved compile-size baseline to '
                '/out/Default/.compile_size_probe/base.json\n'
                'Total compiler inputs size across 1 translation unit: '
                '1.50 KiB (1,536 B)\n'
                '  gen/net/duration.pb.cc: 1.50 KiB (1,536 B) (2 files)'
            ),
            summary,
            msg=(
                'Expected FormatBaselineSummary to include the saved path '
                'line, singular translation unit label, and file count.'),
        )

    def testMainExitsOneOnWindowsPlatform(self):
        captured_stderr = io.StringIO()
        with (
            mock.patch.object(sys, 'platform', 'win32'),
            mock.patch(
                'compile_size_probe.MeasureProbeBaseline',
                side_effect=AssertionError(
                    'MeasureProbeBaseline should not be called on win32.'),
            ),
            mock.patch('sys.stderr', captured_stderr),
        ):
            exit_code = compile_size_probe.main(['-C', '/out/Default'])

        self.assertEqual(
            1,
            exit_code,
            msg=(
                'Expected main() to return exit code 1 on win32 before '
                'running MeasureProbeBaseline.'),
        )

    def testMainWritesWindowsUnsupportedErrorToStderrOnWin32(self):
        captured_stderr = io.StringIO()
        with (
            mock.patch.object(sys, 'platform', 'win32'),
            mock.patch('sys.stderr', captured_stderr),
        ):
            compile_size_probe.main(['-C', '/out/Default'])

        self.assertIn(
            'not supported on Windows',
            captured_stderr.getvalue(),
            msg=(
                'Expected main() on win32 to explain on stderr that '
                'compile_size_probe.py is not supported on Windows.'),
        )

    def testMainRejectsEmptySaveBaselineNameWithExitCodeOne(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            captured_stderr = io.StringIO()
            with (
                mock.patch.object(sys, 'platform', 'linux'),
                mock.patch(
                    'compile_size_probe.MeasureProbeBaseline',
                    side_effect=AssertionError(
                        'MeasureProbeBaseline should not be called when '
                        '--save name is empty.'),
                ),
                mock.patch('sys.stderr', captured_stderr),
            ):
                exit_code = compile_size_probe.main([
                    '-C',
                    temporary_directory,
                    '--save',
                    '',
                ])

        self.assertEqual(
            1,
            exit_code,
            msg=(
                'Expected main(["-C", ..., "--save", ""]) to reject an '
                'empty baseline name and return exit code 1 before '
                'running MeasureProbeBaseline.'),
        )

    def testMainWritesInvalidBaselineNameErrorToStderrForEmptySaveName(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            captured_stderr = io.StringIO()
            with (
                mock.patch.object(sys, 'platform', 'linux'),
                mock.patch('sys.stderr', captured_stderr),
            ):
                compile_size_probe.main([
                    '-C',
                    temporary_directory,
                    '--save',
                    '',
                ])

        self.assertIn(
            'Invalid baseline name',
            captured_stderr.getvalue(),
            msg=(
                'Expected main(["-C", ..., "--save", ""]) to report '
                '"Invalid baseline name" on stderr.'),
        )

    def testMainRejectsExistingBaselineBeforeRunningMeasurement(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            baseline_directory = (
                compile_size_probe.ResolveBaselineStorageDirectory(
                    temporary_directory))
            compile_size_probe.SaveProbeBaseline(
                baseline=compile_size_probe.ProbeBaseline(translation_units=()),
                baseline_directory=baseline_directory,
                baseline_name='existing',
            )
            captured_stderr = io.StringIO()
            with (
                mock.patch.object(sys, 'platform', 'linux'),
                mock.patch(
                    'compile_size_probe.MeasureProbeBaseline',
                    side_effect=AssertionError(
                        'MeasureProbeBaseline should not be called when '
                        'the target baseline file already exists.'),
                ),
                mock.patch('sys.stderr', captured_stderr),
            ):
                exit_code = compile_size_probe.main([
                    '-C',
                    temporary_directory,
                    '--save',
                    'existing',
                ])

        self.assertEqual(
            1,
            exit_code,
            msg=(
                'Expected main --save to fail with exit code 1 before calling '
                'MeasureProbeBaseline when the baseline already exists.'),
        )

    def testMainWritesAlreadyExistsErrorToStderrWhenBaselineExists(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            baseline_directory = (
                compile_size_probe.ResolveBaselineStorageDirectory(
                    temporary_directory))
            compile_size_probe.SaveProbeBaseline(
                baseline=compile_size_probe.ProbeBaseline(translation_units=()),
                baseline_directory=baseline_directory,
                baseline_name='existing',
            )
            captured_stderr = io.StringIO()
            with (
                mock.patch.object(sys, 'platform', 'linux'),
                mock.patch('sys.stderr', captured_stderr),
            ):
                compile_size_probe.main([
                    '-C',
                    temporary_directory,
                    '--save',
                    'existing',
                ])

        self.assertIn(
            'already exists',
            captured_stderr.getvalue(),
            msg=(
                'Expected main --save to report on stderr that the target '
                'baseline already exists.'),
        )

    def testMainRejectsOverwriteWithoutSaveFlag(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            captured_stderr = io.StringIO()
            with (
                mock.patch.object(sys, 'platform', 'linux'),
                mock.patch(
                    'compile_size_probe.MeasureProbeBaseline',
                    side_effect=AssertionError(
                        'MeasureProbeBaseline should not be called when '
                        '--overwrite is passed without --save.'),
                ),
                mock.patch('sys.stderr', captured_stderr),
            ):
                exit_code = compile_size_probe.main([
                    '-C',
                    temporary_directory,
                    '--overwrite',
                ])

        self.assertEqual(
            1,
            exit_code,
            msg=(
                'Expected main() to reject --overwrite without --save and '
                'return exit code 1.'),
        )

    def testMainWritesOverwriteRequiresSaveErrorToStderr(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            captured_stderr = io.StringIO()
            with (
                mock.patch.object(sys, 'platform', 'linux'),
                mock.patch(
                    'compile_size_probe.MeasureProbeBaseline',
                    side_effect=AssertionError(
                        'MeasureProbeBaseline should not be called when '
                        '--overwrite is passed without --save.'),
                ),
                mock.patch('sys.stderr', captured_stderr),
            ):
                compile_size_probe.main([
                    '-C',
                    temporary_directory,
                    '--overwrite',
                ])

        self.assertIn(
            '--overwrite can only be used with --save.',
            captured_stderr.getvalue(),
            msg=(
                'Expected main() to write the "--overwrite can only be used '
                'with --save." error message to stderr.'),
        )

    @_SKIP_ON_WINDOWS
    def testCommandLineSaveWritesBaselineUnderBuildDirectory(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_directory = os.path.join(
                temporary_directory, 'out', 'Default')
            _CreateSyntheticNinjaWorkspace(
                temporary_directory,
                build_directory,
                compile_size_probe.DEFAULT_TRANSLATION_UNITS,
            )
            extension_set_header = os.path.join(
                temporary_directory,
                'third_party',
                'protobuf',
                'src',
                'google',
                'protobuf',
                'extension_set.h',
            )
            _WriteFileWithByteSize(extension_set_header, 2000)
            fake_clang_path = _WriteFakeClangScript(
                os.path.join(temporary_directory, 'fake_clang.py'))

            with (
                mock.patch.object(
                    compile_size_probe,
                    '_DEFAULT_REPOSITORY_ROOT',
                    temporary_directory,
                ),
                mock.patch.object(
                    compile_size_probe,
                    '_ResolveDefaultClangBinary',
                    return_value=fake_clang_path,
                ),
                mock.patch('sys.stdout', io.StringIO()),
            ):
                compile_size_probe.main([
                    '-C',
                    build_directory,
                    '--no-build',
                    '--save',
                    'before_roll',
                ])
            saved_on_disk = compile_size_probe.LoadProbeBaseline(
                compile_size_probe.ResolveBaselineStorageDirectory(
                    build_directory),
                'before_roll',
            )

        self.assertEqual(
            7800,
            saved_on_disk.TotalBytes(),
            msg=(
                'Expected main --save to write the measured 7800-byte '
                'baseline under <build_directory>/.compile_size_probe.'),
        )

    @_SKIP_ON_WINDOWS
    def testCommandLineSaveWithOverwriteReplacesExistingBaseline(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_directory = os.path.join(
                temporary_directory, 'out', 'Default')
            _CreateSyntheticNinjaWorkspace(
                temporary_directory,
                build_directory,
                compile_size_probe.DEFAULT_TRANSLATION_UNITS,
            )
            baseline_directory = (
                compile_size_probe.ResolveBaselineStorageDirectory(
                    build_directory))
            compile_size_probe.SaveProbeBaseline(
                baseline=compile_size_probe.ProbeBaseline(translation_units=()),
                baseline_directory=baseline_directory,
                baseline_name='before_roll',
            )
            extension_set_header = os.path.join(
                temporary_directory,
                'third_party',
                'protobuf',
                'src',
                'google',
                'protobuf',
                'extension_set.h',
            )
            _WriteFileWithByteSize(extension_set_header, 2000)
            fake_clang_path = _WriteFakeClangScript(
                os.path.join(temporary_directory, 'fake_clang.py'))

            captured_stdout = io.StringIO()
            with (
                mock.patch.object(
                    compile_size_probe,
                    '_DEFAULT_REPOSITORY_ROOT',
                    temporary_directory,
                ),
                mock.patch.object(
                    compile_size_probe,
                    '_ResolveDefaultClangBinary',
                    return_value=fake_clang_path,
                ),
                mock.patch('sys.stdout', captured_stdout),
            ):
                compile_size_probe.main([
                    '-C',
                    build_directory,
                    '--no-build',
                    '--save',
                    'before_roll',
                    '--overwrite',
                    '--json',
                ])
            parsed_output = json.loads(captured_stdout.getvalue())

        self.assertEqual(
            7800,
            parsed_output['total_bytes'],
            msg=(
                'Expected main --save --overwrite --json to replace the '
                'existing baseline and emit 7800 total_bytes in JSON.'),
        )

    def testWriteJsonAtomicallyRemovesTemporaryFileWhenReplaceFails(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            target_path = os.path.join(temporary_directory, 'baseline.json')
            with (
                mock.patch(
                    'os.replace',
                    side_effect=OSError('simulated disk error'),
                ),
                self.assertRaises(
                    OSError,
                    msg=(
                        'Expected _WriteJsonAtomically to propagate '
                        'OSError when os.replace fails.'),
                ),
            ):
                compile_size_probe._WriteJsonAtomically(
                    target_path, {'format_version': 1})
            remaining_entries = os.listdir(temporary_directory)

        self.assertEqual(
            [],
            remaining_entries,
            msg=(
                'Expected _WriteJsonAtomically to delete its temporary '
                '.tmp.<pid> file when os.replace raises an error.'),
        )

    def testCompareTranslationUnitSnapshotsOrdersAddedHeadersLargestFirst(self):
        before_snapshot = compile_size_probe.TranslationUnitSnapshot(
            translation_unit='gen/sync/sync_entity.pb.cc',
            total_bytes=2000,
            included_files={'gen/sync/sync_entity.pb.cc': 2000},
            include_chains={},
        )
        after_snapshot = compile_size_probe.TranslationUnitSnapshot(
            translation_unit='gen/sync/sync_entity.pb.cc',
            total_bytes=43000,
            included_files={
                'gen/sync/sync_entity.pb.cc': 2000,
                'third_party/abseil-cpp/absl/container/layout.h': 11000,
                'third_party/abseil-cpp/absl/container/btree_map.h': 30000,
            },
            include_chains={
                'third_party/abseil-cpp/absl/container/btree_map.h': (
                    'gen/sync/sync_entity.pb.cc',
                    'third_party/abseil-cpp/absl/container/btree_map.h',
                ),
            },
        )

        comparison = compile_size_probe.CompareTranslationUnitSnapshots(
            before_snapshot, after_snapshot)

        self.assertEqual(
            [
                'third_party/abseil-cpp/absl/container/btree_map.h',
                'third_party/abseil-cpp/absl/container/layout.h',
            ],
            [header.path for header in comparison.added_headers],
            msg=(
                'Expected added_headers to be sorted from largest size_bytes '
                'to smallest.'),
        )

    def testCompareTranslationUnitSnapshotsFallsBackToSingletonIncludeChain(
            self):
        before_snapshot = compile_size_probe.TranslationUnitSnapshot(
            translation_unit='gen/sync/sync_entity.pb.cc',
            total_bytes=2000,
            included_files={'gen/sync/sync_entity.pb.cc': 2000},
            include_chains={},
        )
        after_snapshot = compile_size_probe.TranslationUnitSnapshot(
            translation_unit='gen/sync/sync_entity.pb.cc',
            total_bytes=13000,
            included_files={
                'gen/sync/sync_entity.pb.cc': 2000,
                'third_party/abseil-cpp/absl/container/layout.h': 11000,
            },
            include_chains={},
        )

        comparison = compile_size_probe.CompareTranslationUnitSnapshots(
            before_snapshot, after_snapshot)

        self.assertEqual(
            ('third_party/abseil-cpp/absl/container/layout.h',),
            comparison.added_headers[0].include_chain,
            msg=(
                'Expected AddedHeaderDelta.include_chain to fall back to '
                '(path,) when the header is absent from include_chains.'),
        )

    def testCompareTranslationUnitSnapshotsOrdersRemovedHeadersLargestFirst(
            self):
        before_snapshot = compile_size_probe.TranslationUnitSnapshot(
            translation_unit='gen/sync/sync_entity.pb.cc',
            total_bytes=5000,
            included_files={
                'gen/sync/sync_entity.pb.cc': 2000,
                'third_party/protobuf/src/old_small.h': 1000,
                'third_party/protobuf/src/old_large.h': 2000,
            },
            include_chains={},
        )
        after_snapshot = compile_size_probe.TranslationUnitSnapshot(
            translation_unit='gen/sync/sync_entity.pb.cc',
            total_bytes=2000,
            included_files={'gen/sync/sync_entity.pb.cc': 2000},
            include_chains={},
        )

        comparison = compile_size_probe.CompareTranslationUnitSnapshots(
            before_snapshot, after_snapshot)

        self.assertEqual(
            [
                'third_party/protobuf/src/old_large.h',
                'third_party/protobuf/src/old_small.h',
            ],
            [header.path for header in comparison.removed_headers],
            msg=(
                'Expected removed_headers to be sorted from largest '
                'size_bytes to smallest.'),
        )

    def testCompareTranslationUnitSnapshotsOrdersResizedFilesLargestMagnitude(
            self):
        before_snapshot = compile_size_probe.TranslationUnitSnapshot(
            translation_unit='gen/sync/sync_entity.pb.cc',
            total_bytes=7000,
            included_files={
                'gen/sync/sync_entity.pb.cc': 2000,
                'third_party/protobuf/src/extension_set.h': 5000,
            },
            include_chains={},
        )
        after_snapshot = compile_size_probe.TranslationUnitSnapshot(
            translation_unit='gen/sync/sync_entity.pb.cc',
            total_bytes=8000,
            included_files={
                'gen/sync/sync_entity.pb.cc': 2100,
                'third_party/protobuf/src/extension_set.h': 5900,
            },
            include_chains={},
        )

        comparison = compile_size_probe.CompareTranslationUnitSnapshots(
            before_snapshot, after_snapshot)

        self.assertEqual(
            [
                'third_party/protobuf/src/extension_set.h',
                'gen/sync/sync_entity.pb.cc',
            ],
            [resized_file.path for resized_file in comparison.resized_files],
            msg=(
                'Expected resized_files to be sorted by absolute delta_bytes '
                'descending.'),
        )

    def testCompareTranslationUnitSnapshotsOrdersLargeShrinkBeforeSmallGrowth(
            self):
        before_snapshot = compile_size_probe.TranslationUnitSnapshot(
            translation_unit='gen/sync/sync_entity.pb.cc',
            total_bytes=7000,
            included_files={
                'gen/sync/sync_entity.pb.cc': 2000,
                'third_party/protobuf/src/extension_set.h': 5000,
            },
            include_chains={},
        )
        after_snapshot = compile_size_probe.TranslationUnitSnapshot(
            translation_unit='gen/sync/sync_entity.pb.cc',
            total_bytes=6200,
            included_files={
                'gen/sync/sync_entity.pb.cc': 2100,
                'third_party/protobuf/src/extension_set.h': 4100,
            },
            include_chains={},
        )

        comparison = compile_size_probe.CompareTranslationUnitSnapshots(
            before_snapshot, after_snapshot)

        self.assertEqual(
            [
                'third_party/protobuf/src/extension_set.h',
                'gen/sync/sync_entity.pb.cc',
            ],
            [resized_file.path for resized_file in comparison.resized_files],
            msg=(
                'Expected _ResizedFileSortKey to sort by abs(delta_bytes) '
                'so a -900 B shrink ranks ahead of a +100 B growth.'),
        )

    def testCompareTranslationUnitSnapshotsComputesPerUnitDeltaBytes(self):
        before_snapshot = compile_size_probe.TranslationUnitSnapshot(
            translation_unit='gen/sync/sync_entity.pb.cc',
            total_bytes=1000,
            included_files={'gen/sync/sync_entity.pb.cc': 1000},
            include_chains={},
        )
        after_snapshot = compile_size_probe.TranslationUnitSnapshot(
            translation_unit='gen/sync/sync_entity.pb.cc',
            total_bytes=1500,
            included_files={'gen/sync/sync_entity.pb.cc': 1500},
            include_chains={},
        )

        comparison = compile_size_probe.CompareTranslationUnitSnapshots(
            before_snapshot, after_snapshot)

        self.assertEqual(
            500,
            comparison.delta_bytes,
            msg=(
                'Expected TranslationUnitComparison.delta_bytes to equal '
                'after_bytes - before_bytes.'),
        )

    def testCompareTranslationUnitSnapshotsComputesResizedFileDeltaBytes(self):
        before_snapshot = compile_size_probe.TranslationUnitSnapshot(
            translation_unit='gen/sync/sync_entity.pb.cc',
            total_bytes=1000,
            included_files={'gen/sync/sync_entity.pb.cc': 1000},
            include_chains={},
        )
        after_snapshot = compile_size_probe.TranslationUnitSnapshot(
            translation_unit='gen/sync/sync_entity.pb.cc',
            total_bytes=1300,
            included_files={'gen/sync/sync_entity.pb.cc': 1300},
            include_chains={},
        )

        comparison = compile_size_probe.CompareTranslationUnitSnapshots(
            before_snapshot, after_snapshot)

        self.assertEqual(
            300,
            comparison.resized_files[0].delta_bytes,
            msg=(
                'Expected ResizedFileDelta.delta_bytes to equal '
                'after_bytes - before_bytes.'),
        )

    def testCompareTranslationUnitSnapshotsExcludesUnchangedFilesFromResized(
            self):
        before_snapshot = compile_size_probe.TranslationUnitSnapshot(
            translation_unit='gen/sync/sync_entity.pb.cc',
            total_bytes=1000,
            included_files={'gen/sync/sync_entity.pb.cc': 1000},
            include_chains={},
        )
        after_snapshot = compile_size_probe.TranslationUnitSnapshot(
            translation_unit='gen/sync/sync_entity.pb.cc',
            total_bytes=1000,
            included_files={'gen/sync/sync_entity.pb.cc': 1000},
            include_chains={},
        )

        comparison = compile_size_probe.CompareTranslationUnitSnapshots(
            before_snapshot, after_snapshot)

        self.assertEqual(
            (),
            comparison.resized_files,
            msg=(
                'Expected CompareTranslationUnitSnapshots to omit unchanged '
                'files from resized_files.'),
        )

    def testCompareProbeBaselinesAggregatesBeforeAfterAndDeltaTotalBytes(self):
        before_baseline = compile_size_probe.ProbeBaseline(
            translation_units=(
                compile_size_probe.TranslationUnitSnapshot(
                    translation_unit='gen/net/cert/duration.pb.cc',
                    total_bytes=1000,
                    included_files={'gen/net/cert/duration.pb.cc': 1000},
                    include_chains={},
                ),
                compile_size_probe.TranslationUnitSnapshot(
                    translation_unit=(
                        'components/sync/model/processor_entity.cc'),
                    total_bytes=2500,
                    included_files={
                        'components/sync/model/processor_entity.cc': 2500,
                    },
                    include_chains={},
                ),
            ))
        after_baseline = compile_size_probe.ProbeBaseline(
            translation_units=(
                compile_size_probe.TranslationUnitSnapshot(
                    translation_unit='gen/net/cert/duration.pb.cc',
                    total_bytes=1400,
                    included_files={'gen/net/cert/duration.pb.cc': 1400},
                    include_chains={},
                ),
            ))

        report = compile_size_probe.CompareProbeBaselines(
            baseline_name='before',
            before_baseline=before_baseline,
            after_baseline=after_baseline,
        )

        self.assertEqual(
            400,
            report.delta_total_bytes,
            msg=(
                'Expected CompareProbeBaselines to compute delta_total_bytes '
                'across the compared subset of translation units.'),
        )

    def testFormatComparisonReportIncludesHeaderDeltasAndIncludeChains(self):
        report = compile_size_probe.ProbeComparisonReport(
            baseline_name='before',
            before_total_bytes=10000,
            after_total_bytes=45000,
            delta_total_bytes=35000,
            translation_units=(
                compile_size_probe.TranslationUnitComparison(
                    translation_unit='gen/net/cert/duration.pb.cc',
                    before_bytes=10000,
                    after_bytes=45000,
                    delta_bytes=35000,
                    added_headers=(
                        compile_size_probe.AddedHeaderDelta(
                            path=(
                                'third_party/abseil-cpp/absl/container/'
                                'btree_map.h'),
                            size_bytes=34668,
                            include_chain=(
                                'gen/net/cert/duration.pb.cc',
                                'gen/net/cert/duration.pb.h',
                                'third_party/protobuf/src/google/protobuf/'
                                'extension_set.h',
                                'third_party/abseil-cpp/absl/container/'
                                'btree_map.h',
                            ),
                        ),
                    ),
                    removed_headers=(),
                    resized_files=(),
                ),
            ),
        )

        formatted_report = compile_size_probe.FormatComparisonReport(report)

        self.assertIn(
            'via: gen/net/cert/duration.pb.cc -> gen/net/cert/duration.pb.h '
            '-> third_party/protobuf/src/google/protobuf/extension_set.h '
            '-> third_party/abseil-cpp/absl/container/btree_map.h',
            formatted_report,
            msg=(
                'Expected FormatComparisonReport to display the #include '
                'chain for newly added headers.'),
        )

    def testFormatComparisonReportRendersPositivePercentageWithPlusSign(self):
        report = compile_size_probe.ProbeComparisonReport(
            baseline_name='before',
            before_total_bytes=1000,
            after_total_bytes=1500,
            delta_total_bytes=500,
            translation_units=(
                compile_size_probe.TranslationUnitComparison(
                    translation_unit='gen/net/cert/duration.pb.cc',
                    before_bytes=1000,
                    after_bytes=1500,
                    delta_bytes=500,
                    added_headers=(),
                    removed_headers=(),
                    resized_files=(),
                ),
            ),
        )

        formatted_report = compile_size_probe.FormatComparisonReport(report)

        self.assertIn(
            '(+50.00%, 1,000 B -> 1,500 B)',
            formatted_report,
            msg=(
                'Expected FormatComparisonReport to prefix positive '
                'percentage deltas with "+".'),
        )

    def testFormatComparisonReportRendersRemovedResizedAndUnchangedSections(
            self):
        report = compile_size_probe.ProbeComparisonReport(
            baseline_name='before',
            before_total_bytes=2000,
            after_total_bytes=1800,
            delta_total_bytes=-200,
            translation_units=(
                compile_size_probe.TranslationUnitComparison(
                    translation_unit='gen/net/cert/duration.pb.cc',
                    before_bytes=1500,
                    after_bytes=1300,
                    delta_bytes=-200,
                    added_headers=(),
                    removed_headers=(
                        compile_size_probe.RemovedHeaderDelta(
                            path='third_party/protobuf/src/old_header.h',
                            size_bytes=300,
                        ),
                    ),
                    resized_files=(
                        compile_size_probe.ResizedFileDelta(
                            path='gen/net/cert/duration.pb.h',
                            before_bytes=1200,
                            after_bytes=1300,
                            delta_bytes=100,
                        ),
                    ),
                ),
                compile_size_probe.TranslationUnitComparison(
                    translation_unit=(
                        'components/sync/model/processor_entity.cc'),
                    before_bytes=500,
                    after_bytes=500,
                    delta_bytes=0,
                    added_headers=(),
                    removed_headers=(),
                    resized_files=(),
                ),
            ),
        )

        formatted_report = compile_size_probe.FormatComparisonReport(report)

        self.assertEqual(
            (
                'Compile-size comparison against baseline "before":\n'
                'Total across 2 translation units: 1.95 KiB (2,000 B) -> '
                '1.76 KiB (1,800 B) (-200 B)\n\n'
                '=== gen/net/cert/duration.pb.cc: -200 B '
                '(-13.33%, 1,500 B -> 1,300 B) ===\n'
                '  Removed headers (1):\n'
                '    -300 B  third_party/protobuf/src/old_header.h\n'
                '  Resized files (1):\n'
                '    +100 B  gen/net/cert/duration.pb.h '
                '(1,200 B -> 1,300 B)\n\n'
                '=== components/sync/model/processor_entity.cc: 0 B '
                '(0.00%, 500 B -> 500 B) ===\n'
                '  No header changes.'
            ),
            formatted_report,
            msg=(
                'Expected FormatComparisonReport to render Removed headers, '
                'Resized files, and "No header changes." sections.'),
        )

    def testFormatHumanByteSizePrefixesPositiveDeltaWithPlusSign(self):
        formatted_size = compile_size_probe.FormatHumanByteSize(
            2048, should_include_sign=True)

        self.assertEqual(
            '+2.00 KiB (+2,048 B)',
            formatted_size,
            msg=(
                'Expected FormatHumanByteSize(2048, should_include_sign=True) '
                'to prefix both the KiB value and raw byte count with "+".'),
        )

    def testFormatHumanByteSizePrefixesNegativeDeltaWithMinusSign(self):
        formatted_size = compile_size_probe.FormatHumanByteSize(
            -2048, should_include_sign=True)

        self.assertEqual(
            '-2.00 KiB (-2,048 B)',
            formatted_size,
            msg=(
                'Expected FormatHumanByteSize(-2048, should_include_sign=True) '
                'to prefix both the KiB value and raw byte count with "-".'),
        )

    def testCompareProbeBaselinesRaisesValueErrorForMissingUnitInBeforeBaseline(
            self):
        before_baseline = compile_size_probe.ProbeBaseline(translation_units=())
        after_baseline = compile_size_probe.ProbeBaseline(
            translation_units=(
                compile_size_probe.TranslationUnitSnapshot(
                    translation_unit='gen/net/cert/duration.pb.cc',
                    total_bytes=100,
                    included_files={'gen/net/cert/duration.pb.cc': 100},
                    include_chains={},
                ),
            ))

        with self.assertRaises(
                ValueError,
                msg=(
                    'Expected CompareProbeBaselines to raise ValueError when '
                    'an after_baseline unit is absent from before_baseline.')):
            compile_size_probe.CompareProbeBaselines(
                baseline_name='before',
                before_baseline=before_baseline,
                after_baseline=after_baseline,
            )

    def testSelectTranslationUnitsToMeasureReturnsSavedBaselineUnitsForCompare(
            self):
        before_baseline = compile_size_probe.ProbeBaseline(
            translation_units=(
                compile_size_probe.TranslationUnitSnapshot(
                    translation_unit='components/custom/custom_target.cc',
                    total_bytes=100,
                    included_files={'components/custom/custom_target.cc': 100},
                    include_chains={},
                ),
            ))
        parsed_arguments = compile_size_probe._ParseCommandLineArguments([
            '-C',
            'out/Default',
            '--compare',
            'before',
        ])

        selected_units = compile_size_probe._SelectTranslationUnitsToMeasure(
            parsed_arguments=parsed_arguments,
            build_directory='/out/Default',
            repository_root='/src',
            before_baseline=before_baseline,
        )

        self.assertEqual(
            ['components/custom/custom_target.cc'],
            selected_units,
            msg=(
                'Expected _SelectTranslationUnitsToMeasure to return the '
                'saved baseline translation units when --compare is invoked '
                'without --tu.'),
        )

    def testSelectTranslationUnitsToMeasureRejectsUnknownUnitDuringCompare(
            self):
        before_baseline = compile_size_probe.ProbeBaseline(
            translation_units=(
                compile_size_probe.TranslationUnitSnapshot(
                    translation_unit='gen/net/cert/duration.pb.cc',
                    total_bytes=100,
                    included_files={'gen/net/cert/duration.pb.cc': 100},
                    include_chains={},
                ),
            ))
        parsed_arguments = compile_size_probe._ParseCommandLineArguments([
            '-C',
            'out/Default',
            '--compare',
            'before',
            '--tu',
            'components/sync/model/processor_entity.cc',
        ])

        with self.assertRaises(
                ValueError,
                msg=(
                    'Expected _SelectTranslationUnitsToMeasure to raise '
                    'ValueError before measurement when --compare is passed '
                    'a --tu path absent from the saved baseline.')):
            compile_size_probe._SelectTranslationUnitsToMeasure(
                parsed_arguments=parsed_arguments,
                build_directory='/out/Default',
                repository_root='/src',
                before_baseline=before_baseline,
            )

    def testSelectTranslationUnitsToMeasureDeduplicatesExtraUnitsDuringCompare(
            self):
        before_baseline = compile_size_probe.ProbeBaseline(
            translation_units=(
                compile_size_probe.TranslationUnitSnapshot(
                    translation_unit=(
                        'components/sync/model/processor_entity.cc'),
                    total_bytes=100,
                    included_files={
                        'components/sync/model/processor_entity.cc': 100,
                    },
                    include_chains={},
                ),
            ))
        parsed_arguments = compile_size_probe._ParseCommandLineArguments([
            '-C',
            'out/Default',
            '--compare',
            'before',
            '--tu',
            'components/sync/model/processor_entity.cc',
            '--tu',
            '//components/sync/model/processor_entity.cc',
        ])
        selected_units = compile_size_probe._SelectTranslationUnitsToMeasure(
            parsed_arguments=parsed_arguments,
            build_directory='/out/Default',
            repository_root='/src',
            before_baseline=before_baseline,
        )

        self.assertEqual(
            ['components/sync/model/processor_entity.cc'],
            selected_units,
            msg=(
                'Expected _SelectTranslationUnitsToMeasure to deduplicate '
                'repeated --tu arguments during --compare.'),
        )

    @_SKIP_ON_WINDOWS
    def testCommandLineSaveAndCompareProducesMachineReadableJsonDiff(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_directory = os.path.join(
                temporary_directory, 'out', 'Default')
            _CreateSyntheticNinjaWorkspace(
                temporary_directory,
                build_directory,
                compile_size_probe.DEFAULT_TRANSLATION_UNITS,
            )
            extension_set_header = os.path.join(
                temporary_directory,
                'third_party',
                'protobuf',
                'src',
                'google',
                'protobuf',
                'extension_set.h',
            )
            btree_map_header = os.path.join(
                temporary_directory,
                'third_party',
                'abseil-cpp',
                'absl',
                'container',
                'btree_map.h',
            )
            _WriteFileWithByteSize(extension_set_header, 2000)
            _WriteFileWithByteSize(btree_map_header, 34668)

            mode_file_path = os.path.join(temporary_directory, 'mode.txt')
            with open(mode_file_path, 'w', encoding='utf-8') as file_handle:
                file_handle.write('before')
            fake_clang_path = _WriteFakeClangScript(
                os.path.join(temporary_directory, 'fake_clang.py'),
                mode_file_path,
            )

            with (
                mock.patch.object(
                    compile_size_probe,
                    '_DEFAULT_REPOSITORY_ROOT',
                    temporary_directory,
                ),
                mock.patch.object(
                    compile_size_probe,
                    '_ResolveDefaultClangBinary',
                    return_value=fake_clang_path,
                ),
                mock.patch('sys.stdout', io.StringIO()),
            ):
                compile_size_probe.main([
                    '-C',
                    build_directory,
                    '--no-build',
                    '--save',
                    'before',
                ])

            _WriteFileWithByteSize(extension_set_header, 2500)
            with open(mode_file_path, 'w', encoding='utf-8') as file_handle:
                file_handle.write('after')

            compare_stdout = io.StringIO()
            with (
                mock.patch.object(
                    compile_size_probe,
                    '_DEFAULT_REPOSITORY_ROOT',
                    temporary_directory,
                ),
                mock.patch.object(
                    compile_size_probe,
                    '_ResolveDefaultClangBinary',
                    return_value=fake_clang_path,
                ),
                mock.patch('sys.stdout', compare_stdout),
            ):
                compile_size_probe.main([
                    '-C',
                    build_directory,
                    '--no-build',
                    '--compare',
                    'before',
                    '--json',
                ])
            parsed_report = json.loads(compare_stdout.getvalue())

        first_unit = parsed_report['translation_units'][0]
        self.assertEqual(
            [{
                'path': 'third_party/abseil-cpp/absl/container/btree_map.h',
                'size_bytes': 34668,
                'include_chain': [
                    'gen/net/cert/root_store_proto_lite/duration.pb.cc',
                    (
                        'third_party/protobuf/src/google/protobuf/'
                        'extension_set.h'
                    ),
                    'third_party/abseil-cpp/absl/container/btree_map.h',
                ],
            }],
            first_unit['added_headers'],
            msg=(
                'Expected --compare --json to report btree_map.h as an added '
                'header with its include chain through extension_set.h.'),
        )

    def testMainReturnsErrorWhenComparedBaselineJsonIsMalformed(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            baseline_directory = (
                compile_size_probe.ResolveBaselineStorageDirectory(
                    temporary_directory))
            os.makedirs(baseline_directory, exist_ok=True)
            broken_path = compile_size_probe.BaselineFilePath(
                baseline_directory, 'broken')
            with open(broken_path, 'w', encoding='utf-8') as file_handle:
                file_handle.write('[]\n')
            captured_stderr = io.StringIO()
            with (
                mock.patch.object(sys, 'platform', 'linux'),
                mock.patch(
                    'compile_size_probe.MeasureProbeBaseline',
                    side_effect=AssertionError(
                        'MeasureProbeBaseline should not run when the '
                        'compared baseline JSON is malformed.'),
                ),
                mock.patch('sys.stderr', captured_stderr),
            ):
                exit_code = compile_size_probe.main([
                    '-C',
                    temporary_directory,
                    '--compare',
                    'broken',
                ])

        self.assertEqual(
            (1, True),
            (
                exit_code,
                'Malformed baseline JSON' in captured_stderr.getvalue(),
            ),
            msg=(
                'Expected main --compare to exit 1 with a Malformed baseline '
                'JSON error on stderr instead of an uncaught TypeError.'),
        )


if __name__ == '__main__':
    unittest.main()
