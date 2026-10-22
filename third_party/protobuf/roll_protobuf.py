#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Verifies that //third_party/protobuf is upstream plus patches/*.patch.

Chromium embeds upstream protobuf in //third_party/protobuf. Local changes
are applied to src/ and python/, and also recorded as top-level
patches/*.patch files (described in README.chromium). A roll replaces the tree
with a fresh upstream archive and replays patches/*.patch, so an edit to src/
that never made it into a patch is silently lost on the next roll
(https://crbug.com/568074905, for example https://crrev.com/c/8422202).
Design: https://crbug.com/568074175.

Subcommands:
  check: Applies patches/*.patch in sorted order to the upstream commit on
    README.chromium's 'Revision:' line and verifies that the result matches
    //third_party/protobuf, modulo known Chromium exceptions.

Exit codes: 0 when all checks passed, 1 when verification failed or an error
occurred, 2 for invalid usage (argparse).
"""

from collections.abc import Collection, Iterable, Sequence
import argparse
import dataclasses
import enum
import filecmp
import functools
import json
import os
import posixpath
import re
import subprocess
import sys
import tarfile
import tempfile

_PROTOBUF_DIRECTORY = os.path.dirname(os.path.abspath(__file__))
_README_FILENAME = 'README.chromium'
_REQUESTED_REVISION_REF = 'refs/protobuf-roll/requested'
UPSTREAM_REPOSITORY_URL = 'https://github.com/protocolbuffers/protobuf.git'
EXIT_CODE_SUCCESS = 0
EXIT_CODE_ERROR = 1

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
    error_message: str | None = None  # `git apply` diagnostics, for ERROR.

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


def main(command_line_arguments: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description='Verifies //third_party/protobuf against upstream plus '
        'patches/*.patch.')
    subparsers = parser.add_subparsers(dest='subcommand', required=True)
    subparsers.add_parser(
        'check',
        help='Verify that upstream at README.chromium Revision plus '
        'patches/*.patch reproduces //third_party/protobuf.')
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
        result = CheckTreeMatchesPatchedUpstream(arguments.protobuf_directory,
                                                 *upstream_arguments)
        report = FormatTreeVerificationReport(result)
        exit_code = EXIT_CODE_SUCCESS if result.IsClean() else EXIT_CODE_ERROR
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
