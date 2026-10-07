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

from collections.abc import Iterable, Mapping, Sequence
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
_PATCHES_DIRECTORY_NAME = 'patches'
_DEFAULT_XDG_CACHE_SUBDIRECTORY = 'chromium-protobuf-roll'
_BUILD_DIRECTORY_CACHE_SUBDIRECTORY = '.protobuf_roll'
_UPSTREAM_CACHE_REPOSITORY_NAME = 'upstream-protobuf'

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

_README_REVISION_PATTERN = re.compile(
    r'^Revision:\s*(\S+)\s*$', re.MULTILINE)
_FULL_COMMIT_HASH_PATTERN = re.compile(r'[0-9a-f]{40}')
_REQUESTED_REVISION_REF = 'refs/protobuf-roll/requested'


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
    """Returns True if relative_path is a top-level patches/*.{patch,md} file.

    Upstream protobuf stores its own Bazel module patches in subdirectories such
    as patches/protobuf_v25/*.patch. Chromium stores its patches and companion
    notes directly under patches/, so any path containing a second '/' belongs
    to upstream rather than Chromium.
    """
    patches_prefix = _PATCHES_DIRECTORY_NAME + '/'
    if not relative_path.startswith(patches_prefix):
        return False
    filename_in_patches = relative_path[len(patches_prefix):]
    if not filename_in_patches or '/' in filename_in_patches:
        return False
    return filename_in_patches.endswith(('.patch', '.md'))


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
    """Extracts the upstream git revision from README.chromium text.

    Args:
      readme_text: Full text content of //third_party/protobuf/README.chromium.

    Returns:
      The non-empty revision token following 'Revision:'.

    Raises:
      RollProtobufError: If no 'Revision:' line is present in readme_text.
    """
    match = _README_REVISION_PATTERN.search(readme_text)
    if match is None:
        raise RollProtobufError(
            f'Could not find a "Revision:" line in {_README_FILENAME}.')
    return match.group(1)


def ReadReadmeRevisionFromDirectory(protobuf_directory: str) -> str:
    """Reads README.chromium in protobuf_directory and returns its Revision.

    Args:
      protobuf_directory: Path to the //third_party/protobuf directory.

    Returns:
      The non-empty revision token following 'Revision:'.

    Raises:
      RollProtobufError: If README.chromium is missing or lacks a 'Revision:'
        line.
    """
    readme_path = os.path.join(protobuf_directory, _README_FILENAME)
    if not os.path.isfile(readme_path):
        raise RollProtobufError(f'Missing {_README_FILENAME} at {readme_path}.')
    with open(readme_path, 'r', encoding='utf-8') as readme_file:
        return ReadReadmeRevision(readme_file.read())


def _IsolatedGitEnvironment() -> dict[str, str]:
    """Returns os.environ without inherited GIT_* repository overrides.

    When git hooks or wrapper scripts set GIT_DIR or GIT_WORK_TREE, child git
    invocations ignore '-C <repository>' unless those variables are cleared.
    """
    environment = os.environ.copy()
    for variable_name in (
        'GIT_ALTERNATE_OBJECT_DIRECTORIES',
        'GIT_COMMON_DIR',
        'GIT_DIR',
        'GIT_INDEX_FILE',
        'GIT_OBJECT_DIRECTORY',
        'GIT_WORK_TREE',
    ):
        environment.pop(variable_name, None)
    return environment


def _GitCommandForRepository(
        repository_path: str,
        git_arguments: Sequence[str]) -> list[str]:
    """Builds a git command targeting either a working-tree or bare repository.

    Developer environments with 'safe.bareRepository = explicit' reject
    'git -C <bare_repository>' unless '--git-dir=<bare_repository>' is passed
    explicitly. Non-bare repositories and git worktrees contain a '.git' entry
    in repository_path and use '-C'.
    """
    dot_git_entry = os.path.join(repository_path, '.git')
    if os.path.exists(dot_git_entry):
        return ['git', '-C', repository_path, *git_arguments]
    return ['git', f'--git-dir={repository_path}', *git_arguments]


