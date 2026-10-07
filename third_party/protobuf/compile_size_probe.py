#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Measures and compares compiler input sizes, symbol sizes, and LLVM IR diffs.

Chromium's `compile-size` trybot measures the total byte size of every source
file and `#include`d header compiled by a translation unit, using the metric in
`//tools/clang/scripts/compiler_inputs_size.py`, while `android-binary-size`
measures compiled machine code and data tables linked into Chrome. During a
`//third_party/protobuf` roll or header change, regressions come in two forms:

1. Header bloat: A single new `#include` inside a widely included protobuf
   runtime header adds hundreds of megabytes of compiler input across the build.
2. Codegen bloat: Upstream changes to generated methods such as `ByteSizeLong`
   or `MergeImpl` increase compiled symbol sizes across thousands of `.pb.o`
   files even when the `#include` graph is unchanged.

When saving or measuring without a baseline, this standalone CLI probes three
representative translation units (plus any extra translation units passed via
`--tu`); when comparing against a saved baseline via `--compare`, it probes the
baseline's translation units (or the subset selected via `--tu`):

1. `gen/net/cert/root_store_proto_lite/duration.pb.cc`: A tiny generated
   message dominated by protobuf runtime headers, Abseil, and libc++, isolating
   constant per-file header weight and baseline per-message symbol sizes.
2. `gen/components/sync/protocol/sync_entity.pb.cc`: A large generated Chrome
   Sync message where roughly half of the compiler input is generated `.pb.h`
   code, exposing `protoc` codegen growth in methods like `ByteSizeLong` and
   `MergeImpl`.
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

On Linux and Android, the script also measures per-symbol compiled byte sizes
via Chromium's bundled `llvm-nm` (reusing the existing `.o` file when fresh or
compiling a temporary native object with `-fno-lto -g0` when the `.o` file is
stale or ThinLTO bitcode, as on `android-binary-size`). On macOS, Mach-O `.o`
files do not store per-symbol byte sizes, so symbol-size collection is skipped.
When `--diff-ir [SYMBOL]` or `--diff-source` is passed to `--compare`, it diffs
demangled LLVM IR functions (emitted via bundled `clang++ -S -emit-llvm` and
`llvm-cxxfilt`, without requiring external `llvm-dis` or `llvm-diff` binaries)
or generated `.pb.h`/`.pb.cc` sources against the saved baseline.

