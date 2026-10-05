#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Hermetic unit tests for //third_party/protobuf/roll_protobuf.py."""

from collections.abc import Mapping
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