def _IsValidGitRepository(repository_path: str) -> bool:
    """Returns True if repository_path is an initialized git repository."""
    if not os.path.isdir(repository_path):
        return False
    completed_process = subprocess.run(
        _GitCommandForRepository(repository_path, ['rev-parse', '--git-dir']),
        env=_IsolatedGitEnvironment(),
        capture_output=True,
        text=True,
        check=False,
    )
    return completed_process.returncode == 0


def _ResolveCommitHash(repository_path: str, revision: str) -> str | None:
    """Returns the resolved commit SHA if revision exists in repository_path."""
    completed_process = subprocess.run(
        _GitCommandForRepository(
            repository_path,
            ['rev-parse', '--verify', '--quiet', f'{revision}^{{commit}}'],
        ),
        env=_IsolatedGitEnvironment(),
        capture_output=True,
        text=True,
        check=False,
    )
    if completed_process.returncode != 0:
        return None
    return completed_process.stdout.strip()


def ResolveDefaultCacheDirectory(
        build_directory: str | None = None,
        environment: Mapping[str, str] | None = None) -> str:
    """Returns the default bare upstream cache repository path.

    When build_directory is supplied, stores the upstream cache at
    <build_directory>/.protobuf_roll/upstream-protobuf. Otherwise, uses
    $XDG_CACHE_HOME/chromium-protobuf-roll/upstream-protobuf when XDG_CACHE_HOME
    is set and non-empty, or falls back to
    ~/.cache/chromium-protobuf-roll/upstream-protobuf. Keeping the cache under
    XDG_CACHE_HOME or the build directory avoids writing inside src/.git/ while
    preserving the cache across reboots and branch switches.

    Args:
      build_directory: Optional GN build output directory.
      environment: Optional environment dictionary for resolving XDG_CACHE_HOME
        (defaults to os.environ).

    Returns:
      Absolute path to the default upstream cache repository directory.
    """
    if build_directory is not None:
        return os.path.join(
            os.path.abspath(build_directory),
            _BUILD_DIRECTORY_CACHE_SUBDIRECTORY,
            _UPSTREAM_CACHE_REPOSITORY_NAME,
        )
    active_environment = os.environ if environment is None else environment
    xdg_cache_home = active_environment.get('XDG_CACHE_HOME', '').strip()
    if xdg_cache_home:
        cache_base_directory = os.path.abspath(
            os.path.expanduser(xdg_cache_home))
    else:
        cache_base_directory = os.path.abspath(
            os.path.expanduser('~/.cache'))
    return os.path.join(
        cache_base_directory,
        _DEFAULT_XDG_CACHE_SUBDIRECTORY,
        _UPSTREAM_CACHE_REPOSITORY_NAME,
    )


def _CloneBareUpstreamRepository(
        upstream_url: str,
        cache_directory: str) -> None:
    """Clones upstream_url as a bare repository into cache_directory."""
    parent_directory = os.path.dirname(os.path.abspath(cache_directory))
    os.makedirs(parent_directory, exist_ok=True)
    completed_process = subprocess.run(
        ['git', 'clone', '--bare', upstream_url, cache_directory],
        env=_IsolatedGitEnvironment(),
        capture_output=True,
        text=True,
        check=False,
    )
    if completed_process.returncode != 0:
        raise RollProtobufError(
            f'Failed to clone upstream protobuf from {upstream_url} into '
            f'{cache_directory}: {completed_process.stderr.strip()}')


def _FetchRevisionIntoCache(
        cache_directory: str,
        upstream_url: str,
        revision: str) -> None:
    """Fetches revision and tags from upstream_url into cache_directory."""
    completed_process = subprocess.run(
        _GitCommandForRepository(
            cache_directory,
            [
                'fetch',
                '--tags',
                '--force',
                upstream_url,
                f'+{revision}:{_REQUESTED_REVISION_REF}',
            ],
        ),
        env=_IsolatedGitEnvironment(),
        capture_output=True,
        text=True,
        check=False,
    )
    if completed_process.returncode != 0:
        raise RollProtobufError(
            f'Failed to fetch revision {revision} from {upstream_url} into '
            f'{cache_directory}: {completed_process.stderr.strip()}')


