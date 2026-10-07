#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Measures compiler input sizes and include chains for protobuf probe targets.

Chromium's `compile-size` trybot measures the total byte size of every source
file and `#include`d header compiled by a translation unit, using the metric in
`//tools/clang/scripts/compiler_inputs_size.py`. During a
`//third_party/protobuf` roll or header change, a single new `#include` inside a
widely included protobuf runtime header or a code generator expansion in
`protoc` can add hundreds of megabytes across the build, while the bot only
reports one aggregate delta.

This standalone CLI probes three representative translation units (plus any
extra translation units passed via `--tu`) in a single `clang++ -M -H` pass per
file:

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

from collections.abc import Mapping, Sequence
import dataclasses
import os
import pathlib
import re
import shlex
import subprocess
from typing import Any

_TOP_LEVEL_VARIABLE_PATTERN = re.compile(
    r'^([A-Za-z0-9_]+)[ \t]*=[ \t]*(.*)$', re.MULTILINE)
_INDENTED_VARIABLE_PATTERN = re.compile(
    r'^[ \t]+([A-Za-z0-9_]+)[ \t]*=[ \t]*(.*)$')
_BUILD_CXX_LINE_PATTERN = re.compile(
    r'^build\s+(\S+)\s*:\s*\S*cxx\s+(\S+)(.*)$')
_MODULE_FILE_FLAG_PATTERN = re.compile(r'-fmodule-file=(?:[^=\s]+=)?([^\s]+)')
_CLANG_HEADER_TRACE_LINE_PATTERN = re.compile(r'^(\.+)\s+(.+)$')


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
