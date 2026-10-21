#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Verifies //third_party/protobuf against upstream plus patches/*.patch.

Chromium embeds the upstream protobuf repository directly inside
//third_party/protobuf and maintains local modifications in two places:
  1. Applied to the source tree under src/ or python/.
  2. Recorded as top-level patch files in //third_party/protobuf/patches/*.patch
     and documented under 'Description of the patches:' in README.chromium.

During a roll, //third_party/protobuf is replaced with a fresh upstream release
archive and patches/*.patch are replayed on top. If a prior commit edited src/
or python/ without updating patches/*.patch (such as https://crrev.com/c/8422202
for 0070-fix-deprecated-get-enum-in-message.patch), that edit is silently lost
on the next roll (https://crbug.com/568074905).

Subcommands:
  check: Extracts the upstream protobuf commit recorded in README.chromium's
    'Revision:' line from a cached upstream repository, applies all top-level
    patches/*.patch in sorted order, and verifies that the resulting tree
    matches //third_party/protobuf modulo known Chromium exceptions.

Exit codes:
  0 (EXIT_CODE_SUCCESS): Command completed and all checks passed.
  1 (EXIT_CODE_ERROR): Verification failed or an operational error occurred.
  2: Invalid command-line usage (reported by argparse).
"""

from collections.abc import Collection, Iterable, Sequence
import argparse
import dataclasses
import filecmp
import json
import os
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

# Top-level files in //third_party/protobuf that belong to Chromium rather than
# the upstream protobuf release archive. Kept in one place so roll verification
# and automation tooling share a single inventory matching README.chromium
# step 2.
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

# Files under src/ and python/ regenerated in roll step 4 by
# gen_extra_chromium_files.py using Chromium's patched protoc binary. The C++
# outputs legitimately differ from upstream's unpatched copies, and the Python
# outputs do not exist in the raw upstream git archive at all.
BOOTSTRAP_GENERATED_FILES = frozenset({
    'src/google/protobuf/descriptor.pb.h',
    'src/google/protobuf/descriptor.pb.cc',
    'src/google/protobuf/compiler/plugin.pb.h',
    'src/google/protobuf/compiler/plugin.pb.cc',
    'python/google/protobuf/descriptor_pb2.py',
    'python/google/protobuf/compiler/plugin_pb2.py',
    'python/google/protobuf/internal/python_edition_defaults.py',
})

# Upstream bundles pre-generated C++ sources for the Well-Known Types (except
# descriptor.proto and compiler/plugin.proto, which remain bootstrapped).
# Chromium deletes these in gen_extra_chromium_files.py and compiles them from
# .proto files via //third_party/protobuf:well_known_type_protos instead.
WELL_KNOWN_TYPE_NAMES = (
    'any',
    'api',
    'duration',
    'empty',
    'field_mask',
    'source_context',
    'struct',
    'timestamp',
    'type',
    'wrappers',
)

DELETED_WELL_KNOWN_TYPE_GENERATED_FILES = frozenset(
    f'src/google/protobuf/{well_known_type_name}.pb.{extension}'
    for well_known_type_name in WELL_KNOWN_TYPE_NAMES
    for extension in ('h', 'cc')
)

# Upstream directories removed intentionally during rolls because they contain
# large historical test artifacts that trip Chromium presubmit checks.
DELETED_UPSTREAM_DIRECTORY_PREFIXES = (
    'compatibility/',
)


class RollProtobufError(Exception):
    """Raised when a git, filesystem, or metadata operation cannot proceed."""


@dataclasses.dataclass(frozen=True)
class TreeVerificationResult:
    """Outcome of comparing //third_party/protobuf against patched upstream.

    Attributes:
      upstream_revision: Commit SHA or ref extracted from README.chromium.
      applied_patches: Tuple of top-level patches/*.patch filenames that applied
        cleanly prior to completion or failure.
      failing_patch: Filename of the first patch that failed to apply, or None
        if every patch in patches/*.patch applied cleanly.
      failing_patch_error: Diagnostic output from git apply when failing_patch
        is set, or None otherwise.
      modified_files: Files present in both trees (tracked, or untracked but
        not git-ignored, on the Chromium side) whose byte contents differ,
        excluding Chromium-owned and bootstrap-generated files.
      chromium_only_files: Files in //third_party/protobuf absent from the
        expected patched upstream tree, excluding Chromium-owned files
        (CHROMIUM_OWNED_FILES and top-level patches/*.{patch,md}) and
        bootstrap-generated files.
      upstream_only_files: Files in the expected patched upstream tree absent
        from //third_party/protobuf, excluding intentionally deleted upstream
        files and paths git-ignored in //third_party/protobuf.
    """

    upstream_revision: str
    applied_patches: tuple[str, ...]
    failing_patch: str | None = None
    failing_patch_error: str | None = None
    modified_files: tuple[str, ...] = ()
    chromium_only_files: tuple[str, ...] = ()
    upstream_only_files: tuple[str, ...] = ()

    def IsClean(self) -> bool:
        """Returns True if all patches applied and no unexpected diffs exist."""
        return (self.failing_patch is None
                and not self.modified_files
                and not self.chromium_only_files
                and not self.upstream_only_files)

    def ToDictionary(self) -> dict[str, object]:
        """Returns a JSON-serializable dictionary representation."""
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


def IsTopLevelChromiumPatchOrNote(relative_path: str) -> bool:
    """Returns True for top-level patches/*.patch and patches/*.md files.

    Upstream keeps its own Bazel patches in subdirectories such as
    patches/protobuf_v25/, so a path with a second '/' is upstream's.
    """
    directory, _, filename = relative_path.partition('/')
    return (directory == 'patches' and '/' not in filename
            and filename.endswith(('.patch', '.md')))


def IsChromiumOwnedFile(relative_path: str) -> bool:
    """Returns True if relative_path belongs to Chromium, not upstream."""
    return (relative_path in CHROMIUM_OWNED_FILES
            or IsTopLevelChromiumPatchOrNote(relative_path))


def IsBootstrapGeneratedFile(relative_path: str) -> bool:
    """Returns True if gen_extra_chromium_files.py generates relative_path."""
    return relative_path in BOOTSTRAP_GENERATED_FILES


def IsIntentionallyDeletedUpstreamFile(relative_path: str) -> bool:
    """Returns True if relative_path is removed from upstream during rolls."""
    return (relative_path.startswith(DELETED_UPSTREAM_DIRECTORY_PREFIXES)
            or relative_path in DELETED_WELL_KNOWN_TYPE_GENERATED_FILES)


def ReadReadmeRevision(readme_text: str) -> str:
    match = re.search(r'^Revision:\s*(\S+)\s*$', readme_text, re.MULTILINE)
    if match is None:
        raise RollProtobufError(
            f'Could not find a "Revision:" line in {_README_FILENAME}.')
    return match.group(1)


def ReadReadmeRevisionFromDirectory(protobuf_directory: str) -> str:
    readme_path = os.path.join(protobuf_directory, _README_FILENAME)
    if not os.path.isfile(readme_path):
        raise RollProtobufError(f'Missing {_README_FILENAME} at {readme_path}.')
    with open(readme_path, encoding='utf-8') as readme_file:
        return ReadReadmeRevision(readme_file.read())


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


def _FetchRevisionIntoCache(cache_directory: str, upstream_url: str,
                            revision: str) -> None:
    _RunGit(_GitCommandForRepository(cache_directory, [
        'fetch', '--tags', '--force', upstream_url,
        f'+{revision}:{_REQUESTED_REVISION_REF}'
    ]),
            failure_message=(f'Failed to fetch revision {revision} from '
                             f'{upstream_url} into {cache_directory}'))


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
        _FetchRevisionIntoCache(cache, upstream_url, revision)
        commit = _ResolveCommitHash(cache, _REQUESTED_REVISION_REF)
    if commit is None:
        raise RollProtobufError(
            f'Revision {revision} could not be resolved in cache {cache}.')
    return cache, commit


def _StreamGitArchiveToDirectory(
        git_archive_command: Sequence[str],
        destination_directory: str) -> tuple[int, str, Exception | None]:
    """Extracts `git archive` output; returns (exit code, stderr, error).

    stderr goes to a temporary file, not a pipe: trace output
    (GIT_TRACE_PACK_ACCESS=1) can fill a pipe and deadlock while tarfile
    drains stdout. tarfile's 'data' filter rejects unsafe paths and links.
    """
    extraction_error = None
    with tempfile.TemporaryFile() as stderr_file:
        with subprocess.Popen(git_archive_command,
                              env=_IsolatedGitEnvironment(),
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
        stderr = stderr_file.read().decode('utf-8', errors='replace').strip()
    return archive_process.returncode, stderr, extraction_error


def ExtractUpstreamArchive(repository_path: str, commit_revision: str,
                           destination_directory: str) -> None:
    """Extracts the tree of commit_revision into destination_directory.

    When extraction fails, git usually dies of SIGPIPE (-13, or 141 behind a
    shell shim), so the extraction error is reported together with git's
    stderr instead of keying on the exit code.
    """
    return_code, git_error, extraction_error = _StreamGitArchiveToDirectory(
        _GitCommandForRepository(repository_path,
                                 ['archive', '--format=tar', commit_revision]),
        destination_directory)
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
            **_IsolatedGitEnvironment(),
            'GIT_CEILING_DIRECTORIES':
            os.path.dirname(target_directory),
        },
        cwd=target_directory,
        accepted_return_codes=None)
    if process.returncode == 0:
        return None
    return process.stderr.decode('utf-8', errors='replace').strip()


def _ListRegularFilesInDirectory(root_directory: str) -> set[str]:
    """Returns POSIX relative paths of regular files under root_directory."""
    relative_paths: set[str] = set()
    for current_root, directory_names, filenames in os.walk(root_directory):
        directory_names[:] = [
            directory_name
            for directory_name in directory_names
            if directory_name not in ('.git', '__pycache__')
        ]
        for filename in filenames:
            if filename.endswith('.pyc'):
                continue
            full_path = os.path.join(current_root, filename)
            if os.path.islink(full_path) or not os.path.isfile(full_path):
                continue
            relative_path = os.path.relpath(full_path, root_directory)
            relative_paths.add(relative_path.replace('\\', '/'))
    return relative_paths


def _IsInsideGitWorkTree(directory_path: str) -> bool:
    """Returns True if directory_path resides inside a git working tree."""
    completed_process = subprocess.run(
        ['git', '-C', directory_path, 'rev-parse', '--is-inside-work-tree'],
        env=_IsolatedGitEnvironment(),
        capture_output=True,
        text=True,
        check=False,
    )
    return (completed_process.returncode == 0
            and completed_process.stdout.strip() == 'true')


def _ListChromiumProtobufFiles(protobuf_directory: str) -> set[str]:
    """Returns relative POSIX paths of non-ignored regular files in protobuf.

    Queries `git ls-files --cached --others --exclude-standard` with
    `core.excludesFile` set to `os.devnull` so user-level global ignore rules do
    not hide untracked local files inside `protobuf_directory`. Symlinks and
    missing working-tree entries are skipped.

    Args:
      protobuf_directory: Path to `//third_party/protobuf`.

    Returns:
      Set of relative POSIX file paths present in `protobuf_directory`.

    Raises:
      RollProtobufError: If `protobuf_directory` is outside a git working tree,
        `git ls-files` fails, or `README.chromium` is absent (for example, when
        `protobuf_directory` itself is git-ignored).
    """
    if not _IsInsideGitWorkTree(protobuf_directory):
        raise RollProtobufError(
            f'{protobuf_directory} is not inside a git working tree; check '
            f'needs git to apply .gitignore the way `git add` does.')

    completed_process = subprocess.run(
        [
            'git',
            '-C',
            protobuf_directory,
            '-c',
            f'core.excludesFile={os.devnull}',
            'ls-files',
            '--cached',
            '--others',
            '--exclude-standard',
            '-z',
        ],
        env=_IsolatedGitEnvironment(),
        capture_output=True,
        text=True,
        check=False,
    )
    if completed_process.returncode != 0:
        raise RollProtobufError(
            f'git ls-files failed in {protobuf_directory}: '
            f'{completed_process.stderr.strip()}')

    present_paths: set[str] = set()
    for raw_entry in completed_process.stdout.split('\0'):
        if not raw_entry:
            continue
        normalized_path = raw_entry.replace('\\', '/')
        full_path = os.path.join(protobuf_directory, normalized_path)
        if not os.path.islink(full_path) and os.path.isfile(full_path):
            present_paths.add(normalized_path)
    if _README_FILENAME not in present_paths:
        raise RollProtobufError(
            f'git ls-files returned no {_README_FILENAME} in '
            f'{protobuf_directory}; ensure the directory is not git-ignored.')
    return present_paths


def _FindGitIgnoredUpstreamPaths(
        protobuf_directory: str,
        candidate_paths: Iterable[str]) -> set[str]:
    """Returns the subset of `candidate_paths` ignored by git in protobuf.

    When an upstream release archive is added to Chromium's repository via
    `git add third_party/protobuf`, Chromium's `.gitignore` excludes a small set
    of C# and Xcode project files (`*.targets`, `*.props`, `*.sln`, and
    `*.xcodeproj/*`). Querying `git check-ignore -z --stdin` (without
    `--no-index`) filters those untracked repository-ignored paths while still
    flagging any tracked upstream file that matches a `.gitignore` pattern (such
    as `src/solaris/libstdc++.la` matching `*.la`) if it is deleted from disk.
    Passing `-c core.excludesFile=/dev/null` neutralizes the developer's global
    gitignore while keeping in-tree `.gitignore` files and repository-local
    `$GIT_DIR/info/exclude` rules active.
    """
    sorted_candidates = sorted(candidate_paths)
    if not sorted_candidates:
        return set()

    completed_process = subprocess.run(
        [
            'git',
            '-C',
            protobuf_directory,
            '-c',
            f'core.excludesFile={os.devnull}',
            'check-ignore',
            '-z',
            '--stdin',
        ],
        env=_IsolatedGitEnvironment(),
        input='\0'.join(sorted_candidates) + '\0',
        capture_output=True,
        text=True,
        check=False,
    )
    if completed_process.returncode not in (0, 1):
        raise RollProtobufError(
            f'git check-ignore failed in {protobuf_directory}: '
            f'{completed_process.stderr.strip()}')
    return {
        ignored_path.replace('\\', '/')
        for ignored_path in completed_process.stdout.split('\0')
        if ignored_path
    }


def _FindModifiedFiles(
        patched_upstream_directory: str,
        protobuf_directory: str,
        expected_upstream_files: set[str],
        chromium_files: set[str]) -> tuple[str, ...]:
    """Returns sorted shared files whose contents differ unexpectedly."""
    modified_files: list[str] = []
    for relative_path in sorted(expected_upstream_files & chromium_files):
        if (IsBootstrapGeneratedFile(relative_path)
                or IsChromiumOwnedFile(relative_path)):
            continue
        upstream_file_path = os.path.join(
            patched_upstream_directory, relative_path)
        chromium_file_path = os.path.join(protobuf_directory, relative_path)
        if not filecmp.cmp(
                upstream_file_path, chromium_file_path, shallow=False):
            modified_files.append(relative_path)
    return tuple(modified_files)


def _FindUnexpectedChromiumOnlyFiles(
        expected_upstream_files: set[str],
        chromium_files: set[str]) -> tuple[str, ...]:
    """Returns sorted Chromium-only files not in known exception lists."""
    return tuple(
        sorted(
            relative_path
            for relative_path in (chromium_files - expected_upstream_files)
            if not IsChromiumOwnedFile(relative_path)
            and not IsBootstrapGeneratedFile(relative_path)
        ))


def _FindMissingUpstreamFiles(
        protobuf_directory: str,
        expected_upstream_files: set[str],
        chromium_files: set[str]) -> tuple[str, ...]:
    """Returns sorted expected upstream files absent from Chromium."""
    upstream_only_candidates = expected_upstream_files - chromium_files
    git_ignored_upstream_files = _FindGitIgnoredUpstreamPaths(
        protobuf_directory, upstream_only_candidates)
    return tuple(
        sorted(upstream_only_candidates - git_ignored_upstream_files))


def _ComparePatchedUpstreamWithChromium(
        patched_upstream_directory: str,
        protobuf_directory: str,
        upstream_revision: str,
        applied_patches: Sequence[str]) -> TreeVerificationResult:
    """Compares a patched upstream tree against //third_party/protobuf."""
    raw_upstream_files = _ListRegularFilesInDirectory(
        patched_upstream_directory)
    expected_upstream_files = {
        relative_path
        for relative_path in raw_upstream_files
        if not IsIntentionallyDeletedUpstreamFile(relative_path)
    }
    chromium_files = _ListChromiumProtobufFiles(protobuf_directory)
    return TreeVerificationResult(
        upstream_revision=upstream_revision,
        applied_patches=tuple(applied_patches),
        modified_files=_FindModifiedFiles(
            patched_upstream_directory,
            protobuf_directory,
            expected_upstream_files,
            chromium_files,
        ),
        chromium_only_files=_FindUnexpectedChromiumOnlyFiles(
            expected_upstream_files, chromium_files),
        upstream_only_files=_FindMissingUpstreamFiles(
            protobuf_directory, expected_upstream_files, chromium_files),
    )


def CheckTreeMatchesPatchedUpstream(
        protobuf_directory: str = _PROTOBUF_DIRECTORY,
        upstream_repository_path: str | None = None,
        cache_directory: str | None = None,
        build_directory: str | None = None,
        upstream_url: str = UPSTREAM_REPOSITORY_URL) -> TreeVerificationResult:
    """Verifies //third_party/protobuf matches upstream Revision + patches/*.

    Extracts the upstream commit recorded in `README.chromium`, replays all
    top-level `patches/*.patch` files in lexicographic order, and compares the
    regular file byte contents of the patched upstream tree against
    `protobuf_directory` (file modes and symlinks are not compared).

    Args:
      protobuf_directory: Path to //third_party/protobuf.
      upstream_repository_path: Optional path to an existing local upstream git
        repository.
      cache_directory: Optional path to the bare cache repository directory.
      build_directory: Optional GN build output directory passed via -C.
      upstream_url: Remote URL used when initializing or fetching the cache.

    Returns:
      A TreeVerificationResult describing applied patches and any unexplained
      differences.
    """
    absolute_protobuf_directory = os.path.abspath(protobuf_directory)
    revision = ReadReadmeRevisionFromDirectory(absolute_protobuf_directory)
    repository_path, resolved_commit = EnsureUpstreamCommitCached(
        revision=revision,
        upstream_repository_path=upstream_repository_path,
        cache_directory=cache_directory,
        build_directory=build_directory,
        upstream_url=upstream_url,
    )

    with tempfile.TemporaryDirectory(
            prefix='protobuf_roll_check_') as temporary_directory:
        ExtractUpstreamArchive(
            repository_path=repository_path,
            commit_revision=resolved_commit,
            destination_directory=temporary_directory,
        )

        applied_patches: list[str] = []
        for patch_path in ListTopLevelPatchPaths(absolute_protobuf_directory):
            patch_filename = os.path.basename(patch_path)
            patch_error = ApplyPatchFile(patch_path, temporary_directory)
            if patch_error is not None:
                return TreeVerificationResult(
                    upstream_revision=revision,
                    applied_patches=tuple(applied_patches),
                    failing_patch=patch_filename,
                    failing_patch_error=patch_error,
                )
            applied_patches.append(patch_filename)

        return _ComparePatchedUpstreamWithChromium(
            patched_upstream_directory=temporary_directory,
            protobuf_directory=absolute_protobuf_directory,
            upstream_revision=revision,
            applied_patches=applied_patches,
        )


def _AppendFileListSection(
        lines: list[str],
        heading: str,
        prefix_symbol: str,
        file_paths: Sequence[str]) -> None:
    """Appends a titled section of prefixed file paths when non-empty."""
    if not file_paths:
        return
    lines.append('')
    lines.append(f'{heading} ({len(file_paths)}):')
    for relative_path in file_paths:
        lines.append(f'  {prefix_symbol} {relative_path}')


def FormatTreeVerificationReport(
        verification_result: TreeVerificationResult) -> str:
    """Formats a human-readable report for a TreeVerificationResult.

    Returns a single summary line when `verification_result.IsClean()` is True,
    the failing patch name and `git apply` diagnostic when a patch fails to
    apply, or grouped sections listing modified (`M`), Chromium-only (`+`), and
    missing upstream (`-`) files.
    """
    if verification_result.IsClean():
        patch_count = len(verification_result.applied_patches)
        return (
            f'Verified //third_party/protobuf matches upstream '
            f'{verification_result.upstream_revision} plus '
            f'{patch_count} patch(es).')

    if verification_result.failing_patch is not None:
        lines = [
            f'Patch failed to apply to upstream '
            f'{verification_result.upstream_revision}: '
            f'{verification_result.failing_patch}',
        ]
        if verification_result.failing_patch_error:
            lines.append(verification_result.failing_patch_error)
        return '\n'.join(lines)

    lines = [
        f'//third_party/protobuf does not match upstream '
        f'{verification_result.upstream_revision} plus patches/*.patch:',
    ]
    _AppendFileListSection(
        lines,
        'Modified files not explained by patches/',
        'M',
        verification_result.modified_files,
    )
    _AppendFileListSection(
        lines,
        'Chromium-only files not in known exceptions',
        '+',
        verification_result.chromium_only_files,
    )
    _AppendFileListSection(
        lines,
        'Upstream files missing from //third_party/protobuf',
        '-',
        verification_result.upstream_only_files,
    )
    return '\n'.join(lines)


def _AddRepositoryOptions(parser: argparse.ArgumentParser) -> None:
    """Registers directory and upstream repository flags on parser."""
    parser.add_argument(
        '-C',
        '--build-dir',
        dest='build_directory',
        help=(
            'Optional GN build output directory (for example, out/Default). '
            'When provided without --cache-dir, stores roll cache data under '
            '<build_dir>/.protobuf_roll/.'))
    parser.add_argument(
        '--protobuf-dir',
        default=_PROTOBUF_DIRECTORY,
        dest='protobuf_directory',
        help='Path to //third_party/protobuf (default: script directory).')
    parser.add_argument(
        '--upstream-repo',
        dest='upstream_repository_path',
        help=(
            'Path to an existing local upstream protobuf git repository '
            '(skips cache clone and network fetch).'))
    parser.add_argument(
        '--cache-dir',
        dest='cache_directory',
        help=(
            'Path to the bare upstream cache repository (default: '
            '$XDG_CACHE_HOME/chromium-protobuf-roll/upstream-protobuf or '
            '~/.cache/chromium-protobuf-roll/upstream-protobuf, or '
            '<build_dir>/.protobuf_roll/upstream-protobuf when -C is set).'))
    parser.add_argument(
        '--upstream-url',
        default=UPSTREAM_REPOSITORY_URL,
        dest='upstream_url',
        help=(
            f'Remote git URL for upstream protobuf '
            f'(default: {UPSTREAM_REPOSITORY_URL}).'))
    parser.add_argument(
        '--json',
        action='store_true',
        dest='should_output_json',
        help='Print the verification result as machine-readable JSON.')


def _ParseCommandLineArguments(
        command_line_arguments: Sequence[str] | None) -> argparse.Namespace:
    """Parses command-line arguments for roll_protobuf.py."""
    parser = argparse.ArgumentParser(
        description=(
            'Verifies //third_party/protobuf against upstream plus '
            'patches/*.patch.'))
    subparsers = parser.add_subparsers(
        dest='subcommand',
        required=True,
    )

    check_parser = subparsers.add_parser(
        'check',
        help=(
            'Verify that upstream at README.chromium Revision plus '
            'patches/*.patch reproduces //third_party/protobuf.'))
    _AddRepositoryOptions(check_parser)
    return parser.parse_args(command_line_arguments)


def _RunCheckSubcommand(arguments: argparse.Namespace) -> int:
    """Executes the `check` subcommand and returns its process exit code."""
    verification_result = CheckTreeMatchesPatchedUpstream(
        protobuf_directory=arguments.protobuf_directory,
        upstream_repository_path=arguments.upstream_repository_path,
        cache_directory=arguments.cache_directory,
        build_directory=arguments.build_directory,
        upstream_url=arguments.upstream_url,
    )
    if arguments.should_output_json:
        print(json.dumps(verification_result.ToDictionary(), indent=2))
    else:
        print(FormatTreeVerificationReport(verification_result))
    return (EXIT_CODE_SUCCESS
            if verification_result.IsClean() else EXIT_CODE_ERROR)


def main(command_line_arguments: Sequence[str] | None = None) -> int:
    """Runs the `roll_protobuf.py` CLI and returns its process exit code.

    Args:
      command_line_arguments: Optional sequence of CLI tokens (defaults to
        `sys.argv[1:]`).

    Returns:
      `EXIT_CODE_SUCCESS` (0) when verification succeeds, or `EXIT_CODE_ERROR`
      (1) when differences or runtime errors occur.
    """
    arguments = _ParseCommandLineArguments(command_line_arguments)
    try:
        return _RunCheckSubcommand(arguments)
    except (RollProtobufError, OSError) as error:
        if arguments.should_output_json:
            error_payload = {'is_clean': False, 'error': str(error)}
            print(json.dumps(error_payload, indent=2))
        else:
            print(f'Error: {error}', file=sys.stderr)
        return EXIT_CODE_ERROR


if __name__ == '__main__':
    sys.exit(main())
