#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Measures and compares compiler input sizes and include chains for protobuf.

Chromium's `compile-size` trybot measures the total byte size of every source
file and `#include`d header compiled by a translation unit, using the metric in
`//tools/clang/scripts/compiler_inputs_size.py`. During a
`//third_party/protobuf` roll or header change, a single new `#include` inside a
widely included protobuf runtime header or a code generator expansion in
`protoc` can add hundreds of megabytes across the build, while the bot only
reports one aggregate delta.

When saving or measuring without a baseline, this standalone CLI probes three
representative translation units (plus any extra translation units passed via
`--tu`); when comparing against a saved baseline via `--compare`, it probes the
baseline's translation units (or the subset selected via `--tu`) in a single
`clang++ -M -H` pass per file:

1. `gen/net/cert/root_store_proto_lite/duration.pb.cc`: A tiny generated
   message dominated by protobuf runtime headers, Abseil, and libc++, isolating
   constant per-file header weight.
2. `gen/components/sync/protocol/sync_entity.pb.cc`: A large generated Chrome
   Sync message where roughly half of the compiler input is generated `.pb.h`
   code, exposing `protoc` codegen growth.
3. `components/sync/model/processor_entity.cc`: An ordinary hand-written C++
   translation unit that `#include`s large generated protobuf headers.

Because `compiler_inputs_size.py` relies on `system_headers_in_deps=true`
(which passes `-MD` instead of `-MMD` in `//build/toolchain/gcc_toolchain.gni`)
to include system headers in `.ninja_deps`, whereas standard developer builds
leave `system_headers_in_deps=false`, this script invokes `clang++` directly
with `-M -H` and the target's Ninja compile flags (`defines`, `include_dirs`,
`cflags`, `cflags_cc`, `module_deps`, and `cc_module_name`). `-M` outputs every
dependency including system headers and module maps on `stdout` (any `.pcm`
module cache paths are excluded), and `-H` outputs the `#include` tree on
`stderr` so newly added headers can be traced back to the exact header that
pulled them in.

