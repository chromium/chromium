#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Hermetic unit tests for //third_party/protobuf/roll_protobuf.py."""

from collections.abc import Mapping
import io
import json
import os
import re
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

_PROTOBUF_DIRECTORY = os.path.dirname(os.path.abspath(__file__))
if _PROTOBUF_DIRECTORY not in sys.path:
    sys.path.insert(0, _PROTOBUF_DIRECTORY)

import PRESUBMIT
import roll_protobuf

_STDERR_FLOOD_THEN_ARCHIVE_SCRIPT = """
import os, subprocess, sys
os.set_blocking(2, False)
remaining = memoryview(b't' * (256 * 1024))
while remaining:
    try:
        remaining = remaining[os.write(2, remaining):]
    except BlockingIOError:
        sys.exit(3)
sys.exit(subprocess.call(sys.argv[1:]))
"""


def _SkipOnWindows(reason: str):
    """Skips a unit test on Windows (`win32`) with a test-specific reason."""
    return unittest.skipIf(sys.platform == 'win32', reason)


def _WriteFilesToDirectory(
        root_directory: str,
        files_by_relative_path: Mapping[str, str]) -> None:
    """Writes UTF-8 text files under root_directory."""
    for relative_path, content in files_by_relative_path.items():
        full_path = os.path.join(root_directory, *relative_path.split('/'))
        os.makedirs(os.path.dirname(full_path), exist_ok=True)
        with open(full_path, 'w', encoding='utf-8', newline='\n') as output:
            output.write(content)


def _RunGitCommand(working_directory: str, arguments: list[str]) -> str:
    """Runs git in working_directory with hermetic author identity."""
    environment = os.environ.copy()
    environment['GIT_AUTHOR_NAME'] = 'Test Author'
    environment['GIT_AUTHOR_EMAIL'] = 'test@example.com'
    environment['GIT_COMMITTER_NAME'] = 'Test Author'
    environment['GIT_COMMITTER_EMAIL'] = 'test@example.com'
    completed_process = subprocess.run(
        ['git', '-C', working_directory, *arguments],
        env=environment,
        capture_output=True,
        text=True,
        check=True,
    )
    return completed_process.stdout.strip()


def _InitializeGitRepositoryWithFiles(
        repository_directory: str,
        files_by_relative_path: Mapping[str, str]) -> str:
    """Initializes a git repository, commits files, and returns HEAD SHA."""
    os.makedirs(repository_directory, exist_ok=True)
    _RunGitCommand(repository_directory, ['init', '--quiet'])
    _WriteFilesToDirectory(repository_directory, files_by_relative_path)
    _RunGitCommand(repository_directory, ['add', '-f', '.'])
    _RunGitCommand(
        repository_directory, ['commit', '--quiet', '-m', 'Initial commit'])
    return _RunGitCommand(repository_directory, ['rev-parse', 'HEAD'])


def _CreateUpstreamAndChromiumRepositories(
        temporary_directory: str,
        upstream_files: Mapping[str, str],
        chromium_files: Mapping[str, str]) -> tuple[str, str, str]:
    """Creates paired upstream and Chromium git repositories for testing.

    Unless chromium_files explicitly overrides 'README.chromium', populates
    'README.chromium' with the upstream repository's HEAD commit SHA.
    """
    upstream_directory = os.path.join(temporary_directory, 'upstream')
    chromium_directory = os.path.join(temporary_directory, 'chromium')
    commit_hash = _InitializeGitRepositoryWithFiles(
        upstream_directory, upstream_files)
    merged_chromium_files = {'README.chromium': f'Revision: {commit_hash}\n'}
    merged_chromium_files.update(chromium_files)
    _InitializeGitRepositoryWithFiles(
        chromium_directory, merged_chromium_files)
    return upstream_directory, chromium_directory, commit_hash