def EnsureUpstreamCommitCached(
        revision: str,
        upstream_repository_path: str | None = None,
        cache_directory: str | None = None,
        build_directory: str | None = None,
        upstream_url: str = UPSTREAM_REPOSITORY_URL) -> tuple[str, str]:
    """Ensures revision is cached and returns (repository_path, commit_sha).

    When upstream_repository_path is supplied, that local repository is used
    directly without cloning or network fetches. Otherwise, a bare clone at
    cache_directory (defaulting to ResolveDefaultCacheDirectory) is created
    and fetched on demand when revision is not already a cached 40-character
    commit hash. Concurrent invocations against the same cache_directory are
    not supported because _REQUESTED_REVISION_REF is updated in place.

    Args:
      revision: Commit SHA, tag, or ref to resolve.
      upstream_repository_path: Optional path to an existing local upstream git
        repository (bare or non-bare).
      cache_directory: Optional path to the bare cache repository directory.
      build_directory: Optional GN build output directory.
      upstream_url: Remote URL used when initializing or fetching the cache.

    Returns:
      A (repository_path, resolved_commit_sha) tuple.

    Raises:
      RollProtobufError: If the repository cannot be initialized or revision
        cannot be resolved to a commit.
    """
    if upstream_repository_path is not None:
        repository_path = os.path.abspath(upstream_repository_path)
        if not _IsValidGitRepository(repository_path):
            raise RollProtobufError(
                f'Upstream repository path is not a git repository: '
                f'{repository_path}')
        resolved_commit = _ResolveCommitHash(repository_path, revision)
        if resolved_commit is None:
            raise RollProtobufError(
                f'Revision {revision} not found in upstream repository '
                f'{repository_path}.')
        return repository_path, resolved_commit

    resolved_cache_directory = (
        os.path.abspath(cache_directory)
        if cache_directory is not None
        else ResolveDefaultCacheDirectory(build_directory=build_directory))
    if not _IsValidGitRepository(resolved_cache_directory):
        _CloneBareUpstreamRepository(upstream_url, resolved_cache_directory)

    resolved_commit = None
    if _FULL_COMMIT_HASH_PATTERN.fullmatch(revision):
        resolved_commit = _ResolveCommitHash(resolved_cache_directory, revision)
    if resolved_commit is None:
        _FetchRevisionIntoCache(
            resolved_cache_directory, upstream_url, revision)
        resolved_commit = _ResolveCommitHash(
            resolved_cache_directory, _REQUESTED_REVISION_REF)
    if resolved_commit is None:
        raise RollProtobufError(
            f'Revision {revision} could not be resolved in cache '
            f'{resolved_cache_directory}.')
    return resolved_cache_directory, resolved_commit


def _StreamGitArchiveToDirectory(
        git_archive_command: Sequence[str],
        destination_directory: str) -> tuple[int, str, Exception | None]:
    """Runs `git archive` and streams stdout into `destination_directory`.

    Redirects `stderr` to a temporary file rather than an OS pipe so verbose
    git trace output (such as `GIT_TRACE_PACK_ACCESS=1`) cannot fill the OS
    pipe buffer and deadlock while `tarfile` drains `stdout`.

    Returns:
      A `(return_code, stderr_text, extraction_error)` tuple where
      `extraction_error` is the `tarfile.TarError` or `OSError` raised while
      streaming `stdout` (or `None` if extraction succeeded).
    """
    extraction_error: Exception | None = None
    with tempfile.TemporaryFile() as stderr_file:
        with subprocess.Popen(
            git_archive_command,
            env=_IsolatedGitEnvironment(),
            stdout=subprocess.PIPE,
            stderr=stderr_file,
        ) as archive_process:
            assert archive_process.stdout is not None
            try:
                with tarfile.open(
                        fileobj=archive_process.stdout, mode='r|') as archive:
                    archive.extractall(destination_directory, filter='data')
            except (tarfile.TarError, OSError) as caught_error:
                extraction_error = caught_error
            finally:
                archive_process.stdout.close()
            return_code = archive_process.wait()
        stderr_file.seek(0)
        stderr_text = stderr_file.read().decode(
            'utf-8', errors='replace').strip()
    return return_code, stderr_text, extraction_error


