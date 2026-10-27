#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Verifies //third_party/protobuf and sizes rolls to new upstream versions.

Chromium embeds upstream protobuf in //third_party/protobuf. Local changes
are applied to src/ and python/, and also recorded as top-level
patches/*.patch files (described in README.chromium). A roll replaces the tree
with a fresh upstream archive and replays patches/*.patch, so an edit to src/
that never made it into a patch is silently lost on the next roll
(https://crbug.com/568074905, for example https://crrev.com/c/8422202).
Design: https://crbug.com/568074175.

Subcommands:
  check [-C <out_dir>] [--baseline <name>]: Verifies that patches/*.patch
    reproduces //third_party/protobuf, README.chromium's patch descriptions
    match patches/, no .rej/.orig/conflict markers remain, and bootstrap files
    are up to date; also reports unfollowed src/file_lists.cmake changes and
    optional compile_size_probe.py deltas (https://crbug.com/568074591).
  plan --to <tag or commit>: Dry run of a roll in temporary repositories
    (https://crbug.com/568074498). Replays every patch onto the target with
    `git apply -3`, classifies it (see PatchReplayStatus), and lists upstream
    library files added or removed between the two versions.
  prepare --to <tag or commit>: Writes GITDIR/protobuf-roll/ (chain/*.patch
    with full blob IDs and state.json) and copies preimage blobs into the
    checkout without modifying the working tree.
  start --to <tag or commit>: Prepares GITDIR/protobuf-roll/, commits the
    unmodified upstream release, and replays patches/*.patch one commit at a
    time with `git apply -3`, stopping on the first conflict
    (https://crbug.com/568074155).
  status: Reports where an in-progress roll is and which commands are valid
    next.
  continue [--allow-extra-files]: Commits a resolved patch conflict, refreshes
    its .patch file, and continues replaying remaining patches
    (https://crbug.com/568074254).
  drop --reason <why>: Discards the current conflicting patch, deletes its
    .patch file, and continues replaying remaining patches.
  resume: Reconciles GITDIR/protobuf-roll/state.json with manual Apply/Drop
    commits and continues replaying remaining patches.
  generate -C <out_dir>: Runs gen_extra_chromium_files.py -C <out_dir>,
    force-stages gitignored *_pb2.py outputs alongside bootstrap files, and
    commits "Regenerate bootstrap files".
  abort: Resets the branch to the commit recorded by `start` and removes
    GITDIR/protobuf-roll/.

Exit codes: 0 when the step finished (for `plan`, whatever the per-patch
classification), 2 when `start`, `continue`, `drop`, or `resume` stopped on a
conflict (or for invalid CLI arguments), 1 when verification failed or an error
occurred.
"""

from collections.abc import Collection, Iterable, Mapping, Sequence
import argparse
import collections
import dataclasses
import enum
import filecmp
import functools
import hashlib
import importlib.util
import json
import os
import pathlib
import posixpath
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile

import check_file_lists

_PROTOBUF_DIRECTORY = os.path.dirname(os.path.abspath(__file__))
_README_FILENAME = 'README.chromium'
_UPSTREAM_FILE_LISTS_PATH = 'src/file_lists.cmake'
_REQUESTED_REVISION_REF = 'refs/protobuf-roll/requested'
UPSTREAM_REPOSITORY_URL = 'https://github.com/protocolbuffers/protobuf.git'
EXIT_CODE_SUCCESS = 0
EXIT_CODE_ERROR = 1
EXIT_CODE_CONFLICT = 2

# Top-level files that belong to Chromium rather than upstream. Must be exactly
# the files README.chromium roll step 2 restores (roll_protobuf_test.py checks).
CHROMIUM_OWNED_FILES = frozenset({
    'BUILD.gn',
    'DEPS',
    'DIR_METADATA',
    'OWNERS',
    'PRESUBMIT.py',
    'PRESUBMIT_test.py',
    'README.chromium',
    'check_file_lists.py',
    'check_file_lists_test.py',
    'compile_size_probe.py',
    'compile_size_probe_test.py',
    'gen_extra_chromium_files.py',
    'proto_library.gni',
    'proto_sources.gni',
    'proto_wkt.gni',
    'roll_protobuf.py',
    'roll_protobuf_test.py',
})

# Regenerated in roll step 4 by gen_extra_chromium_files.py with Chromium's
# patched protoc. The C++ outputs legitimately differ from upstream's copies;
# the Python outputs are not in the upstream archive at all.
BOOTSTRAP_GENERATED_FILES = frozenset({
    'src/google/protobuf/descriptor.pb.h',
    'src/google/protobuf/descriptor.pb.cc',
    'src/google/protobuf/compiler/plugin.pb.h',
    'src/google/protobuf/compiler/plugin.pb.cc',
    'python/google/protobuf/descriptor_pb2.py',
    'python/google/protobuf/compiler/plugin_pb2.py',
    'python/google/protobuf/internal/python_edition_defaults.py',
})

# Non-compiler files that gen_extra_chromium_files.py reads or bakes into
# BOOTSTRAP_GENERATED_FILES alongside src/google/protobuf/compiler/.
BOOTSTRAP_GENERATOR_EXACT_INPUTS = frozenset({
    'gen_extra_chromium_files.py',
    'python/build_targets.bzl',
    'python/google/protobuf/internal/python_edition_defaults.py.template',
    'src/google/protobuf/descriptor.proto',
    'src/google/protobuf/feature_resolver.cc',
    'src/google/protobuf/feature_resolver.h',
    'src/google/protobuf/generated_message_tctable_gen.cc',
    'src/google/protobuf/generated_message_tctable_gen.h',
    'src/google/protobuf/port_def.inc',
    'src/google/protobuf/runtime_version.h',
})
_BOOTSTRAP_INPUTS_TRAILER_KEY = 'Bootstrap-Inputs-Hash'

# Upstream ships pre-generated C++ for the Well-Known Types. Chromium deletes
# them in gen_extra_chromium_files.py and builds them from the .proto files
# (//third_party/protobuf:well_known_type_protos) instead.
WELL_KNOWN_TYPE_NAMES = ('any', 'api', 'duration', 'empty', 'field_mask',
                         'source_context', 'struct', 'timestamp', 'type',
                         'wrappers')
DELETED_WELL_KNOWN_TYPE_GENERATED_FILES = frozenset(
    f'src/google/protobuf/{name}.pb.{extension}'
    for name in WELL_KNOWN_TYPE_NAMES for extension in ('h', 'cc'))

# Deleted during rolls: large historical test artifacts trip presubmit checks.
DELETED_UPSTREAM_DIRECTORY_PREFIXES = ('compatibility/', )


class RollProtobufError(Exception):
    """Raised when a git, filesystem, or metadata operation cannot proceed."""


def IsChromiumOwnedFile(relative_path: str) -> bool:
    """Returns True for Chromium-owned files and top-level patches/*.{patch,md}.

    Upstream keeps its own Bazel patches in subdirectories such as
    patches/protobuf_v25/, so a path with a second '/' is upstream's.
    """
    directory, _, filename = relative_path.partition('/')
    return relative_path in CHROMIUM_OWNED_FILES or (directory == 'patches'
                                                     and '/' not in filename
                                                     and filename.endswith(
                                                         ('.patch', '.md')))


def IsIntentionallyDeletedUpstreamFile(relative_path: str) -> bool:
    return (relative_path.startswith(DELETED_UPSTREAM_DIRECTORY_PREFIXES)
            or relative_path in DELETED_WELL_KNOWN_TYPE_GENERATED_FILES)


def IsBootstrapGeneratorInput(relative_path: str) -> bool:
    """Returns True if editing relative_path requires `generate` to re-run."""
    return (relative_path not in BOOTSTRAP_GENERATED_FILES
            and (relative_path in BOOTSTRAP_GENERATOR_EXACT_INPUTS
                 or relative_path.startswith('src/google/protobuf/compiler/')))


def ReadReadmeRevision(protobuf_directory: str) -> str:
    readme_path = os.path.join(protobuf_directory, _README_FILENAME)
    if not os.path.isfile(readme_path):
        raise RollProtobufError(f'Missing {_README_FILENAME} at {readme_path}.')
    with open(readme_path, encoding='utf-8') as readme_file:
        match = re.search(r'^Revision:\s*(\S+)\s*$', readme_file.read(),
                          re.MULTILINE)
    if match is None:
        raise RollProtobufError(
            f'Could not find a "Revision:" line in {_README_FILENAME}.')
    return match.group(1)


def _IsolatedGitEnvironment() -> dict[str, str]:
    """Returns os.environ minus the variables that override '-C <repo>'.

    Git hooks and wrapper scripts set these, and child git commands would then
    ignore the repository they were pointed at.
    """
    overrides = ('GIT_ALTERNATE_OBJECT_DIRECTORIES', 'GIT_COMMON_DIR',
                 'GIT_DIR', 'GIT_INDEX_FILE', 'GIT_OBJECT_DIRECTORY',
                 'GIT_WORK_TREE')
    return {
        name: value
        for name, value in os.environ.items() if name not in overrides
    }


def _ScratchGitEnvironment() -> dict[str, str]:
    """Returns the environment for git commands whose output must not vary.

    Used to extract upstream, apply patches, and in throwaway repositories, so
    `check` and `plan` come out the same for every developer. All global,
    system and environment-injected config is dropped: `core.autocrlf` would
    rewrite `git archive` line endings, `apply.whitespace` would reject
    patches, and `core.hooksPath`, `commit.gpgsign`, `diff.noprefix` or
    `merge.conflictStyle` would change commits, diffs and conflict markers.
    """
    environment = {
        name: value
        for name, value in _IsolatedGitEnvironment().items()
        if name != 'GIT_CONFIG_PARAMETERS' and not name.startswith(
            ('GIT_CONFIG_COUNT', 'GIT_CONFIG_KEY_', 'GIT_CONFIG_VALUE_'))
    }
    environment.update(
        GIT_CONFIG_GLOBAL=os.devnull,
        GIT_CONFIG_NOSYSTEM='1',
        # Paths after `--` are file names, never glob patterns.
        GIT_LITERAL_PATHSPECS='1',
        # Commits in throwaway repositories work without a configured user.
        GIT_AUTHOR_NAME='roll_protobuf.py',
        GIT_AUTHOR_EMAIL='roll_protobuf@chromium.invalid',
        GIT_COMMITTER_NAME='roll_protobuf.py',
        GIT_COMMITTER_EMAIL='roll_protobuf@chromium.invalid',
    )
    return environment


def _GitCommandForRepository(repository_path: str,
                             git_arguments: Sequence[str]) -> list[str]:
    """Returns a git command line for a working-tree or bare repository.

    With `safe.bareRepository=explicit`, `git -C <bare repository>` fails, so
    repositories without a '.git' entry get `--git-dir=` instead.
    """
    if os.path.exists(os.path.join(repository_path, '.git')):
        return ['git', '-C', repository_path, *git_arguments]
    return ['git', f'--git-dir={repository_path}', *git_arguments]


def _RunGit(command: Sequence[str],
            *,
            env: dict[str, str] | None = None,
            input_bytes: bytes | None = None,
            accepted_return_codes: Collection[int] | None = (0, ),
            failure_message: str | None = None,
            cwd: str | None = None) -> subprocess.CompletedProcess:
    """Runs a git command line and returns the process, with byte output.

    Raises RollProtobufError, with git's stderr, if the exit code is not in
    accepted_return_codes (None accepts any exit code).
    """
    process = subprocess.run(
        command,
        env=env if env is not None else _IsolatedGitEnvironment(),
        input=input_bytes,
        stdin=None if input_bytes is not None else subprocess.DEVNULL,
        capture_output=True,
        cwd=cwd,
        check=False)
    if (accepted_return_codes is not None
            and process.returncode not in accepted_return_codes):
        message = failure_message or f'{" ".join(command)} failed'
        stderr = process.stderr.decode('utf-8', errors='replace').strip()
        raise RollProtobufError(f'{message}: {stderr}')
    return process


def _RunScratchGit(repository_directory: str, *git_arguments: str,
                   **options) -> subprocess.CompletedProcess:
    """Runs git in a throwaway repository; options are _RunGit's."""
    return _RunGit(['git', '-C', repository_directory, *git_arguments],
                   env=_ScratchGitEnvironment(),
                   **options)


def _SplitNulSeparatedPaths(output: bytes) -> list[str]:
    return [
        path for path in output.decode(
            'utf-8', errors='surrogateescape').split('\0') if path
    ]


def _ResolveCommitHash(repository_path: str, revision: str) -> str | None:
    process = _RunGit(_GitCommandForRepository(
        repository_path,
        ['rev-parse', '--verify', '--quiet', f'{revision}^{{commit}}']),
                      accepted_return_codes=None)
    return process.stdout.decode().strip() if process.returncode == 0 else None


def _IsValidGitRepository(repository_path: str) -> bool:
    return os.path.isdir(repository_path) and _RunGit(
        _GitCommandForRepository(repository_path, ['rev-parse', '--git-dir']),
        accepted_return_codes=None).returncode == 0


def ResolveDefaultCacheDirectory(
        build_directory: str | None = None,
        environment: dict[str, str] | None = None) -> str:
    """Returns the default path of the bare upstream cache repository.

    That is <build_directory>/.protobuf_roll/upstream-protobuf with -C, and
    otherwise chromium-protobuf-roll/upstream-protobuf under $XDG_CACHE_HOME
    (default ~/.cache; environment defaults to os.environ). Either survives
    reboots and branch switches without writing inside src/.git/.
    """
    if build_directory is not None:
        return os.path.join(os.path.abspath(build_directory), '.protobuf_roll',
                            'upstream-protobuf')
    environment = os.environ if environment is None else environment
    cache_home = environment.get('XDG_CACHE_HOME', '').strip() or '~/.cache'
    return os.path.join(os.path.abspath(os.path.expanduser(cache_home)),
                        'chromium-protobuf-roll', 'upstream-protobuf')


def EnsureUpstreamCommitCached(
        revision: str,
        upstream_repository_path: str | None = None,
        cache_directory: str | None = None,
        build_directory: str | None = None,
        upstream_url: str = UPSTREAM_REPOSITORY_URL) -> tuple[str, str]:
    """Returns (repository_path, commit ID) for an upstream revision.

    An upstream_repository_path (bare or not) is used as is, without network
    access. Otherwise a bare clone of upstream_url at cache_directory (default:
    ResolveDefaultCacheDirectory(build_directory)) is created on demand and
    fetched unless revision is a full commit ID it already has. Concurrent runs
    on one cache are unsupported: the fetch moves _REQUESTED_REVISION_REF.
    """
    if upstream_repository_path is not None:
        repository_path = os.path.abspath(upstream_repository_path)
        if not _IsValidGitRepository(repository_path):
            raise RollProtobufError(
                'Upstream repository path is not a git repository: '
                f'{repository_path}')
        commit = _ResolveCommitHash(repository_path, revision)
        if commit is None:
            raise RollProtobufError(f'Revision {revision} not found in '
                                    f'upstream repository {repository_path}.')
        return repository_path, commit

    cache = (os.path.abspath(cache_directory) if cache_directory is not None
             else ResolveDefaultCacheDirectory(build_directory))
    if not _IsValidGitRepository(cache):
        os.makedirs(os.path.dirname(cache), exist_ok=True)
        _RunGit(['git', 'clone', '--bare', upstream_url, cache],
                failure_message=(f'Failed to clone upstream protobuf from '
                                 f'{upstream_url} into {cache}'))
    commit = None
    if re.fullmatch(r'[0-9a-f]{40}', revision):
        commit = _ResolveCommitHash(cache, revision)
    if commit is None:
        _RunGit(_GitCommandForRepository(cache, [
            'fetch', '--tags', '--force', upstream_url,
            f'+{revision}:{_REQUESTED_REVISION_REF}'
        ]),
                failure_message=(f'Failed to fetch revision {revision} from '
                                 f'{upstream_url} into {cache}'))
        commit = _ResolveCommitHash(cache, _REQUESTED_REVISION_REF)
    if commit is None:
        raise RollProtobufError(
            f'Revision {revision} could not be resolved in cache {cache}.')
    return cache, commit


def ExtractUpstreamArchive(repository_path: str, commit_revision: str,
                           destination_directory: str) -> None:
    """Extracts the tree of commit_revision into destination_directory.

    stderr goes to a temporary file, not a pipe: trace output
    (GIT_TRACE_PACK_ACCESS=1) can fill a pipe and deadlock while tarfile
    drains stdout. tarfile's 'data' filter rejects unsafe paths and links.
    When extraction fails, git usually dies of SIGPIPE (-13, or 141 behind a
    shell shim), so the extraction error is reported together with git's
    stderr instead of keying on the exit code.
    """
    command = _GitCommandForRepository(
        repository_path, ['archive', '--format=tar', commit_revision])
    extraction_error = None
    with tempfile.TemporaryFile() as stderr_file:
        with subprocess.Popen(command,
                              env=_ScratchGitEnvironment(),
                              stdout=subprocess.PIPE,
                              stderr=stderr_file) as archive_process:
            try:
                with tarfile.open(fileobj=archive_process.stdout,
                                  mode='r|') as archive:
                    archive.extractall(destination_directory, filter='data')
            except (tarfile.TarError, OSError) as error:
                extraction_error = error
            finally:
                archive_process.stdout.close()
        stderr_file.seek(0)
        git_error = stderr_file.read().decode('utf-8',
                                              errors='replace').strip()
    return_code = archive_process.returncode
    if extraction_error is not None:
        git_suffix = (f' (git archive failed: {git_error})'
                      if return_code != 0 and git_error else '')
        raise RollProtobufError(
            f'Failed to extract archive for {commit_revision} from '
            f'{repository_path}: {extraction_error}{git_suffix}'
        ) from extraction_error
    if return_code != 0:
        raise RollProtobufError(f'git archive failed for {commit_revision} in '
                                f'{repository_path}: {git_error}')


def ListTopLevelPatchPaths(protobuf_directory: str) -> list[str]:
    """Returns the paths of patches/*.patch in application (sorted) order."""
    patches_directory = os.path.join(protobuf_directory, 'patches')
    if not os.path.isdir(patches_directory):
        return []
    patch_paths = (os.path.join(patches_directory, name)
                   for name in sorted(os.listdir(patches_directory))
                   if name.endswith('.patch'))
    return [path for path in patch_paths if os.path.isfile(path)]


def ApplyPatchFile(patch_path: str, target_directory: str) -> str | None:
    """Applies a patch; returns None on success, else git's diagnostics.

    `--unidiff-zero` accepts zero-context hunks (0072-remove-version-stamp).
    GIT_CEILING_DIRECTORIES keeps git from walking up into an enclosing work
    tree, where `git apply` silently skips paths outside the subdirectory and
    still exits 0.
    """
    target_directory = os.path.realpath(target_directory)
    process = _RunGit(
        ['git', 'apply', '-p1', '--unidiff-zero',
         os.path.abspath(patch_path)],
        env={
            **_ScratchGitEnvironment(),
            'GIT_CEILING_DIRECTORIES':
            os.path.dirname(target_directory),
        },
        cwd=target_directory,
        accepted_return_codes=None)
    if process.returncode == 0:
        return None
    return process.stderr.decode('utf-8', errors='replace').strip()


def _IsRegularFile(path: str) -> bool:
    return os.path.isfile(path) and not os.path.islink(path)


def _ListRegularFilesInDirectory(root_directory: str) -> set[str]:
    """Returns POSIX relative paths of regular files under root_directory."""
    relative_paths = set()
    for current_root, directory_names, filenames in os.walk(root_directory):
        directory_names[:] = [
            name for name in directory_names
            if name not in ('.git', '__pycache__')
        ]
        relative_paths.update(
            os.path.relpath(os.path.join(current_root, filename),
                            root_directory).replace('\\', '/')
            for filename in filenames if not filename.endswith('.pyc')
            and _IsRegularFile(os.path.join(current_root, filename)))
    return relative_paths


def _ListChromiumProtobufFiles(protobuf_directory: str) -> set[str]:
    """Returns the files in //third_party/protobuf that `git add` would see.

    That is tracked and untracked files minus .gitignore matches, with the
    developer's global `core.excludesFile` neutralized. README.chromium must be
    listed: otherwise protobuf_directory itself is git-ignored (a copy under
    out/, say) and every comparison would pass vacuously.
    """
    work_tree_process = _RunGit([
        'git', '-C', protobuf_directory, 'rev-parse', '--is-inside-work-tree'
    ],
                                accepted_return_codes=None)
    if (work_tree_process.returncode != 0
            or work_tree_process.stdout.decode().strip() != 'true'):
        raise RollProtobufError(
            f'{protobuf_directory} is not inside a git working tree; check '
            'needs git to apply .gitignore the way `git add` does.')
    listed_paths = _SplitNulSeparatedPaths(
        _RunGit([
            'git', '-C', protobuf_directory, '-c',
            f'core.excludesFile={os.devnull}', 'ls-files', '--cached',
            '--others', '--exclude-standard', '-z'
        ],
                failure_message=f'git ls-files failed in {protobuf_directory}'
                ).stdout)
    present_paths = {
        path
        for path in listed_paths
        if _IsRegularFile(os.path.join(protobuf_directory, path))
    }
    if _README_FILENAME not in present_paths:
        raise RollProtobufError(
            f'git ls-files returned no {_README_FILENAME} in '
            f'{protobuf_directory}; ensure the directory is not git-ignored.')
    return present_paths


def _FindGitIgnoredPaths(protobuf_directory: str,
                         candidate_paths: set[str]) -> set[str]:
    """Returns the candidate_paths that git ignores in protobuf_directory.

    Chromium's .gitignore drops 21 upstream C# and Xcode files (*.targets,
    *.props, *.sln, *.xcodeproj/*) when a release is `git add`ed. Without
    `--no-index`, a *tracked* file matching a pattern (src/solaris/libstdc++.la
    matches *.la) is still reported when it is deleted. Only the global
    `core.excludesFile` is neutralized; in-tree .gitignore files and
    $GIT_DIR/info/exclude stay active.
    """
    if not candidate_paths:
        return set()
    process = _RunGit(
        [
            'git', '-C', protobuf_directory, '-c',
            f'core.excludesFile={os.devnull}', 'check-ignore', '-z', '--stdin'
        ],
        input_bytes=''.join(f'{path}\0'
                            for path in sorted(candidate_paths)).encode(
                                'utf-8', errors='surrogateescape'),
        accepted_return_codes=(0, 1),
        failure_message=f'git check-ignore failed in {protobuf_directory}')
    return set(_SplitNulSeparatedPaths(process.stdout))


@dataclasses.dataclass(frozen=True)
class TreeVerificationResult:
    """Outcome of comparing //third_party/protobuf with patched upstream.

    modified_files and chromium_only_files omit Chromium-owned and
    bootstrap-generated files; upstream_only_files omits intentionally deleted
    and git-ignored files.
    """

    upstream_revision: str  # README.chromium's Revision.
    # Patches that applied before the first failure, or all of them.
    applied_patches: tuple[str, ...]
    # The first patch that failed to apply, and `git apply`'s diagnostics.
    failing_patch: str | None = None
    failing_patch_error: str | None = None
    modified_files: tuple[str, ...] = ()  # In both trees, bytes differ.
    chromium_only_files: tuple[str, ...] = ()
    upstream_only_files: tuple[str, ...] = ()

    def IsClean(self) -> bool:
        return (self.failing_patch is None and not self.modified_files
                and not self.chromium_only_files
                and not self.upstream_only_files)

    def ToDictionary(self) -> dict[str, object]:
        return {
            'is_clean': self.IsClean(),
            'upstream_revision': self.upstream_revision,
            'applied_patches': list(self.applied_patches),
            'failing_patch': self.failing_patch,
            'failing_patch_error': self.failing_patch_error,
            'modified_files': list(self.modified_files),
            'chromium_only_files': list(self.chromium_only_files),
            'upstream_only_files': list(self.upstream_only_files),
        }


def CheckTreeMatchesPatchedUpstream(
        protobuf_directory: str = _PROTOBUF_DIRECTORY,
        upstream_repository_path: str | None = None,
        cache_directory: str | None = None,
        build_directory: str | None = None,
        upstream_url: str = UPSTREAM_REPOSITORY_URL) -> TreeVerificationResult:
    """Verifies that upstream at Revision plus patches/*.patch is the tree.

    Extracts the upstream commit on README.chromium's 'Revision:' line,
    applies the top-level patches in sorted order, and compares the bytes of
    regular files with protobuf_directory (modes and symlinks are not
    compared). The other arguments are as for EnsureUpstreamCommitCached.
    """
    protobuf_directory = os.path.abspath(protobuf_directory)
    revision = ReadReadmeRevision(protobuf_directory)
    repository_path, commit = EnsureUpstreamCommitCached(
        revision, upstream_repository_path, cache_directory, build_directory,
        upstream_url)
    with tempfile.TemporaryDirectory(
            prefix='protobuf_roll_check_') as upstream_directory:
        ExtractUpstreamArchive(repository_path, commit, upstream_directory)
        applied_patches = []
        for patch_path in ListTopLevelPatchPaths(protobuf_directory):
            patch_name = os.path.basename(patch_path)
            patch_error = ApplyPatchFile(patch_path, upstream_directory)
            if patch_error is not None:
                return TreeVerificationResult(revision,
                                              tuple(applied_patches),
                                              failing_patch=patch_name,
                                              failing_patch_error=patch_error)
            applied_patches.append(patch_name)

        upstream_files = {
            path
            for path in _ListRegularFilesInDirectory(upstream_directory)
            if not IsIntentionallyDeletedUpstreamFile(path)
        }
        chromium_files = _ListChromiumProtobufFiles(protobuf_directory)
        # Never compared, but only exempt on the Chromium side: a deleted
        # upstream descriptor.pb.h is still reported as missing.
        exempt_files = {
            path
            for path in upstream_files | chromium_files
            if IsChromiumOwnedFile(path) or path in BOOTSTRAP_GENERATED_FILES
        }
        upstream_only_files = upstream_files - chromium_files
        return TreeVerificationResult(
            upstream_revision=revision,
            applied_patches=tuple(applied_patches),
            modified_files=tuple(
                path for path in sorted((upstream_files & chromium_files) -
                                        exempt_files)
                if not filecmp.cmp(os.path.join(upstream_directory, path),
                                   os.path.join(protobuf_directory, path),
                                   shallow=False)),
            chromium_only_files=tuple(
                sorted(chromium_files - upstream_files - exempt_files)),
            upstream_only_files=tuple(
                sorted(upstream_only_files - _FindGitIgnoredPaths(
                    protobuf_directory, upstream_only_files))),
        )


def FormatTreeVerificationReport(result: TreeVerificationResult) -> str:
    revision = result.upstream_revision
    if result.IsClean():
        return (f'Verified //third_party/protobuf matches upstream {revision} '
                f'plus {len(result.applied_patches)} patch(es).')
    if result.failing_patch is not None:
        lines = [
            f'Patch failed to apply to upstream {revision}: '
            f'{result.failing_patch}'
        ]
        if result.failing_patch_error:
            lines.append(result.failing_patch_error)
        return '\n'.join(lines)
    lines = [
        f'//third_party/protobuf does not match upstream {revision} plus '
        'patches/*.patch:'
    ]
    for heading, symbol, paths in (
        ('Modified files not explained by patches/', 'M',
         result.modified_files),
        ('Chromium-only files not in known exceptions', '+',
         result.chromium_only_files),
        ('Upstream files missing from //third_party/protobuf', '-',
         result.upstream_only_files),
    ):
        if paths:
            lines += ['', f'{heading} ({len(paths)}):']
            lines += [f'  {symbol} {path}' for path in paths]
    return '\n'.join(lines)


def _CommitEverything(repository_directory: str, message: str) -> None:
    # --force also commits files that .gitignore matches.
    _RunScratchGit(repository_directory, 'add', '--all', '--force', '.')
    _RunScratchGit(repository_directory, 'commit', '--quiet', '--no-verify',
                   '--allow-empty', '-m', message)


def _CreateScratchRepository(upstream_repository_path: str,
                             upstream_commit: str,
                             repository_directory: str) -> str:
    """Creates repository_directory holding upstream_commit as one commit.

    The tree comes from `git archive`, like a release tarball and like
    `check`, so export-ignore paths such as compatibility/ are left out.
    Returns the commit ID.
    """
    os.makedirs(repository_directory)
    ExtractUpstreamArchive(upstream_repository_path, upstream_commit,
                           repository_directory)
    _RunScratchGit(repository_directory, 'init', '--quiet')
    _CommitEverything(repository_directory,
                      f'Import upstream protobuf {upstream_commit}')
    return _RunScratchGit(repository_directory, 'rev-parse',
                          'HEAD').stdout.decode().strip()


@dataclasses.dataclass(frozen=True)
class ChainPatch:
    """One patches/*.patch file, re-expressed as a commit on old upstream."""

    patch_name: str  # File name in patches/.
    # `git diff --full-index --binary --no-renames`. The full blob IDs on every
    # `index` line are what let `git apply -3` merge on another version.
    diff: bytes
    # Maps each modified or deleted upstream-relative POSIX path to its blob ID
    # as of old upstream plus all earlier patches (`git apply -3` merge base).
    preimage_blobs: dict[str, str]
    # Lines added plus removed (`git diff --numstat`); binary files count 0.
    changed_line_count: int


@dataclasses.dataclass(frozen=True)
class PatchChain:
    """patches/*.patch applied in order to old upstream, one commit each."""

    # Scratch repository with the commits; the source of preimage blobs.
    repository_directory: str
    base_commit: str  # Unmodified old upstream.
    patches: tuple[ChainPatch, ...]


def _CountChangedLines(numstat_output: bytes) -> int:
    """Sums added and removed lines in `git diff --numstat -z` output.

    Binary files report '-' for both counts and contribute 0.
    """
    records = (record.split('\t') for record in numstat_output.decode(
        'utf-8', errors='surrogateescape').split('\0'))
    return sum(
        int(count) for fields in records if len(fields) >= 3
        for count in fields[:2] if count.isdigit())


def BuildPatchChain(upstream_repository_path: str, old_upstream_commit: str,
                    patch_paths: Sequence[str],
                    chain_directory: str) -> PatchChain:
    """Commits patch_paths in order onto old upstream (flow step 1).

    The patches apply exactly there. Re-exporting each commit with
    `git diff --full-index` attaches the full blob IDs that `git apply -3`
    needs to merge the patch onto another version. chain_directory must not
    exist yet; the caller owns it, as the source of preimage blobs.
    """
    base_commit = _CreateScratchRepository(upstream_repository_path,
                                           old_upstream_commit,
                                           chain_directory)
    head_range = ('--no-renames', '--no-color', '--no-ext-diff', 'HEAD~1',
                  'HEAD')
    chain_patches = []
    for patch_path in patch_paths:
        patch_name = os.path.basename(patch_path)
        patch_error = ApplyPatchFile(patch_path, chain_directory)
        if patch_error is not None:
            raise RollProtobufError(
                f'{patch_name} does not apply to upstream '
                f'{old_upstream_commit}, the README.chromium Revision; run '
                f'`roll_protobuf.py check` to diagnose: {patch_error}')
        _CommitEverything(chain_directory, patch_name)
        diff = _RunScratchGit(chain_directory, 'diff', '--full-index',
                              '--binary', '--no-textconv', *head_range)
        diff_tree_fields = _RunScratchGit(
            chain_directory, 'diff-tree', '-r', '-z', '--no-abbrev',
            '--no-renames', 'HEAD~1',
            'HEAD').stdout.decode('utf-8',
                                  errors='surrogateescape').split('\0')
        # Records are ':<old mode> <new mode> <old blob> <new blob> <status>',
        # NUL, path. Added files have an all-zero old blob and no preimage.
        preimage_blobs = {
            path: metadata.split(' ')[2]
            for metadata, path in zip(diff_tree_fields[::2],
                                      diff_tree_fields[1::2])
            if set(metadata.split(' ')[2]) != {'0'}
        }
        numstat = _RunScratchGit(chain_directory, 'diff', '--numstat', '-z',
                                 *head_range)
        chain_patches.append(
            ChainPatch(patch_name, diff.stdout, preimage_blobs,
                       _CountChangedLines(numstat.stdout)))
    return PatchChain(chain_directory, base_commit, tuple(chain_patches))


def CopyPreimageBlobs(source_repository_directory: str,
                      destination_repository_directory: str,
                      blob_ids: Iterable[str]) -> tuple[str, ...]:
    """Copies the blobs the destination lacks (flow step 2).

    A blob ID depends only on content, so `git cat-file blob` piped to
    `git hash-object -w --stdin` recreates the identical object. Returns the
    sorted IDs of the copied blobs.
    """
    missing_blob_ids = tuple(
        blob_id for blob_id in sorted(set(blob_ids))
        if _RunScratchGit(destination_repository_directory,
                          'cat-file',
                          '-e',
                          blob_id,
                          accepted_return_codes=(0, 1, 128)).returncode != 0)
    for blob_id in missing_blob_ids:
        blob = _RunScratchGit(source_repository_directory, 'cat-file', 'blob',
                              blob_id).stdout
        _RunScratchGit(destination_repository_directory,
                       'hash-object',
                       '-w',
                       '--stdin',
                       '--no-filters',
                       input_bytes=blob)
    return missing_blob_ids


class PatchReplayStatus(enum.Enum):
    """How one chain patch fared on the new upstream version."""

    CLEAN = 'CLEAN'  # Applied without conflicts.
    EMPTY = 'EMPTY'  # Applied, changed nothing: it is already upstream.
    CONFLICT = 'CONFLICT'  # The three-way merge left conflict markers.
    # A file the patch modifies or deletes is gone. `git apply -3` does not
    # follow renames, so an upstream rename lands here too.
    MISSING_FILE = 'MISSING_FILE'
    DROPPED = 'DROPPED'  # Explicitly dropped via `roll_protobuf.py drop`.
    ERROR = 'ERROR'  # `git apply` failed for any other reason.


@dataclasses.dataclass(frozen=True)
class ConflictedFile:
    path: str  # POSIX path relative to the repository root.
    conflict_region_count: int  # Number of '<<<<<<< ' markers in the file.


@dataclasses.dataclass(frozen=True)
class PatchReplayResult:
    """Outcome of applying one ChainPatch to the new upstream version."""

    patch_name: str
    status: PatchReplayStatus
    changed_line_count_before: int  # Lines added plus removed on old upstream.
    # The same on new upstream, for CLEAN only. A patch that shrank a lot may
    # have been partly upstreamed.
    changed_line_count_after: int | None = None
    conflicted_files: tuple[ConflictedFile, ...] = ()  # For CONFLICT.
    # Upstream-relative paths the new version lacks, for MISSING_FILE.
    missing_files: tuple[str, ...] = ()
    # Drop reason for DROPPED, or `git apply` diagnostics for ERROR.
    error_message: str | None = None

    def ToDictionary(self) -> dict[str, object]:
        return {
            'patch':
            self.patch_name,
            'status':
            self.status.value,
            'changed_lines_before':
            self.changed_line_count_before,
            'changed_lines_after':
            self.changed_line_count_after,
            'conflicted_files': [{
                'path':
                conflicted_file.path,
                'conflict_regions':
                conflicted_file.conflict_region_count
            } for conflicted_file in self.conflicted_files],
            'missing_files':
            list(self.missing_files),
            'error':
            self.error_message,
        }


def _CountConflictRegions(file_path: str) -> int:
    if not os.path.isfile(file_path):
        return 0
    with open(file_path, 'rb') as conflicted_file:
        return sum(1 for line in conflicted_file
                   if line.startswith(b'<<<<<<< '))


def ApplyChainPatchThreeWay(
        repository_directory: str,
        chain_patch: ChainPatch,
        directory_prefix: str | None = None) -> PatchReplayResult:
    """Applies chain_patch with `git apply -3` and classifies it (step 3).

    Leaves the index and working tree as `git apply -3` left them, so the
    caller commits a CLEAN result, stops for a human on CONFLICT, or resets to
    skip the patch. Call CopyPreimageBlobs first, or the three-way fallback
    has no base to merge from. directory_prefix is where the upstream root is
    in the repository, such as 'third_party/protobuf' in a Chromium checkout,
    or None for the root.
    """
    result = functools.partial(
        PatchReplayResult,
        chain_patch.patch_name,
        changed_line_count_before=chain_patch.changed_line_count)
    # git reports index paths without './' or a trailing '/'.
    directory_prefix = posixpath.normpath(directory_prefix or '.')
    directory_prefix = '' if directory_prefix == '.' else directory_prefix
    upstream_path_by_repository_path = {
        posixpath.join(directory_prefix, upstream_path): upstream_path
        for upstream_path in chain_patch.preimage_blobs
    }
    if upstream_path_by_repository_path:
        indexed_paths = set(
            _SplitNulSeparatedPaths(
                _RunScratchGit(
                    repository_directory, 'ls-files', '-z', '--',
                    *sorted(upstream_path_by_repository_path)).stdout))
        missing_files = tuple(
            sorted(upstream_path for repository_path, upstream_path in
                   upstream_path_by_repository_path.items()
                   if repository_path not in indexed_paths))
        if missing_files:
            return result(PatchReplayStatus.MISSING_FILE,
                          missing_files=missing_files)

    directory_option = ([f'--directory={directory_prefix}']
                        if directory_prefix else [])
    apply_process = _RunScratchGit(repository_directory,
                                   'apply',
                                   '-3',
                                   *directory_option,
                                   '-',
                                   input_bytes=chain_patch.diff,
                                   accepted_return_codes=None)
    if apply_process.returncode == 0:
        staged_numstat = _RunScratchGit(repository_directory, 'diff',
                                        '--cached', '--numstat', '-z',
                                        '--no-renames', '--no-ext-diff').stdout
        if not staged_numstat:
            return result(PatchReplayStatus.EMPTY)
        return result(
            PatchReplayStatus.CLEAN,
            changed_line_count_after=_CountChangedLines(staged_numstat))
    unmerged_paths = _SplitNulSeparatedPaths(
        _RunScratchGit(repository_directory, 'diff', '--name-only', '-z',
                       '--diff-filter=U').stdout)
    if unmerged_paths:
        return result(
            PatchReplayStatus.CONFLICT,
            conflicted_files=tuple(
                ConflictedFile(
                    path,
                    _CountConflictRegions(
                        os.path.join(repository_directory, *path.split('/'))))
                for path in sorted(set(unmerged_paths))))
    return result(PatchReplayStatus.ERROR,
                  error_message=apply_process.stderr.decode(
                      'utf-8', errors='replace').strip())


@dataclasses.dataclass(frozen=True)
class RollPlan:
    """Dry-run result of rolling //third_party/protobuf to a new version."""

    old_revision: str  # README.chromium's Revision.
    old_commit: str
    new_revision: str  # As passed to `plan --to`.
    new_commit: str
    patch_results: tuple[PatchReplayResult, ...]  # In patches/ order.
    # Preimage blobs copied into the replay repository; a roll in the
    # checkout copies the same blobs into Chromium's object store.
    copied_blob_count: int
    # Upstream library files added or removed that GN does not follow yet, or
    # None if either version lacks src/file_lists.cmake.
    file_lists_diff: check_file_lists.FileListsDiff | None

    def CountPatchesByStatus(self) -> dict[PatchReplayStatus, int]:
        return dict(
            collections.Counter(patch_result.status
                                for patch_result in self.patch_results))

    def ToDictionary(self) -> dict[str, object]:
        return {
            'old_revision':
            self.old_revision,
            'old_commit':
            self.old_commit,
            'new_revision':
            self.new_revision,
            'new_commit':
            self.new_commit,
            'patch_counts': {
                status.value: count
                for status, count in self.CountPatchesByStatus().items()
            },
            'patches':
            [result.ToDictionary() for result in self.patch_results],
            'copied_blob_count':
            self.copied_blob_count,
            'file_lists': (self.file_lists_diff.ToDictionary()
                           if self.file_lists_diff is not None else None),
        }


def _DiffFileListsAgainstGitSpec(
        protobuf_directory: str, old_repository: str, old_spec: str,
        new_tree_directory: str) -> check_file_lists.FileListsDiff | None:
    new_cmake_path = os.path.join(new_tree_directory,
                                  *_UPSTREAM_FILE_LISTS_PATH.split('/'))
    if not os.path.isfile(new_cmake_path) or not all(
            os.path.isfile(os.path.join(protobuf_directory, name))
            for name in ('proto_sources.gni', 'BUILD.gn')):
        return None
    old_cmake = _RunScratchGit(old_repository,
                               'cat-file',
                               'blob',
                               old_spec,
                               accepted_return_codes=None)
    if old_cmake.returncode != 0:
        return None
    return check_file_lists.ComputeFileListsDiff(
        old_cmake_text=old_cmake.stdout.decode('utf-8', errors='replace'),
        new_cmake_text=pathlib.Path(new_cmake_path).read_text(
            encoding='utf-8', errors='replace'),
        chromium_gn_files=check_file_lists.ReadChromiumGnSourceFiles(
            protobuf_directory),
        new_tree_directory=new_tree_directory)


def _ComputeUpstreamFileListsDiff(
        protobuf_directory: str, patch_chain: PatchChain,
        replay_directory: str) -> check_file_lists.FileListsDiff | None:
    """Diffs src/file_lists.cmake of old and new upstream against GN.

    The new tree is the replay repository after the CLEAN patches, so files a
    patch adds are on disk and do not show up as removed.
    """
    return _DiffFileListsAgainstGitSpec(
        protobuf_directory, patch_chain.repository_directory,
        f'{patch_chain.base_commit}:{_UPSTREAM_FILE_LISTS_PATH}',
        replay_directory)


def PlanRoll(to_revision: str,
             protobuf_directory: str = _PROTOBUF_DIRECTORY,
             upstream_repository_path: str | None = None,
             cache_directory: str | None = None,
             build_directory: str | None = None,
             upstream_url: str = UPSTREAM_REPOSITORY_URL) -> RollPlan:
    """Sizes a roll to to_revision without touching the checkout.

    Builds the patch chain on README.chromium's Revision and replays it onto
    to_revision in a second scratch repository, all in a temporary directory.
    CLEAN patches are committed and every other patch is skipped (reset), so
    later patches are measured against new upstream plus CLEAN patches only.
    Taking the patch side of conflicts instead gave the same classification
    for all 24 patches in the prototype (https://crbug.com/568074498), and
    skipping invents no content. The other arguments are as for
    EnsureUpstreamCommitCached.
    """
    protobuf_directory = os.path.abspath(protobuf_directory)
    upstream_arguments = (upstream_repository_path, cache_directory,
                          build_directory, upstream_url)
    old_revision = ReadReadmeRevision(protobuf_directory)
    repository_path, old_commit = EnsureUpstreamCommitCached(
        old_revision, *upstream_arguments)
    _, new_commit = EnsureUpstreamCommitCached(to_revision,
                                               *upstream_arguments)
    with tempfile.TemporaryDirectory(
            prefix='protobuf_roll_plan_') as temporary_directory:
        patch_chain = BuildPatchChain(
            repository_path, old_commit,
            ListTopLevelPatchPaths(protobuf_directory),
            os.path.join(temporary_directory, 'chain'))
        replay_directory = os.path.join(temporary_directory, 'replay')
        _CreateScratchRepository(repository_path, new_commit, replay_directory)
        patch_results = []
        copied_blob_count = 0
        for chain_patch in patch_chain.patches:
            copied_blob_count += len(
                CopyPreimageBlobs(patch_chain.repository_directory,
                                  replay_directory,
                                  chain_patch.preimage_blobs.values()))
            patch_result = ApplyChainPatchThreeWay(replay_directory,
                                                   chain_patch)
            if patch_result.status is PatchReplayStatus.CLEAN:
                _RunScratchGit(replay_directory, 'commit', '--quiet',
                               '--no-verify', '-m',
                               f'Apply patches/{chain_patch.patch_name}')
            else:
                _RunScratchGit(replay_directory, 'reset', '--quiet', '--hard',
                               'HEAD')
                _RunScratchGit(replay_directory, 'clean', '-ffdxq')
            patch_results.append(patch_result)
        file_lists_diff = _ComputeUpstreamFileListsDiff(
            protobuf_directory, patch_chain, replay_directory)
    return RollPlan(old_revision, old_commit, to_revision, new_commit,
                    tuple(patch_results), copied_blob_count, file_lists_diff)


def _DescribePatchReplayResult(result: PatchReplayResult) -> str:
    if result.status is PatchReplayStatus.CLEAN:
        return (f'{result.changed_line_count_before} -> '
                f'{result.changed_line_count_after} changed lines')
    if result.status is PatchReplayStatus.EMPTY:
        return 'already upstream'
    if result.status is PatchReplayStatus.DROPPED:
        return (f'dropped ({result.error_message})'
                if result.error_message else 'dropped')
    if result.status is PatchReplayStatus.CONFLICT:
        # `git apply -3` writes no conflict markers into binary files.
        return ', '.join(
            f'{conflicted.path} ({conflicted.conflict_region_count} region(s))'
            if conflicted.conflict_region_count else
            f'{conflicted.path} (no conflict markers, binary?)'
            for conflicted in result.conflicted_files)
    if result.status is PatchReplayStatus.MISSING_FILE:
        return ('not in new upstream (renamed or deleted?): ' +
                ', '.join(result.missing_files))
    return result.error_message or 'git apply failed'


def FormatRollPlanReport(roll_plan: RollPlan) -> str:
    status_width = max(len(status.value) for status in PatchReplayStatus)
    lines = [
        f'Plan for rolling //third_party/protobuf from '
        f'{roll_plan.old_revision} to {roll_plan.new_revision} '
        f'({roll_plan.new_commit}):', ''
    ]
    lines += [
        f'  {result.status.value:<{status_width}}  {result.patch_name}: '
        f'{_DescribePatchReplayResult(result)}'
        for result in roll_plan.patch_results
    ]
    counts = roll_plan.CountPatchesByStatus()
    count_summary = ', '.join(f'{counts[status]} {status.value}'
                              for status in PatchReplayStatus
                              if status in counts)
    lines += [
        '', f'{count_summary or "No patches"}; copied '
        f'{roll_plan.copied_blob_count} preimage blob(s).', ''
    ]
    if roll_plan.file_lists_diff is None:
        lines.append(f'Skipped the upstream file-list report: '
                     f'{_UPSTREAM_FILE_LISTS_PATH} is missing from the old or '
                     'new version.')
    else:
        lines.append(
            check_file_lists.FormatFileListsDiffReport(
                roll_plan.file_lists_diff))
    return '\n'.join(lines)


@dataclasses.dataclass(frozen=True)
class _CheckoutPaths:
    protobuf_directory: str
    repository_root: str
    # POSIX path of protobuf_directory in repository_root, or '' at the root.
    protobuf_prefix: str
    roll_state_directory: str
    state_file_path: str
    chain_directory: str


def _ResolveCheckoutPaths(protobuf_directory: str) -> _CheckoutPaths:
    protobuf_directory = os.path.abspath(protobuf_directory)
    process = _RunGit([
        'git', '-C', protobuf_directory, 'rev-parse', '--show-toplevel',
        '--absolute-git-dir', '--show-prefix'
    ],
                      accepted_return_codes=None)
    if process.returncode != 0:
        raise RollProtobufError(
            f'{protobuf_directory} is not inside a git working tree.')
    repository_root, git_directory, prefix = (
        line.rstrip('\r') for line in process.stdout.decode().split('\n')[:3])
    repository_root = os.path.normpath(repository_root)
    git_directory = os.path.normpath(git_directory)
    prefix = prefix.rstrip('/')
    canonical_protobuf = (os.path.join(repository_root, *prefix.split('/'))
                          if prefix else repository_root)
    roll_state_directory = os.path.join(git_directory, 'protobuf-roll')
    return _CheckoutPaths(
        canonical_protobuf,
        repository_root,
        prefix,
        roll_state_directory,
        os.path.join(roll_state_directory, 'state.json'),
        os.path.join(roll_state_directory, 'chain'),
    )


_ROLL_STATE_SCALAR_KEYS = (
    'phase',
    'start_commit',
    'expected_head',
    'old_revision',
    'new_revision',
    'new_commit',
    'copied_blob_count',
    'current_index',
)


@dataclasses.dataclass(frozen=True)
class RollState:
    """Saved state of an in-progress roll under GITDIR/protobuf-roll/."""

    phase: str  # 'prepared', 'conflict', or 'replayed'.
    start_commit: str  # HEAD before the roll modified the branch.
    expected_head: str  # HEAD most recently written by the script.
    old_revision: str
    new_revision: str
    new_commit: str
    copied_blob_count: int
    current_index: int  # 0-based index into patches.
    patches: tuple[ChainPatch, ...]
    conflict_files: tuple[str, ...] = ()
    patch_results: tuple[PatchReplayResult, ...] = ()

    def ToDictionary(self) -> dict[str, object]:
        return {
            **{
                key: getattr(self, key)
                for key in _ROLL_STATE_SCALAR_KEYS
            },
            'conflict_files':
            list(self.conflict_files),
            'patches': [{
                'patch': patch.patch_name,
                'preimage_blobs': patch.preimage_blobs,
                'changed_lines_before': patch.changed_line_count,
            } for patch in self.patches],
            'patch_results':
            [result.ToDictionary() for result in self.patch_results],
        }


def _SaveRollState(checkout: _CheckoutPaths, state: RollState) -> None:
    os.makedirs(checkout.roll_state_directory, exist_ok=True)
    pathlib.Path(checkout.state_file_path).write_text(
        json.dumps(state.ToDictionary(), indent=2) + '\n',
        encoding='utf-8',
        newline='\n')


def LoadRollState(
        protobuf_directory: str = _PROTOBUF_DIRECTORY) -> RollState | None:
    """Loads GITDIR/protobuf-roll/state.json and chain/*.patch, or None."""
    checkout = _ResolveCheckoutPaths(protobuf_directory)
    if not os.path.isfile(checkout.state_file_path):
        return None
    try:
        raw = json.loads(
            pathlib.Path(checkout.state_file_path).read_text(encoding='utf-8'))
        patches = tuple(
            ChainPatch(
                entry['patch'],
                pathlib.Path(checkout.chain_directory,
                             entry['patch']).read_bytes(),
                dict(entry['preimage_blobs']),
                entry['changed_lines_before'],
            ) for entry in raw['patches'])
        patch_results = tuple(
            PatchReplayResult(
                entry['patch'],
                PatchReplayStatus(entry['status']),
                entry['changed_lines_before'],
                entry['changed_lines_after'],
                tuple(
                    ConflictedFile(item['path'], item['conflict_regions'])
                    for item in entry['conflicted_files']),
                tuple(entry['missing_files']),
                entry['error'],
            ) for entry in raw.get('patch_results', ()))
        return RollState(**{key: raw[key]
                            for key in _ROLL_STATE_SCALAR_KEYS},
                         patches=patches,
                         conflict_files=tuple(raw.get('conflict_files', ())),
                         patch_results=patch_results)
    except (json.JSONDecodeError, KeyError, OSError, TypeError,
            ValueError) as error:
        raise RollProtobufError(
            f'Corrupt roll state in {checkout.state_file_path}: {error}'
        ) from error


def PrepareRoll(to_revision: str,
                protobuf_directory: str = _PROTOBUF_DIRECTORY,
                upstream_repository_path: str | None = None,
                cache_directory: str | None = None,
                build_directory: str | None = None,
                upstream_url: str = UPSTREAM_REPOSITORY_URL) -> RollState:
    """Prepares GITDIR/protobuf-roll/ without changing the working tree.

    Builds the patch chain on README.chromium's Revision, copies missing
    preimage blobs into the checkout's object store, and writes
    chain/*.patch and state.json under GITDIR/protobuf-roll/.
    """
    checkout = _ResolveCheckoutPaths(protobuf_directory)
    existing_state = LoadRollState(checkout.protobuf_directory)
    if existing_state is not None and (
        (existing_state.phase == 'conflict'
         and _FindLocalRollBranch(checkout, existing_state) is not None) or
        (existing_state.phase == 'replayed'
         and _IsRollBranchHead(checkout, existing_state))):
        raise RollProtobufError(
            f'A roll to {existing_state.new_revision} is already in '
            f'progress (phase: {existing_state.phase}); run '
            '`roll_protobuf.py status` or `roll_protobuf.py abort` first.')
    upstream_arguments = (upstream_repository_path, cache_directory,
                          build_directory, upstream_url)
    old_revision = ReadReadmeRevision(checkout.protobuf_directory)
    repository_path, old_commit = EnsureUpstreamCommitCached(
        old_revision, *upstream_arguments)
    _, new_commit = EnsureUpstreamCommitCached(to_revision,
                                               *upstream_arguments)
    head_commit = _RunScratchGit(checkout.repository_root, 'rev-parse',
                                 'HEAD').stdout.decode().strip()
    with tempfile.TemporaryDirectory(
            prefix='protobuf_roll_prepare_') as temporary_directory:
        patch_chain = BuildPatchChain(
            repository_path, old_commit,
            ListTopLevelPatchPaths(checkout.protobuf_directory),
            os.path.join(temporary_directory, 'chain'))
        all_preimage_blobs = (blob_id for patch in patch_chain.patches
                              for blob_id in patch.preimage_blobs.values())
        copied_blobs = CopyPreimageBlobs(patch_chain.repository_directory,
                                         checkout.repository_root,
                                         all_preimage_blobs)
    prior_copied_count = (existing_state.copied_blob_count if existing_state
                          and existing_state.old_revision == old_revision else
                          0)
    shutil.rmtree(checkout.roll_state_directory, ignore_errors=True)
    os.makedirs(checkout.chain_directory, exist_ok=True)
    for chain_patch in patch_chain.patches:
        pathlib.Path(checkout.chain_directory,
                     chain_patch.patch_name).write_bytes(chain_patch.diff)
    state = RollState(
        phase='prepared',
        start_commit=head_commit,
        expected_head=head_commit,
        old_revision=old_revision,
        new_revision=to_revision,
        new_commit=new_commit,
        copied_blob_count=prior_copied_count + len(copied_blobs),
        current_index=0,
        patches=patch_chain.patches,
    )
    _SaveRollState(checkout, state)
    return state


def _UpdateReadmeMetadata(protobuf_directory: str, to_revision: str,
                          new_commit: str) -> None:
    version_match = re.match(r'^(?:refs/tags/)?v?(\d+\.\d+\S*)$', to_revision)
    version = version_match.group(1) if version_match else None
    version_path = pathlib.Path(protobuf_directory, 'version.json')
    if version is None and version_path.is_file():
        match = re.search(r'"protoc_version"\s*:\s*"([^"]+)"',
                          version_path.read_text(encoding='utf-8'))
        version = match.group(1) if match else None
    readme_path = pathlib.Path(protobuf_directory, _README_FILENAME)
    text = readme_path.read_text(encoding='utf-8')
    replacements = [('Revision', new_commit)]
    if version is not None:
        replacements = [
            ('Version', version),
            ('CPEPrefix', f'cpe:/a:google:protobuf:{version}'),
            *replacements,
        ]
    for field, value in replacements:
        text = re.sub(rf'(?m)^{field}:\s*\S+[ \t]*$',
                      f'{field}: {value}',
                      text,
                      count=1)
    readme_path.write_text(text, encoding='utf-8', newline='\n')


def ImportUpstreamRelease(protobuf_directory: str,
                          upstream_repository_path: str, to_revision: str,
                          new_commit: str) -> str:
    """Replaces upstream files in protobuf_directory and commits step 2a.

    Keeps Chromium-owned files (including top-level patches/*.{patch,md},
    but not upstream's patches/protobuf_vNN/ subdirectories) and existing
    bootstrap-generated files, omits compatibility/, pre-generated
    Well-Known Type .pb.{h,cc} files, and files excluded by .gitignore,
    updates README.chromium, and commits the result. Returns the new HEAD
    commit ID.
    """
    checkout = _ResolveCheckoutPaths(protobuf_directory)
    with tempfile.TemporaryDirectory(
            prefix='protobuf_roll_import_') as archive_directory:
        ExtractUpstreamArchive(upstream_repository_path, new_commit,
                               archive_directory)
        for relative_path in _ListRegularFilesInDirectory(
                checkout.protobuf_directory):
            if (not IsChromiumOwnedFile(relative_path)
                    and relative_path not in BOOTSTRAP_GENERATED_FILES):
                os.remove(
                    os.path.join(checkout.protobuf_directory,
                                 *relative_path.split('/')))
        imported_paths = set()
        for relative_path in sorted(
                _ListRegularFilesInDirectory(archive_directory)):
            if (IsChromiumOwnedFile(relative_path)
                    or IsIntentionallyDeletedUpstreamFile(relative_path)):
                continue
            destination_path = os.path.join(checkout.protobuf_directory,
                                            *relative_path.split('/'))
            os.makedirs(os.path.dirname(destination_path), exist_ok=True)
            shutil.copy2(
                os.path.join(archive_directory, *relative_path.split('/')),
                destination_path)
            imported_paths.add(relative_path)
    for ignored_path in _FindGitIgnoredPaths(checkout.protobuf_directory,
                                             imported_paths):
        os.remove(
            os.path.join(checkout.protobuf_directory,
                         *ignored_path.split('/')))
    for current_root, _, filenames in os.walk(checkout.protobuf_directory,
                                              topdown=False):
        if '.git' in current_root.split(os.sep):
            continue
        if (current_root != checkout.protobuf_directory and not filenames
                and not os.listdir(current_root)):
            os.rmdir(current_root)
    _UpdateReadmeMetadata(checkout.protobuf_directory, to_revision, new_commit)
    _RunScratchGit(checkout.repository_root, 'add', '--all', '--',
                   checkout.protobuf_directory)
    _RunScratchGit(checkout.repository_root, 'commit', '--quiet',
                   '--no-verify', '--allow-empty', '-m',
                   f'Import upstream protobuf {to_revision} (unmodified)')
    return _RunScratchGit(checkout.repository_root, 'rev-parse',
                          'HEAD').stdout.decode().strip()


def _RefreshPatchFileFromHeadCommit(checkout: _CheckoutPaths,
                                    patch_name: str) -> None:
    """Rewrites patches/<patch_name> from HEAD~1..HEAD and amends HEAD."""
    relative_option = ([f'--relative={checkout.protobuf_prefix}']
                       if checkout.protobuf_prefix else [])
    refreshed_diff = _RunScratchGit(checkout.repository_root, 'diff',
                                    '--binary', '--no-renames', '--no-color',
                                    '--no-ext-diff', '--no-textconv',
                                    *relative_option, 'HEAD~1', 'HEAD').stdout
    patch_file_path = os.path.join(checkout.protobuf_directory, 'patches',
                                   patch_name)
    pathlib.Path(patch_file_path).write_bytes(refreshed_diff)
    _RunScratchGit(checkout.repository_root, 'add', '--', patch_file_path)
    _RunScratchGit(checkout.repository_root, 'commit', '--quiet',
                   '--no-verify', '--amend', '--no-edit')


def _ReplayRemainingPatches(checkout: _CheckoutPaths,
                            state: RollState) -> RollState:
    patch_results = list(state.patch_results)
    expected_head = state.expected_head
    for index in range(state.current_index, len(state.patches)):
        chain_patch = state.patches[index]
        patch_result = ApplyChainPatchThreeWay(
            checkout.repository_root,
            chain_patch,
            directory_prefix=checkout.protobuf_prefix)
        if patch_result.status is PatchReplayStatus.CLEAN:
            _RunScratchGit(checkout.repository_root, 'commit', '--quiet',
                           '--no-verify', '-m',
                           f'Apply patches/{chain_patch.patch_name}')
            _RefreshPatchFileFromHeadCommit(checkout, chain_patch.patch_name)
        elif patch_result.status is PatchReplayStatus.EMPTY:
            patch_file_path = os.path.join(checkout.protobuf_directory,
                                           'patches', chain_patch.patch_name)
            _RunScratchGit(checkout.repository_root, 'rm', '--quiet', '--',
                           patch_file_path)
            _RunScratchGit(
                checkout.repository_root, 'commit', '--quiet', '--no-verify',
                '-m',
                f'Drop patches/{chain_patch.patch_name} (already upstream)')
        elif patch_result.status in (PatchReplayStatus.CONFLICT,
                                     PatchReplayStatus.MISSING_FILE):
            patch_results.append(patch_result)
            conflict_files = tuple(
                conflicted.path
                for conflicted in patch_result.conflicted_files) or tuple(
                    posixpath.join(checkout.protobuf_prefix, path) if checkout.
                    protobuf_prefix else path
                    for path in patch_result.missing_files)
            conflict_state = dataclasses.replace(
                state,
                phase='conflict',
                expected_head=expected_head,
                current_index=index,
                conflict_files=conflict_files,
                patch_results=tuple(patch_results))
            _SaveRollState(checkout, conflict_state)
            return conflict_state
        else:
            _RunScratchGit(checkout.repository_root, 'reset', '--quiet',
                           '--hard', 'HEAD')
            raise RollProtobufError(
                f'Failed to apply {chain_patch.patch_name}: '
                f'{patch_result.error_message}')
        expected_head = _RunScratchGit(checkout.repository_root, 'rev-parse',
                                       'HEAD').stdout.decode().strip()
        patch_results.append(patch_result)
        state = dataclasses.replace(state,
                                    expected_head=expected_head,
                                    current_index=index + 1,
                                    conflict_files=(),
                                    patch_results=tuple(patch_results))
        _SaveRollState(checkout, state)
    replayed_state = dataclasses.replace(state,
                                         phase='replayed',
                                         expected_head=expected_head,
                                         current_index=len(state.patches),
                                         conflict_files=(),
                                         patch_results=tuple(patch_results))
    _SaveRollState(checkout, replayed_state)
    return replayed_state


def _RequireActiveConflictState(checkout: _CheckoutPaths,
                                subcommand: str) -> RollState:
    state = LoadRollState(checkout.protobuf_directory)
    roll_branch = (_FindLocalRollBranch(checkout, state) if state is not None
                   and state.phase == 'conflict' else None)
    if (state is None or roll_branch is None
            or not 0 <= state.current_index < len(state.patches)):
        raise RollProtobufError(
            f'No conflicting protobuf patch is waiting for `{subcommand}`; '
            'run `roll_protobuf.py status` first.')
    if roll_branch != 'HEAD':
        raise RollProtobufError(
            f'Current HEAD is not on the {state.new_revision} roll branch; '
            f'run `git checkout {roll_branch}` before `{subcommand}`.')
    current_head = _RunScratchGit(checkout.repository_root, 'rev-parse',
                                  'HEAD').stdout.decode().strip()
    if current_head != state.expected_head:
        raise RollProtobufError(
            f'HEAD is {current_head}, expected {state.expected_head}; '
            'run `roll_protobuf.py resume` if you committed by hand.')
    return state


# Matches git's `diff --check` conflict markers (`<`, `|`, `>`), omitting
# standalone `=======` so Markdown Setext H1 underlines (csharp/README.md) do
# not false-positive.
_CONFLICT_MARKER_PATTERN = re.compile(rb'^(?:<{7}|\|{7}|>{7})(?: |\r?$)',
                                      re.MULTILINE)


def _FindFilesWithConflictMarkers(
        root_directory: str, relative_paths: Iterable[str]) -> tuple[str, ...]:
    flagged = []
    for relative_path in sorted(set(relative_paths)):
        full_path = os.path.join(root_directory, *relative_path.split('/'))
        if os.path.isfile(full_path) and _CONFLICT_MARKER_PATTERN.search(
                pathlib.Path(full_path).read_bytes()):
            flagged.append(relative_path)
    return tuple(flagged)


def _PatchTouchedRepositoryPaths(checkout: _CheckoutPaths,
                                 chain_patch: ChainPatch) -> set[str]:
    upstream_paths = set(chain_patch.preimage_blobs)
    if chain_patch.diff:
        numstat = _RunScratchGit(checkout.repository_root,
                                 'apply',
                                 '--numstat',
                                 '-z',
                                 '-',
                                 input_bytes=chain_patch.diff).stdout
        upstream_paths |= {
            record.split('\t', 2)[2]
            for record in _SplitNulSeparatedPaths(numstat) if '\t' in record
        }
    return {
        posixpath.join(checkout.protobuf_prefix, path)
        for path in upstream_paths
    }


def _RejectStagedChangesOutsideProtobuf(checkout: _CheckoutPaths) -> None:
    if not checkout.protobuf_prefix:
        return
    prefix_with_slash = f'{checkout.protobuf_prefix}/'
    outside_staged = sorted(path for path in _SplitNulSeparatedPaths(
        _RunScratchGit(checkout.repository_root, 'diff', '--cached',
                       '--name-only', '-z').stdout)
                            if not path.startswith(prefix_with_slash))
    if outside_staged:
        raise RollProtobufError(
            'Staged changes outside //third_party/protobuf cannot be '
            'included in a patch commit: ' + ', '.join(outside_staged))


def ContinueRoll(protobuf_directory: str = _PROTOBUF_DIRECTORY,
                 allow_extra_files: bool = False) -> RollState:
    """Commits a resolved patch conflict, refreshes its .patch, and continues."""
    checkout = _ResolveCheckoutPaths(protobuf_directory)
    state = _RequireActiveConflictState(checkout, 'roll_protobuf.py continue')
    leftover_rejects = tuple(
        path
        for path in _ListRegularFilesInDirectory(checkout.protobuf_directory)
        if path.endswith(('.rej', '.orig')))
    if leftover_rejects:
        raise RollProtobufError(
            'Remove .rej and .orig files under //third_party/protobuf before '
            'continuing: ' + ', '.join(leftover_rejects))
    protobuf_pathspec = checkout.protobuf_prefix or '.'
    prefix_with_slash = (f'{checkout.protobuf_prefix}/'
                         if checkout.protobuf_prefix else '')
    _RejectStagedChangesOutsideProtobuf(checkout)
    changed_paths = sorted(
        set(
            _SplitNulSeparatedPaths(
                _RunScratchGit(checkout.repository_root, 'diff', '--name-only',
                               '-z', 'HEAD', '--', protobuf_pathspec).stdout))
        | set(
            _SplitNulSeparatedPaths(
                _RunScratchGit(checkout.repository_root, 'ls-files',
                               '--others', '--exclude-standard', '-z', '--',
                               protobuf_pathspec).stdout)))
    marked_files = _FindFilesWithConflictMarkers(
        checkout.repository_root, (*changed_paths, *state.conflict_files))
    if marked_files:
        raise RollProtobufError('Unresolved conflict markers remain in: ' +
                                ', '.join(marked_files))
    chain_patch = state.patches[state.current_index]
    if not changed_paths:
        raise RollProtobufError(
            f'No changes remain for {chain_patch.patch_name}; resolve any '
            'binary conflict in the working tree or use '
            '`roll_protobuf.py drop --reason "..."` to drop the patch.')
    unmerged_paths = _SplitNulSeparatedPaths(
        _RunScratchGit(checkout.repository_root, 'diff', '--name-only', '-z',
                       '--diff-filter=U', '--', protobuf_pathspec).stdout)
    unresolved_unmerged = sorted(set(unmerged_paths) - set(changed_paths))
    if unresolved_unmerged:
        raise RollProtobufError(
            'Unresolved unmerged files remain at HEAD (write the merged file '
            'or `git add` it to keep HEAD): ' + ', '.join(unresolved_unmerged))
    non_upstream_paths = [
        path for path in changed_paths
        if IsChromiumOwnedFile(path.removeprefix(prefix_with_slash))
        or path.removeprefix(prefix_with_slash) in BOOTSTRAP_GENERATED_FILES
    ]
    if non_upstream_paths:
        raise RollProtobufError(
            'Chromium-owned or bootstrap-generated files cannot be included '
            'in an upstream patch commit: ' + ', '.join(non_upstream_paths))
    extra_paths = sorted(
        set(changed_paths) -
        _PatchTouchedRepositoryPaths(checkout, chain_patch))
    if extra_paths and not allow_extra_files:
        raise RollProtobufError(
            f'Modified files not touched by {chain_patch.patch_name} '
            '(pass --allow-extra-files if intentional): ' +
            ', '.join(extra_paths))
    _RunScratchGit(checkout.repository_root, 'add', '--all', '--',
                   protobuf_pathspec)
    staged_numstat = _RunScratchGit(checkout.repository_root, 'diff',
                                    '--cached', '--numstat', '-z',
                                    '--no-renames', '--no-ext-diff').stdout
    _RunScratchGit(checkout.repository_root, 'commit', '--quiet',
                   '--no-verify', '-m',
                   f'Apply patches/{chain_patch.patch_name}')
    _RefreshPatchFileFromHeadCommit(checkout, chain_patch.patch_name)
    new_head = _RunScratchGit(checkout.repository_root, 'rev-parse',
                              'HEAD').stdout.decode().strip()
    resolved_result = PatchReplayResult(
        chain_patch.patch_name,
        PatchReplayStatus.CLEAN,
        chain_patch.changed_line_count,
        changed_line_count_after=_CountChangedLines(staged_numstat))
    next_state = dataclasses.replace(
        state,
        expected_head=new_head,
        current_index=state.current_index + 1,
        conflict_files=(),
        patch_results=(*state.patch_results[:state.current_index],
                       resolved_result))
    _SaveRollState(checkout, next_state)
    return _ReplayRemainingPatches(checkout, next_state)


def _CleanUntrackedAndBackupFiles(protobuf_directory: str) -> None:
    # Chromium's root .gitignore ignores *.orig and *.rej; remove them before
    # `git clean -fdq` so untracked parent directories are pruned too, without
    # `-x` wiping unrelated git-ignored files.
    for path in _ListRegularFilesInDirectory(protobuf_directory):
        if path.endswith(('.rej', '.orig')):
            os.remove(os.path.join(protobuf_directory, *path.split('/')))
    _RunScratchGit(protobuf_directory, 'clean', '-fdq')


def DropCurrentPatch(
        reason: str,
        protobuf_directory: str = _PROTOBUF_DIRECTORY) -> RollState:
    """Discards the conflicting patch, removes its .patch file, and continues."""
    reason = reason.strip()
    if not reason or '\n' in reason or '\r' in reason:
        raise RollProtobufError(
            'Provide a single-line, non-empty --reason explaining why the '
            'patch is being dropped.')
    checkout = _ResolveCheckoutPaths(protobuf_directory)
    state = _RequireActiveConflictState(checkout, 'roll_protobuf.py drop')
    _RejectStagedChangesOutsideProtobuf(checkout)
    chain_patch = state.patches[state.current_index]
    _RunScratchGit(checkout.repository_root, 'restore', '--quiet',
                   '--source=HEAD', '--staged', '--worktree', '--',
                   checkout.protobuf_directory)
    _CleanUntrackedAndBackupFiles(checkout.protobuf_directory)
    patch_file_path = os.path.join(checkout.protobuf_directory, 'patches',
                                   chain_patch.patch_name)
    _RunScratchGit(checkout.repository_root, 'rm', '--quiet', '--',
                   patch_file_path)
    _RunScratchGit(checkout.repository_root, 'commit', '--quiet',
                   '--no-verify', '-m',
                   f'Drop patches/{chain_patch.patch_name} ({reason})')
    new_head = _RunScratchGit(checkout.repository_root, 'rev-parse',
                              'HEAD').stdout.decode().strip()
    dropped_result = PatchReplayResult(chain_patch.patch_name,
                                       PatchReplayStatus.DROPPED,
                                       chain_patch.changed_line_count,
                                       error_message=reason)
    next_state = dataclasses.replace(
        state,
        expected_head=new_head,
        current_index=state.current_index + 1,
        conflict_files=(),
        patch_results=(*state.patch_results[:state.current_index],
                       dropped_result))
    _SaveRollState(checkout, next_state)
    return _ReplayRemainingPatches(checkout, next_state)


def ResumeRoll(protobuf_directory: str = _PROTOBUF_DIRECTORY) -> RollState:
    """Reconciles state.json with manual Apply/Drop commits and continues."""
    checkout = _ResolveCheckoutPaths(protobuf_directory)
    state = LoadRollState(checkout.protobuf_directory)
    roll_branch = (_FindLocalRollBranch(checkout, state) if state is not None
                   and state.phase != 'prepared' else None)
    if state is None or roll_branch is None:
        raise RollProtobufError(
            'No active protobuf roll to resume; run `roll_protobuf.py start` '
            'first.')
    if roll_branch != 'HEAD':
        raise RollProtobufError(
            f'Current HEAD is not on the {state.new_revision} roll branch; '
            f'run `git checkout {roll_branch}` before '
            '`roll_protobuf.py resume`.')
    dirty_status = _RunScratchGit(checkout.repository_root, 'status',
                                  '--porcelain',
                                  '--ignore-submodules=all').stdout.strip()
    leftover_rejects = tuple(
        path
        for path in _ListRegularFilesInDirectory(checkout.protobuf_directory)
        if path.endswith(('.rej', '.orig')))
    if dirty_status or leftover_rejects:
        raise RollProtobufError(
            'Working tree has uncommitted changes or .rej/.orig files; '
            'finish committing the manual step (or run '
            '`roll_protobuf.py continue`) before running '
            '`roll_protobuf.py resume`.')
    commit_lines = _RunScratchGit(checkout.repository_root, 'log',
                                  '--no-show-signature', '--reverse',
                                  '--format=%H%x00%s',
                                  f'{state.start_commit}..HEAD').stdout.decode(
                                      'utf-8', errors='replace').splitlines()
    expected_import = (
        f'Import upstream protobuf {state.new_revision} (unmodified)')
    if not commit_lines or commit_lines[0].split('\0',
                                                 1)[1] != expected_import:
        raise RollProtobufError(
            f'Expected first roll commit {expected_import!r} after '
            f'{state.start_commit}.')
    patch_commits = commit_lines[1:]
    if len(patch_commits) > len(state.patches):
        raise RollProtobufError(
            f'Found {len(patch_commits)} patch commit(s) after import, but '
            f'the roll only has {len(state.patches)} patch(es).')
    reconciled_results = []
    for index, line in enumerate(patch_commits):
        commit_sha, subject = line.split('\0', 1)
        chain_patch = state.patches[index]
        drop_match = re.match(
            rf'^Drop patches/{re.escape(chain_patch.patch_name)}'
            r'(?:\s+\((.+)\))?$', subject)
        if subject == f'Apply patches/{chain_patch.patch_name}':
            numstat = _RunScratchGit(checkout.protobuf_directory,
                                     '--no-literal-pathspecs', 'diff',
                                     '--numstat', '-z', '--no-renames',
                                     '--no-ext-diff', f'{commit_sha}~1',
                                     commit_sha, '--', '.',
                                     ':(exclude)patches').stdout
            reconciled_results.append(
                PatchReplayResult(
                    chain_patch.patch_name,
                    PatchReplayStatus.CLEAN,
                    chain_patch.changed_line_count,
                    changed_line_count_after=_CountChangedLines(numstat)))
        elif drop_match:
            reason = (drop_match.group(1) or '').strip() or 'dropped by hand'
            status = (PatchReplayStatus.EMPTY if reason == 'already upstream'
                      else PatchReplayStatus.DROPPED)
            reconciled_results.append(
                PatchReplayResult(chain_patch.patch_name,
                                  status,
                                  chain_patch.changed_line_count,
                                  error_message=None if status
                                  is PatchReplayStatus.EMPTY else reason))
        else:
            raise RollProtobufError(
                f'Unexpected commit subject {subject!r} at patch '
                f'[{index + 1}/{len(state.patches)}]; expected '
                f'"Apply patches/{chain_patch.patch_name}" or '
                f'"Drop patches/{chain_patch.patch_name} (...)".')
    current_head = _RunScratchGit(checkout.repository_root, 'rev-parse',
                                  'HEAD').stdout.decode().strip()
    resumed_state = dataclasses.replace(
        state,
        expected_head=current_head,
        current_index=len(reconciled_results),
        conflict_files=(),
        patch_results=tuple(reconciled_results))
    _SaveRollState(checkout, resumed_state)
    return _ReplayRemainingPatches(checkout, resumed_state)


def StartRoll(to_revision: str,
              protobuf_directory: str = _PROTOBUF_DIRECTORY,
              upstream_repository_path: str | None = None,
              cache_directory: str | None = None,
              build_directory: str | None = None,
              upstream_url: str = UPSTREAM_REPOSITORY_URL) -> RollState:
    """Imports to_revision into the checkout and replays patches/*.patch.

    Refuses to run if the working tree has uncommitted changes. Prepares
    GITDIR/protobuf-roll/, commits the unmodified upstream release, and
    commits one patch at a time until all patches replay or a patch conflicts.
    """
    checkout = _ResolveCheckoutPaths(protobuf_directory)
    dirty_status = _RunScratchGit(checkout.repository_root, 'status',
                                  '--porcelain',
                                  '--ignore-submodules=all').stdout.strip()
    if dirty_status:
        raise RollProtobufError(
            'Working tree has uncommitted changes; commit or stash them '
            'before running `roll_protobuf.py start`.')
    upstream_arguments = (upstream_repository_path, cache_directory,
                          build_directory, upstream_url)
    state = PrepareRoll(to_revision, checkout.protobuf_directory,
                        *upstream_arguments)
    repository_path, _ = EnsureUpstreamCommitCached(state.new_commit,
                                                    *upstream_arguments)
    import_commit = ImportUpstreamRelease(checkout.protobuf_directory,
                                          repository_path, to_revision,
                                          state.new_commit)
    state = dataclasses.replace(
        state,
        phase='conflict' if state.patches else 'replayed',
        expected_head=import_commit)
    _SaveRollState(checkout, state)
    return _ReplayRemainingPatches(checkout, state)


def _IsAncestorOfHead(repository_root: str,
                      commit: str,
                      ref: str = 'HEAD') -> bool:
    return _RunScratchGit(repository_root,
                          'merge-base',
                          '--is-ancestor',
                          commit,
                          ref,
                          accepted_return_codes=(0, 1, 128)).returncode == 0


def _IsRollBranchHead(checkout: _CheckoutPaths,
                      state: RollState,
                      ref: str = 'HEAD') -> bool:
    """Returns True if ref is on the roll branch recorded by state."""
    root = checkout.repository_root
    if (_ResolveCommitHash(root, ref) in (None, state.start_commit)
            or not _IsAncestorOfHead(root, state.start_commit, ref)):
        return False
    if (state.expected_head != state.start_commit
            and _IsAncestorOfHead(root, state.expected_head, ref)):
        return True
    subjects = _RunScratchGit(root, 'log', '--no-show-signature',
                              '--format=%s', '-n', '200',
                              f'{state.start_commit}..{ref}').stdout.decode(
                                  'utf-8', errors='replace').splitlines()
    return (f'Import upstream protobuf {state.new_revision} (unmodified)'
            in subjects)


def _CanResetRollHead(checkout: _CheckoutPaths, state: RollState) -> bool:
    return state.phase != 'prepared' and (
        _IsRollBranchHead(checkout, state) or
        (state.phase == 'conflict'
         and state.expected_head == state.start_commit and _ResolveCommitHash(
             checkout.repository_root, 'HEAD') == state.expected_head))


def _FindLocalRollBranch(checkout: _CheckoutPaths,
                         state: RollState) -> str | None:
    if state.phase == 'prepared':
        return None
    if _CanResetRollHead(checkout, state):
        return 'HEAD'
    root = checkout.repository_root
    if _ResolveCommitHash(root, state.start_commit) is None:
        return None
    records = _RunScratchGit(
        root, 'for-each-ref', '--format=%(objectname) %(refname:lstrip=2)',
        '--contains', state.start_commit,
        'refs/heads').stdout.decode('utf-8', errors='replace').splitlines()
    for record in records:
        sha, _, branch_name = record.partition(' ')
        if (sha != state.start_commit and _IsRollBranchHead(
                checkout, state, f'refs/heads/{branch_name}')):
            return branch_name
    return None


def _ComputeBootstrapInputsDigest(protobuf_directory: str) -> str:
    """Hashes README.chromium's Revision and all bootstrap generator inputs."""
    digest = hashlib.sha256(
        f'Revision:{ReadReadmeRevision(protobuf_directory)}\0'.encode('utf-8'))
    for relative_path in sorted(
            path for path in _ListChromiumProtobufFiles(protobuf_directory)
            if IsBootstrapGeneratorInput(path)):
        raw_bytes = pathlib.Path(protobuf_directory,
                                 *relative_path.split('/')).read_bytes()
        digest.update(
            relative_path.encode('utf-8') + b'\0' +
            raw_bytes.replace(b'\r\n', b'\n') + b'\0')
    return digest.hexdigest()


def _RestoreProtobufWorktreeAfterFailedGenerate(
        checkout: _CheckoutPaths) -> None:
    for path in BOOTSTRAP_GENERATED_FILES:
        full_path = os.path.join(checkout.protobuf_directory, *path.split('/'))
        if os.path.lexists(full_path):
            os.remove(full_path)
    if _ResolveCommitHash(checkout.repository_root, 'HEAD') is not None:
        _RunScratchGit(checkout.repository_root, 'restore', '--quiet',
                       '--source=HEAD', '--worktree', '--',
                       checkout.protobuf_directory)
    _CleanUntrackedAndBackupFiles(checkout.protobuf_directory)


@dataclasses.dataclass(frozen=True)
class GenerateResult:
    """Outcome of regenerating and committing bootstrap files."""

    commit: str
    inputs_digest: str
    changed_files: tuple[str, ...]

    def ToDictionary(self) -> dict[str, object]:
        return {
            'commit': self.commit,
            'inputs_digest': self.inputs_digest,
            'changed_files': list(self.changed_files),
        }


def GenerateBootstrapFiles(
        build_directory: str,
        protobuf_directory: str = _PROTOBUF_DIRECTORY) -> GenerateResult:
    """Runs gen_extra_chromium_files.py and commits bootstrap outputs."""
    if not build_directory or not build_directory.strip():
        raise RollProtobufError(
            'Provide -C <out_dir> so gen_extra_chromium_files.py can build '
            'protoc.')
    checkout = _ResolveCheckoutPaths(protobuf_directory)
    state = LoadRollState(checkout.protobuf_directory)
    current_head = _ResolveCommitHash(checkout.repository_root, 'HEAD')
    if (state is not None and state.phase in ('prepared', 'conflict')
            and (current_head == state.expected_head
                 or _IsRollBranchHead(checkout, state))):
        raise RollProtobufError(
            f'Cannot regenerate bootstrap files while roll phase is '
            f'{state.phase!r}; finish replaying patches first.')
    if _RunScratchGit(checkout.repository_root, 'diff', '--cached',
                      '--name-only', '-z').stdout.strip():
        raise RollProtobufError(
            'Repository has staged changes; commit or unstage them before '
            'running `roll_protobuf.py generate`.')
    protobuf_pathspec = checkout.protobuf_prefix or '.'
    dirty_protobuf = _RunScratchGit(checkout.repository_root, 'status',
                                    '--porcelain', '--ignore-submodules=all',
                                    '--', protobuf_pathspec).stdout.strip()
    leftover_rejects = any(
        p.endswith(('.rej', '.orig'))
        for p in _ListRegularFilesInDirectory(checkout.protobuf_directory))
    if dirty_protobuf or leftover_rejects:
        raise RollProtobufError(
            '//third_party/protobuf has uncommitted changes or .rej/.orig '
            'files; commit or clean them before running '
            '`roll_protobuf.py generate`.')
    generator_script = os.path.join(checkout.protobuf_directory,
                                    'gen_extra_chromium_files.py')
    if not os.path.isfile(generator_script):
        raise RollProtobufError(
            f'Missing gen_extra_chromium_files.py at {generator_script}.')
    ReadReadmeRevision(checkout.protobuf_directory)
    _ListChromiumProtobufFiles(checkout.protobuf_directory)
    process = subprocess.run([
        sys.executable, generator_script, '-C',
        os.path.abspath(build_directory)
    ],
                             env=_IsolatedGitEnvironment(),
                             stdin=subprocess.DEVNULL,
                             capture_output=True,
                             cwd=checkout.repository_root,
                             check=False)
    if process.returncode != 0:
        _RestoreProtobufWorktreeAfterFailedGenerate(checkout)
        diagnostics = '\n'.join(
            s.decode('utf-8', errors='replace').strip()
            for s in (process.stderr, process.stdout) if s.strip())
        raise RollProtobufError(
            f'gen_extra_chromium_files.py failed (exit {process.returncode}): '
            f'{diagnostics}')
    missing_outputs = sorted(
        path for path in BOOTSTRAP_GENERATED_FILES if not _IsRegularFile(
            os.path.join(checkout.protobuf_directory, *path.split('/'))))
    if missing_outputs:
        _RestoreProtobufWorktreeAfterFailedGenerate(checkout)
        raise RollProtobufError(
            'gen_extra_chromium_files.py did not produce expected bootstrap '
            'files: ' + ', '.join(missing_outputs))
    _RunScratchGit(checkout.repository_root, 'add', '--update', '--',
                   protobuf_pathspec)
    _RunScratchGit(
        checkout.repository_root, 'add', '--force', '--',
        *(os.path.join(checkout.protobuf_directory, *p.split('/'))
          for p in sorted(BOOTSTRAP_GENERATED_FILES)))
    prefix_with_slash = (f'{checkout.protobuf_prefix}/'
                         if checkout.protobuf_prefix else '')
    changed_files = tuple(
        sorted(
            path.removeprefix(prefix_with_slash)
            for path in _SplitNulSeparatedPaths(
                _RunScratchGit(checkout.repository_root, 'diff', '--cached',
                               '--name-only', '-z', '--',
                               protobuf_pathspec).stdout)))
    inputs_digest = _ComputeBootstrapInputsDigest(checkout.protobuf_directory)
    commit_message = (f'Regenerate bootstrap files\n\n'
                      f'{_BOOTSTRAP_INPUTS_TRAILER_KEY}: {inputs_digest}')
    _RunScratchGit(checkout.repository_root, 'commit', '--quiet',
                   '--no-verify', '--allow-empty', '-m', commit_message)
    new_head = _RunScratchGit(checkout.repository_root, 'rev-parse',
                              'HEAD').stdout.decode().strip()
    if (state is not None and state.phase == 'replayed'
            and _IsRollBranchHead(checkout, state)):
        _SaveRollState(checkout,
                       dataclasses.replace(state, expected_head=new_head))
    return GenerateResult(new_head, inputs_digest, changed_files)


def FormatGenerateReport(result: GenerateResult) -> str:
    if not result.changed_files:
        return (f'Regenerated bootstrap files at {result.commit} '
                '(already up to date).')
    lines = [
        f'Regenerated {len(result.changed_files)} bootstrap file(s) at '
        f'{result.commit}:'
    ]
    lines += [f'  {path}' for path in result.changed_files]
    return '\n'.join(lines)


@functools.cache
def _LoadProtobufPresubmitModule():
    spec = importlib.util.spec_from_file_location(
        '_protobuf_presubmit', os.path.join(_PROTOBUF_DIRECTORY,
                                            'PRESUBMIT.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _FindPatchInventoryErrors(protobuf_directory: str) -> tuple[str, ...]:
    """Checks patches/*.{patch,md} against README.chromium via PRESUBMIT."""
    readme_path = pathlib.Path(protobuf_directory, _README_FILENAME)
    if not readme_path.is_file():
        return (
            f'{_README_FILENAME} is missing from //third_party/protobuf.', )
    readme_lines = readme_path.read_text(encoding='utf-8').splitlines()
    patches_directory = os.path.join(protobuf_directory, 'patches')
    top_level_files = ({
        name
        for name in os.listdir(patches_directory)
        if os.path.isfile(os.path.join(patches_directory, name))
    } if os.path.isdir(patches_directory) else set())
    presubmit = _LoadProtobufPresubmitModule()
    return tuple(
        presubmit.FindPatchInventorySyncErrors(readme_lines, top_level_files) +
        presubmit.FindCompanionMarkdownErrors(readme_lines, top_level_files))


def _ProtobufPathsFromGit(checkout: _CheckoutPaths, *git_arguments:
                          str) -> set[str]:
    prefix = f'{checkout.protobuf_prefix}/' if checkout.protobuf_prefix else ''
    output = _RunScratchGit(checkout.repository_root, *git_arguments, '--',
                            checkout.protobuf_prefix or '.').stdout
    return {
        path.removeprefix(prefix)
        for path in _SplitNulSeparatedPaths(output)
    }


def _FindLocalHygieneErrors(checkout: _CheckoutPaths) -> tuple[str, ...]:
    """Lists leftover .rej/.orig files, conflict markers, or unmerged paths."""
    errors = [
        f'{path}: leftover patch .rej/.orig file' for path in sorted(
            _ListRegularFilesInDirectory(checkout.protobuf_directory))
        if path.endswith(('.rej', '.orig'))
    ]
    marked = set(
        _FindFilesWithConflictMarkers(
            checkout.protobuf_directory,
            _ListChromiumProtobufFiles(checkout.protobuf_directory)))
    errors += [
        f'{path}: unresolved conflict markers' for path in sorted(marked)
    ]
    unmerged = _ProtobufPathsFromGit(checkout, 'diff', '--name-only', '-z',
                                     '--diff-filter=U')
    errors += [
        f'{path}: unresolved unmerged index entry'
        for path in sorted(unmerged - marked)
    ]
    return tuple(errors)


def _ResolveRollBaseCommit(checkout: _CheckoutPaths) -> str | None:
    """Returns the baseline commit before the current roll or branch."""
    root = checkout.repository_root
    if _ResolveCommitHash(root, 'HEAD') is None:
        return None
    state = LoadRollState(checkout.protobuf_directory)
    if (state is not None and state.phase in ('conflict', 'replayed')
            and _IsRollBranchHead(checkout, state)):
        return state.start_commit
    merge_base = _RunScratchGit(root,
                                'merge-base',
                                'HEAD',
                                'origin/main',
                                accepted_return_codes=None)
    if merge_base.returncode == 0:
        return merge_base.stdout.decode().strip()
    log_lines = _RunScratchGit(root, 'log', '--no-show-signature', '--reverse',
                               '-n', '200', '--format=%H%x00%s',
                               'HEAD').stdout.decode(
                                   'utf-8', errors='replace').splitlines()
    for line in log_lines:
        sha, subject = line.split('\0', 1)
        if (subject.startswith('Import upstream protobuf ')
                or subject == 'Regenerate bootstrap files'):
            return _ResolveCommitHash(root, f'{sha}~1')
    return log_lines[0].split('\0', 1)[0] if log_lines else None


def _ReadGitReadmeRevision(checkout: _CheckoutPaths,
                           commit: str) -> str | None:
    prefix = f'{checkout.protobuf_prefix}/' if checkout.protobuf_prefix else ''
    shown = _RunScratchGit(checkout.repository_root,
                           'cat-file',
                           'blob',
                           f'{commit}:{prefix}{_README_FILENAME}',
                           accepted_return_codes=None)
    if shown.returncode != 0:
        return None
    match = re.search(r'^Revision:\s*(\S+)\s*$',
                      shown.stdout.decode('utf-8', errors='replace'),
                      re.MULTILINE)
    return match.group(1) if match else None


def _ParseNameStatusWrittenPaths(raw_bytes: bytes, prefix: str) -> set[str]:
    tokens = _SplitNulSeparatedPaths(raw_bytes.lstrip(b'\x00\n'))
    return {
        tokens[i + 1].removeprefix(prefix)
        for i in range(0,
                       len(tokens) - 1, 2)
        if tokens[i] != 'D' and (not prefix or tokens[i +
                                                      1].startswith(prefix))
    }


def _FindStaleBootstrapTriggers(checkout: _CheckoutPaths) -> tuple[str, ...]:
    """Returns inputs or missing outputs requiring `generate` to run."""
    root = checkout.repository_root
    if _ResolveCommitHash(root, 'HEAD') is None:
        return ()
    base_commit = _ResolveRollBaseCommit(checkout)
    prefix = f'{checkout.protobuf_prefix}/' if checkout.protobuf_prefix else ''
    visible_files = _ListChromiumProtobufFiles(checkout.protobuf_directory)
    has_generator = os.path.isfile(
        os.path.join(checkout.protobuf_directory,
                     'gen_extra_chromium_files.py'))
    uncommitted = (_ProtobufPathsFromGit(checkout, 'diff', '--no-renames',
                                         '--name-only', '-z', 'HEAD')
                   | _ProtobufPathsFromGit(checkout, 'ls-files', '--others',
                                           '--exclude-standard', '-z'))
    current_revision = ReadReadmeRevision(checkout.protobuf_directory)

    # Omit pathspec so `--allow-empty` regeneration commits stay visible.
    log_args = ([f'{base_commit}..HEAD']
                if base_commit else ['-n', '200', 'HEAD'])
    raw_log = _RunScratchGit(root, 'log', '--no-show-signature', '--root',
                             '--no-merges', '--reverse', '--no-renames', '-z',
                             '--name-status',
                             '--format=%x1e%H%x1f%s%x1f%B%x1d',
                             *log_args).stdout
    base_rev = (_ReadGitReadmeRevision(checkout, base_commit)
                if base_commit else None)
    prev_rev = base_rev
    regen_commit = None
    regen_rev = None
    recorded_hashes: tuple[str, ...] = ()
    step5_ready = False
    unregenerated_revision_change = False
    for raw_record in raw_log.split(b'\x1e')[1:]:
        header, _, status_blob = raw_record.partition(b'\x1d')
        sha, subject, body = header.decode('utf-8',
                                           errors='replace').split('\x1f', 2)
        written = _ParseNameStatusWrittenPaths(status_blob, prefix)
        commit_rev = (_ReadGitReadmeRevision(checkout, sha)
                      if _README_FILENAME in written else prev_rev)
        rev_changed = prev_rev is not None and commit_rev != prev_rev
        prev_rev = commit_rev
        hashes = tuple(
            re.findall(
                rf'^[ \t]*{_BOOTSTRAP_INPUTS_TRAILER_KEY}:\s*([0-9a-f]{{64}})'
                r'[ \t]*$', body, re.MULTILINE))
        wrote_bootstrap = written & BOOTSTRAP_GENERATED_FILES
        wrote_python_bootstrap = any(
            p.startswith('python/') for p in wrote_bootstrap)
        # Upstream archives include C++ bootstrap .pb.{h,cc} files, whereas
        # python/ bootstrap outputs are only produced by
        # gen_extra_chromium_files.py.
        is_manual_regen = (not subject.startswith('Import upstream protobuf ')
                           and (wrote_python_bootstrap if
                                (rev_changed or unregenerated_revision_change)
                                else bool(wrote_bootstrap)))
        is_step5_readme_update = (
            rev_changed and step5_ready
            and not subject.startswith('Import upstream protobuf ')
            and all(IsChromiumOwnedFile(p) for p in written))
        if (hashes or subject == 'Regenerate bootstrap files'
                or is_manual_regen):
            regen_commit = sha
            regen_rev = commit_rev
            recorded_hashes = hashes
            step5_ready = (not hashes and not rev_changed
                           and not unregenerated_revision_change
                           and (step5_ready or wrote_python_bootstrap
                                or subject == 'Regenerate bootstrap files'))
            unregenerated_revision_change = False
        elif ((rev_changed and not is_step5_readme_update)
              or subject.startswith('Import upstream protobuf ')):
            regen_commit = None
            regen_rev = None
            recorded_hashes = ()
            step5_ready = False
            unregenerated_revision_change = True
        elif is_step5_readme_update:
            regen_rev = commit_rev
            step5_ready = False

    reference_commit = regen_commit or base_commit
    changed = set(uncommitted)
    if reference_commit is not None:
        changed |= _ProtobufPathsFromGit(checkout, 'diff', '--no-renames',
                                         '--name-only', '-z', reference_commit)
    stale = {p for p in changed if IsBootstrapGeneratorInput(p)}
    if has_generator or regen_commit is not None:
        stale |= {
            f'missing bootstrap output: {p}'
            for p in BOOTSTRAP_GENERATED_FILES - visible_files
        }
    expected_rev = regen_rev if regen_commit is not None else base_rev
    uncommitted_step5_readme = (step5_ready and current_revision != regen_rev
                                and all(
                                    IsChromiumOwnedFile(p)
                                    for p in uncommitted))
    if (unregenerated_revision_change or
        (reference_commit is not None and expected_rev != current_revision
         and not uncommitted_step5_readme)):
        stale.add('upstream revision changed')
    if (not stale and recorded_hashes and _ComputeBootstrapInputsDigest(
            checkout.protobuf_directory) not in recorded_hashes):
        stale.add('generator inputs changed since last '
                  '`roll_protobuf.py generate`')
    return tuple(sorted(stale))


def _SelectCompileSizeBaseline(build_directory: str,
                               baseline_name: str | None) -> str | None:
    if baseline_name is not None:
        if not baseline_name.strip():
            raise RollProtobufError(
                'Provide a non-empty baseline name to --baseline.')
        return baseline_name.strip()
    probe_dir = os.path.join(os.path.abspath(build_directory),
                             '.compile_size_probe')
    if not os.path.isdir(probe_dir):
        return None
    candidates = [
        stem for name in os.listdir(probe_dir) if not name.startswith('.')
        and name.endswith('.json') and not name.endswith('.artifacts.json')
        and os.path.isfile(os.path.join(probe_dir, name))
        and re.fullmatch(r'[A-Za-z0-9_.-]+', stem := name[:-len('.json')])
        and '..' not in stem
    ]
    if not candidates:
        return None
    preferred = [name for name in ('before', 'pre_roll') if name in candidates]
    return max(
        preferred or candidates,
        key=lambda name:
        (os.path.getmtime(os.path.join(probe_dir, f'{name}.json')), name))


def _RunCompileSizeProbeComparison(
        checkout: _CheckoutPaths, build_directory: str | None,
        baseline_name: str | None
) -> tuple[dict[str, object] | None, str | None]:
    """Invokes compile_size_probe.py --compare=<name> --json if available."""
    if not build_directory or not build_directory.strip():
        if baseline_name is not None:
            raise RollProtobufError('--baseline requires -C <out_dir>.')
        return None, None
    selected = _SelectCompileSizeBaseline(build_directory, baseline_name)
    probe_script = os.path.join(checkout.protobuf_directory,
                                'compile_size_probe.py')
    if not os.path.isfile(probe_script):
        if baseline_name is not None:
            return None, f'Missing compile_size_probe.py at {probe_script}.'
        return None, None
    if selected is None:
        return None, None
    command = [
        sys.executable, probe_script, '-C',
        os.path.abspath(build_directory), f'--compare={selected}', '--json'
    ]
    process = subprocess.run(command,
                             env=_IsolatedGitEnvironment(),
                             stdin=subprocess.DEVNULL,
                             capture_output=True,
                             cwd=checkout.repository_root,
                             check=False)
    if process.returncode != 0:
        stderr_text = process.stderr.decode('utf-8', errors='replace').strip()
        stdout_text = process.stdout.decode('utf-8', errors='replace').strip()
        details = stderr_text or stdout_text
        return None, details or f'exit {process.returncode}'
    try:
        payload = json.loads(process.stdout.decode('utf-8'))
    except ValueError as error:
        return None, f'invalid JSON from compile_size_probe.py: {error}'
    if not isinstance(payload, dict):
        return None, 'invalid JSON from compile_size_probe.py: expected object'
    return payload, None


def _FormatCompileSizeSummary(comparison: Mapping[str, object]) -> str:
    baseline = comparison.get('baseline_name', 'before')
    before = int(comparison.get('before_total_bytes', 0))
    after = int(comparison.get('after_total_bytes', 0))
    delta = int(comparison.get('delta_total_bytes', after - before))
    lines = [
        f'Compile-size comparison against baseline "{baseline}": '
        f'{before:,} B -> {after:,} B ({delta:+,} B)'
    ]
    sym_before = int(comparison.get('before_total_symbol_bytes', 0))
    sym_after = int(comparison.get('after_total_symbol_bytes', 0))
    if sym_before or sym_after:
        sym_delta = int(
            comparison.get('delta_total_symbol_bytes', sym_after - sym_before))
        lines.append(f'  Compiled symbol bytes: {sym_before:,} B -> '
                     f'{sym_after:,} B ({sym_delta:+,} B)')
    for unit in comparison.get('translation_units', ()):
        u_before = int(unit.get('before_bytes', 0))
        u_after = int(unit.get('after_bytes', 0))
        u_delta = int(unit.get('delta_bytes', u_after - u_before))
        lines.append(f'  {unit.get("translation_unit")}: {u_delta:+,} B '
                     f'({u_before:,} B -> {u_after:,} B)')
        for header in list(unit.get('added_headers', ()))[:5]:
            size = int(header.get('size_bytes', 0))
            lines.append(f'    +{size:,} B  {header.get("path", "")}')
            chain = header.get('include_chain', ())
            if len(chain) > 1:
                lines.append('      via: ' + ' -> '.join(chain))
        for header in list(unit.get('removed_headers', ()))[:5]:
            size = int(header.get('size_bytes', 0))
            lines.append(f'    -{size:,} B  {header.get("path", "")}')
        for resized in list(unit.get('resized_files', ()))[:5]:
            change = int(resized.get('delta_bytes', 0))
            lines.append(
                f'    {change:+,} B  {resized.get("path", "")} (resized)')
    return '\n'.join(lines)


def _ComputeBranchFileListsDiff(
        checkout: _CheckoutPaths) -> check_file_lists.FileListsDiff | None:
    """Diffs src/file_lists.cmake against the pre-roll baseline and GN lists."""
    base_commit = _ResolveRollBaseCommit(checkout)
    if base_commit is None:
        return None
    prefix = f'{checkout.protobuf_prefix}/' if checkout.protobuf_prefix else ''
    return _DiffFileListsAgainstGitSpec(
        checkout.protobuf_directory, checkout.repository_root,
        f'{base_commit}:{prefix}{_UPSTREAM_FILE_LISTS_PATH}',
        checkout.protobuf_directory)


@dataclasses.dataclass(frozen=True)
class RollCheckResult:
    """Full verification and informational report for `roll_protobuf.py check`."""

    tree_result: TreeVerificationResult
    patch_inventory_errors: tuple[str, ...] = ()
    hygiene_errors: tuple[str, ...] = ()
    stale_bootstrap_triggers: tuple[str, ...] = ()
    file_lists_diff: check_file_lists.FileListsDiff | None = None
    compile_size_comparison: dict[str, object] | None = None
    compile_size_error: str | None = None

    def IsClean(self) -> bool:
        return (self.tree_result.IsClean() and not self.patch_inventory_errors
                and not self.hygiene_errors
                and not self.stale_bootstrap_triggers)

    def ToDictionary(self) -> dict[str, object]:
        result = self.tree_result.ToDictionary()
        result.update(is_clean=self.IsClean(),
                      patch_inventory_errors=list(self.patch_inventory_errors),
                      hygiene_errors=list(self.hygiene_errors),
                      stale_bootstrap_triggers=list(
                          self.stale_bootstrap_triggers),
                      file_lists=(None if self.file_lists_diff is None else
                                  self.file_lists_diff.ToDictionary()),
                      compile_size=self.compile_size_comparison,
                      compile_size_error=self.compile_size_error)
        return result


def CheckRoll(protobuf_directory: str = _PROTOBUF_DIRECTORY,
              upstream_repository_path: str | None = None,
              cache_directory: str | None = None,
              build_directory: str | None = None,
              upstream_url: str = UPSTREAM_REPOSITORY_URL,
              baseline_name: str | None = None) -> RollCheckResult:
    """Runs blocking verification checks plus informational roll reports."""
    if baseline_name is not None:
        if not baseline_name.strip():
            raise RollProtobufError(
                'Provide a non-empty baseline name to --baseline.')
        if not build_directory or not build_directory.strip():
            raise RollProtobufError('--baseline requires -C <out_dir>.')
    tree_result = CheckTreeMatchesPatchedUpstream(protobuf_directory,
                                                  upstream_repository_path,
                                                  cache_directory,
                                                  build_directory,
                                                  upstream_url)
    checkout = _ResolveCheckoutPaths(protobuf_directory)
    compile_size, compile_size_error = _RunCompileSizeProbeComparison(
        checkout, build_directory, baseline_name)
    return RollCheckResult(
        tree_result=tree_result,
        patch_inventory_errors=_FindPatchInventoryErrors(
            checkout.protobuf_directory),
        hygiene_errors=_FindLocalHygieneErrors(checkout),
        stale_bootstrap_triggers=_FindStaleBootstrapTriggers(checkout),
        file_lists_diff=_ComputeBranchFileListsDiff(checkout),
        compile_size_comparison=compile_size,
        compile_size_error=compile_size_error)


def FormatRollCheckReport(result: RollCheckResult) -> str:
    sections = [FormatTreeVerificationReport(result.tree_result)]
    if result.patch_inventory_errors:
        sections.append('Patch inventory / README.chromium errors:\n' +
                        '\n'.join(f'  {e}'
                                  for e in result.patch_inventory_errors))
    if result.hygiene_errors:
        sections.append('Local hygiene errors:\n' +
                        '\n'.join(f'  {e}' for e in result.hygiene_errors))
    if result.stale_bootstrap_triggers:
        sections.append(
            'Stale bootstrap files (run `roll_protobuf.py generate -C '
            '<out_dir>`):\n' +
            '\n'.join(f'  {t}' for t in result.stale_bootstrap_triggers))
    if (result.file_lists_diff is not None
            and result.file_lists_diff.HasUnfollowedChanges()):
        sections.append(
            check_file_lists.FormatFileListsDiffReport(result.file_lists_diff))
    if result.compile_size_comparison is not None:
        sections.append(
            _FormatCompileSizeSummary(result.compile_size_comparison))
    elif result.compile_size_error:
        sections.append('Compile-size probe skipped due to error: '
                        f'{result.compile_size_error}')
    return '\n\n'.join(sections)


@dataclasses.dataclass(frozen=True)
class RollStatus:
    """Current state of a checkout roll and the valid next commands."""

    state: str  # 'idle', 'prepared', 'conflict', or 'replayed'.
    index: int  # 1-based patch index when in conflict, total when replayed.
    total: int
    patch: str | None = None
    files: tuple[str, ...] = ()
    next_commands: tuple[str, ...] = ()

    def ToDictionary(self) -> dict[str, object]:
        return {
            'state': self.state,
            'index': self.index,
            'total': self.total,
            'patch': self.patch,
            'files': list(self.files),
            'next': list(self.next_commands),
        }


def GetRollStatus(protobuf_directory: str = _PROTOBUF_DIRECTORY) -> RollStatus:
    checkout = _ResolveCheckoutPaths(protobuf_directory)
    roll_state = LoadRollState(checkout.protobuf_directory)
    roll_branch = (_FindLocalRollBranch(checkout, roll_state)
                   if roll_state is not None and roll_state.phase == 'conflict'
                   else None)
    if (roll_state is None
            or (roll_state.phase == 'conflict' and roll_branch is None)
            or (roll_state.phase == 'replayed'
                and not _IsRollBranchHead(checkout, roll_state))):
        return RollStatus(
            state='idle',
            index=0,
            total=0,
            next_commands=('roll_protobuf.py plan --to <version>',
                           'roll_protobuf.py start --to <version>'))
    total = len(roll_state.patches)
    if roll_state.phase == 'prepared':
        return RollStatus(
            state='prepared',
            index=0,
            total=total,
            next_commands=(
                f'roll_protobuf.py start --to {roll_state.new_revision}',
                'roll_protobuf.py abort'))
    if roll_state.phase == 'conflict' and 0 <= roll_state.current_index < total:
        on_roll_head = roll_branch == 'HEAD'
        return RollStatus(
            state='conflict',
            index=roll_state.current_index + 1,
            total=total,
            patch=roll_state.patches[roll_state.current_index].patch_name,
            files=roll_state.conflict_files if on_roll_head else (),
            next_commands=('roll_protobuf.py continue',
                           'roll_protobuf.py drop --reason <why>',
                           'roll_protobuf.py abort') if on_roll_head else
            (f'git checkout {roll_branch}', ))
    stale = _FindStaleBootstrapTriggers(checkout)
    next_step = ('roll_protobuf.py generate -C <out_dir>'
                 if stale else 'roll_protobuf.py check -C <out_dir>')
    return RollStatus(state='replayed',
                      index=total,
                      total=total,
                      next_commands=(next_step, 'roll_protobuf.py abort'))


def AbortRoll(protobuf_directory: str = _PROTOBUF_DIRECTORY) -> RollState:
    """Resets an active roll to start_commit or clears stale roll state."""
    checkout = _ResolveCheckoutPaths(protobuf_directory)
    roll_state = LoadRollState(checkout.protobuf_directory)
    if roll_state is None:
        raise RollProtobufError('No protobuf roll is in progress (missing '
                                f'{checkout.state_file_path}).')
    roll_branch = _FindLocalRollBranch(checkout, roll_state)
    if roll_branch != 'HEAD':
        if roll_state.phase == 'prepared' or roll_branch is None:
            shutil.rmtree(checkout.roll_state_directory)
            return roll_state
        raise RollProtobufError(
            f'Current HEAD is not on the {roll_state.new_revision} roll '
            f'branch; run `git checkout {roll_branch}` before running '
            '`roll_protobuf.py abort`.')
    _RunScratchGit(checkout.repository_root, 'reset', '--quiet', '--hard',
                   roll_state.start_commit)
    _CleanUntrackedAndBackupFiles(checkout.protobuf_directory)
    shutil.rmtree(checkout.roll_state_directory)
    return roll_state


def FormatRollStateReport(state: RollState) -> str:
    if state.phase == 'prepared':
        return (f'Prepared roll from {state.old_revision} to '
                f'{state.new_revision} ({state.new_commit}): '
                f'{len(state.patches)} patch(es), copied '
                f'{state.copied_blob_count} preimage blob(s).')
    total = len(state.patches)
    lines = [
        f'Rolling //third_party/protobuf from {state.old_revision} to '
        f'{state.new_revision} ({state.new_commit}):', ''
    ]
    lines += [
        f'  [{index}/{total}] {result.status.value}  {result.patch_name}: '
        f'{_DescribePatchReplayResult(result)}'
        for index, result in enumerate(state.patch_results, start=1)
    ]
    if state.phase == 'conflict' and 0 <= state.current_index < total:
        lines += [
            '',
            f'Stopped at [{state.current_index + 1}/{total}] '
            f'patches/{state.patches[state.current_index].patch_name}.',
            'Resolve the conflict in the checkout and run '
            '`roll_protobuf.py continue`, drop the patch with '
            '`roll_protobuf.py drop --reason "<why>"`, finish by hand (see '
            '"Doing it by hand" in README.chromium), or run '
            f'`roll_protobuf.py abort` to reset to {state.start_commit}.',
        ]
    else:
        lines += ['', f'Replayed all {total} patch(es).']
    return '\n'.join(lines)


def FormatRollStatusReport(status: RollStatus) -> str:
    if status.state == 'idle':
        header = 'No protobuf roll in progress.'
    elif status.state == 'prepared':
        header = f'State: prepared ({status.total} patch(es)).'
    elif status.state == 'conflict':
        header = (f'State: conflict [{status.index}/{status.total}] in '
                  f'patches/{status.patch}')
    else:
        header = f'State: replayed ({status.total}/{status.total} patch(es)).'
    lines = [header, *[f'  {path}' for path in status.files]]
    lines.append('Next: ' + ', '.join(status.next_commands))
    return '\n'.join(lines)


def main(command_line_arguments: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description='Verifies //third_party/protobuf against upstream plus '
        'patches/*.patch, and sizes rolls to new upstream versions.')
    subparsers = parser.add_subparsers(dest='subcommand', required=True)
    check_parser = subparsers.add_parser(
        'check',
        help='Verify //third_party/protobuf invariants and report '
        'file_lists.cmake and optional compile_size_probe.py deltas.')
    check_parser.add_argument(
        '--baseline',
        dest='baseline_name',
        help='Saved compile_size_probe.py baseline name to compare against '
        'when -C is set (default: "before" or "pre_roll", else newest).')
    plan_parser = subparsers.add_parser(
        'plan',
        help='Dry run: classify every patch as CLEAN, EMPTY, CONFLICT or '
        'MISSING_FILE on a new upstream version, without touching the '
        'checkout.')
    prepare_parser = subparsers.add_parser(
        'prepare',
        help='Populate GITDIR/protobuf-roll/ and copy preimage blobs without '
        'modifying the working tree.')
    start_parser = subparsers.add_parser(
        'start',
        help='Import a new upstream version into the checkout and replay '
        'patches/*.patch one commit at a time.')
    subparsers.add_parser(
        'status',
        help='Show where an in-progress roll is and which commands are valid '
        'next.')
    continue_parser = subparsers.add_parser(
        'continue',
        help='Commit a resolved patch conflict, refresh its .patch file, and '
        'continue replaying remaining patches.')
    continue_parser.add_argument(
        '--allow-extra-files',
        action='store_true',
        help='Allow committing modified files that the conflicting patch did '
        'not originally touch.')
    drop_parser = subparsers.add_parser(
        'drop',
        help='Discard the current conflicting patch, delete its .patch file, '
        'and continue replaying remaining patches.')
    drop_parser.add_argument(
        '--reason',
        required=True,
        help='Reason for dropping the patch (recorded in the commit message).')
    subparsers.add_parser(
        'resume',
        help='Reconcile roll state after manual Apply/Drop commits and '
        'continue replaying remaining patches.')
    subparsers.add_parser(
        'generate',
        help='Run gen_extra_chromium_files.py -C <out_dir> and commit '
        'regenerated bootstrap files.')
    subparsers.add_parser(
        'abort',
        help='Reset an active roll branch to start_commit or clear '
        'GITDIR/protobuf-roll/.')
    for target_parser in (plan_parser, prepare_parser, start_parser):
        target_parser.add_argument(
            '--to',
            required=True,
            dest='to_revision',
            help='Upstream tag or commit to roll to (for example v37.0).')
    for subparser in subparsers.choices.values():
        subparser.add_argument(
            '-C',
            '--build-dir',
            dest='build_directory',
            help='Optional GN build output directory (for example, '
            'out/Default). When provided without --cache-dir, stores roll '
            'cache data under <build_dir>/.protobuf_roll/.')
        subparser.add_argument(
            '--protobuf-dir',
            default=_PROTOBUF_DIRECTORY,
            dest='protobuf_directory',
            help='Path to //third_party/protobuf (default: script directory).')
        subparser.add_argument(
            '--upstream-repo',
            dest='upstream_repository_path',
            help='Path to an existing local upstream protobuf git repository '
            '(skips cache clone and network fetch).')
        subparser.add_argument(
            '--cache-dir',
            dest='cache_directory',
            help='Path to the bare upstream cache repository (default: '
            '$XDG_CACHE_HOME/chromium-protobuf-roll/upstream-protobuf or '
            '~/.cache/chromium-protobuf-roll/upstream-protobuf, or '
            '<build_dir>/.protobuf_roll/upstream-protobuf when -C is set).')
        subparser.add_argument('--upstream-url',
                               default=UPSTREAM_REPOSITORY_URL,
                               help='Remote git URL for upstream protobuf '
                               f'(default: {UPSTREAM_REPOSITORY_URL}).')
        subparser.add_argument(
            '--json',
            action='store_true',
            dest='should_output_json',
            help='Print the result as machine-readable JSON.')
    arguments = parser.parse_args(command_line_arguments)
    upstream_arguments = (arguments.upstream_repository_path,
                          arguments.cache_directory, arguments.build_directory,
                          arguments.upstream_url)

    try:
        if arguments.subcommand == 'plan':
            result = PlanRoll(arguments.to_revision,
                              arguments.protobuf_directory,
                              *upstream_arguments)
            report = FormatRollPlanReport(result)
            # A plan with conflicts is what `plan` is for.
            exit_code = EXIT_CODE_SUCCESS
        elif arguments.subcommand == 'prepare':
            result = PrepareRoll(arguments.to_revision,
                                 arguments.protobuf_directory,
                                 *upstream_arguments)
            report = FormatRollStateReport(result)
            exit_code = EXIT_CODE_SUCCESS
        elif arguments.subcommand == 'start':
            result = StartRoll(arguments.to_revision,
                               arguments.protobuf_directory,
                               *upstream_arguments)
            report = FormatRollStateReport(result)
            exit_code = (EXIT_CODE_CONFLICT
                         if result.phase == 'conflict' else EXIT_CODE_SUCCESS)
        elif arguments.subcommand == 'continue':
            result = ContinueRoll(
                arguments.protobuf_directory,
                allow_extra_files=arguments.allow_extra_files)
            report = FormatRollStateReport(result)
            exit_code = (EXIT_CODE_CONFLICT
                         if result.phase == 'conflict' else EXIT_CODE_SUCCESS)
        elif arguments.subcommand == 'drop':
            result = DropCurrentPatch(arguments.reason,
                                      arguments.protobuf_directory)
            report = FormatRollStateReport(result)
            exit_code = (EXIT_CODE_CONFLICT
                         if result.phase == 'conflict' else EXIT_CODE_SUCCESS)
        elif arguments.subcommand == 'resume':
            result = ResumeRoll(arguments.protobuf_directory)
            report = FormatRollStateReport(result)
            exit_code = (EXIT_CODE_CONFLICT
                         if result.phase == 'conflict' else EXIT_CODE_SUCCESS)
        elif arguments.subcommand == 'generate':
            result = GenerateBootstrapFiles(arguments.build_directory or '',
                                            arguments.protobuf_directory)
            report = FormatGenerateReport(result)
            exit_code = EXIT_CODE_SUCCESS
        elif arguments.subcommand == 'status':
            result = GetRollStatus(arguments.protobuf_directory)
            report = FormatRollStatusReport(result)
            exit_code = EXIT_CODE_SUCCESS
        elif arguments.subcommand == 'abort':
            checkout = _ResolveCheckoutPaths(arguments.protobuf_directory)
            prior_state = LoadRollState(checkout.protobuf_directory)
            did_reset = (prior_state is not None
                         and _CanResetRollHead(checkout, prior_state))
            result = AbortRoll(arguments.protobuf_directory)
            if did_reset:
                report = (f'Aborted roll to {result.new_revision} and reset '
                          f'to {result.start_commit}.')
            elif result.phase == 'prepared':
                report = f'Aborted prepared roll to {result.new_revision}.'
            else:
                report = f'Removed stale roll state for {result.new_revision}.'
            exit_code = EXIT_CODE_SUCCESS
        else:
            result = CheckRoll(arguments.protobuf_directory,
                               *upstream_arguments,
                               baseline_name=arguments.baseline_name)
            report = FormatRollCheckReport(result)
            exit_code = (EXIT_CODE_SUCCESS
                         if result.IsClean() else EXIT_CODE_ERROR)
        if arguments.should_output_json:
            report = json.dumps(result.ToDictionary(), indent=2)
        print(report)
        return exit_code
    except (RollProtobufError, OSError) as error:
        if arguments.should_output_json:
            print(
                json.dumps({
                    'is_clean': False,
                    'error': str(error)
                }, indent=2))
        else:
            print(f'Error: {error}', file=sys.stderr)
        return EXIT_CODE_ERROR


if __name__ == '__main__':
    sys.exit(main())