class RollProtobufTest(unittest.TestCase):
    """Proves upstream cache, archive extraction, and patch invariants."""

    def setUp(self):
        super().setUp()
        environment_patcher = mock.patch.dict(
            os.environ,
            {
                'GIT_CONFIG_GLOBAL': os.devnull,
                'GIT_CONFIG_NOSYSTEM': '1',
            },
        )
        environment_patcher.start()
        self.addCleanup(environment_patcher.stop)

    def testBootstrapGeneratedFilesMatchPresubmitInventory(self):
        self.assertEqual(
            PRESUBMIT._BOOTSTRAP_GENERATED_FILES,
            roll_protobuf.BOOTSTRAP_GENERATED_FILES,
            msg=(
                'Expected PRESUBMIT.py and roll_protobuf.py to agree on '
                'which files gen_extra_chromium_files.py generates.'),
        )

    def testChromiumOwnedFilesMatchReadmeRollStepTwo(self):
        readme_path = os.path.join(_PROTOBUF_DIRECTORY, 'README.chromium')
        with open(readme_path, 'r', encoding='utf-8') as readme_file:
            readme_text = readme_file.read()
        checkout_command = re.search(
            r"git checkout origin/main -- (.*?)'\:\(glob\)patches/\*'",
            readme_text,
            re.DOTALL,
        )
        restored_files = frozenset(
            token
            for token in (
                checkout_command.group(1).split()
                if checkout_command is not None else ()
            )
            if token != '\\'
        )

        self.assertEqual(
            roll_protobuf.CHROMIUM_OWNED_FILES,
            restored_files,
            msg=(
                'Expected CHROMIUM_OWNED_FILES to list the exact files '
                'that README.chromium roll step 2 restores with git checkout.'),
        )

    def testReadReadmeRevisionExtractsCommitHashFromReadmeText(self):
        readme_text = (
            'Name: Protocol Buffers\n'
            'Version: 36.0\n'
            'Revision: 3f17acbf7ffbda81db8be17505554c726e558bb2\n'
            'Security Critical: yes\n'
        )

        revision = roll_protobuf.ReadReadmeRevision(readme_text)

        self.assertEqual(
            '3f17acbf7ffbda81db8be17505554c726e558bb2',
            revision,
            msg=(
                'Expected ReadReadmeRevision to extract the 40-character '
                'commit hash from the Revision: metadata line.'),
        )

    def testReadReadmeRevisionRaisesErrorWhenRevisionLineIsMissing(self):
        readme_text = 'Name: Protocol Buffers\nVersion: 36.0\n'

        with self.assertRaises(
                roll_protobuf.RollProtobufError,
                msg=(
                    'Expected ReadReadmeRevision to raise RollProtobufError '
                    'when README.chromium lacks a Revision: line.')):
            roll_protobuf.ReadReadmeRevision(readme_text)

    def testIsTopLevelChromiumPatchOrNoteIncludesDirectPatchAndMarkdownFiles(
            self):
        classification_by_path = {
            path: roll_protobuf.IsTopLevelChromiumPatchOrNote(path)
            for path in (
                'patches/0004-fix-shared-library-exports.patch',
                'patches/0044-trim-protoc-main.md',
            )
        }

        self.assertEqual(
            {
                'patches/0004-fix-shared-library-exports.patch': True,
                'patches/0044-trim-protoc-main.md': True,
            },
            classification_by_path,
            msg=(
                'Expected top-level patches/*.patch and patches/*.md files to '
                'be recognized as Chromium-owned patch inventory files.'),
        )

    def testIsTopLevelChromiumPatchOrNoteExcludesUpstreamSubdirectories(self):
        classification_by_path = {
            path: roll_protobuf.IsTopLevelChromiumPatchOrNote(path)
            for path in (
                'patches/protobuf_v25/0001-bazel.patch',
                'patches/protobuf_v25/README.md',
                'src/google/protobuf/message.h',
            )
        }

        self.assertEqual(
            {
                'patches/protobuf_v25/0001-bazel.patch': False,
                'patches/protobuf_v25/README.md': False,
                'src/google/protobuf/message.h': False,
            },
            classification_by_path,
            msg=(
                'Expected upstream patches/protobuf_vNN/ subdirectories and '
                'non-patch paths to be excluded from Chromium patch files.'),
        )

    def testResolveDefaultCacheDirectoryUsesXdgCacheHomeWhenSet(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            xdg_cache_directory = os.path.join(temporary_directory, 'xdg')

            resolved_path = roll_protobuf.ResolveDefaultCacheDirectory(
                environment={'XDG_CACHE_HOME': xdg_cache_directory})

        self.assertEqual(
            os.path.join(
                xdg_cache_directory,
                'chromium-protobuf-roll',
                'upstream-protobuf',
            ),
            resolved_path,
            msg=(
                'Expected ResolveDefaultCacheDirectory to place the upstream '
                'cache under $XDG_CACHE_HOME/chromium-protobuf-roll when '
                'XDG_CACHE_HOME is set.'),
        )

    def testResolveDefaultCacheDirectoryFallsBackToHomeDotCacheWhenXdgUnset(
            self):
        resolved_path = roll_protobuf.ResolveDefaultCacheDirectory(
            environment={})

        self.assertEqual(
            os.path.join(
                os.path.abspath(os.path.expanduser('~/.cache')),
                'chromium-protobuf-roll',
                'upstream-protobuf',
            ),
            resolved_path,
            msg=(
                'Expected ResolveDefaultCacheDirectory to fall back to '
                '~/.cache/chromium-protobuf-roll/upstream-protobuf when '
                'XDG_CACHE_HOME is unset.'),
        )

    def testResolveDefaultCacheDirectoryUsesBuildDirectoryWhenProvided(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_directory = os.path.join(
                temporary_directory, 'out_linux', 'Release')

            resolved_path = roll_protobuf.ResolveDefaultCacheDirectory(
                build_directory=build_directory,
                environment={'XDG_CACHE_HOME': '/unused/xdg'},
            )

        self.assertEqual(
            os.path.join(
                build_directory,
                '.protobuf_roll',
                'upstream-protobuf',
            ),
            resolved_path,
            msg=(
                'Expected ResolveDefaultCacheDirectory to store the upstream '
                'cache under <build_directory>/.protobuf_roll when '
                'build_directory is supplied.'),
        )

    def testEnsureUpstreamCommitCachedResolvesBareRepositoryUnderSafeConfig(
            self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            source_directory = os.path.join(temporary_directory, 'source')
            bare_directory = os.path.join(temporary_directory, 'bare.git')
            commit_hash = _InitializeGitRepositoryWithFiles(
                source_directory,
                {'src/google/protobuf/arena.h': '// arena\n'},
            )
            subprocess.run(
                ['git', 'clone', '--bare', '--quiet',
                 source_directory, bare_directory],
                check=True,
            )

            with mock.patch.dict(
                os.environ,
                {
                    'GIT_CONFIG_COUNT': '1',
                    'GIT_CONFIG_KEY_0': 'safe.bareRepository',
                    'GIT_CONFIG_VALUE_0': 'explicit',
                },
            ):
                _, resolved_commit = (
                    roll_protobuf.EnsureUpstreamCommitCached(
                        revision=commit_hash,
                        upstream_repository_path=bare_directory,
                    ))

        self.assertEqual(
            commit_hash,
            resolved_commit,
            msg=(
                'Expected EnsureUpstreamCommitCached to resolve a commit in a '
                'bare repository via --git-dir without tripping '
                'safe.bareRepository=explicit.'),
        )

    def testEnsureUpstreamCommitCachedClonesMissingCacheRepositoryOnDemand(
            self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory = os.path.join(temporary_directory, 'upstream')
            cache_directory = os.path.join(
                temporary_directory, 'nested', 'cache', 'upstream-protobuf')
            commit_hash = _InitializeGitRepositoryWithFiles(
                upstream_directory,
                {'src/google/protobuf/arena.h': '// v1\n'},
            )

            _, resolved_commit = roll_protobuf.EnsureUpstreamCommitCached(
                revision=commit_hash,
                cache_directory=cache_directory,
                upstream_url=upstream_directory,
            )

        self.assertEqual(
            commit_hash,
            resolved_commit,
            msg=(
                'Expected EnsureUpstreamCommitCached to create and bare-clone '
                'cache_directory on demand when it does not exist yet.'),
        )

    def testEnsureUpstreamCommitCachedFetchesCommitMissingFromExistingCache(
            self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory = os.path.join(temporary_directory, 'upstream')
            cache_directory = os.path.join(temporary_directory, 'cache.git')
            _InitializeGitRepositoryWithFiles(
                upstream_directory,
                {'src/google/protobuf/arena.h': '// v1\n'},
            )
            subprocess.run(
                ['git', 'clone', '--bare', '--quiet',
                 upstream_directory, cache_directory],
                check=True,
            )
            _WriteFilesToDirectory(
                upstream_directory,
                {'src/google/protobuf/arena.h': '// v2\n'},
            )
            _RunGitCommand(upstream_directory, ['add', '.'])
            _RunGitCommand(
                upstream_directory, ['commit', '--quiet', '-m', 'Release v2'])
            second_commit_hash = _RunGitCommand(
                upstream_directory, ['rev-parse', 'HEAD'])

            _, resolved_commit = roll_protobuf.EnsureUpstreamCommitCached(
                revision=second_commit_hash,
                cache_directory=cache_directory,
                upstream_url=upstream_directory,
            )

        self.assertEqual(
            second_commit_hash,
            resolved_commit,
            msg=(
                'Expected EnsureUpstreamCommitCached to fetch a missing '
                'commit into an existing bare cache on demand.'),
        )

    def testEnsureUpstreamCommitCachedRefreshesMovingBranchReference(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory = os.path.join(temporary_directory, 'upstream')
            cache_directory = os.path.join(temporary_directory, 'cache.git')
            _InitializeGitRepositoryWithFiles(
                upstream_directory,
                {'src/google/protobuf/arena.h': '// v1\n'},
            )
            branch_name = _RunGitCommand(
                upstream_directory, ['rev-parse', '--abbrev-ref', 'HEAD'])
            subprocess.run(
                ['git', 'clone', '--bare', '--quiet',
                 upstream_directory, cache_directory],
                check=True,
            )
            _WriteFilesToDirectory(
                upstream_directory,
                {'src/google/protobuf/arena.h': '// v2\n'},
            )
            _RunGitCommand(upstream_directory, ['add', '.'])
            _RunGitCommand(
                upstream_directory, ['commit', '--quiet', '-m', 'Release v2'])
            second_commit_hash = _RunGitCommand(
                upstream_directory, ['rev-parse', 'HEAD'])

            _, resolved_commit = roll_protobuf.EnsureUpstreamCommitCached(
                revision=branch_name,
                cache_directory=cache_directory,
                upstream_url=upstream_directory,
            )

        self.assertEqual(
            second_commit_hash,
            resolved_commit,
            msg=(
                'Expected EnsureUpstreamCommitCached to re-fetch a moving '
                'branch reference into the bare cache rather than returning '
                'a stale cached branch tip.'),
        )

    def testEnsureUpstreamCommitCachedIgnoresInheritedGitDirEnvironmentVariable(
            self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory = os.path.join(temporary_directory, 'upstream')
            unrelated_directory = os.path.join(temporary_directory, 'unrelated')
            upstream_commit = _InitializeGitRepositoryWithFiles(
                upstream_directory,
                {'src/google/protobuf/arena.h': '// upstream\n'},
            )
            _InitializeGitRepositoryWithFiles(
                unrelated_directory,
                {'other.txt': '// unrelated\n'},
            )

            with mock.patch.dict(
                os.environ,
                {'GIT_DIR': os.path.join(unrelated_directory, '.git')},
            ):
                _, resolved_commit = roll_protobuf.EnsureUpstreamCommitCached(
                    revision='HEAD',
                    upstream_repository_path=upstream_directory,
                )

        self.assertEqual(
            upstream_commit,
            resolved_commit,
            msg=(
                'Expected EnsureUpstreamCommitCached to strip inherited '
                'GIT_DIR so git -C resolves the target repository HEAD.'),
        )

    def testEnsureUpstreamCommitCachedRaisesErrorForNonGitUpstreamPath(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            with self.assertRaises(
                    roll_protobuf.RollProtobufError,
                    msg=(
                        'Expected EnsureUpstreamCommitCached to raise '
                        'RollProtobufError when upstream_repository_path is '
                        'not a git repository.')):
                roll_protobuf.EnsureUpstreamCommitCached(
                    revision='HEAD',
                    upstream_repository_path=temporary_directory,
                )

    def testEnsureUpstreamCommitCachedRaisesErrorForMissingUpstreamRevision(
            self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory = os.path.join(temporary_directory, 'upstream')
            _InitializeGitRepositoryWithFiles(
                upstream_directory,
                {'src/google/protobuf/arena.h': '// v1\n'},
            )

            with self.assertRaisesRegex(
                    roll_protobuf.RollProtobufError,
                    'not found in upstream repository',
                    msg=(
                        'Expected EnsureUpstreamCommitCached to raise '
                        'RollProtobufError when revision is missing from '
                        'upstream_repository_path.')):
                roll_protobuf.EnsureUpstreamCommitCached(
                    revision='0000000000000000000000000000000000000000',
                    upstream_repository_path=upstream_directory,
                )

    def testEnsureUpstreamCommitCachedSkipsFetchWhenFullCommitHashIsCached(
            self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory = os.path.join(temporary_directory, 'upstream')
            cache_directory = os.path.join(temporary_directory, 'cache.git')
            commit_hash = _InitializeGitRepositoryWithFiles(
                upstream_directory,
                {'src/google/protobuf/arena.h': '// v1\n'},
            )
            subprocess.run(
                ['git', 'clone', '--bare', '--quiet',
                 upstream_directory, cache_directory],
                check=True,
            )

            with mock.patch.object(
                    roll_protobuf, '_FetchRevisionIntoCache') as mock_fetch:
                roll_protobuf.EnsureUpstreamCommitCached(
                    revision=commit_hash,
                    cache_directory=cache_directory,
                    upstream_url='/nonexistent/remote',
                )

        self.assertEqual(
            0,
            mock_fetch.call_count,
            msg=(
                'Expected EnsureUpstreamCommitCached not to run git fetch '
                'when a 40-character commit SHA is already present in '
                'cache_directory.'),
        )

    def testEnsureUpstreamCommitCachedRaisesErrorWhenFetchFails(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory = os.path.join(temporary_directory, 'upstream')
            cache_directory = os.path.join(temporary_directory, 'cache.git')
            missing_remote_directory = os.path.join(
                temporary_directory, 'missing_remote')
            _InitializeGitRepositoryWithFiles(
                upstream_directory,
                {'src/google/protobuf/arena.h': '// v1\n'},
            )
            subprocess.run(
                ['git', 'clone', '--bare', '--quiet',
                 upstream_directory, cache_directory],
                check=True,
            )

            # Use a non-repository path rather than a missing ref in a valid
            # repository because LUCI's infra/tools/git wrapper matches
            # "fatal: couldn't find remote ref" in DefaultGitRetryRegexp and
            # retries 12 times (~12 minutes) before returning non-zero.
            with self.assertRaisesRegex(
                    roll_protobuf.RollProtobufError,
                    'Failed to fetch revision',
                    msg=(
                        'Expected EnsureUpstreamCommitCached to raise '
                        'RollProtobufError when git fetch fails.')):
                roll_protobuf.EnsureUpstreamCommitCached(
                    revision='v33.5',
                    cache_directory=cache_directory,
                    upstream_url=missing_remote_directory,
                )

    def testExtractUpstreamArchiveMaterializesFilesAtCommitRevision(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory = os.path.join(temporary_directory, 'upstream')
            destination_directory = os.path.join(
                temporary_directory, 'destination')
            os.makedirs(destination_directory, exist_ok=True)
            commit_hash = _InitializeGitRepositoryWithFiles(
                upstream_directory,
                {'src/google/protobuf/arena.h': '// arena v1\n'},
            )

            roll_protobuf.ExtractUpstreamArchive(
                repository_path=upstream_directory,
                commit_revision=commit_hash,
                destination_directory=destination_directory,
            )
            extracted_header_path = os.path.join(
                destination_directory, 'src', 'google', 'protobuf', 'arena.h')
            with open(extracted_header_path, 'r', encoding='utf-8') as header:
                extracted_content = header.read()

        self.assertEqual(
            '// arena v1\n',
            extracted_content,
            msg=(
                'Expected ExtractUpstreamArchive to extract the tracked file '
                'contents from commit_revision into destination_directory.'),
        )

    def testExtractUpstreamArchivePreservesGitArchiveStderrForUnknownRevision(
            self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory = os.path.join(temporary_directory, 'upstream')
            destination_directory = os.path.join(
                temporary_directory, 'destination')
            os.makedirs(destination_directory, exist_ok=True)
            _InitializeGitRepositoryWithFiles(
                upstream_directory,
                {'src/google/protobuf/arena.h': '// arena\n'},
            )

            with self.assertRaisesRegex(
                    roll_protobuf.RollProtobufError,
                    r'git archive failed.*fatal:',
                    msg=(
                        'Expected ExtractUpstreamArchive to preserve git '
                        'archive stderr when commit_revision is invalid.')):
                roll_protobuf.ExtractUpstreamArchive(
                    repository_path=upstream_directory,
                    commit_revision='0000000000000000000000000000000000000000',
                    destination_directory=destination_directory,
                )

    def testExtractUpstreamArchiveRaisesWhenGitArchiveExitsNonZeroAfterStream(
            self):
        with (
            mock.patch.object(
                roll_protobuf,
                '_StreamGitArchiveToDirectory',
                return_value=(1, 'fatal: corrupt pack', None),
            ),
            self.assertRaisesRegex(
                roll_protobuf.RollProtobufError,
                r'git archive failed.*fatal: corrupt pack',
                msg=(
                    'Expected ExtractUpstreamArchive to raise '
                    'RollProtobufError with git stderr when git archive '
                    'exits non-zero after tarfile finishes reading.'),
            ),
        ):
            roll_protobuf.ExtractUpstreamArchive(
                repository_path='/tmp/upstream',
                commit_revision='deadbeef',
                destination_directory='/tmp/destination',
            )

    @_SkipOnWindows('os.symlink and SIGPIPE require POSIX.')
    def testExtractUpstreamArchiveReportsTarFilterErrorBeforeGitSigpipe(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory = os.path.join(temporary_directory, 'upstream')
            destination_directory = os.path.join(
                temporary_directory, 'destination')
            os.makedirs(upstream_directory, exist_ok=True)
            os.makedirs(destination_directory, exist_ok=True)
            _RunGitCommand(upstream_directory, ['init', '--quiet'])
            os.symlink(
                '/etc/passwd',
                os.path.join(upstream_directory, 'a_unsafe_symlink'),
            )
            _WriteFilesToDirectory(
                upstream_directory,
                {'z_padding.txt': 'x' * (4 * 1024 * 1024)},
            )
            _RunGitCommand(upstream_directory, ['add', '.'])
            _RunGitCommand(
                upstream_directory, ['commit', '--quiet', '-m', 'Add symlink'])
            commit_hash = _RunGitCommand(
                upstream_directory, ['rev-parse', 'HEAD'])

            with self.assertRaisesRegex(
                    roll_protobuf.RollProtobufError,
                    'absolute path',
                    msg=(
                        'Expected ExtractUpstreamArchive to report the '
                        'tarfile filter error rather than a subsequent '
                        'git archive SIGPIPE exit code.')):
                roll_protobuf.ExtractUpstreamArchive(
                    repository_path=upstream_directory,
                    commit_revision=commit_hash,
                    destination_directory=destination_directory,
                )

    @_SkipOnWindows('os.set_blocking on pipe file descriptors is POSIX-only.')
    def testExtractUpstreamArchiveDoesNotBlockGitOnVerboseStderr(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory = os.path.join(temporary_directory, 'upstream')
            destination_directory = os.path.join(
                temporary_directory, 'destination')
            os.makedirs(destination_directory, exist_ok=True)
            commit_hash = _InitializeGitRepositoryWithFiles(
                upstream_directory,
                {'src/google/protobuf/arena.h': '// arena\n'},
            )
            real_command_builder = roll_protobuf._GitCommandForRepository

            def BuildVerboseStderrGitCommand(
                    repository_path: str,
                    git_arguments: list[str]) -> list[str]:
                return [
                    sys.executable,
                    '-c',
                    _STDERR_FLOOD_THEN_ARCHIVE_SCRIPT,
                    *real_command_builder(repository_path, git_arguments),
                ]

            with mock.patch.object(
                    roll_protobuf,
                    '_GitCommandForRepository',
                    BuildVerboseStderrGitCommand):
                roll_protobuf.ExtractUpstreamArchive(
                    repository_path=upstream_directory,
                    commit_revision=commit_hash,
                    destination_directory=destination_directory,
                )
            extracted_arena_exists = os.path.exists(
                os.path.join(
                    destination_directory,
                    'src',
                    'google',
                    'protobuf',
                    'arena.h',
                ))

        self.assertTrue(
            extracted_arena_exists,
            msg=(
                'Expected ExtractUpstreamArchive to stream stderr to a '
                'temporary file so verbose git trace output exceeding the '
                'OS pipe buffer does not block extraction.'),
        )

    @_SkipOnWindows('POSIX sh SIGPIPE exit status 141 is Unix-only.')
    def testExtractUpstreamArchiveReportsOsErrorWhenGitWrapperExits141(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory = os.path.join(temporary_directory, 'upstream')
            destination_file_path = os.path.join(
                temporary_directory, 'regular_file')
            with open(destination_file_path, 'w', encoding='utf-8'):
                pass
            commit_hash = _InitializeGitRepositoryWithFiles(
                upstream_directory,
                {'z_padding.txt': 'x' * (8 * 1024 * 1024)},
            )
            real_command_builder = roll_protobuf._GitCommandForRepository

            def BuildShellWrappedGitCommand(
                    repository_path: str,
                    git_arguments: list[str]) -> list[str]:
                return [
                    'sh',
                    '-c',
                    '"$@"; exit $?',
                    'sh',
                    *real_command_builder(repository_path, git_arguments),
                ]

            with (
                mock.patch.object(
                    roll_protobuf,
                    '_GitCommandForRepository',
                    BuildShellWrappedGitCommand),
                self.assertRaisesRegex(
                    roll_protobuf.RollProtobufError,
                    'Not a directory',
                    msg=(
                        'Expected ExtractUpstreamArchive to report the '
                        'underlying OSError when extraction into a regular '
                        'file fails and a shell wrapper exits with 141.'),
                ),
            ):
                roll_protobuf.ExtractUpstreamArchive(
                    repository_path=upstream_directory,
                    commit_revision=commit_hash,
                    destination_directory=destination_file_path,
                )

    def testApplyPatchFileAcceptsZeroContextUnidiffPatch(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            target_directory = os.path.join(temporary_directory, 'target')
            patch_path = os.path.join(
                temporary_directory, '0072-remove-version-stamp.patch')
            _WriteFilesToDirectory(
                target_directory,
                {
                    'src/google/protobuf/port.cc': (
                        'line 1\n'
                        'bool kVersionStampBuildHasHardeningProtobuf = true;\n'
                        'line 3\n'),
                },
            )
            _WriteFilesToDirectory(
                temporary_directory,
                {
                    '0072-remove-version-stamp.patch': (
                        'diff --git a/src/google/protobuf/port.cc '
                        'b/src/google/protobuf/port.cc\n'
                        '--- a/src/google/protobuf/port.cc\n'
                        '+++ b/src/google/protobuf/port.cc\n'
                        '@@ -2,1 +2,0 @@\n'
                        '-bool kVersionStampBuildHasHardeningProtobuf = true;\n'
                    ),
                },
            )

            roll_protobuf.ApplyPatchFile(
                patch_path=patch_path,
                target_directory=target_directory,
            )
            port_source_path = os.path.join(
                target_directory, 'src', 'google', 'protobuf', 'port.cc')
            with open(port_source_path, 'r', encoding='utf-8') as port_file:
                updated_content = port_file.read()

        self.assertEqual(
            'line 1\nline 3\n',
            updated_content,
            msg=(
                'Expected zero-context unified diffs (like '
                '0072-remove-version-stamp.patch) to apply cleanly via '
                'git apply --unidiff-zero.'),
        )

    def testApplyPatchFileModifiesSubdirectoryInsideEnclosingGitRepository(
            self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            outer_repository = os.path.join(temporary_directory, 'chromium')
            protobuf_subdirectory = os.path.join(
                outer_repository, 'third_party', 'protobuf')
            _InitializeGitRepositoryWithFiles(
                outer_repository,
                {
                    'third_party/protobuf/src/google/protobuf/message.h': (
                        'int Value() { return 1; }\n'),
                },
            )
            patch_path = os.path.join(temporary_directory, '0001-update.patch')
            _WriteFilesToDirectory(
                temporary_directory,
                {
                    '0001-update.patch': (
                        'diff --git a/src/google/protobuf/message.h '
                        'b/src/google/protobuf/message.h\n'
                        '--- a/src/google/protobuf/message.h\n'
                        '+++ b/src/google/protobuf/message.h\n'
                        '@@ -1 +1 @@\n'
                        '-int Value() { return 1; }\n'
                        '+int Value() { return 2; }\n'
                    ),
                },
            )

            roll_protobuf.ApplyPatchFile(
                patch_path=patch_path,
                target_directory=protobuf_subdirectory,
            )
            message_header_path = os.path.join(
                protobuf_subdirectory, 'src', 'google', 'protobuf', 'message.h')
            with open(message_header_path, 'r', encoding='utf-8') as header:
                updated_content = header.read()

        self.assertEqual(
            'int Value() { return 2; }\n',
            updated_content,
            msg=(
                'Expected ApplyPatchFile to isolate git apply from the '
                'enclosing git repository via GIT_CEILING_DIRECTORIES and '
                'modify the file inside the target subdirectory.'),
        )

    def testApplyPatchFileReturnsGitDiagnosticWhenPatchDoesNotApply(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            target_directory = os.path.join(temporary_directory, 'target')
            patch_path = os.path.join(temporary_directory, '0001-stale.patch')
            _WriteFilesToDirectory(
                target_directory,
                {
                    'src/google/protobuf/message.h': (
                        'int Value() { return 3; }\n'
                    ),
                },
            )
            _WriteFilesToDirectory(
                temporary_directory,
                {
                    '0001-stale.patch': (
                        '--- a/src/google/protobuf/message.h\n'
                        '+++ b/src/google/protobuf/message.h\n'
                        '@@ -1 +1 @@\n'
                        '-int Value() { return 1; }\n'
                        '+int Value() { return 2; }\n'
                    ),
                },
            )

            patch_error = roll_protobuf.ApplyPatchFile(
                patch_path=patch_path,
                target_directory=target_directory,
            )

        self.assertIn(
            'patch does not apply',
            '' if patch_error is None else patch_error,
            msg=(
                'Expected ApplyPatchFile to return git apply diagnostic '
                'output when the patch context does not match the target.'),
        )

    def testCheckTreePassesWhenUpstreamPlusPatchesMatchesChromiumTree(self):
        patch_content = (
            'diff --git a/src/google/protobuf/message.h '
            'b/src/google/protobuf/message.h\n'
            '--- a/src/google/protobuf/message.h\n'
            '+++ b/src/google/protobuf/message.h\n'
            '@@ -1 +1 @@\n'
            '-int Value() { return 1; }\n'
            '+int Value() { return 2; }\n'
        )
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory, chromium_directory, commit_hash = (
                _CreateUpstreamAndChromiumRepositories(
                    temporary_directory,
                    upstream_files={
                        'src/google/protobuf/message.h': (
                            'int Value() { return 1; }\n'),
                        'src/google/protobuf/port.cc': (
                            'int Port() { return 0; }\n'),
                    },
                    chromium_files={
                        'patches/0001-update-value.patch': patch_content,
                        'src/google/protobuf/message.h': (
                            'int Value() { return 2; }\n'),
                        'src/google/protobuf/port.cc': (
                            'int Port() { return 0; }\n'),
                    },
                ))

            verification_result = roll_protobuf.CheckTreeMatchesPatchedUpstream(
                protobuf_directory=chromium_directory,
                upstream_repository_path=upstream_directory,
            )

        self.assertEqual(
            roll_protobuf.TreeVerificationResult(
                upstream_revision=commit_hash,
                applied_patches=('0001-update-value.patch',),
            ),
            verification_result,
            msg=(
                'Expected CheckTreeMatchesPatchedUpstream to return a clean '
                'result when upstream + patches reproduces the Chromium tree.'),
        )

    def testCheckTreeDetectsSourceModificationWhenPatchFileIsMissing(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            # Simulate https://crrev.com/c/8422202: message.h was updated in the
            # Chromium tree, but the corresponding .patch file is missing.
            upstream_directory, chromium_directory, _ = (
                _CreateUpstreamAndChromiumRepositories(
                    temporary_directory,
                    upstream_files={
                        'src/google/protobuf/message.h': (
                            'const EnumValueDescriptor* GetEnum() const;\n'),
                    },
                    chromium_files={
                        'src/google/protobuf/message.h': (
                            'ABSL_DEPRECATED("Use field") '
                            'const EnumValueDescriptor* GetEnum() const;\n'),
                    },
                ))

            verification_result = roll_protobuf.CheckTreeMatchesPatchedUpstream(
                protobuf_directory=chromium_directory,
                upstream_repository_path=upstream_directory,
            )

        self.assertEqual(
            ('src/google/protobuf/message.h',),
            verification_result.modified_files,
            msg=(
                'Expected a source modification in '
                'src/google/protobuf/message.h without a matching patch in '
                'patches/ to be flagged.'),
        )

    def testCheckTreeDetectsUnpatchedDirectEditAlongsideAppliedPatch(self):
        patch_content = (
            'diff --git a/src/google/protobuf/message.h '
            'b/src/google/protobuf/message.h\n'
            '--- a/src/google/protobuf/message.h\n'
            '+++ b/src/google/protobuf/message.h\n'
            '@@ -1 +1 @@\n'
            '-int Value() { return 1; }\n'
            '+int Value() { return 2; }\n'
        )
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory, chromium_directory, _ = (
                _CreateUpstreamAndChromiumRepositories(
                    temporary_directory,
                    upstream_files={
                        'src/google/protobuf/message.h': (
                            'int Value() { return 1; }\n'),
                        'src/google/protobuf/port.cc': (
                            'int Port() { return 0; }\n'),
                    },
                    chromium_files={
                        'patches/0001-update-value.patch': patch_content,
                        'src/google/protobuf/message.h': (
                            'int Value() { return 2; }\n'),
                        'src/google/protobuf/port.cc': (
                            'int Port() { return 99; }\n'),
                    },
                ))

            verification_result = roll_protobuf.CheckTreeMatchesPatchedUpstream(
                protobuf_directory=chromium_directory,
                upstream_repository_path=upstream_directory,
            )

        self.assertEqual(
            ('src/google/protobuf/port.cc',),
            verification_result.modified_files,
            msg=(
                'Expected an unpatched edit in src/google/protobuf/port.cc to '
                'be flagged while patched message.h is accepted.'),
        )

    def testCheckTreeDetectsUnexpectedExtraChromiumFile(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory, chromium_directory, _ = (
                _CreateUpstreamAndChromiumRepositories(
                    temporary_directory,
                    upstream_files={
                        'src/google/protobuf/arena.h': '// arena\n',
                    },
                    chromium_files={
                        'src/google/protobuf/arena.h': '// arena\n',
                        'src/google/protobuf/stray_local_file.cc': '// stray\n',
                    },
                ))

            verification_result = roll_protobuf.CheckTreeMatchesPatchedUpstream(
                protobuf_directory=chromium_directory,
                upstream_repository_path=upstream_directory,
            )

        self.assertEqual(
            ('src/google/protobuf/stray_local_file.cc',),
            verification_result.chromium_only_files,
            msg=(
                'Expected a non-exception file present only in Chromium to be '
                'reported in chromium_only_files.'),
        )

    def testCheckTreeDetectsMissingUpstreamFileNotInExceptionList(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory, chromium_directory, _ = (
                _CreateUpstreamAndChromiumRepositories(
                    temporary_directory,
                    upstream_files={
                        'src/google/protobuf/arena.h': '// arena\n',
                        'src/google/protobuf/extension_set.h': '// ext\n',
                    },
                    chromium_files={
                        'src/google/protobuf/arena.h': '// arena\n',
                    },
                ))

            verification_result = roll_protobuf.CheckTreeMatchesPatchedUpstream(
                protobuf_directory=chromium_directory,
                upstream_repository_path=upstream_directory,
            )

        self.assertEqual(
            ('src/google/protobuf/extension_set.h',),
            verification_result.upstream_only_files,
            msg=(
                'Expected an upstream file missing from //third_party/protobuf '
                'and not in the exception list to be reported in '
                'upstream_only_files.'),
        )

    def testCheckTreeReportsFailingPatchWhenPatchDoesNotApplyToUpstream(self):
        conflicting_patch = (
            'diff --git a/src/google/protobuf/message.h '
            'b/src/google/protobuf/message.h\n'
            '--- a/src/google/protobuf/message.h\n'
            '+++ b/src/google/protobuf/message.h\n'
            '@@ -1 +1 @@\n'
            '-int NonExistentUpstreamFunction();\n'
            '+int ReplacementFunction();\n'
        )
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory, chromium_directory, _ = (
                _CreateUpstreamAndChromiumRepositories(
                    temporary_directory,
                    upstream_files={
                        'src/google/protobuf/message.h': (
                            'int Value() { return 1; }\n'),
                    },
                    chromium_files={
                        'patches/0015-broken-context.patch': conflicting_patch,
                        'src/google/protobuf/message.h': (
                            'int Value() { return 1; }\n'),
                    },
                ))

            verification_result = roll_protobuf.CheckTreeMatchesPatchedUpstream(
                protobuf_directory=chromium_directory,
                upstream_repository_path=upstream_directory,
            )

        self.assertEqual(
            '0015-broken-context.patch',
            verification_result.failing_patch,
            msg=(
                'Expected CheckTreeMatchesPatchedUpstream to record the exact '
                'filename of the patch that failed to apply.'),
        )

    def testCheckTreeIgnoresChromiumOwnedAndBootstrapGeneratedFiles(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory, chromium_directory, _ = (
                _CreateUpstreamAndChromiumRepositories(
                    temporary_directory,
                    upstream_files={
                        'src/google/protobuf/arena.h': '// arena\n',
                        'src/google/protobuf/descriptor.pb.h': (
                            '// upstream unpatched descriptor.pb.h\n'),
                        'src/google/protobuf/descriptor.pb.cc': (
                            '// upstream unpatched descriptor.pb.cc\n'),
                        'src/google/protobuf/compiler/plugin.pb.h': (
                            '// upstream unpatched plugin.pb.h\n'),
                        'src/google/protobuf/compiler/plugin.pb.cc': (
                            '// upstream unpatched plugin.pb.cc\n'),
                    },
                    chromium_files={
                        'BUILD.gn': '# Chromium GN build\n',
                        'DEPS': 'deps = {}\n',
                        'DIR_METADATA': 'monorail {}\n',
                        'OWNERS': 'file://OWNERS\n',
                        'PRESUBMIT.py': 'PRESUBMIT_VERSION = "2.0.0"\n',
                        'PRESUBMIT_test.py': '# test\n',
                        'check_file_lists.py': '# check_file_lists\n',
                        'check_file_lists_test.py': '# test\n',
                        'gen_extra_chromium_files.py': '# gen\n',
                        'proto_library.gni': '# gni\n',
                        'proto_sources.gni': '# gni\n',
                        'proto_wkt.gni': '# gni\n',
                        'roll_protobuf.py': '# roll\n',
                        'roll_protobuf_test.py': '# test\n',
                        'patches/0044-trim-protoc-main.md': '# note\n',
                        'src/google/protobuf/arena.h': '// arena\n',
                        'src/google/protobuf/descriptor.pb.h': (
                            '// Chromium regenerated descriptor.pb.h\n'),
                        'src/google/protobuf/descriptor.pb.cc': (
                            '// Chromium regenerated descriptor.pb.cc\n'),
                        'src/google/protobuf/compiler/plugin.pb.h': (
                            '// Chromium regenerated plugin.pb.h\n'),
                        'src/google/protobuf/compiler/plugin.pb.cc': (
                            '// Chromium regenerated plugin.pb.cc\n'),
                        'python/google/protobuf/descriptor_pb2.py': (
                            '# generated descriptor_pb2\n'),
                        'python/google/protobuf/compiler/plugin_pb2.py': (
                            '# generated plugin_pb2\n'),
                        'python/google/protobuf/internal/'
                        'python_edition_defaults.py': (
                            '# generated edition defaults\n'),
                    },
                ))

            verification_result = roll_protobuf.CheckTreeMatchesPatchedUpstream(
                protobuf_directory=chromium_directory,
                upstream_repository_path=upstream_directory,
            )

        self.assertTrue(
            verification_result.IsClean(),
            msg=(
                'Expected Chromium-owned files and bootstrap files regenerated '
                'by gen_extra_chromium_files.py to be ignored during check.'),
        )

    def testCheckTreeIgnoresCompatibilityAndWellKnownTypeGeneratedFiles(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory, chromium_directory, _ = (
                _CreateUpstreamAndChromiumRepositories(
                    temporary_directory,
                    upstream_files={
                        'src/google/protobuf/arena.h': '// arena\n',
                        'compatibility/smoke/large_artifact.bin': 'payload\n',
                        'src/google/protobuf/any.pb.h': '// any.pb.h\n',
                        'src/google/protobuf/any.pb.cc': '// any.pb.cc\n',
                        'src/google/protobuf/timestamp.pb.h': '// ts.pb.h\n',
                        'src/google/protobuf/timestamp.pb.cc': '// ts.pb.cc\n',
                    },
                    chromium_files={
                        'src/google/protobuf/arena.h': '// arena\n',
                    },
                ))

            verification_result = roll_protobuf.CheckTreeMatchesPatchedUpstream(
                protobuf_directory=chromium_directory,
                upstream_repository_path=upstream_directory,
            )

        self.assertTrue(
            verification_result.IsClean(),
            msg=(
                'Expected compatibility/ and pre-generated Well-Known Type '
                '.pb.{h,cc} files deleted during rolls to be ignored.'),
        )

    def testCheckTreeIgnoresUpstreamFilesMatchedByGitignore(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory, chromium_directory, _ = (
                _CreateUpstreamAndChromiumRepositories(
                    temporary_directory,
                    upstream_files={
                        '.gitignore': '*.sln\n*.targets\n*.xcodeproj/\n',
                        'src/google/protobuf/arena.h': '// arena\n',
                        'csharp/src/Google.Protobuf.sln': 'Visual Studio\n',
                        'csharp/Google.Protobuf.Tools.targets': '<Project/>\n',
                        'objectivec/ProtocolBuffers_OSX.xcodeproj/'
                        'project.pbxproj': '// !$*UTF8*$!\n',
                    },
                    chromium_files={
                        '.gitignore': '*.sln\n*.targets\n*.xcodeproj/\n',
                        'src/google/protobuf/arena.h': '// arena\n',
                    },
                ))

            verification_result = roll_protobuf.CheckTreeMatchesPatchedUpstream(
                protobuf_directory=chromium_directory,
                upstream_repository_path=upstream_directory,
            )

        self.assertTrue(
            verification_result.IsClean(),
            msg=(
                'Expected upstream files excluded by .gitignore during '
                'git add to be filtered out via git check-ignore.'),
        )

    def testCheckTreeRaisesErrorWhenProtobufDirectoryIsOutsideGitWorkTree(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory = os.path.join(temporary_directory, 'upstream')
            commit_hash = _InitializeGitRepositoryWithFiles(
                upstream_directory,
                {'src/google/protobuf/arena.h': '// arena\n'},
            )
            non_git_directory = os.path.join(temporary_directory, 'plain_dir')
            _WriteFilesToDirectory(
                non_git_directory,
                {
                    'README.chromium': f'Revision: {commit_hash}\n',
                    'src/google/protobuf/arena.h': '// arena\n',
                },
            )

            with mock.patch.dict(
                    os.environ,
                    {'GIT_CEILING_DIRECTORIES': temporary_directory}):
                with self.assertRaises(
                        roll_protobuf.RollProtobufError,
                        msg=(
                            'Expected CheckTreeMatchesPatchedUpstream to '
                            'raise RollProtobufError when protobuf_directory '
                            'is not inside a git work tree.')):
                    roll_protobuf.CheckTreeMatchesPatchedUpstream(
                        protobuf_directory=non_git_directory,
                        upstream_repository_path=upstream_directory,
                    )

    def testFormatTreeVerificationReportRendersChromiumAndUpstreamOnlyFiles(
            self):
        verification_result = roll_protobuf.TreeVerificationResult(
            upstream_revision='abc1234',
            applied_patches=('0001-first.patch',),
            chromium_only_files=('src/google/protobuf/stray.cc',),
            upstream_only_files=('src/google/protobuf/missing.cc',),
        )

        report_text = roll_protobuf.FormatTreeVerificationReport(
            verification_result)

        self.assertEqual(
            (
                '//third_party/protobuf does not match upstream abc1234 plus '
                'patches/*.patch:\n\n'
                'Chromium-only files not in known exceptions (1):\n'
                '  + src/google/protobuf/stray.cc\n\n'
                'Upstream files missing from //third_party/protobuf (1):\n'
                '  - src/google/protobuf/missing.cc'
            ),
            report_text,
            msg=(
                'Expected FormatTreeVerificationReport to list both '
                'chromium_only_files (+) and upstream_only_files (-).'),
        )

    def testMainCheckExitsZeroWhenTreeIsClean(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory, chromium_directory, _ = (
                _CreateUpstreamAndChromiumRepositories(
                    temporary_directory,
                    upstream_files={
                        'src/google/protobuf/arena.h': '// arena\n',
                    },
                    chromium_files={
                        'src/google/protobuf/arena.h': '// arena\n',
                    },
                ))
            with mock.patch('sys.stdout', io.StringIO()):
                exit_code = roll_protobuf.main([
                    'check',
                    '--protobuf-dir',
                    chromium_directory,
                    '--upstream-repo',
                    upstream_directory,
                ])

        self.assertEqual(
            roll_protobuf.EXIT_CODE_SUCCESS,
            exit_code,
            msg='Expected main(["check", ...]) to exit 0 when tree is clean.',
        )

    def testMainCheckPrintsSummaryWhenTreeIsClean(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory, chromium_directory, commit_hash = (
                _CreateUpstreamAndChromiumRepositories(
                    temporary_directory,
                    upstream_files={
                        'src/google/protobuf/arena.h': '// arena\n',
                    },
                    chromium_files={
                        'src/google/protobuf/arena.h': '// arena\n',
                    },
                ))
            captured_stdout = io.StringIO()
            with mock.patch('sys.stdout', captured_stdout):
                roll_protobuf.main([
                    'check',
                    '--protobuf-dir',
                    chromium_directory,
                    '--upstream-repo',
                    upstream_directory,
                ])

        self.assertEqual(
            (
                f'Verified //third_party/protobuf matches upstream '
                f'{commit_hash} plus 0 patch(es).'
            ),
            captured_stdout.getvalue().strip(),
            msg=(
                'Expected main(["check", ...]) to print a single '
                'verification summary line when the tree matches.'),
        )

    def testMainCheckExitsOneWhenPatchIsMissing(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory, chromium_directory, _ = (
                _CreateUpstreamAndChromiumRepositories(
                    temporary_directory,
                    upstream_files={
                        'src/google/protobuf/message.h': (
                            'int Value() { return 1; }\n'),
                    },
                    chromium_files={
                        'src/google/protobuf/message.h': (
                            'int Value() { return 2; }\n'),
                    },
                ))
            with mock.patch('sys.stdout', io.StringIO()):
                exit_code = roll_protobuf.main([
                    'check',
                    '--protobuf-dir',
                    chromium_directory,
                    '--upstream-repo',
                    upstream_directory,
                ])

        self.assertEqual(
            roll_protobuf.EXIT_CODE_ERROR,
            exit_code,
            msg=(
                'Expected main(["check", ...]) to exit 1 when a tracked file '
                'differs without a corresponding patch.'),
        )

    def testMainCheckListsModifiedFileWhenPatchIsMissing(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory, chromium_directory, commit_hash = (
                _CreateUpstreamAndChromiumRepositories(
                    temporary_directory,
                    upstream_files={
                        'src/google/protobuf/message.h': (
                            'int Value() { return 1; }\n'),
                    },
                    chromium_files={
                        'src/google/protobuf/message.h': (
                            'int Value() { return 2; }\n'),
                    },
                ))
            captured_stdout = io.StringIO()
            with mock.patch('sys.stdout', captured_stdout):
                roll_protobuf.main([
                    'check',
                    '--protobuf-dir',
                    chromium_directory,
                    '--upstream-repo',
                    upstream_directory,
                ])

        self.assertEqual(
            (
                f'//third_party/protobuf does not match upstream '
                f'{commit_hash} plus patches/*.patch:\n\n'
                'Modified files not explained by patches/ (1):\n'
                '  M src/google/protobuf/message.h'
            ),
            captured_stdout.getvalue().strip(),
            msg=(
                'Expected main(["check", ...]) to list '
                'M src/google/protobuf/message.h when its patch is missing.'),
        )

    def testMainCheckNamesFailingPatchWhenPatchApplicationFails(self):
        conflicting_patch = (
            'diff --git a/src/google/protobuf/message.h '
            'b/src/google/protobuf/message.h\n'
            '--- a/src/google/protobuf/message.h\n'
            '+++ b/src/google/protobuf/message.h\n'
            '@@ -1 +1 @@\n'
            '-int NonExistentUpstreamFunction();\n'
            '+int ReplacementFunction();\n'
        )
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory, chromium_directory, commit_hash = (
                _CreateUpstreamAndChromiumRepositories(
                    temporary_directory,
                    upstream_files={
                        'src/google/protobuf/message.h': (
                            'int Value() { return 1; }\n'),
                    },
                    chromium_files={
                        'patches/0099-broken.patch': conflicting_patch,
                        'src/google/protobuf/message.h': (
                            'int Value() { return 1; }\n'),
                    },
                ))
            captured_stdout = io.StringIO()
            with mock.patch('sys.stdout', captured_stdout):
                roll_protobuf.main([
                    'check',
                    '--protobuf-dir',
                    chromium_directory,
                    '--upstream-repo',
                    upstream_directory,
                ])
            first_report_line = captured_stdout.getvalue().splitlines()[0]

        self.assertEqual(
            (
                f'Patch failed to apply to upstream {commit_hash}: '
                '0099-broken.patch'
            ),
            first_report_line,
            msg=(
                'Expected main(["check", ...]) to name 0099-broken.patch '
                'on the first report line when patch application fails.'),
        )

    def testMainCheckOutputsMachineReadableJsonWhenFlagIsSet(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory, chromium_directory, commit_hash = (
                _CreateUpstreamAndChromiumRepositories(
                    temporary_directory,
                    upstream_files={
                        'src/google/protobuf/arena.h': '// arena\n',
                    },
                    chromium_files={
                        'src/google/protobuf/arena.h': '// arena suffix\n',
                    },
                ))
            captured_stdout = io.StringIO()
            with mock.patch('sys.stdout', captured_stdout):
                roll_protobuf.main([
                    'check',
                    '--protobuf-dir',
                    chromium_directory,
                    '--upstream-repo',
                    upstream_directory,
                    '--json',
                ])
            parsed_json = json.loads(captured_stdout.getvalue())

        self.assertEqual(
            {
                'is_clean': False,
                'upstream_revision': commit_hash,
                'applied_patches': [],
                'failing_patch': None,
                'failing_patch_error': None,
                'modified_files': ['src/google/protobuf/arena.h'],
                'chromium_only_files': [],
                'upstream_only_files': [],
            },
            parsed_json,
            msg=(
                'Expected --json to emit the complete TreeVerificationResult '
                'dictionary when differences are present.'),
        )

    def testMainCheckIncludesJsonErrorMessageWhenRevisionIsMissingFromUpstream(
            self):
        missing_revision = '0' * 40
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory, chromium_directory, _ = (
                _CreateUpstreamAndChromiumRepositories(
                    temporary_directory,
                    upstream_files={
                        'src/google/protobuf/arena.h': '// arena\n',
                    },
                    chromium_files={
                        'README.chromium': f'Revision: {missing_revision}\n',
                        'src/google/protobuf/arena.h': '// arena\n',
                    },
                ))
            captured_stdout = io.StringIO()
            with mock.patch('sys.stdout', captured_stdout):
                roll_protobuf.main([
                    'check',
                    '--protobuf-dir',
                    chromium_directory,
                    '--upstream-repo',
                    upstream_directory,
                    '--json',
                ])
            parsed_json = json.loads(captured_stdout.getvalue())

        self.assertEqual(
            {
                'is_clean': False,
                'error': (
                    f'Revision {missing_revision} not found in '
                    f'upstream repository {upstream_directory}.'
                ),
            },
            parsed_json,
            msg=(
                'Expected main(["check", "--json"]) to include the exact '
                'error message when Revision is absent from upstream.'),
        )

    def testMainCheckOutputsJsonErrorWhenReadmeCannotBeRead(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            expected_readme_path = os.path.join(
                os.path.abspath(temporary_directory), 'README.chromium')
            captured_stdout = io.StringIO()
            with mock.patch('sys.stdout', captured_stdout):
                roll_protobuf.main([
                    'check',
                    '--protobuf-dir',
                    temporary_directory,
                    '--upstream-repo',
                    temporary_directory,
                    '--json',
                ])
            parsed_json = json.loads(captured_stdout.getvalue())

        self.assertEqual(
            {
                'is_clean': False,
                'error': (
                    f'Missing README.chromium at {expected_readme_path}.'),
            },
            parsed_json,
            msg=(
                'Expected main(["check", "--json"]) to catch missing '
                'README.chromium and emit a JSON error payload.'),
        )

    def testMainCheckExitsOneWhenReadmeIsMissing(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            with mock.patch('sys.stderr', io.StringIO()):
                exit_code = roll_protobuf.main([
                    'check',
                    '--protobuf-dir',
                    temporary_directory,
                    '--upstream-repo',
                    temporary_directory,
                ])

        self.assertEqual(
            roll_protobuf.EXIT_CODE_ERROR,
            exit_code,
            msg=(
                'Expected main(["check"]) to exit 1 when README.chromium '
                'is missing.'),
        )

    def testMainCheckPrintsHumanErrorToStderrWhenReadmeIsMissing(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            expected_readme_path = os.path.join(
                os.path.abspath(temporary_directory), 'README.chromium')
            captured_stderr = io.StringIO()
            with mock.patch('sys.stderr', captured_stderr):
                roll_protobuf.main([
                    'check',
                    '--protobuf-dir',
                    temporary_directory,
                    '--upstream-repo',
                    temporary_directory,
                ])

        self.assertEqual(
            f'Error: Missing README.chromium at {expected_readme_path}.',
            captured_stderr.getvalue().strip(),
            msg=(
                'Expected main(["check"]) without --json to print Error: to '
                'stderr when README.chromium is missing.'),
        )

    def testMainCheckClonesCacheUnderBuildDirectoryWhenDashCFlagIsSet(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_directory = os.path.join(temporary_directory, 'out_release')
            upstream_directory, chromium_directory, _ = (
                _CreateUpstreamAndChromiumRepositories(
                    temporary_directory,
                    upstream_files={
                        'src/google/protobuf/arena.h': '// arena\n',
                    },
                    chromium_files={
                        'src/google/protobuf/arena.h': '// arena\n',
                    },
                ))
            expected_cache_directory = os.path.join(
                build_directory, '.protobuf_roll', 'upstream-protobuf')
            with mock.patch('sys.stdout', io.StringIO()):
                roll_protobuf.main([
                    'check',
                    '-C',
                    build_directory,
                    '--protobuf-dir',
                    chromium_directory,
                    '--upstream-url',
                    upstream_directory,
                ])
            has_bare_head_file = os.path.isfile(
                os.path.join(expected_cache_directory, 'HEAD'))

        self.assertTrue(
            has_bare_head_file,
            msg=(
                'Expected main(["check", "-C", ...]) to clone a bare git '
                'cache containing HEAD into '
                '<build_dir>/.protobuf_roll/upstream-protobuf.'),
        )

    def testCheckTreeFlagsUntrackedNonIgnoredFileInChromiumWorkingTree(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory, chromium_directory, _ = (
                _CreateUpstreamAndChromiumRepositories(
                    temporary_directory,
                    upstream_files={
                        'src/google/protobuf/arena.h': '// arena\n',
                    },
                    chromium_files={
                        'src/google/protobuf/arena.h': '// arena\n',
                    },
                ))
            _WriteFilesToDirectory(
                chromium_directory,
                {
                    'src/google/protobuf/untracked_scratch.h': '// scratch\n',
                },
            )

            verification_result = roll_protobuf.CheckTreeMatchesPatchedUpstream(
                protobuf_directory=chromium_directory,
                upstream_repository_path=upstream_directory,
            )

        self.assertEqual(
            ('src/google/protobuf/untracked_scratch.h',),
            verification_result.chromium_only_files,
            msg=(
                'Expected CheckTreeMatchesPatchedUpstream to flag untracked '
                'non-ignored files in the Chromium working tree as '
                'chromium_only_files.'),
        )

    def testCheckTreeDoesNotMaskMissingUpstreamFileMatchedByUserExcludesFile(
            self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory, chromium_directory, _ = (
                _CreateUpstreamAndChromiumRepositories(
                    temporary_directory,
                    upstream_files={
                        'src/google/protobuf/arena.h': '// arena\n',
                        'src/google/protobuf/user_ignored.h': '// missing\n',
                    },
                    chromium_files={
                        'src/google/protobuf/arena.h': '// arena\n',
                    },
                ))
            user_excludes_file = os.path.join(
                temporary_directory, 'global_gitignore')
            with open(user_excludes_file, 'w', encoding='utf-8') as handle:
                handle.write('*user_ignored*\n')

            with mock.patch.dict(
                    os.environ,
                    {
                        'GIT_CONFIG_COUNT': '1',
                        'GIT_CONFIG_KEY_0': 'core.excludesFile',
                        'GIT_CONFIG_VALUE_0': user_excludes_file,
                    }):
                verification_result = (
                    roll_protobuf.CheckTreeMatchesPatchedUpstream(
                        protobuf_directory=chromium_directory,
                        upstream_repository_path=upstream_directory,
                    ))

        self.assertEqual(
            ('src/google/protobuf/user_ignored.h',),
            verification_result.upstream_only_files,
            msg=(
                'Expected CheckTreeMatchesPatchedUpstream to ignore a user '
                'core.excludesFile so missing upstream files are still '
                'detected.'),
        )

    def testCheckTreeDoesNotSkipUntrackedChromiumFileMatchedByUserExcludesFile(
            self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory, chromium_directory, _ = (
                _CreateUpstreamAndChromiumRepositories(
                    temporary_directory,
                    upstream_files={
                        'src/google/protobuf/arena.h': '// arena\n',
                    },
                    chromium_files={
                        'src/google/protobuf/arena.h': '// arena\n',
                    },
                ))
            _WriteFilesToDirectory(
                chromium_directory,
                {
                    'src/google/protobuf/user_excluded.h': '// untracked\n',
                },
            )
            user_excludes_file = os.path.join(
                temporary_directory, 'global_gitignore')
            with open(user_excludes_file, 'w', encoding='utf-8') as handle:
                handle.write('*user_excluded*\n')

            with mock.patch.dict(
                    os.environ,
                    {
                        'GIT_CONFIG_COUNT': '1',
                        'GIT_CONFIG_KEY_0': 'core.excludesFile',
                        'GIT_CONFIG_VALUE_0': user_excludes_file,
                    }):
                verification_result = (
                    roll_protobuf.CheckTreeMatchesPatchedUpstream(
                        protobuf_directory=chromium_directory,
                        upstream_repository_path=upstream_directory,
                    ))

        self.assertEqual(
            ('src/google/protobuf/user_excluded.h',),
            verification_result.chromium_only_files,
            msg=(
                'Expected _ListChromiumProtobufFiles to neutralize '
                'core.excludesFile so untracked local files matched by '
                'a user global gitignore are still flagged.'),
        )

    def testCheckTreeDetectsDeletedTrackedFileMatchingGitignore(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory, chromium_directory, _ = (
                _CreateUpstreamAndChromiumRepositories(
                    temporary_directory,
                    upstream_files={
                        '.gitignore': '*.la\n',
                        'src/google/protobuf/arena.h': '// arena\n',
                        'src/solaris/libstdc++.la': '# libtool archive\n',
                    },
                    chromium_files={
                        '.gitignore': '*.la\n',
                        'src/google/protobuf/arena.h': '// arena\n',
                        'src/solaris/libstdc++.la': '# libtool archive\n',
                    },
                ))
            os.remove(
                os.path.join(chromium_directory, 'src', 'solaris',
                             'libstdc++.la'))

            verification_result = roll_protobuf.CheckTreeMatchesPatchedUpstream(
                protobuf_directory=chromium_directory,
                upstream_repository_path=upstream_directory,
            )

        self.assertEqual(
            ('src/solaris/libstdc++.la',),
            verification_result.upstream_only_files,
            msg=(
                'Expected CheckTreeMatchesPatchedUpstream to report a '
                'deleted tracked upstream file even when its filename '
                'matches a pattern in .gitignore.'),
        )

    def testCheckTreeRaisesErrorWhenProtobufDirectoryIsGitIgnored(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            upstream_directory, chromium_directory, commit_hash = (
                _CreateUpstreamAndChromiumRepositories(
                    temporary_directory,
                    upstream_files={
                        'src/google/protobuf/arena.h': '// arena\n',
                    },
                    chromium_files={
                        '.gitignore': 'ignored_subdir/\n',
                        'src/google/protobuf/arena.h': '// arena\n',
                    },
                ))
            ignored_protobuf_directory = os.path.join(
                chromium_directory, 'ignored_subdir')
            _WriteFilesToDirectory(
                ignored_protobuf_directory,
                {
                    'README.chromium': f'Revision: {commit_hash}\n',
                    'src/google/protobuf/arena.h': '// arena\n',
                },
            )

            with self.assertRaisesRegex(
                    roll_protobuf.RollProtobufError,
                    r'git ls-files returned no README\.chromium',
                    msg=(
                        'Expected CheckTreeMatchesPatchedUpstream to raise '
                        'RollProtobufError when protobuf_directory is '
                        'git-ignored instead of silently passing.')):
                roll_protobuf.CheckTreeMatchesPatchedUpstream(
                    protobuf_directory=ignored_protobuf_directory,
                    upstream_repository_path=upstream_directory,
                )

    @_SkipOnWindows('os.symlink requires elevated privileges on Windows.')
    def testListRegularFilesInDirectorySkipsGitPycachePycAndSymlinks(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            _WriteFilesToDirectory(
                temporary_directory,
                {
                    '.git/HEAD': 'ref: refs/heads/main\n',
                    '__pycache__/cached.cpython-311.pyc': 'bytecode\n',
                    'src/stale.pyc': 'bytecode\n',
                    'src/google/protobuf/arena.h': '// arena\n',
                },
            )
            os.symlink(
                os.path.join(
                    temporary_directory, 'src', 'google', 'protobuf',
                    'arena.h'),
                os.path.join(temporary_directory, 'src', 'symlink_arena.h'),
            )

            listed_files = roll_protobuf._ListRegularFilesInDirectory(
                temporary_directory)

        self.assertEqual(
            {'src/google/protobuf/arena.h'},
            listed_files,
            msg=(
                'Expected _ListRegularFilesInDirectory to exclude .git/, '
                '__pycache__/, .pyc files, and symlinks.'),
        )

    def testListTopLevelPatchPathsReturnsPatchesInReplayOrder(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            _WriteFilesToDirectory(
                temporary_directory,
                {
                    'patches/0010-second.patch': '// second\n',
                    'patches/0009-first.patch': '// first\n',
                    'patches/0009-first.md': '# note\n',
                    'patches/protobuf_v25/0001-upstream.patch': '// v25\n',
                },
            )

            patch_paths = roll_protobuf.ListTopLevelPatchPaths(
                temporary_directory)

        self.assertEqual(
            [
                os.path.join(
                    temporary_directory, 'patches', '0009-first.patch'),
                os.path.join(
                    temporary_directory, 'patches', '0010-second.patch'),
            ],
            patch_paths,
            msg=(
                'Expected ListTopLevelPatchPaths to return top-level .patch '
                'files in lexicographic replay order while skipping .md '
                'notes and upstream subdirectories.'),
        )

    def testListTopLevelPatchPathsReturnsEmptyListWhenPatchesDirectoryIsMissing(
            self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            patch_paths = roll_protobuf.ListTopLevelPatchPaths(
                temporary_directory)

        self.assertEqual(
            [],
            patch_paths,
            msg=(
                'Expected ListTopLevelPatchPaths to return an empty list '
                'when the patches/ directory does not exist.'),
        )

    def testIsChromiumOwnedFileClassifiesTrackedChromiumAndUpstreamPaths(self):
        classification_by_path = {
            path: roll_protobuf.IsChromiumOwnedFile(path)
            for path in (
                'BUILD.gn',
                'roll_protobuf.py',
                'patches/0004-fix-shared-library-exports.patch',
                'patches/protobuf_v25/0001-bazel.patch',
                'src/google/protobuf/arena.h',
            )
        }

        self.assertEqual(
            {
                'BUILD.gn': True,
                'roll_protobuf.py': True,
                'patches/0004-fix-shared-library-exports.patch': True,
                'patches/protobuf_v25/0001-bazel.patch': False,
                'src/google/protobuf/arena.h': False,
            },
            classification_by_path,
            msg=(
                'Expected IsChromiumOwnedFile to recognize '
                'CHROMIUM_OWNED_FILES and top-level patches while excluding '
                'upstream files.'),
        )

    def testIsIntentionallyDeletedUpstreamFileClassifiesRemovedPaths(self):
        classification_by_path = {
            path: roll_protobuf.IsIntentionallyDeletedUpstreamFile(path)
            for path in (
                'compatibility/smoke/Test.java',
                'src/google/protobuf/any.pb.cc',
                'src/google/protobuf/descriptor.pb.cc',
                'src/google/protobuf/arena.h',
            )
        }

        self.assertEqual(
            {
                'compatibility/smoke/Test.java': True,
                'src/google/protobuf/any.pb.cc': True,
                'src/google/protobuf/descriptor.pb.cc': False,
                'src/google/protobuf/arena.h': False,
            },
            classification_by_path,
            msg=(
                'Expected IsIntentionallyDeletedUpstreamFile to match '
                'removed directories and non-bootstrap .pb.h/.pb.cc files '
                'while keeping bootstrap-generated descriptor.pb.cc.'),
        )

    def testReadReadmeRevisionFromDirectoryReadsRevisionFromReadmeFile(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            _WriteFilesToDirectory(
                temporary_directory,
                {
                    'README.chromium': (
                        'Name: Protocol Buffers\n'
                        'Revision: 3f17acbf7ffbda81db8be17505554c726e558bb2\n'
                    ),
                },
            )

            revision = roll_protobuf.ReadReadmeRevisionFromDirectory(
                temporary_directory)

        self.assertEqual(
            '3f17acbf7ffbda81db8be17505554c726e558bb2',
            revision,
            msg=(
                'Expected ReadReadmeRevisionFromDirectory to read '
                'README.chromium from the given directory and return the '
                'Revision: commit hash.'),
        )

    def testReadReadmeRevisionFromDirectoryRaisesErrorWhenReadmeFileIsMissing(
            self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            with self.assertRaises(
                    roll_protobuf.RollProtobufError,
                    msg=(
                        'Expected ReadReadmeRevisionFromDirectory to raise '
                        'RollProtobufError when README.chromium is missing.')):
                roll_protobuf.ReadReadmeRevisionFromDirectory(
                    temporary_directory)


if __name__ == '__main__':
    unittest.main()
