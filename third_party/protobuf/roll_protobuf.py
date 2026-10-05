# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Upstream release cache, archive extraction, and patch primitives for rolls.

Chromium embeds the upstream protobuf repository directly inside
//third_party/protobuf and maintains local modifications in two places:
  1. Applied to the source tree under src/ or python/.
  2. Recorded as top-level patch files in //third_party/protobuf/patches/*.patch
     and documented under 'Description of the patches:' in README.chromium.

During a roll, //third_party/protobuf is replaced with a fresh upstream release
archive and patches/*.patch are replayed on top. This module provides the
shared file-ownership inventory, upstream git cache management, archive
extraction, and isolated patch application primitives used by roll verification
and automation commands (https://crbug.com/568074905).
"""

from collections.abc import Mapping, Sequence
import os
import re
import subprocess
import tarfile
import tempfile

_README_FILENAME = 'README.chromium'
_PATCHES_DIRECTORY_NAME = 'patches'
_DEFAULT_XDG_CACHE_SUBDIRECTORY = 'chromium-protobuf-roll'
_BUILD_DIRECTORY_CACHE_SUBDIRECTORY = '.protobuf_roll'
_UPSTREAM_CACHE_REPOSITORY_NAME = 'upstream-protobuf'

UPSTREAM_REPOSITORY_URL = 'https://github.com/protocolbuffers/protobuf.git'

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