See https://crbug.com/568074904 for full background.
"""

from __future__ import annotations

import argparse
from collections.abc import Mapping, Sequence
import dataclasses
import difflib
import json
import os
import pathlib
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
from typing import Any

_THIS_DIRECTORY = os.path.abspath(os.path.dirname(__file__))
_DEFAULT_REPOSITORY_ROOT = os.path.abspath(
    os.path.join(_THIS_DIRECTORY, '..', '..'))
_LLVM_BIN_RELATIVE_DIRECTORY = 'third_party/llvm-build/Release+Asserts/bin'
_DEFAULT_CLANG_RELATIVE_PATH = f'{_LLVM_BIN_RELATIVE_DIRECTORY}/clang++'
_DEFAULT_LLVM_NM_RELATIVE_PATH = f'{_LLVM_BIN_RELATIVE_DIRECTORY}/llvm-nm'
_DEFAULT_LLVM_CXXFILT_RELATIVE_PATH = (
    f'{_LLVM_BIN_RELATIVE_DIRECTORY}/llvm-cxxfilt')
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
_LLVM_NM_SIZED_SYMBOL_PATTERN = re.compile(
    r'^([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([A-Za-z])\s+(.+)$')
_LOCAL_CONSTANTS_SYMBOL_BUCKET = '<local constants (.L*)>'
_LLVM_IR_DEFINE_BLOCK_PATTERN = re.compile(
    r'^define\b[^\n]*?@("[^"]+"|[A-Za-z0-9_$.]+)\([^\n]*\{\n.*?\n\}',
    re.MULTILINE | re.DOTALL,
)
_LLVM_IR_ATTRIBUTE_GROUP_PATTERN = re.compile(r'\s+#\d+\b')
_LLVM_IR_METADATA_ATTACHMENT_PATTERN = re.compile(
    r',?\s+![A-Za-z0-9_.]+\s+!\d+')
_LLVM_IR_STRING_CONSTANT_PATTERN = re.compile(r'@\.str(?:\.\d+)?\b')
_LLVM_IR_BLOCK_SPLIT_SENTINEL = (
    '\n; --- COMPILE_SIZE_PROBE_BLOCK_SPLIT ---\n')
_LLVM_BITCODE_MAGIC_HEADERS = (b'BC\xc0\xde', b'\xde\xc0\x17\x0b')
_LTO_FLAG_PREFIXES = (
    '-flto',
    '-fsplit-lto-unit',
    '-fwhole-program-vtables',
    '-fvirtual-function-elimination',
    '-fsanitize=cfi',
    '-fsanitize-cfi-',
    '-fno-sanitize-trap=cfi',
    '-fsanitize-recover=cfi',
)


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
    """Stores compiler input sizes, include chains, and symbol sizes.

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
      symbol_sizes: Mapping from demangled symbol name to compiled byte size in
        the translation unit's object file.
    """

    translation_unit: str
    total_bytes: int
    included_files: Mapping[str, int]
    include_chains: Mapping[str, tuple[str, ...]]
    symbol_sizes: Mapping[str, int] = dataclasses.field(default_factory=dict)

    def TotalSymbolBytes(self) -> int:
        """Returns the sum of compiled byte sizes across `symbol_sizes`."""
        return sum(self.symbol_sizes.values())

    def ToDict(self) -> dict[str, Any]:
        """Serializes this snapshot to a JSON-compatible dictionary."""
        return {
            'translation_unit': self.translation_unit,
            'total_bytes': self.total_bytes,
            'total_symbol_bytes': self.TotalSymbolBytes(),
            'included_files': dict(sorted(self.included_files.items())),
            'include_chains': {
                header_path: list(chain)
                for header_path, chain in sorted(self.include_chains.items())
            },
            'symbol_sizes': dict(sorted(self.symbol_sizes.items())),
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
        symbol_sizes = {
            str(symbol_name): int(byte_size)
            for symbol_name, byte_size in raw_dictionary.get(
                'symbol_sizes', {}).items()
        }
        return cls(
            translation_unit=str(raw_dictionary['translation_unit']),
            total_bytes=int(raw_dictionary['total_bytes']),
            included_files=included_files,
            include_chains=include_chains,
            symbol_sizes=symbol_sizes,
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

    def TotalSymbolBytes(self) -> int:
        """Returns the sum of compiled symbol bytes across all units."""
        return sum(unit.TotalSymbolBytes() for unit in self.translation_units)

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
            'total_symbol_bytes': self.TotalSymbolBytes(),
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
class SymbolSizeDelta:
    """Describes an added, removed, or resized compiled symbol in an object.

    Attributes:
      symbol_name: Demangled C++ symbol name (or `<local constants (.L*)>`).
      before_bytes: Compiled byte size in the saved baseline (`0` if added).
      after_bytes: Compiled byte size in the current run (`0` if removed).
      delta_bytes: Signed byte difference (`after_bytes - before_bytes`).
    """

    symbol_name: str
    before_bytes: int
    after_bytes: int
    delta_bytes: int


@dataclasses.dataclass(frozen=True)
class CodegenDiffEntry:
    """Holds a unified diff for a function's LLVM IR or a generated source.

    Attributes:
      translation_unit: Normalized path of the translation unit that produced
        the diff.
      target_label: Demangled symbol signature (for `--diff-ir`) or generated
        `.pb.h`/`.pb.cc` path (for `--diff-source`).
      unified_diff: Unified diff lines formatted without trailing newlines.
    """

    translation_unit: str
    target_label: str
    unified_diff: str


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
      before_symbol_bytes: Total compiled symbol bytes in the saved baseline.
      after_symbol_bytes: Total compiled symbol bytes in the current run.
      delta_symbol_bytes: Signed difference (`after_symbol_bytes -
        before_symbol_bytes`).
      added_symbols: Newly emitted symbols sorted by `abs(delta_bytes)`
        descending.
      removed_symbols: Removed symbols sorted by `abs(delta_bytes)` descending.
      resized_symbols: Symbols whose byte size changed, sorted by
        `abs(delta_bytes)` descending.
    """

    translation_unit: str
    before_bytes: int
    after_bytes: int
    delta_bytes: int
    added_headers: tuple[AddedHeaderDelta, ...]
    removed_headers: tuple[RemovedHeaderDelta, ...]
    resized_files: tuple[ResizedFileDelta, ...]
    before_symbol_bytes: int = 0
    after_symbol_bytes: int = 0
    delta_symbol_bytes: int = 0
    added_symbols: tuple[SymbolSizeDelta, ...] = ()
    removed_symbols: tuple[SymbolSizeDelta, ...] = ()
    resized_symbols: tuple[SymbolSizeDelta, ...] = ()


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
      before_total_symbol_bytes: Sum of `before_symbol_bytes` across units.
      after_total_symbol_bytes: Sum of `after_symbol_bytes` across units.
      delta_total_symbol_bytes: Signed difference (`after_total_symbol_bytes -
        before_total_symbol_bytes`).
      codegen_diffs: Unified diffs from `--diff-ir` and `--diff-source`.
      codegen_diff_filter: Filter string when `--diff-ir` or `--diff-source`
        was requested, or None when neither flag was passed.
    """

    baseline_name: str
    before_total_bytes: int
    after_total_bytes: int
    delta_total_bytes: int
    translation_units: tuple[TranslationUnitComparison, ...]
    before_total_symbol_bytes: int = 0
    after_total_symbol_bytes: int = 0
    delta_total_symbol_bytes: int = 0
    codegen_diffs: tuple[CodegenDiffEntry, ...] = ()
    codegen_diff_filter: str | None = None

    def ToDict(self) -> dict[str, Any]:
        """Serializes this comparison report to a JSON-compatible dict."""
        return json.loads(json.dumps(dataclasses.asdict(self)))


def UnescapeNinjaValue(raw_value: str) -> str:
    """Unescapes Ninja `$:`, `$ `, and `$$` sequences in a variable binding."""
    return raw_value.replace('$:', ':').replace('$ ', ' ').replace('$$', '$')


def _IsPathWithinDirectory(candidate_path: str, directory_path: str) -> bool:
    """Returns True if `candidate_path` is inside `directory_path`."""
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


def _StripLtoFlags(compiler_flags: Sequence[str]) -> list[str]:
    """Removes ThinLTO and CFI flags so `clang++` emits native code or IR.

    Clang requires `-flto` when Control Flow Integrity (`-fsanitize=cfi*`) or
    whole-program vtable flags are active, so those flags must be stripped
    alongside `-flto*` before passing `-fno-lto`.
    """
    return [
        flag for flag in compiler_flags
        if not flag.startswith(_LTO_FLAG_PREFIXES)
    ]


def BuildClangCodegenCommand(
        compile_rule: NinjaCompileRule,
        build_directory: str,
        clang_binary: str,
        codegen_flags: Sequence[str]) -> list[str]:
    """Constructs a non-LTO `clang++` command for `.o` or `.ll` output."""
    compiler_flags = _StripLtoFlags(
        _CollectTargetCompilerFlags(compile_rule, build_directory))
    return (
        [clang_binary]
        + compiler_flags
        + ['-fno-lto', '-g0', *codegen_flags, compile_rule.ninja_source_path]
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


def _IsLlvmBitcodeFile(file_path: str) -> bool:
    """Returns True if `file_path` begins with an LLVM IR bitcode header."""
    try:
        with open(file_path, 'rb') as file_handle:
            magic_bytes = file_handle.read(4)
    except OSError:
        return False
    return magic_bytes in _LLVM_BITCODE_MAGIC_HEADERS


def _IsObjectFileUpToDate(
        object_file_path: str,
        dependency_disk_paths: Sequence[str]) -> bool:
    """Returns True if `object_file_path` exists and is newer than inputs."""
    if not os.path.isfile(object_file_path):
        return False
    try:
        object_mtime = os.path.getmtime(object_file_path)
        return all(
            os.path.isfile(dependency_path)
            and object_mtime >= os.path.getmtime(dependency_path)
            for dependency_path in dependency_disk_paths
        )
    except OSError:
        return False


def ParseLlvmNmSymbolSizes(llvm_nm_stdout: str) -> dict[str, int]:
    """Parses `llvm-nm --print-size --demangle` output into `{symbol: bytes}`.

    Deduplicates identical `(address, size, type, name)` ELF alias lines (such
    as `C1`/`C2` constructor or `D1`/`D2` destructor aliases sharing the same
    section offset and size) while summing distinct ABI functions (such as `D0`
    deleting destructors) that share a demangled C++ signature. Note that under
    `-ffunction-sections`, every function starts at section offset `0`, so two
    separate ABI variants that demangle to the same signature and compile to
    the exact same byte size will also share an `(address, size, type, name)`
    tuple and be deduplicated together. Folds compiler-generated `.L*` local
    constants into `<local constants (.L*)>` so symbol renumbering does not
    produce spurious added/removed entries.
    """
    symbol_sizes: dict[str, int] = {}
    seen_symbol_entries: set[tuple[str, str, str, str]] = set()
    for raw_line in llvm_nm_stdout.splitlines():
        match = _LLVM_NM_SIZED_SYMBOL_PATTERN.match(raw_line.strip())
        if match is None:
            continue
        address_hex, size_hex, symbol_type, raw_symbol_name = match.groups()
        size_bytes = int(size_hex, 16)
        if size_bytes <= 0:
            continue
        symbol_name = raw_symbol_name.strip()
        if symbol_name.startswith('.L'):
            symbol_name = _LOCAL_CONSTANTS_SYMBOL_BUCKET
        else:
            # C1/C2 constructors and D1/D2 destructors are often emitted as ELF
            # symbol-table aliases pointing at the same address and size. Under
            # -ffunction-sections, distinct sections also start at offset 0, so
            # equal-sized ABI variants with the same demangled name cannot be
            # distinguished from aliases in llvm-nm output and are merged here.
            entry_key = (address_hex, size_hex, symbol_type, symbol_name)
            if entry_key in seen_symbol_entries:
                continue
            seen_symbol_entries.add(entry_key)
        symbol_sizes[symbol_name] = (
            symbol_sizes.get(symbol_name, 0) + size_bytes)
    return symbol_sizes


def _RunLlvmNmOnObjectFile(
        llvm_nm_binary: str, object_file_path: str) -> dict[str, int]:
    """Runs `llvm-nm` on `object_file_path` and returns parsed symbol sizes."""
    completed = subprocess.run(
        [
            llvm_nm_binary,
            '--print-size',
            '--size-sort',
            '--demangle',
            object_file_path,
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f'llvm-nm failed for {object_file_path} '
            f'(exit {completed.returncode}):\n{completed.stderr}')
    return ParseLlvmNmSymbolSizes(completed.stdout)


def MeasureObjectSymbolSizes(
        compile_rule: NinjaCompileRule,
        build_directory: str,
        clang_binary: str,
        llvm_nm_binary: str | None = None,
        dependency_disk_paths: Sequence[str] = ()) -> dict[str, int]:
    """Measures per-symbol compiled byte sizes for `compile_rule`.

    Uses the existing `.o` file in `build_directory` when it is already a native
    object file and is newer than all `dependency_disk_paths`. When the `.o`
    file is stale, missing, or ThinLTO bitcode (as on `android-binary-size`),
    compiles a temporary native `.o` file with `-fno-lto`. Returns `{}` on
    macOS where Mach-O object files do not record per-symbol byte sizes.
    """
    if (sys.platform == 'darwin'
            or not llvm_nm_binary
            or not os.path.isfile(llvm_nm_binary)):
        return {}

    existing_object_path = os.path.normpath(
        os.path.join(build_directory, compile_rule.object_target))
    paths_to_check = dependency_disk_paths or (
        os.path.normpath(
            os.path.join(build_directory, compile_rule.ninja_source_path)),
    )
    if (_IsObjectFileUpToDate(existing_object_path, paths_to_check)
            and not _IsLlvmBitcodeFile(existing_object_path)):
        return _RunLlvmNmOnObjectFile(llvm_nm_binary, existing_object_path)

    with tempfile.TemporaryDirectory() as temporary_directory:
        temporary_object_path = os.path.join(temporary_directory, 'probe.o')
        compile_command = BuildClangCodegenCommand(
            compile_rule=compile_rule,
            build_directory=build_directory,
            clang_binary=clang_binary,
            codegen_flags=['-c', '-o', temporary_object_path],
        )
        completed = subprocess.run(
            compile_command,
            cwd=build_directory,
            capture_output=True,
            text=True,
            check=False,
        )
        if completed.returncode != 0:
            raise RuntimeError(
                f'clang++ codegen failed for {compile_rule.translation_unit} '
                f'(exit {completed.returncode}):\n{completed.stderr}')
        return _RunLlvmNmOnObjectFile(llvm_nm_binary, temporary_object_path)


def _DemangleTextWithLlvmCxxfilt(
        raw_text: str, llvm_cxxfilt_binary: str | None) -> str:
    """Runs `llvm-cxxfilt` over `raw_text` when the binary is available."""
    if not llvm_cxxfilt_binary or not os.path.isfile(llvm_cxxfilt_binary):
        return raw_text
    completed = subprocess.run(
        [llvm_cxxfilt_binary],
        input=raw_text,
        capture_output=True,
        text=True,
        check=False,
    )
    return completed.stdout if completed.returncode == 0 else raw_text


def _NormalizeLlvmIrFunctionBlock(raw_block: str) -> str:
    """Strips unstable attribute, metadata, and string IDs from IR."""
    without_attributes = _LLVM_IR_ATTRIBUTE_GROUP_PATTERN.sub('', raw_block)
    without_metadata = _LLVM_IR_METADATA_ATTACHMENT_PATTERN.sub(
        '', without_attributes)
    return _LLVM_IR_STRING_CONSTANT_PATTERN.sub(
        '@.str', without_metadata).strip()


def ExtractLlvmIrFunctions(
        raw_llvm_ir: str,
        llvm_cxxfilt_binary: str | None = None) -> dict[str, str]:
    """Extracts `{demangled_symbol: normalized_define_block}` from LLVM IR.

    Matches `define ... @<mangled>(...) { ... }` blocks prior to demangling so
    that C++ parameter parentheses in demangled names never ambiguity-split the
    function header. When multiple ABI variants (such as complete-object and
    base-object constructors `C1`/`C2` or destructors `D1`/`D2`) demangle to the
    same C++ signature, their normalized IR blocks are concatenated in module
    order.
    """
    matches = list(_LLVM_IR_DEFINE_BLOCK_PATTERN.finditer(raw_llvm_ir))
    if not matches:
        return {}

    mangled_names = [match.group(1).strip('"') for match in matches]
    demangled_names = _DemangleTextWithLlvmCxxfilt(
        '\n'.join(mangled_names), llvm_cxxfilt_binary).splitlines()
    demangled_blocks = _DemangleTextWithLlvmCxxfilt(
        _LLVM_IR_BLOCK_SPLIT_SENTINEL.join(
            match.group(0) for match in matches),
        llvm_cxxfilt_binary,
    ).split(_LLVM_IR_BLOCK_SPLIT_SENTINEL)

    functions: dict[str, str] = {}
    for demangled_name, demangled_block in zip(
            demangled_names, demangled_blocks):
        clean_name = demangled_name.strip()
        normalized_block = _NormalizeLlvmIrFunctionBlock(demangled_block)
        if clean_name in functions:
            functions[clean_name] = (
                f'{functions[clean_name]}\n\n{normalized_block}')
        else:
            functions[clean_name] = normalized_block
    return functions


def EmitTranslationUnitLlvmIr(
        compile_rule: NinjaCompileRule,
        build_directory: str,
        clang_binary: str,
        llvm_cxxfilt_binary: str | None = None) -> dict[str, str]:
    """Emits textual LLVM IR via `clang++ -S -emit-llvm` without `llvm-dis`.

    Always compiles from source with `-fno-lto -g0 -S -emit-llvm` so that
    `--save` and `--compare --diff-ir` pass through the identical optimization
    pipeline regardless of whether a ThinLTO `.o` file is fresh on disk.
    """
    command = BuildClangCodegenCommand(
        compile_rule=compile_rule,
        build_directory=build_directory,
        clang_binary=clang_binary,
        codegen_flags=['-S', '-emit-llvm', '-o', '-'],
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
            f'clang++ -emit-llvm failed for {compile_rule.translation_unit} '
            f'(exit {completed.returncode}):\n{completed.stderr}')
    return ExtractLlvmIrFunctions(completed.stdout, llvm_cxxfilt_binary)


def CollectGeneratedSourceFiles(
        compile_rule: NinjaCompileRule,
        build_directory: str,
        repository_root: str) -> dict[str, str]:
    """Reads `.pb.h` and `.pb.cc` source contents for generated proto units."""
    unit = compile_rule.translation_unit
    if not (unit.startswith('gen/') and unit.endswith('.pb.cc')):
        return {}

    collected_files: dict[str, str] = {}
    companion_header = unit.removesuffix('.pb.cc') + '.pb.h'
    for relative_path in (companion_header, unit):
        disk_path = ResolveSourceAbsolutePath(
            relative_path, build_directory, repository_root)
        if os.path.isfile(disk_path):
            with open(disk_path, 'r', encoding='utf-8', errors='replace') as (
                    file_handle):
                collected_files[relative_path] = file_handle.read()
    return collected_files


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
        clang_binary: str,
        llvm_nm_binary: str | None = None,
) -> TranslationUnitSnapshot:
    """Runs `clang++ -M -H` and `llvm-nm` for `compile_rule`."""
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
    symbol_sizes = MeasureObjectSymbolSizes(
        compile_rule=compile_rule,
        build_directory=build_directory,
        clang_binary=clang_binary,
        llvm_nm_binary=llvm_nm_binary,
        dependency_disk_paths=tuple(normalized_to_disk_path.values()),
    )
    return TranslationUnitSnapshot(
        translation_unit=compile_rule.translation_unit,
        total_bytes=sum(included_files.values()),
        included_files=included_files,
        include_chains=include_chains,
        symbol_sizes=symbol_sizes,
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
        should_build_targets: bool = True,
        llvm_nm_binary: str | None = None) -> ProbeBaseline:
    """Measures all requested translation units and returns a ProbeBaseline.

    Resolves the Ninja `cxx` compile rule for each translation unit, runs
    `autoninja` on the deduplicated prerequisite targets when
    `should_build_targets` is True so generated `.pb.h`/`.pb.cc` files and
    prebuilt `.pcm` Clang modules are up to date, and runs `clang++ -M -H` and
    `llvm-nm` on each unit.

    Args:
      build_directory: Path to the GN/Ninja build output directory.
      repository_root: Path to the Chromium `src/` checkout root.
      translation_units: Normalized translation unit paths to measure.
      clang_binary: Path to the `clang++` executable.
      should_build_targets: When True (the default), builds each compile rule's
        prerequisite targets via `autoninja` before measuring.
      llvm_nm_binary: Optional path to `llvm-nm` for per-symbol size collection.

    Returns:
      A `ProbeBaseline` containing a `TranslationUnitSnapshot` per unit.

    Raises:
      FileNotFoundError: If `build_directory` does not exist.
      RuntimeError: If any translation unit's Ninja rule, `clang++`, or
        `llvm-nm` invocation fails.
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
            llvm_nm_binary=llvm_nm_binary,
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
    """Raises ValueError if `baseline_name` is unsafe or reserved."""
    if (not _SAFE_BASELINE_NAME_PATTERN.fullmatch(baseline_name)
            or baseline_name == '.'
            or '..' in baseline_name
            or baseline_name.endswith('.artifacts')):
        raise ValueError(
            f'Invalid baseline name "{baseline_name}": must contain only '
            f'alphanumerics, underscores, hyphens, or single dots, and must '
            f'not end with ".artifacts".')