def ExtractUpstreamArchive(
        repository_path: str,
        commit_revision: str,
        destination_directory: str) -> None:
    """Extracts the git tree for commit_revision into destination_directory.

    Streams 'git archive --format=tar' directly through Python's tarfile module
    with the 'data' extraction filter so unsafe paths and links are rejected.

    Args:
      repository_path: Local git repository path (bare or non-bare).
      commit_revision: Commit SHA or ref to extract.
      destination_directory: Directory where archive entries are extracted.

    Raises:
      RollProtobufError: If git archive fails or tar extraction fails.
    """
    git_archive_command = _GitCommandForRepository(
        repository_path,
        ['archive', '--format=tar', commit_revision],
    )
    return_code, stderr_text, extraction_error = _StreamGitArchiveToDirectory(
        git_archive_command, destination_directory)
    if extraction_error is not None:
        git_error_suffix = (
            f' (git archive failed: {stderr_text})'
            if return_code != 0 and stderr_text != '' else ''
        )
        raise RollProtobufError(
            f'Failed to extract archive for {commit_revision} from '
            f'{repository_path}: {extraction_error}{git_error_suffix}'
        ) from extraction_error
    if return_code != 0:
        raise RollProtobufError(
            f'git archive failed for {commit_revision} in {repository_path}: '
            f'{stderr_text}')


def ListTopLevelPatchPaths(protobuf_directory: str) -> list[str]:
    """Returns sorted absolute paths to top-level patches/*.patch files.

    Args:
      protobuf_directory: Path to the //third_party/protobuf directory.

    Returns:
      Lexicographically sorted list of absolute paths to top-level .patch files
      under <protobuf_directory>/patches, or an empty list if patches/ is
      absent.
    """
    patches_directory = os.path.join(
        protobuf_directory, _PATCHES_DIRECTORY_NAME)
    if not os.path.isdir(patches_directory):
        return []
    patch_paths: list[str] = []
    for entry_name in sorted(os.listdir(patches_directory)):
        if not entry_name.endswith('.patch'):
            continue
        full_path = os.path.join(patches_directory, entry_name)
        if os.path.isfile(full_path):
            patch_paths.append(full_path)
    return patch_paths


def ApplyPatchFile(patch_path: str, target_directory: str) -> str | None:
    """Applies patch_path to target_directory, returning None on success.

    Uses 'git apply -p1 --unidiff-zero' so git extended headers and
    zero-context hunks (such as 0072-remove-version-stamp.patch) are handled
    without external tools. Sets GIT_CEILING_DIRECTORIES to the parent of
    target_directory so git never walks up into an enclosing working tree
    (which would otherwise cause git apply to silently skip paths that do not
    begin with the subdirectory prefix).

    Args:
      patch_path: Path to the .patch file to apply.
      target_directory: Root directory of the tree to patch.

    Returns:
      None if the patch applied cleanly, or the diagnostic error message if
      application failed.
    """
    absolute_patch_path = os.path.abspath(patch_path)
    real_target_directory = os.path.realpath(target_directory)
    environment = _IsolatedGitEnvironment()
    environment['GIT_CEILING_DIRECTORIES'] = os.path.dirname(
        real_target_directory)
    git_apply_process = subprocess.run(
        ['git', 'apply', '-p1', '--unidiff-zero', absolute_patch_path],
        cwd=real_target_directory,
        env=environment,
        stdin=subprocess.DEVNULL,
        capture_output=True,
        text=True,
        check=False,
    )
    if git_apply_process.returncode == 0:
        return None
    return git_apply_process.stderr.strip()


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