See https://crbug.com/568074904 for full background.
"""

from __future__ import annotations

import argparse
from collections.abc import Mapping, Sequence
import dataclasses
import json
import os
import pathlib
import re
import shlex
import shutil
import subprocess
import sys
from typing import Any

_THIS_DIRECTORY = os.path.abspath(os.path.dirname(__file__))
_DEFAULT_REPOSITORY_ROOT = os.path.abspath(
    os.path.join(_THIS_DIRECTORY, '..', '..'))
_DEFAULT_CLANG_RELATIVE_PATH = (
    'third_party/llvm-build/Release+Asserts/bin/clang++')
_DEPOT_TOOLS_AUTONINJA_RELATIVE_PATH = 'third_party/depot_tools/autoninja'
_BASELINE_SUBDIRECTORY_NAME = '.compile_size_probe'

DEFAULT_TRANSLATION_UNITS: tuple[str, ...] = (
    'gen/net/cert/root_store_proto_lite/duration.pb.cc',
    'gen/components/sync/protocol/sync_entity.pb.cc',
    'components/sync/model/processor_entity.cc',
)

_TOP_LEVEL_VARIABLE_PATTERN = re.compile(
    r'^([A-Za-z0-9_]+)[ \t]*=[ \t]*(.*)$', re.MULTILINE)
_INDENTED_VARIABLE_PATTERN = re.compile(
    r'^[ \t]+([A-Za-z0-9_]+)[ \t]*=[ \t]*(.*)$')
_BUILD_CXX_LINE_PATTERN = re.compile(
    r'^build\s+(\S+)\s*:\s*\S*cxx\s+(\S+)(.*)$')
_MODULE_FILE_FLAG_PATTERN = re.compile(r'-fmodule-file=(?:[^=\s]+=)?([^\s]+)')
_CLANG_HEADER_TRACE_LINE_PATTERN = re.compile(r'^(\.+)\s+(.+)$')
_SAFE_BASELINE_NAME_PATTERN = re.compile(r'^[A-Za-z0-9_.-]+$')
_BASELINE_FORMAT_VERSION = 1


@dataclasses.dataclass(frozen=True)
class NinjaCompileRule:
    """Represents the resolved Ninja `cxx` build edge for a translation unit.

    Attributes:
      translation_unit: Normalized translation unit path (such as
        `gen/net/cert/root_store_proto_lite/duration.pb.cc` or
        `components/sync/model/processor_entity.cc`).
      ninja_source_path: Source path as written in the `.ninja` build edge,
        relative to `build_directory`.
      object_target: Output `.o` target path relative to `build_directory`.
      ninja_file_path: Path to the `.ninja` file defining the build edge.
      variables: Unescaped Ninja variable bindings (`defines`, `include_dirs`,
        `cflags`, `cflags_cc`, `module_deps`, and related target variables).
      prerequisite_targets: Build targets (generated source plus implicit and
        order-only dependencies on the `build` line) that must be up to date
        before running `clang++ -M -H` on `ninja_source_path`.
    """

    translation_unit: str
    ninja_source_path: str
    object_target: str
    ninja_file_path: str
    variables: Mapping[str, str]
    prerequisite_targets: tuple[str, ...] = ()


@dataclasses.dataclass(frozen=True)
class TranslationUnitSnapshot:
    """Stores measured compiler input sizes and include chains for one unit.

    Attributes:
      translation_unit: Normalized path of the measured translation unit.
      total_bytes: Total compiler input size in bytes (the translation unit's
        own source file plus all unique `#include`d files, excluding `.pcm`).
      included_files: Mapping from normalized file path to its byte size.
      include_chains: Mapping from normalized header path to the tuple of
        normalized paths forming the first `#include` chain from
        `translation_unit` to that header. Files that clang only probes
        (`__has_include`) or reads as module maps appear in
        `included_files` but have no chain.
    """

    translation_unit: str
    total_bytes: int
    included_files: Mapping[str, int]
    include_chains: Mapping[str, tuple[str, ...]]

    def ToDict(self) -> dict[str, Any]:
        """Serializes this snapshot to a JSON-compatible dictionary."""
        return {
            'translation_unit': self.translation_unit,
            'total_bytes': self.total_bytes,
            'included_files': dict(sorted(self.included_files.items())),
            'include_chains': {
                header_path: list(chain)
                for header_path, chain in sorted(self.include_chains.items())
            },
        }

    @classmethod
    def FromDict(cls, raw_dictionary: Mapping[str, Any]) -> (
            TranslationUnitSnapshot):
        """Reconstructs a TranslationUnitSnapshot from a parsed JSON dict."""
        included_files = {
            str(file_path): int(byte_size)
            for file_path, byte_size in raw_dictionary['included_files'].items()
        }
        include_chains = {
            str(header_path): tuple(str(element) for element in chain)
            for header_path, chain in raw_dictionary.get(
                'include_chains', {}).items()
        }
        return cls(
            translation_unit=str(raw_dictionary['translation_unit']),
            total_bytes=int(raw_dictionary['total_bytes']),
            included_files=included_files,
            include_chains=include_chains,
        )


@dataclasses.dataclass(frozen=True)
class ProbeBaseline:
    """Holds snapshots for all translation units in a probe run.

    Attributes:
      translation_units: Ordered tuple of per-unit snapshots captured in the
        probe run.
    """

    translation_units: tuple[TranslationUnitSnapshot, ...]

    def TotalBytes(self) -> int:
        """Returns the sum of `total_bytes` across all translation units."""
        return sum(unit.total_bytes for unit in self.translation_units)

    def ByTranslationUnit(self) -> dict[str, TranslationUnitSnapshot]:
        """Returns snapshots indexed by `translation_unit` path."""
        return {
            unit.translation_unit: unit for unit in self.translation_units
        }

    def ToDict(self) -> dict[str, Any]:
        """Serializes this baseline to a JSON-compatible dictionary."""
        return {
            'format_version': _BASELINE_FORMAT_VERSION,
            'total_bytes': self.TotalBytes(),
            'translation_units': [
                unit.ToDict() for unit in self.translation_units
            ],
        }

    @classmethod
    def FromDict(cls, raw_dictionary: Mapping[str, Any]) -> ProbeBaseline:
        """Reconstructs a ProbeBaseline from a parsed JSON dictionary."""
        if not isinstance(raw_dictionary, Mapping):
            raise ValueError(
                'Malformed baseline JSON: top-level value must be an object.')
        format_version = raw_dictionary.get('format_version')
        if format_version != _BASELINE_FORMAT_VERSION:
            raise ValueError(
                f'Unsupported baseline format_version {format_version!r}; '
                f'expected {_BASELINE_FORMAT_VERSION}.')
        try:
            raw_units = raw_dictionary['translation_units']
            if not isinstance(raw_units, list):
                raise TypeError('translation_units must be a list.')
            snapshots = tuple(
                TranslationUnitSnapshot.FromDict(entry) for entry in raw_units
            )
        except (KeyError, TypeError, AttributeError, ValueError) as error:
            raise ValueError(
                f'Malformed baseline JSON ({error}); re-run with '
                f'--save NAME --overwrite.') from error
        return cls(translation_units=snapshots)


@dataclasses.dataclass(frozen=True)
class AddedHeaderDelta:
    """Describes a newly `#include`d header in a compared translation unit.

    Attributes:
      path: Normalized path of the newly included header.
      size_bytes: On-disk size of the newly included header in bytes.
      include_chain: Tuple of normalized paths from the translation unit to
        `path` showing how the header was pulled in.
    """

    path: str
    size_bytes: int
    include_chain: tuple[str, ...]


@dataclasses.dataclass(frozen=True)
class RemovedHeaderDelta:
    """Describes a header that is no longer `#include`d after a change.

    Attributes:
      path: Normalized path of the removed header.
      size_bytes: Baseline byte size of the removed header.
    """

    path: str
    size_bytes: int


@dataclasses.dataclass(frozen=True)
class ResizedFileDelta:
    """Describes a header or source file whose byte size changed.

    Attributes:
      path: Normalized path of the modified header or source file.
      before_bytes: Byte size in the saved baseline.
      after_bytes: Byte size in the current measurement.
      delta_bytes: Signed byte difference (`after_bytes - before_bytes`).
    """

    path: str
    before_bytes: int
    after_bytes: int
    delta_bytes: int


@dataclasses.dataclass(frozen=True)
class TranslationUnitComparison:
    """Holds the before/after comparison for a single translation unit.

    Attributes:
      translation_unit: Normalized path of the compared translation unit.
      before_bytes: Total compiler input bytes in the saved baseline.
      after_bytes: Total compiler input bytes in the current measurement.
      delta_bytes: Signed byte difference (`after_bytes - before_bytes`).
      added_headers: Newly included headers sorted by `size_bytes` descending.
      removed_headers: Removed headers sorted by `size_bytes` descending.
      resized_files: Modified files sorted by `abs(delta_bytes)` descending.
    """

    translation_unit: str
    before_bytes: int
    after_bytes: int
    delta_bytes: int
    added_headers: tuple[AddedHeaderDelta, ...]
    removed_headers: tuple[RemovedHeaderDelta, ...]
    resized_files: tuple[ResizedFileDelta, ...]


@dataclasses.dataclass(frozen=True)
class ProbeComparisonReport:
    """Holds the comparison between a saved baseline and a fresh run.

    Attributes:
      baseline_name: Name of the saved baseline compared against.
      before_total_bytes: Sum of `before_bytes` across compared units.
      after_total_bytes: Sum of `after_bytes` across compared units.
      delta_total_bytes: Signed difference (`after_total_bytes -
        before_total_bytes`).
      translation_units: Per-unit comparison results in probe order.
    """

    baseline_name: str
    before_total_bytes: int
    after_total_bytes: int
    delta_total_bytes: int
    translation_units: tuple[TranslationUnitComparison, ...]

    def ToDict(self) -> dict[str, Any]:
        """Serializes this comparison report to a JSON-compatible dict."""
        return json.loads(json.dumps(dataclasses.asdict(self)))


def UnescapeNinjaValue(raw_value: str) -> str:
    """Unescapes Ninja `$:`, `$ `, and `$$` sequences in a variable binding."""
    return raw_value.replace('$:', ':').replace('$ ', ' ').replace('$$', '$')


def _IsPathWithinDirectory(candidate_path: str, directory_path: str) -> bool:
    """Returns True if candidate_path is inside directory_path."""
    try:
        return os.path.commonpath(
            [candidate_path, directory_path]) == directory_path
    except ValueError:
        return False


def NormalizeTranslationUnitPath(
        raw_path: str,
        build_directory: str,
        repository_root: str) -> str:
    """Normalizes a user-supplied `--tu` path to a canonical probe path.

    Paths starting with `gen/` or pointing inside `<build_directory>/gen/...`
    normalize to `gen/...` (relative to `build_directory`). Other relative
    paths (with or without a leading `//`) are resolved against
    `repository_root` (`src/`) and normalized to repository-root-relative
    forward-slash paths.
    """
    cleaned_path = raw_path.strip().removeprefix('//').replace(os.sep, '/')
    if cleaned_path.startswith('gen/'):
        return os.path.normpath(cleaned_path).replace(os.sep, '/')

    absolute_build_directory = os.path.abspath(build_directory)
    absolute_repository_root = os.path.abspath(repository_root)
    candidate_path = os.path.abspath(
        os.path.join(absolute_repository_root, cleaned_path))

    if _IsPathWithinDirectory(candidate_path, absolute_build_directory):
        return os.path.relpath(
            candidate_path, absolute_build_directory).replace(os.sep, '/')
    if _IsPathWithinDirectory(candidate_path, absolute_repository_root):
        return os.path.relpath(
            candidate_path, absolute_repository_root).replace(os.sep, '/')
    return os.path.normpath(cleaned_path).replace(os.sep, '/')


def MergeTranslationUnits(
        default_units: Sequence[str],
        extra_units: Sequence[str]) -> list[str]:
    """Combines default and extra translation units without duplicates."""
    merged_units: list[str] = []
    seen_units: set[str] = set()
    for translation_unit in (*default_units, *extra_units):
        if translation_unit not in seen_units:
            seen_units.add(translation_unit)
            merged_units.append(translation_unit)
    return merged_units


def ResolveSourceAbsolutePath(
        translation_unit: str,
        build_directory: str,
        repository_root: str) -> str:
    """Returns the absolute path for a normalized translation unit."""
    if translation_unit.startswith('gen/'):
        return os.path.abspath(os.path.join(build_directory, translation_unit))
    return os.path.abspath(os.path.join(repository_root, translation_unit))


def ComputeNinjaSourcePath(
        translation_unit: str,
        build_directory: str,
        repository_root: str) -> str:
    """Computes the source path relative to `build_directory` used by Ninja."""
    source_absolute_path = ResolveSourceAbsolutePath(
        translation_unit, build_directory, repository_root)
    absolute_build_directory = os.path.abspath(build_directory)
    return os.path.relpath(
        source_absolute_path, absolute_build_directory).replace(os.sep, '/')


def _ParseIndentedTargetVariables(
        lines: Sequence[str], start_index: int) -> dict[str, str]:
    """Parses indented `key = value` overrides following a Ninja build line."""
    overrides: dict[str, str] = {}
    for line in lines[start_index:]:
        if not line.startswith((' ', '\t')):
            break
        indented_match = _INDENTED_VARIABLE_PATTERN.match(line)
        if indented_match:
            overrides[indented_match.group(1)] = UnescapeNinjaValue(
                indented_match.group(2).strip())
    return overrides


def _ParseEdgePrerequisiteTargets(
        source_in_rule: str, edge_tail: str) -> tuple[str, ...]:
    """Returns generated source plus implicit/order-only inputs on the edge."""
    raw_tokens = re.split(r'(?<!\$)\s+', edge_tail.strip())
    dependency_targets = [
        UnescapeNinjaValue(token)
        for token in raw_tokens
        if token and token not in ('|', '||', '|@')
    ]
    generated_sources = (
        [source_in_rule] if source_in_rule.startswith('gen/') else [])
    return tuple(MergeTranslationUnits(generated_sources, dependency_targets))


def ParseNinjaCompileRuleFromText(
        ninja_text: str,
        ninja_file_path: str,
        translation_unit: str,
        expected_ninja_source: str) -> NinjaCompileRule | None:
    """Parses a `.ninja` file's text to find the `cxx` rule for a source file.

    Returns None if `ninja_text` does not contain a `build ...: cxx` edge for
    `expected_ninja_source`.
    """
    lines = ninja_text.splitlines()
    for line_index, line in enumerate(lines):
        if not line.startswith('build '):
            continue
        build_match = _BUILD_CXX_LINE_PATTERN.match(line)
        if build_match is None:
            continue
        object_target = UnescapeNinjaValue(build_match.group(1))
        source_in_rule = UnescapeNinjaValue(build_match.group(2))
        if source_in_rule != expected_ninja_source:
            continue

        prerequisite_targets = _ParseEdgePrerequisiteTargets(
            source_in_rule, build_match.group(3))
        variables = {
            match.group(1): UnescapeNinjaValue(match.group(2).strip())
            for match in _TOP_LEVEL_VARIABLE_PATTERN.finditer(ninja_text)
        }
        variables.update(_ParseIndentedTargetVariables(lines, line_index + 1))
        return NinjaCompileRule(
            translation_unit=translation_unit,
            ninja_source_path=source_in_rule,
            object_target=object_target,
            ninja_file_path=ninja_file_path,
            variables=variables,
            prerequisite_targets=prerequisite_targets,
        )
    return None


def _CandidateObjectAncestorDirectories(
        build_directory: str, translation_unit: str) -> list[str]:
    """Lists `<build_directory>/obj/<ancestor>` directories from inner to outer.

    GN writes per-target `.ninja` files under `<build_directory>/obj/<dir>/`
    matching the target's directory. Walking from the translation unit's parent
    directory up to `obj/` locates the `.ninja` file in milliseconds without
    scanning the full `obj/` tree.
    """
    relative_without_gen = translation_unit.removeprefix('gen/')
    parent_directory = os.path.dirname(relative_without_gen)
    object_root = os.path.join(build_directory, 'obj')

    ancestor_directories: list[str] = []
    current_relative = parent_directory
    while current_relative:
        candidate = os.path.join(object_root, current_relative)
        if os.path.isdir(candidate):
            ancestor_directories.append(candidate)
        current_relative = os.path.dirname(current_relative)

    if os.path.isdir(object_root):
        ancestor_directories.append(object_root)
    return ancestor_directories


def _TryParseNinjaFile(
        ninja_file_path: str,
        translation_unit: str,
        expected_ninja_source: str) -> NinjaCompileRule | None:
    """Reads `ninja_file_path` and parses the rule if it mentions the source."""
    try:
        with open(
                ninja_file_path,
                'r',
                encoding='utf-8',
                errors='surrogateescape') as file_handle:
            ninja_text = file_handle.read()
    except OSError:
        return None
    if os.path.basename(expected_ninja_source) not in ninja_text:
        return None
    return ParseNinjaCompileRuleFromText(
        ninja_text=ninja_text,
        ninja_file_path=ninja_file_path,
        translation_unit=translation_unit,
        expected_ninja_source=expected_ninja_source,
    )


def _SearchNinjaFilesInDirectory(
        directory_path: str,
        translation_unit: str,
        expected_ninja_source: str) -> NinjaCompileRule | None:
    """Searches `*.ninja` files directly inside `directory_path`."""
    try:
        entries = sorted(os.listdir(directory_path))
    except OSError:
        return None

    for entry_name in entries:
        if not entry_name.endswith('.ninja'):
            continue
        rule = _TryParseNinjaFile(
            os.path.join(directory_path, entry_name),
            translation_unit,
            expected_ninja_source,
        )
        if rule is not None:
            return rule
    return None


def _WalkAllObjectNinjaFiles(object_root: str) -> list[str]:
    """Collects all `*.ninja` file paths under `object_root`."""
    ninja_files: list[str] = []
    for current_root, _, filenames in os.walk(object_root):
        for filename in sorted(filenames):
            if filename.endswith('.ninja'):
                ninja_files.append(os.path.join(current_root, filename))
    return ninja_files


def FindNinjaCompileRule(
        build_directory: str,
        repository_root: str,
        translation_unit: str) -> NinjaCompileRule:
    """Locates the NinjaCompileRule for `translation_unit` in `build_directory`.

    Searches `.ninja` files under `<build_directory>/obj` in three stages:
      1. `<build_directory>/obj/<source_dir>/*.ninja` (matching the source
         file's directory).
      2. Ancestor directories walking up toward `<build_directory>/obj`.
      3. A full `os.walk` across `<build_directory>/obj` if no ancestor
         directory defined the compile edge.

    Raises:
      RuntimeError: If no `.ninja` file under `<build_directory>/obj` compiles
        `translation_unit`.
    """
    ninja_source_path = ComputeNinjaSourcePath(
        translation_unit, build_directory, repository_root)
    for ancestor_directory in _CandidateObjectAncestorDirectories(
            build_directory, translation_unit):
        rule = _SearchNinjaFilesInDirectory(
            ancestor_directory, translation_unit, ninja_source_path)
        if rule is not None:
            return rule

    object_root = os.path.join(build_directory, 'obj')
    for ninja_file_path in _WalkAllObjectNinjaFiles(object_root):
        rule = _TryParseNinjaFile(
            ninja_file_path, translation_unit, ninja_source_path)
        if rule is not None:
            return rule

    raise RuntimeError(
        f'Could not find a Ninja cxx build rule for "{translation_unit}" '
        f'(looked for "{ninja_source_path}" under {build_directory}/obj).')


def _AreAllModuleArtifactsPresent(
        build_directory: str, module_deps_flags: str) -> bool:
    """Returns True if every `.pcm` referenced in `module_deps_flags` exists."""
    for pcm_relative_path in _MODULE_FILE_FLAG_PATTERN.findall(
            module_deps_flags):
        pcm_full_path = os.path.join(build_directory, pcm_relative_path)
        if not os.path.isfile(pcm_full_path):
            return False
    return True


def _CollectTargetCompilerFlags(
        compile_rule: NinjaCompileRule,
        build_directory: str) -> list[str]:
    """Extracts the split compiler flags for `compile_rule`."""
    variables = compile_rule.variables
    include_dirs = variables.get('include_dirs', '')

    module_deps = variables.get('module_deps', '')
    if module_deps and not _AreAllModuleArtifactsPresent(
            build_directory, module_deps):
        raise RuntimeError(
            f'{compile_rule.translation_unit} needs prebuilt Clang modules '
            f'that are missing under {build_directory}; rerun without '
            f'--no-build so autoninja builds them.')

    cc_module_name = variables.get('cc_module_name', '')
    module_name_flag = (
        f'-fmodule-name={shlex.quote(f"{cc_module_name}_Private")}'
        if cc_module_name else ''
    )

    flag_sections = [
        variables.get('defines', ''),
        include_dirs,
        variables.get('cflags', ''),
        variables.get('cflags_cc', ''),
        module_deps,
        module_name_flag,
    ]
    combined_flags = ' '.join(
        section for section in flag_sections if section)
    return shlex.split(combined_flags)


def BuildClangProbeCommand(
        compile_rule: NinjaCompileRule,
        build_directory: str,
        clang_binary: str) -> list[str]:
    """Constructs the `clang++ -M -H` argument vector for `compile_rule`.

    Requires all `.pcm` files referenced in `module_deps` to exist in
    `build_directory` so that module-enabled builds resolve the same prebuilt
    modules as the build.
    """
    compiler_flags = _CollectTargetCompilerFlags(compile_rule, build_directory)
    return (
        [clang_binary]
        + compiler_flags
        + ['-M', '-H', compile_rule.ninja_source_path]
    )


def _IsPrebuiltModuleArtifact(file_path: str) -> bool:
    """Returns True if `file_path` is a precompiled Clang module (`.pcm`).

    `.pcm` files are binary module caches rather than source inputs, so they
    are excluded from compiler input byte totals just as in
    `//tools/clang/scripts/compiler_inputs_size.py`.
    """
    return file_path.endswith('.pcm')


def NormalizeDependencyFilePath(
        raw_dependency_path: str,
        build_directory: str,
        repository_root: str) -> tuple[str, str]:
    """Resolves a dependency path and returns `(normalized_path, disk_path)`."""
    resolved_path = pathlib.Path(
        os.path.join(build_directory, raw_dependency_path)).resolve()
    disk_path = str(resolved_path)

    absolute_build_directory = str(pathlib.Path(build_directory).resolve())
    if _IsPathWithinDirectory(disk_path, absolute_build_directory):
        relative_to_build = os.path.relpath(
            disk_path, absolute_build_directory).replace(os.sep, '/')
        return relative_to_build, disk_path

    absolute_repository_root = str(pathlib.Path(repository_root).resolve())
    if _IsPathWithinDirectory(disk_path, absolute_repository_root):
        relative_to_repo = os.path.relpath(
            disk_path, absolute_repository_root).replace(os.sep, '/')
        return relative_to_repo, disk_path

    return disk_path.replace(os.sep, '/'), disk_path


def ParseMakefileDependenciesOutput(
        makefile_stdout: str,
        build_directory: str,
        repository_root: str) -> dict[str, str]:
    """Parses `clang++ -M` stdout into `{normalized_path: disk_path}`.

    Handles backslash-newline continuations and escaped spaces in Makefile
    rules, excluding `.pcm` files to match `compiler_inputs_size.py`.
    """
    unwrapped_text = makefile_stdout.replace('\\\r\n', ' ').replace(
        '\\\n', ' ')
    if ':' not in unwrapped_text:
        raise ValueError(
            f'Unexpected clang -M output (missing ":"): {makefile_stdout!r}')
    _, dependencies_section = unwrapped_text.split(':', 1)

    normalized_to_disk_path: dict[str, str] = {}
    for raw_token in shlex.split(dependencies_section):
        if _IsPrebuiltModuleArtifact(raw_token):
            continue
        normalized_path, disk_path = NormalizeDependencyFilePath(
            raw_dependency_path=raw_token,
            build_directory=build_directory,
            repository_root=repository_root,
        )
        normalized_to_disk_path[normalized_path] = disk_path
    return normalized_to_disk_path


def ParseHeaderIncludeChains(
        header_trace_stderr: str,
        translation_unit: str,
        build_directory: str,
        repository_root: str) -> dict[str, tuple[str, ...]]:
    """Parses `clang++ -H` stderr into `{header_path: include_chain_tuple}`.

    Clang prints one line per `#include` transition prefixed by dots indicating
    the nesting depth (`.` for depth 1, `..` for depth 2, and so on). Records
    the first `#include` chain by which each header is reached from
    `translation_unit`.
    """
    include_stack: list[str] = [translation_unit]
    include_chains: dict[str, tuple[str, ...]] = {
        translation_unit: (translation_unit,),
    }

    for raw_line in header_trace_stderr.splitlines():
        line_match = _CLANG_HEADER_TRACE_LINE_PATTERN.match(raw_line)
        if line_match is None:
            continue
        depth = len(line_match.group(1))
        raw_header_path = line_match.group(2).strip()
        if _IsPrebuiltModuleArtifact(raw_header_path):
            continue

        normalized_header, _ = NormalizeDependencyFilePath(
            raw_dependency_path=raw_header_path,
            build_directory=build_directory,
            repository_root=repository_root,
        )
        include_stack = include_stack[:depth]
        include_stack.append(normalized_header)
        if normalized_header not in include_chains:
            include_chains[normalized_header] = tuple(include_stack)

    return include_chains


def _MeasureFileByteSizes(
        normalized_to_disk_path: Mapping[str, str]) -> dict[str, int]:
    """Reads byte sizes from disk for each path in `normalized_to_disk_path`."""
    return {
        normalized_path: os.path.getsize(disk_path)
        for normalized_path, disk_path in normalized_to_disk_path.items()
    }


def ResolveAutoninjaBinary(repository_root: str) -> str:
    """Finds `autoninja` on `PATH` or in `//third_party/depot_tools`."""
    autoninja_on_path = shutil.which('autoninja')
    if autoninja_on_path is not None:
        return autoninja_on_path
    bundled_autoninja = os.path.join(
        repository_root, *_DEPOT_TOOLS_AUTONINJA_RELATIVE_PATH.split('/'))
    if os.path.isfile(bundled_autoninja):
        return bundled_autoninja
    return 'autoninja'


def BuildObjectTargets(
        build_directory: str,
        repository_root: str,
        object_targets: Sequence[str]) -> None:
    """Builds the specified Ninja targets with `autoninja -C`."""
    if not object_targets:
        return
    autoninja_binary = ResolveAutoninjaBinary(repository_root)
    subprocess.check_call(
        [autoninja_binary, '-C', build_directory, *object_targets],
        stdout=sys.stderr,
    )


def MeasureTranslationUnit(
        compile_rule: NinjaCompileRule,
        build_directory: str,
        repository_root: str,
        clang_binary: str) -> TranslationUnitSnapshot:
    """Runs `clang++ -M -H` for `compile_rule` and returns its snapshot."""
    command = BuildClangProbeCommand(
        compile_rule=compile_rule,
        build_directory=build_directory,
        clang_binary=clang_binary,
    )
    completed = subprocess.run(
        command,
        cwd=build_directory,
        capture_output=True,
        text=True,
        check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f'clang++ -M -H failed for {compile_rule.translation_unit} '
            f'(exit {completed.returncode}):\n{completed.stderr}')

    normalized_to_disk_path = ParseMakefileDependenciesOutput(
        makefile_stdout=completed.stdout,
        build_directory=build_directory,
        repository_root=repository_root,
    )
    included_files = _MeasureFileByteSizes(normalized_to_disk_path)
    include_chains = ParseHeaderIncludeChains(
        header_trace_stderr=completed.stderr,
        translation_unit=compile_rule.translation_unit,
        build_directory=build_directory,
        repository_root=repository_root,
    )
    return TranslationUnitSnapshot(
        translation_unit=compile_rule.translation_unit,
        total_bytes=sum(included_files.values()),
        included_files=included_files,
        include_chains=include_chains,
    )


def _ValidateBuildDirectory(build_directory: str) -> None:
    """Raises FileNotFoundError if `build_directory` does not exist."""
    if not os.path.isdir(build_directory):
        raise FileNotFoundError(
            f'Build directory does not exist: {build_directory}')


def _CollectPrerequisiteTargetsToBuild(
        compile_rules: Sequence[NinjaCompileRule]) -> list[str]:
    """Collects deduplicated prerequisite targets across `compile_rules`."""
    targets_to_build: list[str] = []
    for rule in compile_rules:
        rule_targets = (
            rule.prerequisite_targets
            if rule.prerequisite_targets else (rule.object_target,)
        )
        targets_to_build = MergeTranslationUnits(targets_to_build, rule_targets)
    return targets_to_build


def MeasureProbeBaseline(
        build_directory: str,
        repository_root: str,
        translation_units: Sequence[str],
        clang_binary: str,
        should_build_targets: bool = True) -> ProbeBaseline:
    """Measures all requested translation units and returns a ProbeBaseline.

    Resolves the Ninja `cxx` compile rule for each translation unit, runs
    `autoninja` on the deduplicated prerequisite targets when
    `should_build_targets` is True so generated `.pb.h`/`.pb.cc` files and
    prebuilt `.pcm` Clang modules are up to date, and runs `clang++ -M -H` on
    each unit.

    Args:
      build_directory: Path to the GN/Ninja build output directory.
      repository_root: Path to the Chromium `src/` checkout root.
      translation_units: Normalized translation unit paths to measure.
      clang_binary: Path to the `clang++` executable.
      should_build_targets: When True (the default), builds each compile rule's
        prerequisite targets via `autoninja` before measuring.

    Returns:
      A `ProbeBaseline` containing a `TranslationUnitSnapshot` per unit.

    Raises:
      FileNotFoundError: If `build_directory` does not exist.
      RuntimeError: If any translation unit's Ninja rule or `clang++ -M -H`
        invocation fails.
      subprocess.CalledProcessError: If `autoninja` fails.
    """
    _ValidateBuildDirectory(build_directory)
    compile_rules = [
        FindNinjaCompileRule(
            build_directory, repository_root, translation_unit)
        for translation_unit in translation_units
    ]
    if should_build_targets:
        BuildObjectTargets(
            build_directory=build_directory,
            repository_root=repository_root,
            object_targets=_CollectPrerequisiteTargetsToBuild(compile_rules),
        )

    snapshots = tuple(
        MeasureTranslationUnit(
            compile_rule=rule,
            build_directory=build_directory,
            repository_root=repository_root,
            clang_binary=clang_binary,
        )
        for rule in compile_rules
    )
    return ProbeBaseline(translation_units=snapshots)


def ResolveBaselineStorageDirectory(build_directory: str) -> str:
    """Returns `<build_directory>/.compile_size_probe` for saved baselines.

    Storing baselines inside the `-C <out_dir>` directory keeps snapshots
    outside the tracked Git tree (so branch switches during a roll do not touch
    them) and scopes each baseline to the exact GN build configuration that
    produced it.
    """
    return os.path.join(
        os.path.abspath(build_directory), _BASELINE_SUBDIRECTORY_NAME)


def _ValidateBaselineName(baseline_name: str) -> None:
    """Raises ValueError if `baseline_name` is empty or unsafe."""
    if (not _SAFE_BASELINE_NAME_PATTERN.fullmatch(baseline_name)
            or baseline_name == '.'
            or '..' in baseline_name):
        raise ValueError(
            f'Invalid baseline name "{baseline_name}": must contain only '
            f'alphanumerics, underscores, hyphens, or single dots.')


def BaselineFilePath(baseline_directory: str, baseline_name: str) -> str:
    """Returns the validated JSON file path for `baseline_name`."""
    _ValidateBaselineName(baseline_name)
    return os.path.join(baseline_directory, f'{baseline_name}.json')


def _CheckBaselineSaveTarget(
        baseline_directory: str,
        baseline_name: str,
        should_overwrite: bool) -> str:
    """Validates `baseline_name` and checks for an existing baseline file.

    Raises:
      ValueError: If `baseline_name` is empty or contains unsafe characters.
      FileExistsError: If `<baseline_directory>/<baseline_name>.json` already
        exists and `should_overwrite` is False.
    """
    target_path = BaselineFilePath(baseline_directory, baseline_name)
    if os.path.exists(target_path) and not should_overwrite:
        raise FileExistsError(
            f'Baseline "{baseline_name}" already exists at {target_path}; '
            f'pass --overwrite to replace it.')
    return target_path


def _WriteJsonAtomically(
        target_path: str, payload: Mapping[str, Any]) -> None:
    """Writes `payload` as formatted JSON to `target_path` atomically.

    Serializes `payload` to `<target_path>.tmp.<pid>` and replaces `target_path`
    via `os.replace`, deleting the temporary file if an exception occurs.
    """
    os.makedirs(os.path.dirname(target_path), exist_ok=True)
    temporary_path = f'{target_path}.tmp.{os.getpid()}'
    try:
        with open(temporary_path, 'w', encoding='utf-8') as file_handle:
            json.dump(payload, file_handle, indent=2, sort_keys=True)
            file_handle.write('\n')
        os.replace(temporary_path, target_path)
    except BaseException:
        if os.path.exists(temporary_path):
            os.remove(temporary_path)
        raise


def SaveProbeBaseline(
        baseline: ProbeBaseline,
        baseline_directory: str,
        baseline_name: str,
        should_overwrite: bool = False) -> str:
    """Writes `baseline` atomically to `<baseline_directory>/<name>.json`.

    Args:
      baseline: Measured `ProbeBaseline` to persist.
      baseline_directory: Directory where baseline JSON files are stored.
      baseline_name: Validated baseline identifier (without `.json`).
      should_overwrite: When False, raises `FileExistsError` if the target
        baseline file already exists.

    Returns:
      The absolute path to the written `<baseline_name>.json` file.

    Raises:
      ValueError: If `baseline_name` is invalid.
      FileExistsError: If `should_overwrite` is False and a file exists.
    """
    target_path = _CheckBaselineSaveTarget(
        baseline_directory, baseline_name, should_overwrite)
    _WriteJsonAtomically(target_path, baseline.ToDict())
    return target_path


def LoadProbeBaseline(
        baseline_directory: str, baseline_name: str) -> ProbeBaseline:
    """Loads a saved ProbeBaseline from `<baseline_directory>/<name>.json`."""
    target_path = BaselineFilePath(baseline_directory, baseline_name)
    if not os.path.isfile(target_path):
        raise FileNotFoundError(
            f'Saved baseline "{baseline_name}" not found at {target_path}.')
    with open(target_path, 'r', encoding='utf-8') as file_handle:
        return ProbeBaseline.FromDict(json.load(file_handle))


def _ByteSignPrefix(byte_count: int, should_include_sign: bool) -> str:
    """Returns `'+'`, `'-'`, or `''` for `byte_count`."""
    if byte_count < 0:
        return '-'
    if should_include_sign and byte_count > 0:
        return '+'
    return ''


def FormatHumanByteSize(
        byte_count: int, should_include_sign: bool = False) -> str:
    """Formats `byte_count` with both KiB/MiB and exact bytes.

    Args:
      byte_count: Signed byte count to format.
      should_include_sign: When True, prefixes positive counts with `'+'`.
    """
    sign_prefix = _ByteSignPrefix(byte_count, should_include_sign)
    absolute_bytes = abs(byte_count)
    if absolute_bytes >= 1024 * 1024:
        scaled = f'{absolute_bytes / (1024 * 1024):.2f} MiB'
    elif absolute_bytes >= 1024:
        scaled = f'{absolute_bytes / 1024:.2f} KiB'
    else:
        return f'{sign_prefix}{absolute_bytes:,} B'
    return f'{sign_prefix}{scaled} ({sign_prefix}{absolute_bytes:,} B)'


def FormatBaselineSummary(
        baseline: ProbeBaseline, saved_path: str | None = None) -> str:
    """Formats a human-readable summary of a measured ProbeBaseline."""
    lines: list[str] = []
    if saved_path is not None:
        lines.append(f'Saved compile-size baseline to {saved_path}')
    unit_count = len(baseline.translation_units)
    unit_label = 'translation unit' if unit_count == 1 else 'translation units'
    lines.append(
        f'Total compiler inputs size across '
        f'{unit_count} {unit_label}: '
        f'{FormatHumanByteSize(baseline.TotalBytes())}')
    for snapshot in baseline.translation_units:
        lines.append(
            f'  {snapshot.translation_unit}: '
            f'{FormatHumanByteSize(snapshot.total_bytes)} '
            f'({len(snapshot.included_files)} files)')
    return '\n'.join(lines)


def _AddedHeaderSortKey(header: AddedHeaderDelta) -> tuple[int, str]:
    """Orders AddedHeaderDelta entries by size descending, then path."""
    return (-header.size_bytes, header.path)


def _RemovedHeaderSortKey(header: RemovedHeaderDelta) -> tuple[int, str]:
    """Orders RemovedHeaderDelta entries by size descending, then path."""
    return (-header.size_bytes, header.path)


def _ResizedFileSortKey(resized_file: ResizedFileDelta) -> tuple[int, str]:
    """Orders ResizedFileDelta entries by |delta_bytes| descending, path."""
    return (-abs(resized_file.delta_bytes), resized_file.path)


def CompareTranslationUnitSnapshots(
        before_snapshot: TranslationUnitSnapshot,
        after_snapshot: TranslationUnitSnapshot) -> TranslationUnitComparison:
    """Computes added, removed, and resized files between two snapshots."""
    before_files = before_snapshot.included_files
    after_files = after_snapshot.included_files

    added_paths = set(after_files) - set(before_files)
    removed_paths = set(before_files) - set(after_files)
    common_paths = set(before_files) & set(after_files)

    added_headers = sorted(
        (
            AddedHeaderDelta(
                path=path,
                size_bytes=after_files[path],
                include_chain=after_snapshot.include_chains.get(path, (path,)),
            )
            for path in added_paths
        ),
        key=_AddedHeaderSortKey,
    )
    removed_headers = sorted(
        (
            RemovedHeaderDelta(path=path, size_bytes=before_files[path])
            for path in removed_paths
        ),
        key=_RemovedHeaderSortKey,
    )
    resized_files = sorted(
        (
            ResizedFileDelta(
                path=path,
                before_bytes=before_files[path],
                after_bytes=after_files[path],
                delta_bytes=after_files[path] - before_files[path],
            )
            for path in common_paths
            if after_files[path] != before_files[path]
        ),
        key=_ResizedFileSortKey,
    )

    return TranslationUnitComparison(
        translation_unit=after_snapshot.translation_unit,
        before_bytes=before_snapshot.total_bytes,
        after_bytes=after_snapshot.total_bytes,
        delta_bytes=after_snapshot.total_bytes - before_snapshot.total_bytes,
        added_headers=tuple(added_headers),
        removed_headers=tuple(removed_headers),
        resized_files=tuple(resized_files),
    )


def CompareProbeBaselines(
        baseline_name: str,
        before_baseline: ProbeBaseline,
        after_baseline: ProbeBaseline) -> ProbeComparisonReport:
    """Compares translation units between before and after baselines.

    Args:
      baseline_name: Name of the saved `before_baseline` being compared.
      before_baseline: Previously saved ProbeBaseline snapshot.
      after_baseline: Newly measured ProbeBaseline snapshot.

    Returns:
      A ProbeComparisonReport summarizing total and per-unit header changes.

    Raises:
      ValueError: If any translation unit in `after_baseline` is missing from
        `before_baseline`.
    """
    before_by_unit = before_baseline.ByTranslationUnit()
    comparisons: list[TranslationUnitComparison] = []

    for after_snapshot in after_baseline.translation_units:
        translation_unit = after_snapshot.translation_unit
        before_snapshot = before_by_unit.get(translation_unit)
        if before_snapshot is None:
            raise ValueError(
                f'Translation unit "{translation_unit}" is not present in '
                f'baseline "{baseline_name}".')
        comparisons.append(
            CompareTranslationUnitSnapshots(before_snapshot, after_snapshot))

    before_total = sum(
        comparison.before_bytes for comparison in comparisons)
    after_total = sum(
        comparison.after_bytes for comparison in comparisons)
    return ProbeComparisonReport(
        baseline_name=baseline_name,
        before_total_bytes=before_total,
        after_total_bytes=after_total,
        delta_total_bytes=after_total - before_total,
        translation_units=tuple(comparisons),
    )


def _FormatAddedHeadersLines(
        added_headers: Sequence[AddedHeaderDelta]) -> list[str]:
    """Formats the added-headers list and their `#include` chains."""
    if not added_headers:
        return []
    lines = [f'  Added headers ({len(added_headers)}):']
    for header in added_headers:
        lines.append(
            f'    +{header.size_bytes:,} B  {header.path}')
        if len(header.include_chain) > 1:
            chain_text = ' -> '.join(header.include_chain)
            lines.append(f'      via: {chain_text}')
    return lines


def _FormatRemovedHeadersLines(
        removed_headers: Sequence[RemovedHeaderDelta]) -> list[str]:
    """Formats the removed-headers list."""
    if not removed_headers:
        return []
    lines = [f'  Removed headers ({len(removed_headers)}):']
    for header in removed_headers:
        lines.append(f'    -{header.size_bytes:,} B  {header.path}')
    return lines


def _FormatResizedFilesLines(
        resized_files: Sequence[ResizedFileDelta]) -> list[str]:
    """Formats the resized-files list."""
    if not resized_files:
        return []
    lines = [f'  Resized files ({len(resized_files)}):']
    for resized_file in resized_files:
        sign = '+' if resized_file.delta_bytes > 0 else ''
        lines.append(
            f'    {sign}{resized_file.delta_bytes:,} B  {resized_file.path} '
            f'({resized_file.before_bytes:,} B -> '
            f'{resized_file.after_bytes:,} B)')
    return lines


def _FormatTranslationUnitComparisonSection(
        comparison: TranslationUnitComparison) -> list[str]:
    """Formats the human-readable section for a single translation unit."""
    percentage = (
        (comparison.delta_bytes / comparison.before_bytes) * 100.0
        if comparison.before_bytes
        else 0.0
    )
    sign = '+' if percentage > 0 else ''
    formatted_delta = FormatHumanByteSize(
        comparison.delta_bytes, should_include_sign=True)
    header_line = (
        f'=== {comparison.translation_unit}: '
        f'{formatted_delta} '
        f'({sign}{percentage:.2f}%, '
        f'{comparison.before_bytes:,} B -> {comparison.after_bytes:,} B) ==='
    )
    lines = [header_line]
    if (not comparison.added_headers
            and not comparison.removed_headers
            and not comparison.resized_files):
        lines.append('  No header changes.')
        return lines

    lines.extend(_FormatAddedHeadersLines(comparison.added_headers))
    lines.extend(_FormatRemovedHeadersLines(comparison.removed_headers))
    lines.extend(_FormatResizedFilesLines(comparison.resized_files))
    return lines


def FormatComparisonReport(report: ProbeComparisonReport) -> str:
    """Formats a human-readable comparison report across translation units."""
    unit_count = len(report.translation_units)
    unit_label = 'translation unit' if unit_count == 1 else 'translation units'
    formatted_total_delta = FormatHumanByteSize(
        report.delta_total_bytes, should_include_sign=True)
    lines = [
        f'Compile-size comparison against baseline "{report.baseline_name}":',
        f'Total across {unit_count} {unit_label}: '
        f'{FormatHumanByteSize(report.before_total_bytes)} -> '
        f'{FormatHumanByteSize(report.after_total_bytes)} '
        f'({formatted_total_delta})',
    ]
    for comparison in report.translation_units:
        lines.append('')
        lines.extend(_FormatTranslationUnitComparisonSection(comparison))
    return '\n'.join(lines)


def _ParseCommandLineArguments(
        command_line_arguments: Sequence[str] | None) -> argparse.Namespace:
    """Parses command-line arguments for `compile_size_probe.py`."""
    parser = argparse.ArgumentParser(
        description=(
            'Measures and compares compiler input sizes and #include chains '
            'for representative //third_party/protobuf translation units.'))
    parser.add_argument(
        '-C',
        dest='build_directory',
        required=True,
        metavar='OUT_DIR',
        help='Ninja build directory (for example, out/Default).',
    )
    action_group = parser.add_mutually_exclusive_group()
    action_group.add_argument(
        '--save',
        dest='save_name',
        metavar='NAME',
        help=(
            'Save the measured baseline under '
            '<OUT_DIR>/.compile_size_probe/<NAME>.json.'),
    )
    action_group.add_argument(
        '--compare',
        dest='compare_name',
        metavar='NAME',
        help='Compare current compiler inputs against saved baseline <NAME>.',
    )
    parser.add_argument(
        '--overwrite',
        dest='should_overwrite',
        action='store_true',
        help='Overwrite an existing baseline when used with --save.',
    )
    parser.add_argument(
        '--tu',
        dest='extra_translation_units',
        action='append',
        default=[],
        metavar='PATH',
        help=(
            'Additional translation unit path to append to the defaults with '
            '--save, or subset filter over saved baseline units when passed '
            'to --compare (repeatable; gen/... resolves under OUT_DIR and '
            'other relative paths resolve against the Chromium src/ root).'),
    )
    parser.add_argument(
        '--json',
        dest='should_output_json',
        action='store_true',
        help='Emit machine-readable JSON output to stdout.',
    )
    parser.add_argument(
        '--no-build',
        dest='should_skip_build',
        action='store_true',
        help='Skip running autoninja on probe prerequisites before measuring.',
    )
    return parser.parse_args(command_line_arguments)


def _ResolveDefaultClangBinary(repository_root: str) -> str:
    """Returns the absolute path to Chromium's bundled `clang++` binary."""
    return os.path.abspath(
        os.path.join(repository_root, *_DEFAULT_CLANG_RELATIVE_PATH.split('/')))


def _SelectTranslationUnitsToMeasure(
        parsed_arguments: argparse.Namespace,
        build_directory: str,
        repository_root: str,
        before_baseline: ProbeBaseline | None) -> list[str]:
    """Determines the ordered list of normalized translation units to probe.

    When `before_baseline` is None (`--save` or standalone run), appends any
    `--tu` paths to `DEFAULT_TRANSLATION_UNITS`. When `before_baseline` is
    provided (`--compare`), probes all translation units in `before_baseline`
    unless `--tu` filters to a validated subset of those baseline units.
    """
    normalized_extras = [
        NormalizeTranslationUnitPath(
            raw_path, build_directory, repository_root)
        for raw_path in parsed_arguments.extra_translation_units
    ]
    if before_baseline is not None:
        baseline_units = [
            unit.translation_unit
            for unit in before_baseline.translation_units
        ]
        units_missing_from_baseline = [
            unit for unit in normalized_extras if unit not in baseline_units
        ]
        if units_missing_from_baseline:
            raise ValueError(
                'Translation unit(s) not present in the saved baseline: '
                f'{", ".join(units_missing_from_baseline)}. Re-run --save '
                'with --tu to include them in the baseline.')
        if not normalized_extras:
            return baseline_units
        return MergeTranslationUnits((), normalized_extras)
    return MergeTranslationUnits(DEFAULT_TRANSLATION_UNITS, normalized_extras)


def _EmitProbeOutput(
        baseline: ProbeBaseline,
        saved_path: str | None,
        comparison_report: ProbeComparisonReport | None,
        should_output_json: bool) -> None:
    """Prints either JSON or human-readable output for the probe run."""
    if comparison_report is not None:
        if should_output_json:
            print(
                json.dumps(
                    comparison_report.ToDict(), indent=2, sort_keys=True))
        else:
            print(FormatComparisonReport(comparison_report))
        return

    if should_output_json:
        payload = baseline.ToDict()
        if saved_path is not None:
            payload['saved_path'] = saved_path
        print(json.dumps(payload, indent=2, sort_keys=True))
        return
    print(FormatBaselineSummary(baseline, saved_path=saved_path))


def main(command_line_arguments: Sequence[str] | None = None) -> int:
    """Entry point for the `compile_size_probe.py` CLI."""
    parsed_arguments = _ParseCommandLineArguments(command_line_arguments)
    if sys.platform == 'win32':
        print(
            'compile_size_probe.py is not supported on Windows: the '
            'clang-cl toolchain does not accept clang++ -M -H.',
            file=sys.stderr,
        )
        return 1

    build_directory = os.path.abspath(parsed_arguments.build_directory)
    repository_root = _DEFAULT_REPOSITORY_ROOT
    clang_binary = _ResolveDefaultClangBinary(repository_root)
    baseline_directory = ResolveBaselineStorageDirectory(build_directory)

    try:
        if (parsed_arguments.should_overwrite
                and parsed_arguments.save_name is None):
            raise ValueError('--overwrite can only be used with --save.')
        if parsed_arguments.save_name is not None:
            _CheckBaselineSaveTarget(
                baseline_directory=baseline_directory,
                baseline_name=parsed_arguments.save_name,
                should_overwrite=parsed_arguments.should_overwrite,
            )

        before_baseline: ProbeBaseline | None = None
        if parsed_arguments.compare_name is not None:
            before_baseline = LoadProbeBaseline(
                baseline_directory, parsed_arguments.compare_name)

        translation_units = _SelectTranslationUnitsToMeasure(
            parsed_arguments, build_directory, repository_root, before_baseline)
        current_baseline = MeasureProbeBaseline(
            build_directory=build_directory,
            repository_root=repository_root,
            translation_units=translation_units,
            clang_binary=clang_binary,
            should_build_targets=not parsed_arguments.should_skip_build,
        )

        saved_path: str | None = None
        if parsed_arguments.save_name is not None:
            saved_path = SaveProbeBaseline(
                baseline=current_baseline,
                baseline_directory=baseline_directory,
                baseline_name=parsed_arguments.save_name,
                should_overwrite=parsed_arguments.should_overwrite,
            )

        comparison_report: ProbeComparisonReport | None = None
        if before_baseline is not None:
            comparison_report = CompareProbeBaselines(
                baseline_name=parsed_arguments.compare_name,
                before_baseline=before_baseline,
                after_baseline=current_baseline,
            )
    except (
            FileNotFoundError,
            FileExistsError,
            ValueError,
            RuntimeError,
            subprocess.CalledProcessError,
            OSError,
    ) as error:
        print(f'Error: {error}', file=sys.stderr)
        return 1

    _EmitProbeOutput(
        baseline=current_baseline,
        saved_path=saved_path,
        comparison_report=comparison_report,
        should_output_json=parsed_arguments.should_output_json,
    )
    return 0


if __name__ == '__main__':
    sys.exit(main())