def BaselineFilePath(baseline_directory: str, baseline_name: str) -> str:
    """Returns the validated JSON file path for `baseline_name`."""
    _ValidateBaselineName(baseline_name)
    return os.path.join(baseline_directory, f'{baseline_name}.json')


def ArtifactsFilePath(baseline_directory: str, baseline_name: str) -> str:
    """Returns the companion codegen artifacts path for `baseline_name`."""
    _ValidateBaselineName(baseline_name)
    return os.path.join(baseline_directory, f'{baseline_name}.artifacts.json')


def _CheckBaselineSaveTarget(
        baseline_directory: str,
        baseline_name: str,
        should_overwrite: bool) -> str:
    """Validates `baseline_name` and checks for existing baseline files.

    Raises:
      ValueError: If `baseline_name` is empty, unsafe, or ends with
        `".artifacts"`.
      FileExistsError: If `<baseline_name>.json` or
        `<baseline_name>.artifacts.json` already exists in `baseline_directory`
        and `should_overwrite` is False.
    """
    target_path = BaselineFilePath(baseline_directory, baseline_name)
    artifacts_path = ArtifactsFilePath(baseline_directory, baseline_name)
    if not should_overwrite:
        for existing_path in (target_path, artifacts_path):
            if os.path.exists(existing_path):
                raise FileExistsError(
                    f'Baseline "{baseline_name}" already exists at '
                    f'{existing_path}; pass --overwrite to replace it.')
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
        baseline or its `.artifacts.json` sidecar already exists.

    Returns:
      The absolute path to the written `<baseline_name>.json` file.

    Raises:
      ValueError: If `baseline_name` is invalid.
      FileExistsError: If `should_overwrite` is False and a file exists.
    """
    target_path = _CheckBaselineSaveTarget(
        baseline_directory, baseline_name, should_overwrite)
    if should_overwrite:
        # Remove any earlier run's codegen artifacts before writing the new
        # baseline so a partial save never pairs new sizes with stale IR.
        artifacts_path = ArtifactsFilePath(baseline_directory, baseline_name)
        if os.path.exists(artifacts_path):
            os.remove(artifacts_path)
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


def SaveCodegenArtifacts(
        baseline_directory: str,
        baseline_name: str,
        generated_sources_by_unit: Mapping[str, Mapping[str, str]],
        llvm_ir_by_unit: Mapping[str, Mapping[str, str]]) -> str:
    """Saves generated `.pb.*` sources and demangled LLVM IR atomically."""
    target_path = ArtifactsFilePath(baseline_directory, baseline_name)
    payload = {
        'generated_sources_by_unit': {
            unit: dict(sources)
            for unit, sources in generated_sources_by_unit.items()
        },
        'llvm_ir_by_unit': {
            unit: dict(functions)
            for unit, functions in llvm_ir_by_unit.items()
        },
    }
    _WriteJsonAtomically(target_path, payload)
    return target_path


def _RequireCodegenArtifactsPath(
        baseline_directory: str, baseline_name: str) -> str:
    """Returns the saved codegen artifacts path or raises FileNotFoundError."""
    target_path = ArtifactsFilePath(baseline_directory, baseline_name)
    if not os.path.isfile(target_path):
        raise FileNotFoundError(
            f'Saved codegen artifacts "{baseline_name}" not found at '
            f'{target_path}.')
    return target_path


def LoadCodegenArtifacts(
        baseline_directory: str,
        baseline_name: str,
) -> tuple[dict[str, dict[str, str]], dict[str, dict[str, str]]]:
    """Loads saved `(generated_sources_by_unit, llvm_ir_by_unit)` artifacts."""
    target_path = _RequireCodegenArtifactsPath(
        baseline_directory, baseline_name)
    with open(target_path, 'r', encoding='utf-8') as file_handle:
        raw_payload = json.load(file_handle)
    sources_by_unit = {
        str(unit): {
            str(path): str(content) for path, content in files.items()
        }
        for unit, files in raw_payload.get(
            'generated_sources_by_unit', {}).items()
    }
    ir_by_unit = {
        str(unit): {
            str(symbol): str(block) for symbol, block in blocks.items()
        }
        for unit, blocks in raw_payload.get('llvm_ir_by_unit', {}).items()
    }
    return sources_by_unit, ir_by_unit


def CollectCodegenArtifactsForUnits(
        build_directory: str,
        repository_root: str,
        translation_units: Sequence[str],
        clang_binary: str,
        llvm_cxxfilt_binary: str | None = None,
) -> tuple[dict[str, dict[str, str]], dict[str, dict[str, str]]]:
    """Collects `.pb.*` sources and LLVM IR for `translation_units`."""
    sources_by_unit: dict[str, dict[str, str]] = {}
    ir_by_unit: dict[str, dict[str, str]] = {}
    for translation_unit in translation_units:
        rule = FindNinjaCompileRule(
            build_directory, repository_root, translation_unit)
        sources = CollectGeneratedSourceFiles(
            rule, build_directory, repository_root)
        if sources:
            sources_by_unit[translation_unit] = sources
        ir_functions = EmitTranslationUnitLlvmIr(
            compile_rule=rule,
            build_directory=build_directory,
            clang_binary=clang_binary,
            llvm_cxxfilt_binary=llvm_cxxfilt_binary,
        )
        if ir_functions:
            ir_by_unit[translation_unit] = ir_functions
    return sources_by_unit, ir_by_unit


def ComputeLlvmIrFunctionDiffs(
        before_ir_by_unit: Mapping[str, Mapping[str, str]],
        after_ir_by_unit: Mapping[str, Mapping[str, str]],
        symbol_substring: str) -> tuple[CodegenDiffEntry, ...]:
    """Computes unified diffs for LLVM IR functions matching a substring."""
    diff_entries: list[CodegenDiffEntry] = []
    for translation_unit, after_functions in sorted(after_ir_by_unit.items()):
        before_functions = before_ir_by_unit.get(translation_unit, {})
        candidate_symbols = sorted(
            set(before_functions) | set(after_functions))
        for symbol_name in candidate_symbols:
            if symbol_substring and symbol_substring not in symbol_name:
                continue
            before_ir = before_functions.get(symbol_name, '')
            after_ir = after_functions.get(symbol_name, '')
            if before_ir == after_ir:
                continue
            unified_lines = list(
                difflib.unified_diff(
                    before_ir.splitlines(),
                    after_ir.splitlines(),
                    fromfile=f'before:{symbol_name}',
                    tofile=f'after:{symbol_name}',
                    lineterm='',
                ))
            if unified_lines:
                diff_entries.append(
                    CodegenDiffEntry(
                        translation_unit=translation_unit,
                        target_label=symbol_name,
                        unified_diff='\n'.join(unified_lines),
                    ))
    return tuple(diff_entries)


def ComputeGeneratedSourceDiffs(
        before_sources_by_unit: Mapping[str, Mapping[str, str]],
        after_sources_by_unit: Mapping[str, Mapping[str, str]],
) -> tuple[CodegenDiffEntry, ...]:
    """Computes unified diffs for generated `.pb.h`/`.pb.cc` source files."""
    diff_entries: list[CodegenDiffEntry] = []
    for translation_unit, after_sources in sorted(
            after_sources_by_unit.items()):
        before_sources = before_sources_by_unit.get(translation_unit, {})
        for file_path in sorted(set(before_sources) | set(after_sources)):
            before_text = before_sources.get(file_path, '')
            after_text = after_sources.get(file_path, '')
            if before_text == after_text:
                continue
            unified_lines = list(
                difflib.unified_diff(
                    before_text.splitlines(),
                    after_text.splitlines(),
                    fromfile=f'before/{file_path}',
                    tofile=f'after/{file_path}',
                    lineterm='',
                ))
            if unified_lines:
                diff_entries.append(
                    CodegenDiffEntry(
                        translation_unit=translation_unit,
                        target_label=file_path,
                        unified_diff='\n'.join(unified_lines),
                    ))
    return tuple(diff_entries)


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
    if baseline.TotalSymbolBytes() > 0:
        lines.append(
            f'Total compiled symbol size across '
            f'{unit_count} {unit_label}: '
            f'{FormatHumanByteSize(baseline.TotalSymbolBytes())}')
    for snapshot in baseline.translation_units:
        symbol_suffix = (
            f', {FormatHumanByteSize(snapshot.TotalSymbolBytes())} across '
            f'{len(snapshot.symbol_sizes)} symbols'
            if snapshot.symbol_sizes
            else ''
        )
        lines.append(
            f'  {snapshot.translation_unit}: '
            f'{FormatHumanByteSize(snapshot.total_bytes)} '
            f'({len(snapshot.included_files)} files{symbol_suffix})')
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


def _SymbolDeltaSortKey(symbol_delta: SymbolSizeDelta) -> tuple[int, str]:
    """Orders SymbolSizeDelta entries by |delta_bytes| descending, name."""
    return (-abs(symbol_delta.delta_bytes), symbol_delta.symbol_name)


def _CompareSymbolSizeMaps(
        before_symbols: Mapping[str, int],
        after_symbols: Mapping[str, int],
) -> tuple[
    tuple[SymbolSizeDelta, ...],
    tuple[SymbolSizeDelta, ...],
    tuple[SymbolSizeDelta, ...],
]:
    """Computes sorted `(added, removed, resized)` symbol size deltas."""
    added_names = set(after_symbols) - set(before_symbols)
    removed_names = set(before_symbols) - set(after_symbols)
    common_names = set(before_symbols) & set(after_symbols)

    added_symbols = sorted(
        (
            SymbolSizeDelta(
                symbol_name=name,
                before_bytes=0,
                after_bytes=after_symbols[name],
                delta_bytes=after_symbols[name],
            )
            for name in added_names
        ),
        key=_SymbolDeltaSortKey,
    )
    removed_symbols = sorted(
        (
            SymbolSizeDelta(
                symbol_name=name,
                before_bytes=before_symbols[name],
                after_bytes=0,
                delta_bytes=-before_symbols[name],
            )
            for name in removed_names
        ),
        key=_SymbolDeltaSortKey,
    )
    resized_symbols = sorted(
        (
            SymbolSizeDelta(
                symbol_name=name,
                before_bytes=before_symbols[name],
                after_bytes=after_symbols[name],
                delta_bytes=after_symbols[name] - before_symbols[name],
            )
            for name in common_names
            if after_symbols[name] != before_symbols[name]
        ),
        key=_SymbolDeltaSortKey,
    )
    return (
        tuple(added_symbols),
        tuple(removed_symbols),
        tuple(resized_symbols),
    )


def CompareTranslationUnitSnapshots(
        before_snapshot: TranslationUnitSnapshot,
        after_snapshot: TranslationUnitSnapshot) -> TranslationUnitComparison:
    """Computes header and symbol deltas between two snapshots."""
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
    added_symbols, removed_symbols, resized_symbols = _CompareSymbolSizeMaps(
        before_snapshot.symbol_sizes, after_snapshot.symbol_sizes)
    before_symbol_bytes = before_snapshot.TotalSymbolBytes()
    after_symbol_bytes = after_snapshot.TotalSymbolBytes()

    return TranslationUnitComparison(
        translation_unit=after_snapshot.translation_unit,
        before_bytes=before_snapshot.total_bytes,
        after_bytes=after_snapshot.total_bytes,
        delta_bytes=after_snapshot.total_bytes - before_snapshot.total_bytes,
        added_headers=tuple(added_headers),
        removed_headers=tuple(removed_headers),
        resized_files=tuple(resized_files),
        before_symbol_bytes=before_symbol_bytes,
        after_symbol_bytes=after_symbol_bytes,
        delta_symbol_bytes=after_symbol_bytes - before_symbol_bytes,
        added_symbols=added_symbols,
        removed_symbols=removed_symbols,
        resized_symbols=resized_symbols,
    )


def CompareProbeBaselines(
        baseline_name: str,
        before_baseline: ProbeBaseline,
        after_baseline: ProbeBaseline,
        codegen_diffs: Sequence[CodegenDiffEntry] = (),
        codegen_diff_filter: str | None = None) -> ProbeComparisonReport:
    """Compares translation units between before and after baselines.

    Args:
      baseline_name: Name of the saved `before_baseline` being compared.
      before_baseline: Previously saved ProbeBaseline snapshot.
      after_baseline: Newly measured ProbeBaseline snapshot.
      codegen_diffs: Optional unified diffs of LLVM IR or generated sources.
      codegen_diff_filter: Filter string when `--diff-ir` or `--diff-source`
        was requested, or None when neither flag was passed.

    Returns:
      A ProbeComparisonReport summarizing total and per-unit changes.

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
    before_symbols_total = sum(
        comparison.before_symbol_bytes for comparison in comparisons)
    after_symbols_total = sum(
        comparison.after_symbol_bytes for comparison in comparisons)
    return ProbeComparisonReport(
        baseline_name=baseline_name,
        before_total_bytes=before_total,
        after_total_bytes=after_total,
        delta_total_bytes=after_total - before_total,
        translation_units=tuple(comparisons),
        before_total_symbol_bytes=before_symbols_total,
        after_total_symbol_bytes=after_symbols_total,
        delta_total_symbol_bytes=after_symbols_total - before_symbols_total,
        codegen_diffs=tuple(codegen_diffs),
        codegen_diff_filter=codegen_diff_filter,
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


def _FormatSymbolDeltasLines(
        heading: str, symbol_deltas: Sequence[SymbolSizeDelta]) -> list[str]:
    """Formats added, removed, or resized symbol size lines."""
    if not symbol_deltas:
        return []
    lines = [f'  {heading} ({len(symbol_deltas)}):']
    for delta in symbol_deltas:
        sign = '+' if delta.delta_bytes > 0 else ''
        lines.append(
            f'    {sign}{delta.delta_bytes:,} B  {delta.symbol_name} '
            f'({delta.before_bytes:,} B -> {delta.after_bytes:,} B)')
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
    lines = [
        f'=== {comparison.translation_unit}: '
        f'{formatted_delta} '
        f'({sign}{percentage:.2f}%, '
        f'{comparison.before_bytes:,} B -> {comparison.after_bytes:,} B) ==='
    ]
    has_symbol_sizes = bool(
        comparison.before_symbol_bytes or comparison.after_symbol_bytes)
    if has_symbol_sizes:
        formatted_symbol_delta = FormatHumanByteSize(
            comparison.delta_symbol_bytes, should_include_sign=True)
        lines.append(
            f'  Compiled symbols: {comparison.before_symbol_bytes:,} B -> '
            f'{comparison.after_symbol_bytes:,} B '
            f'({formatted_symbol_delta})')
    else:
        lines.append('  Compiled symbols: unavailable')

    has_header_changes = bool(
        comparison.added_headers
        or comparison.removed_headers
        or comparison.resized_files)
    has_symbol_changes = bool(
        comparison.added_symbols
        or comparison.removed_symbols
        or comparison.resized_symbols)
    if not has_header_changes and not has_symbol_changes:
        no_changes_line = (
            '  No header or symbol changes.'
            if has_symbol_sizes
            else '  No header changes.'
        )
        lines.append(no_changes_line)
        return lines

    lines.extend(_FormatAddedHeadersLines(comparison.added_headers))
    lines.extend(_FormatRemovedHeadersLines(comparison.removed_headers))
    lines.extend(_FormatResizedFilesLines(comparison.resized_files))
    lines.extend(
        _FormatSymbolDeltasLines('Added symbols', comparison.added_symbols))
    lines.extend(
        _FormatSymbolDeltasLines(
            'Removed symbols', comparison.removed_symbols))
    lines.extend(
        _FormatSymbolDeltasLines(
            'Resized symbols', comparison.resized_symbols))
    return lines


def _FormatCodegenDiffsLines(
        codegen_diffs: Sequence[CodegenDiffEntry],
        codegen_diff_filter: str | None = None) -> list[str]:
    """Formats unified LLVM IR or generated source diffs."""
    if not codegen_diffs:
        if codegen_diff_filter is None:
            return []
        filter_suffix = (
            f' (filter: "{codegen_diff_filter}")'
            if codegen_diff_filter
            else ''
        )
        return [
            '',
            '=== Codegen / Source Unified Diffs ===',
            f'No differences found{filter_suffix}.',
        ]
    lines = ['', '=== Codegen / Source Unified Diffs ===']
    for entry in codegen_diffs:
        lines.append(
            f'--- {entry.translation_unit} :: {entry.target_label} ---')
        lines.append(entry.unified_diff)
    return lines


def FormatComparisonReport(report: ProbeComparisonReport) -> str:
    """Formats a human-readable comparison report across translation units."""
    unit_count = len(report.translation_units)
    unit_label = 'translation unit' if unit_count == 1 else 'translation units'
    formatted_total_delta = FormatHumanByteSize(
        report.delta_total_bytes, should_include_sign=True)
    lines = [
        f'Compile-size comparison against baseline "{report.baseline_name}":',
        f'Total compiler inputs across {unit_count} {unit_label}: '
        f'{FormatHumanByteSize(report.before_total_bytes)} -> '
        f'{FormatHumanByteSize(report.after_total_bytes)} '
        f'({formatted_total_delta})',
    ]
    if report.before_total_symbol_bytes or report.after_total_symbol_bytes:
        formatted_total_symbol_delta = FormatHumanByteSize(
            report.delta_total_symbol_bytes, should_include_sign=True)
        lines.append(
            f'Total compiled symbols across {unit_count} {unit_label}: '
            f'{FormatHumanByteSize(report.before_total_symbol_bytes)} -> '
            f'{FormatHumanByteSize(report.after_total_symbol_bytes)} '
            f'({formatted_total_symbol_delta})')
    for comparison in report.translation_units:
        lines.append('')
        lines.extend(_FormatTranslationUnitComparisonSection(comparison))
    lines.extend(
        _FormatCodegenDiffsLines(
            report.codegen_diffs, report.codegen_diff_filter))
    return '\n'.join(lines)


def _ParseCommandLineArguments(
        command_line_arguments: Sequence[str] | None) -> argparse.Namespace:
    """Parses command-line arguments for `compile_size_probe.py`."""
    parser = argparse.ArgumentParser(
        description=(
            'Measures and compares compiler input sizes, #include chains, '
            'compiled symbol sizes, and LLVM IR / generated source diffs '
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
            'Save the measured baseline and codegen snapshots under '
            '<OUT_DIR>/.compile_size_probe/<NAME>.*.'),
    )
    action_group.add_argument(
        '--compare',
        dest='compare_name',
        metavar='NAME',
        help='Compare current compiler inputs and symbols against <NAME>.',
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
        '--diff-ir',
        dest='diff_ir_symbol',
        nargs='?',
        const='',
        default=None,
        metavar='SYMBOL',
        help=(
            'When used with --compare, include a unified diff of demangled '
            'LLVM IR functions (optionally filtered to SYMBOL, such as '
            'ByteSizeLong or MergeImpl).'),
    )
    parser.add_argument(
        '--diff-source',
        dest='should_diff_source',
        action='store_true',
        help=(
            'When used with --compare, include a unified diff of generated '
            '.pb.h/.pb.cc source files.'),
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
    parsed_arguments = parser.parse_args(command_line_arguments)
    if (parsed_arguments.diff_ir_symbol is not None
            or parsed_arguments.should_diff_source) and (
                not parsed_arguments.compare_name):
        parser.error('--diff-ir and --diff-source require --compare.')
    return parsed_arguments


def _ResolveDefaultClangBinary(repository_root: str) -> str:
    """Returns the absolute path to Chromium's bundled `clang++` binary."""
    return os.path.abspath(
        os.path.join(repository_root, *_DEFAULT_CLANG_RELATIVE_PATH.split('/')))


def _ResolveDefaultLlvmNmBinary(repository_root: str) -> str:
    """Returns the absolute path to Chromium's bundled `llvm-nm` binary."""
    return os.path.abspath(
        os.path.join(
            repository_root, *_DEFAULT_LLVM_NM_RELATIVE_PATH.split('/')))


def _ResolveDefaultLlvmCxxfiltBinary(repository_root: str) -> str:
    """Returns the absolute path to Chromium's bundled `llvm-cxxfilt`."""
    return os.path.abspath(
        os.path.join(
            repository_root, *_DEFAULT_LLVM_CXXFILT_RELATIVE_PATH.split('/')))


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


def _ResolveCodegenDiffFilter(
        parsed_arguments: argparse.Namespace) -> str | None:
    """Returns the requested codegen diff filter, or None if not requested."""
    if parsed_arguments.diff_ir_symbol is not None:
        return parsed_arguments.diff_ir_symbol
    if parsed_arguments.should_diff_source:
        return ''
    return None


def _CollectRequestedCodegenDiffs(
        parsed_arguments: argparse.Namespace,
        baseline_directory: str,
        build_directory: str,
        repository_root: str,
        translation_units: Sequence[str],
        clang_binary: str,
        llvm_cxxfilt_binary: str) -> tuple[CodegenDiffEntry, ...]:
    """Computes `--diff-ir` and `--diff-source` entries when requested."""
    should_diff_ir = parsed_arguments.diff_ir_symbol is not None
    if not should_diff_ir and not parsed_arguments.should_diff_source:
        return ()

    before_sources, before_ir = LoadCodegenArtifacts(
        baseline_directory, parsed_arguments.compare_name)
    after_sources, after_ir = CollectCodegenArtifactsForUnits(
        build_directory=build_directory,
        repository_root=repository_root,
        translation_units=translation_units,
        clang_binary=clang_binary,
        llvm_cxxfilt_binary=llvm_cxxfilt_binary,
    )
    diffs: list[CodegenDiffEntry] = []
    if parsed_arguments.should_diff_source:
        diffs.extend(ComputeGeneratedSourceDiffs(before_sources, after_sources))
    if should_diff_ir:
        diffs.extend(
            ComputeLlvmIrFunctionDiffs(
                before_ir, after_ir, parsed_arguments.diff_ir_symbol))
    return tuple(diffs)


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
    if sys.platform == 'darwin':
        print(
            'compile_size_probe.py: Mach-O object files do not record '
            'per-symbol byte sizes, so symbol size reporting is unavailable '
            'on macOS; use --diff-ir to inspect function-level codegen.',
            file=sys.stderr,
        )

    build_directory = os.path.abspath(parsed_arguments.build_directory)
    repository_root = _DEFAULT_REPOSITORY_ROOT
    clang_binary = _ResolveDefaultClangBinary(repository_root)
    llvm_nm_binary = _ResolveDefaultLlvmNmBinary(repository_root)
    llvm_cxxfilt_binary = _ResolveDefaultLlvmCxxfiltBinary(repository_root)
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
            if (parsed_arguments.diff_ir_symbol is not None
                    or parsed_arguments.should_diff_source):
                _RequireCodegenArtifactsPath(
                    baseline_directory, parsed_arguments.compare_name)

        translation_units = _SelectTranslationUnitsToMeasure(
            parsed_arguments, build_directory, repository_root, before_baseline)
        current_baseline = MeasureProbeBaseline(
            build_directory=build_directory,
            repository_root=repository_root,
            translation_units=translation_units,
            clang_binary=clang_binary,
            should_build_targets=not parsed_arguments.should_skip_build,
            llvm_nm_binary=llvm_nm_binary,
        )

        saved_path: str | None = None
        if parsed_arguments.save_name is not None:
            sources_by_unit, ir_by_unit = CollectCodegenArtifactsForUnits(
                build_directory=build_directory,
                repository_root=repository_root,
                translation_units=translation_units,
                clang_binary=clang_binary,
                llvm_cxxfilt_binary=llvm_cxxfilt_binary,
            )
            saved_path = SaveProbeBaseline(
                baseline=current_baseline,
                baseline_directory=baseline_directory,
                baseline_name=parsed_arguments.save_name,
                should_overwrite=parsed_arguments.should_overwrite,
            )
            SaveCodegenArtifacts(
                baseline_directory=baseline_directory,
                baseline_name=parsed_arguments.save_name,
                generated_sources_by_unit=sources_by_unit,
                llvm_ir_by_unit=ir_by_unit,
            )

        comparison_report: ProbeComparisonReport | None = None
        if before_baseline is not None:
            codegen_diffs = _CollectRequestedCodegenDiffs(
                parsed_arguments=parsed_arguments,
                baseline_directory=baseline_directory,
                build_directory=build_directory,
                repository_root=repository_root,
                translation_units=translation_units,
                clang_binary=clang_binary,
                llvm_cxxfilt_binary=llvm_cxxfilt_binary,
            )
            comparison_report = CompareProbeBaselines(
                baseline_name=parsed_arguments.compare_name,
                before_baseline=before_baseline,
                after_baseline=current_baseline,
                codegen_diffs=codegen_diffs,
                codegen_diff_filter=_ResolveCodegenDiffFilter(
                    parsed_arguments),
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
