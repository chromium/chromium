#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Hermetic unit tests for //third_party/protobuf/roll_protobuf.py."""

from collections.abc import Mapping
import dataclasses
import difflib
import io
import json
import os
import pathlib
import re
import shutil
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
    commit_hash = _InitializeGitRepositoryWithFiles(upstream_directory,
                                                    upstream_files)
    top_level_patches = sorted(
        path.removeprefix('patches/') for path in chromium_files
        if path.startswith('patches/') and path.endswith('.patch')
        and '/' not in path.removeprefix('patches/'))
    default_readme = (
        f'Revision: {commit_hash}\n\nDescription of the patches:\n' +
        ''.join(f'- {name}\n' for name in top_level_patches))
    merged_chromium_files = {'README.chromium': default_readme}
    merged_chromium_files.update(chromium_files)
    _InitializeGitRepositoryWithFiles(
        chromium_directory, merged_chromium_files)
    return upstream_directory, chromium_directory, commit_hash


def _RunMain(*arguments: str) -> tuple[int, str, str]:
    """Runs roll_protobuf.main(arguments) and returns (code, stdout, stderr)."""
    stdout, stderr = io.StringIO(), io.StringIO()
    with mock.patch('sys.stdout', stdout), mock.patch('sys.stderr', stderr):
        return (roll_protobuf.main(list(arguments)), stdout.getvalue().strip(),
                stderr.getvalue().strip())


class RollProtobufTest(unittest.TestCase):
    """Proves upstream cache, archive extraction, and patch invariants."""

    def setUp(self):
        super().setUp()
        self.enterContext(
            mock.patch.dict(
                os.environ,
                {
                    'GIT_CONFIG_GLOBAL': os.devnull,
                    'GIT_CONFIG_NOSYSTEM': '1',
                },
            ))
        self.temporary_directory = self.enterContext(
            tempfile.TemporaryDirectory())

    def testBootstrapGeneratedFilesMatchPresubmitInventory(self):
        self.assertEqual(PRESUBMIT._BOOTSTRAP_GENERATED_FILES,
                         roll_protobuf.BOOTSTRAP_GENERATED_FILES)

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

        self.assertEqual(roll_protobuf.CHROMIUM_OWNED_FILES, restored_files)

    def testReadReadmeRevisionExtractsCommitHashFromReadmeFile(self):
        _WriteFilesToDirectory(
            self.temporary_directory,
            {
                'README.chromium':
                ('Name: Protocol Buffers\n'
                 'Version: 36.0\n'
                 'Revision: 3f17acbf7ffbda81db8be17505554c726e558bb2\n'
                 'Security Critical: yes\n'),
            },
        )

        revision = roll_protobuf.ReadReadmeRevision(self.temporary_directory)

        self.assertEqual('3f17acbf7ffbda81db8be17505554c726e558bb2', revision)

    def testReadReadmeRevisionRaisesErrorWhenRevisionLineIsMissing(self):
        _WriteFilesToDirectory(
            self.temporary_directory,
            {'README.chromium': 'Name: Protocol Buffers\nVersion: 36.0\n'},
        )

        with self.assertRaises(roll_protobuf.RollProtobufError):
            roll_protobuf.ReadReadmeRevision(self.temporary_directory)

    def testReadReadmeRevisionRaisesErrorWhenReadmeFileIsMissing(self):
        with self.assertRaises(roll_protobuf.RollProtobufError):
            roll_protobuf.ReadReadmeRevision(self.temporary_directory)

    def testResolveDefaultCacheDirectoryUsesXdgCacheHomeWhenSet(self):
        xdg_cache_directory = os.path.join(self.temporary_directory, 'xdg')

        resolved_path = roll_protobuf.ResolveDefaultCacheDirectory(
            environment={'XDG_CACHE_HOME': xdg_cache_directory})

        self.assertEqual(
            os.path.join(
                xdg_cache_directory,
                'chromium-protobuf-roll',
                'upstream-protobuf',
            ), resolved_path)

    def testResolveDefaultCacheDirectoryFallsBackToHomeDotCacheWhenXdgUnset(
            self):
        resolved_path = roll_protobuf.ResolveDefaultCacheDirectory(
            environment={})

        self.assertEqual(
            os.path.join(
                os.path.abspath(os.path.expanduser('~/.cache')),
                'chromium-protobuf-roll',
                'upstream-protobuf',
            ), resolved_path)

    def testResolveDefaultCacheDirectoryUsesBuildDirectoryWhenProvided(self):
        build_directory = os.path.join(self.temporary_directory, 'out_linux',
                                       'Release')

        resolved_path = roll_protobuf.ResolveDefaultCacheDirectory(
            build_directory=build_directory,
            environment={'XDG_CACHE_HOME': '/unused/xdg'},
        )

        self.assertEqual(
            os.path.join(
                build_directory,
                '.protobuf_roll',
                'upstream-protobuf',
            ), resolved_path)

    def testEnsureUpstreamCommitCachedResolvesBareRepositoryUnderSafeConfig(
            self):
        source_directory = os.path.join(self.temporary_directory, 'source')
        bare_directory = os.path.join(self.temporary_directory, 'bare.git')
        commit_hash = _InitializeGitRepositoryWithFiles(
            source_directory,
            {'src/google/protobuf/arena.h': '// arena\n'},
        )
        subprocess.run(
            [
                'git', 'clone', '--bare', '--quiet', source_directory,
                bare_directory
            ],
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
            _, resolved_commit = (roll_protobuf.EnsureUpstreamCommitCached(
                revision=commit_hash,
                upstream_repository_path=bare_directory,
            ))

        self.assertEqual(commit_hash, resolved_commit)

    def testEnsureUpstreamCommitCachedClonesMissingCacheRepositoryOnDemand(
            self):
        upstream_directory = os.path.join(self.temporary_directory, 'upstream')
        cache_directory = os.path.join(self.temporary_directory, 'nested',
                                       'cache', 'upstream-protobuf')
        commit_hash = _InitializeGitRepositoryWithFiles(
            upstream_directory,
            {'src/google/protobuf/arena.h': '// v1\n'},
        )

        _, resolved_commit = roll_protobuf.EnsureUpstreamCommitCached(
            revision=commit_hash,
            cache_directory=cache_directory,
            upstream_url=upstream_directory,
        )

        self.assertEqual(commit_hash, resolved_commit)

    def testEnsureUpstreamCommitCachedFetchesCommitMissingFromExistingCache(
            self):
        upstream_directory = os.path.join(self.temporary_directory, 'upstream')
        cache_directory = os.path.join(self.temporary_directory, 'cache.git')
        _InitializeGitRepositoryWithFiles(
            upstream_directory,
            {'src/google/protobuf/arena.h': '// v1\n'},
        )
        subprocess.run(
            [
                'git', 'clone', '--bare', '--quiet', upstream_directory,
                cache_directory
            ],
            check=True,
        )
        _WriteFilesToDirectory(
            upstream_directory,
            {'src/google/protobuf/arena.h': '// v2\n'},
        )
        _RunGitCommand(upstream_directory, ['add', '.'])
        _RunGitCommand(upstream_directory,
                       ['commit', '--quiet', '-m', 'Release v2'])
        second_commit_hash = _RunGitCommand(upstream_directory,
                                            ['rev-parse', 'HEAD'])

        _, resolved_commit = roll_protobuf.EnsureUpstreamCommitCached(
            revision=second_commit_hash,
            cache_directory=cache_directory,
            upstream_url=upstream_directory,
        )

        self.assertEqual(second_commit_hash, resolved_commit)

    def testEnsureUpstreamCommitCachedRefreshesMovingBranchReference(self):
        upstream_directory = os.path.join(self.temporary_directory, 'upstream')
        cache_directory = os.path.join(self.temporary_directory, 'cache.git')
        _InitializeGitRepositoryWithFiles(
            upstream_directory,
            {'src/google/protobuf/arena.h': '// v1\n'},
        )
        branch_name = _RunGitCommand(upstream_directory,
                                     ['rev-parse', '--abbrev-ref', 'HEAD'])
        subprocess.run(
            [
                'git', 'clone', '--bare', '--quiet', upstream_directory,
                cache_directory
            ],
            check=True,
        )
        _WriteFilesToDirectory(
            upstream_directory,
            {'src/google/protobuf/arena.h': '// v2\n'},
        )
        _RunGitCommand(upstream_directory, ['add', '.'])
        _RunGitCommand(upstream_directory,
                       ['commit', '--quiet', '-m', 'Release v2'])
        second_commit_hash = _RunGitCommand(upstream_directory,
                                            ['rev-parse', 'HEAD'])

        _, resolved_commit = roll_protobuf.EnsureUpstreamCommitCached(
            revision=branch_name,
            cache_directory=cache_directory,
            upstream_url=upstream_directory,
        )

        self.assertEqual(second_commit_hash, resolved_commit)

    def testEnsureUpstreamCommitCachedIgnoresInheritedGitDirEnvironmentVariable(
            self):
        upstream_directory = os.path.join(self.temporary_directory, 'upstream')
        unrelated_directory = os.path.join(self.temporary_directory,
                                           'unrelated')
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

        self.assertEqual(upstream_commit, resolved_commit)

    def testEnsureUpstreamCommitCachedRaisesErrorForNonGitUpstreamPath(self):
        with self.assertRaises(roll_protobuf.RollProtobufError):
            roll_protobuf.EnsureUpstreamCommitCached(
                revision='HEAD',
                upstream_repository_path=self.temporary_directory,
            )

    def testEnsureUpstreamCommitCachedRaisesErrorForMissingUpstreamRevision(
            self):
        upstream_directory = os.path.join(self.temporary_directory, 'upstream')
        _InitializeGitRepositoryWithFiles(
            upstream_directory,
            {'src/google/protobuf/arena.h': '// v1\n'},
        )

        with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                    'not found in upstream repository'):
            roll_protobuf.EnsureUpstreamCommitCached(
                revision='0000000000000000000000000000000000000000',
                upstream_repository_path=upstream_directory,
            )

    def testEnsureUpstreamCommitCachedSkipsFetchWhenFullCommitHashIsCached(
            self):
        upstream_directory = os.path.join(self.temporary_directory, 'upstream')
        cache_directory = os.path.join(self.temporary_directory, 'cache.git')
        commit_hash = _InitializeGitRepositoryWithFiles(
            upstream_directory,
            {'src/google/protobuf/arena.h': '// v1\n'},
        )
        subprocess.run(
            [
                'git', 'clone', '--bare', '--quiet', upstream_directory,
                cache_directory
            ],
            check=True,
        )

        _, resolved_commit = roll_protobuf.EnsureUpstreamCommitCached(
            revision=commit_hash,
            cache_directory=cache_directory,
            upstream_url='/nonexistent/remote',
        )

        self.assertEqual(commit_hash, resolved_commit)

    def testEnsureUpstreamCommitCachedRaisesErrorWhenFetchFails(self):
        upstream_directory = os.path.join(self.temporary_directory, 'upstream')
        cache_directory = os.path.join(self.temporary_directory, 'cache.git')
        missing_remote_directory = os.path.join(self.temporary_directory,
                                                'missing_remote')
        _InitializeGitRepositoryWithFiles(
            upstream_directory,
            {'src/google/protobuf/arena.h': '// v1\n'},
        )
        subprocess.run(
            [
                'git', 'clone', '--bare', '--quiet', upstream_directory,
                cache_directory
            ],
            check=True,
        )

        # Use a non-repository path rather than a missing ref in a valid
        # repository because LUCI's infra/tools/git wrapper matches
        # "fatal: couldn't find remote ref" in DefaultGitRetryRegexp and
        # retries 12 times (~12 minutes) before returning non-zero.
        with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                    'Failed to fetch revision'):
            roll_protobuf.EnsureUpstreamCommitCached(
                revision='v33.5',
                cache_directory=cache_directory,
                upstream_url=missing_remote_directory,
            )

    def testExtractUpstreamArchiveMaterializesFilesAtCommitRevision(self):
        upstream_directory = os.path.join(self.temporary_directory, 'upstream')
        destination_directory = os.path.join(self.temporary_directory,
                                             'destination')
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
        extracted_header_path = os.path.join(destination_directory, 'src',
                                             'google', 'protobuf', 'arena.h')
        with open(extracted_header_path, 'r', encoding='utf-8') as header:
            extracted_content = header.read()

        self.assertEqual('// arena v1\n', extracted_content)

    def testExtractUpstreamArchivePreservesGitArchiveStderrForUnknownRevision(
            self):
        upstream_directory = os.path.join(self.temporary_directory, 'upstream')
        destination_directory = os.path.join(self.temporary_directory,
                                             'destination')
        os.makedirs(destination_directory, exist_ok=True)
        _InitializeGitRepositoryWithFiles(
            upstream_directory,
            {'src/google/protobuf/arena.h': '// arena\n'},
        )

        with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                    r'git archive failed.*fatal:'):
            roll_protobuf.ExtractUpstreamArchive(
                repository_path=upstream_directory,
                commit_revision='0000000000000000000000000000000000000000',
                destination_directory=destination_directory,
            )

    def testExtractUpstreamArchiveRaisesWhenGitArchiveExitsNonZeroAfterStream(
            self):
        upstream_directory = os.path.join(self.temporary_directory, 'upstream')
        destination_directory = os.path.join(self.temporary_directory,
                                             'destination')
        os.makedirs(destination_directory, exist_ok=True)
        commit_hash = _InitializeGitRepositoryWithFiles(
            upstream_directory,
            {'src/google/protobuf/arena.h': '// arena\n'},
        )
        real_command_builder = roll_protobuf._GitCommandForRepository

        def BuildFailingAfterStreamGitCommand(
                repository_path: str, git_arguments: list[str]) -> list[str]:
            return [
                sys.executable,
                '-c',
                ('import subprocess, sys; '
                 'subprocess.run(sys.argv[1:], check=True); '
                 'sys.stderr.write("fatal: corrupt pack\\n"); '
                 'sys.exit(1)'),
                *real_command_builder(repository_path, git_arguments),
            ]

        with (
                mock.patch.object(
                    roll_protobuf,
                    '_GitCommandForRepository',
                    BuildFailingAfterStreamGitCommand,
                ),
                self.assertRaisesRegex(
                    roll_protobuf.RollProtobufError,
                    r'git archive failed.*fatal: corrupt pack'),
        ):
            roll_protobuf.ExtractUpstreamArchive(
                repository_path=upstream_directory,
                commit_revision=commit_hash,
                destination_directory=destination_directory,
            )

    @_SkipOnWindows('os.symlink and SIGPIPE require POSIX.')
    def testExtractUpstreamArchiveReportsTarFilterErrorBeforeGitSigpipe(self):
        upstream_directory = os.path.join(self.temporary_directory, 'upstream')
        destination_directory = os.path.join(self.temporary_directory,
                                             'destination')
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
        _RunGitCommand(upstream_directory,
                       ['commit', '--quiet', '-m', 'Add symlink'])
        commit_hash = _RunGitCommand(upstream_directory, ['rev-parse', 'HEAD'])

        with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                    'absolute path'):
            roll_protobuf.ExtractUpstreamArchive(
                repository_path=upstream_directory,
                commit_revision=commit_hash,
                destination_directory=destination_directory,
            )

    @_SkipOnWindows('os.set_blocking on pipe file descriptors is POSIX-only.')
    def testExtractUpstreamArchiveDoesNotBlockGitOnVerboseStderr(self):
        upstream_directory = os.path.join(self.temporary_directory, 'upstream')
        destination_directory = os.path.join(self.temporary_directory,
                                             'destination')
        os.makedirs(destination_directory, exist_ok=True)
        commit_hash = _InitializeGitRepositoryWithFiles(
            upstream_directory,
            {'src/google/protobuf/arena.h': '// arena\n'},
        )
        real_command_builder = roll_protobuf._GitCommandForRepository

        def BuildVerboseStderrGitCommand(
                repository_path: str, git_arguments: list[str]) -> list[str]:
            return [
                sys.executable,
                '-c',
                _STDERR_FLOOD_THEN_ARCHIVE_SCRIPT,
                *real_command_builder(repository_path, git_arguments),
            ]

        with mock.patch.object(roll_protobuf, '_GitCommandForRepository',
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

        self.assertTrue(extracted_arena_exists)

    @_SkipOnWindows('POSIX sh SIGPIPE exit status 141 is Unix-only.')
    def testExtractUpstreamArchiveReportsOsErrorWhenGitWrapperExits141(self):
        upstream_directory = os.path.join(self.temporary_directory, 'upstream')
        destination_file_path = os.path.join(self.temporary_directory,
                                             'regular_file')
        with open(destination_file_path, 'w', encoding='utf-8'):
            pass
        commit_hash = _InitializeGitRepositoryWithFiles(
            upstream_directory,
            {'z_padding.txt': 'x' * (8 * 1024 * 1024)},
        )
        real_command_builder = roll_protobuf._GitCommandForRepository

        def BuildShellWrappedGitCommand(repository_path: str,
                                        git_arguments: list[str]) -> list[str]:
            return [
                'sh',
                '-c',
                '"$@"; exit $?',
                'sh',
                *real_command_builder(repository_path, git_arguments),
            ]

        with (
                mock.patch.object(roll_protobuf, '_GitCommandForRepository',
                                  BuildShellWrappedGitCommand),
                self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                       'Not a directory'),
        ):
            roll_protobuf.ExtractUpstreamArchive(
                repository_path=upstream_directory,
                commit_revision=commit_hash,
                destination_directory=destination_file_path,
            )

    def testApplyPatchFileAcceptsZeroContextUnidiffPatch(self):
        target_directory = os.path.join(self.temporary_directory, 'target')
        patch_path = os.path.join(self.temporary_directory,
                                  '0072-remove-version-stamp.patch')
        _WriteFilesToDirectory(
            target_directory,
            {
                'src/google/protobuf/port.cc':
                ('line 1\n'
                 'bool kVersionStampBuildHasHardeningProtobuf = true;\n'
                 'line 3\n'),
            },
        )
        _WriteFilesToDirectory(
            self.temporary_directory,
            {
                '0072-remove-version-stamp.patch':
                ('diff --git a/src/google/protobuf/port.cc '
                 'b/src/google/protobuf/port.cc\n'
                 '--- a/src/google/protobuf/port.cc\n'
                 '+++ b/src/google/protobuf/port.cc\n'
                 '@@ -2,1 +2,0 @@\n'
                 '-bool kVersionStampBuildHasHardeningProtobuf = true;\n'),
            },
        )

        roll_protobuf.ApplyPatchFile(
            patch_path=patch_path,
            target_directory=target_directory,
        )
        port_source_path = os.path.join(target_directory, 'src', 'google',
                                        'protobuf', 'port.cc')
        with open(port_source_path, 'r', encoding='utf-8') as port_file:
            updated_content = port_file.read()

        self.assertEqual('line 1\nline 3\n', updated_content)

    def testApplyPatchFileModifiesSubdirectoryInsideEnclosingGitRepository(
            self):
        outer_repository = os.path.join(self.temporary_directory, 'chromium')
        protobuf_subdirectory = os.path.join(outer_repository, 'third_party',
                                             'protobuf')
        _InitializeGitRepositoryWithFiles(
            outer_repository,
            {
                'third_party/protobuf/src/google/protobuf/message.h':
                ('int Value() { return 1; }\n'),
            },
        )
        patch_path = os.path.join(self.temporary_directory,
                                  '0001-update.patch')
        _WriteFilesToDirectory(
            self.temporary_directory,
            {
                '0001-update.patch':
                ('diff --git a/src/google/protobuf/message.h '
                 'b/src/google/protobuf/message.h\n'
                 '--- a/src/google/protobuf/message.h\n'
                 '+++ b/src/google/protobuf/message.h\n'
                 '@@ -1 +1 @@\n'
                 '-int Value() { return 1; }\n'
                 '+int Value() { return 2; }\n'),
            },
        )

        roll_protobuf.ApplyPatchFile(
            patch_path=patch_path,
            target_directory=protobuf_subdirectory,
        )
        message_header_path = os.path.join(protobuf_subdirectory, 'src',
                                           'google', 'protobuf', 'message.h')
        with open(message_header_path, 'r', encoding='utf-8') as header:
            updated_content = header.read()

        self.assertEqual('int Value() { return 2; }\n', updated_content)

    def testApplyPatchFileReturnsGitDiagnosticWhenPatchDoesNotApply(self):
        target_directory = os.path.join(self.temporary_directory, 'target')
        patch_path = os.path.join(self.temporary_directory, '0001-stale.patch')
        _WriteFilesToDirectory(
            target_directory,
            {
                'src/google/protobuf/message.h':
                ('int Value() { return 3; }\n'),
            },
        )
        _WriteFilesToDirectory(
            self.temporary_directory,
            {
                '0001-stale.patch': ('--- a/src/google/protobuf/message.h\n'
                                     '+++ b/src/google/protobuf/message.h\n'
                                     '@@ -1 +1 @@\n'
                                     '-int Value() { return 1; }\n'
                                     '+int Value() { return 2; }\n'),
            },
        )

        patch_error = roll_protobuf.ApplyPatchFile(
            patch_path=patch_path,
            target_directory=target_directory,
        )

        self.assertIn('patch does not apply',
                      '' if patch_error is None else patch_error)

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
        upstream_directory, chromium_directory, commit_hash = (
            _CreateUpstreamAndChromiumRepositories(
                self.temporary_directory,
                upstream_files={
                    'src/google/protobuf/message.h':
                    ('int Value() { return 1; }\n'),
                    'src/google/protobuf/port.cc':
                    ('int Port() { return 0; }\n'),
                },
                chromium_files={
                    'patches/0001-update-value.patch':
                    patch_content,
                    'src/google/protobuf/message.h':
                    ('int Value() { return 2; }\n'),
                    'src/google/protobuf/port.cc':
                    ('int Port() { return 0; }\n'),
                },
            ))

        verification_result = roll_protobuf.CheckTreeMatchesPatchedUpstream(
            protobuf_directory=chromium_directory,
            upstream_repository_path=upstream_directory,
        )

        self.assertEqual(
            roll_protobuf.TreeVerificationResult(
                upstream_revision=commit_hash,
                applied_patches=('0001-update-value.patch', ),
            ), verification_result)

    def testCheckTreeIgnoresDeveloperGlobalGitConfiguration(self):
        # core.autocrlf writes CRLF in `git archive` output and
        # apply.whitespace=fix strips trailing whitespace from patches.
        patched_message_header = ('int Value() { return 1; }\n'
                                  'int Other() { return 2; }   \n')
        patch_content = ('diff --git a/src/google/protobuf/message.h '
                         'b/src/google/protobuf/message.h\n'
                         '--- a/src/google/protobuf/message.h\n'
                         '+++ b/src/google/protobuf/message.h\n'
                         '@@ -1 +1,2 @@\n'
                         ' int Value() { return 1; }\n'
                         '+int Other() { return 2; }   \n')
        upstream_directory, chromium_directory, commit_hash = (
            _CreateUpstreamAndChromiumRepositories(
                self.temporary_directory,
                upstream_files={
                    'src/google/protobuf/message.h':
                    ('int Value() { return 1; }\n'),
                },
                chromium_files={
                    'patches/0001-add-other.patch': patch_content,
                    'src/google/protobuf/message.h': patched_message_header,
                },
            ))
        _WriteFilesToDirectory(
            self.temporary_directory,
            {
                'gitconfig': ('[core]\n\tautocrlf = true\n'
                              '[apply]\n\twhitespace = fix\n'),
            },
        )

        with mock.patch.dict(
                os.environ, {
                    'GIT_CONFIG_GLOBAL':
                    os.path.join(self.temporary_directory, 'gitconfig'),
                }):
            verification_result = (
                roll_protobuf.CheckTreeMatchesPatchedUpstream(
                    protobuf_directory=chromium_directory,
                    upstream_repository_path=upstream_directory,
                ))

        self.assertEqual(
            roll_protobuf.TreeVerificationResult(
                upstream_revision=commit_hash,
                applied_patches=('0001-add-other.patch', ),
            ),
            verification_result,
        )

    def testCheckTreeDetectsSourceModificationWhenPatchFileIsMissing(self):
        # Simulate https://crrev.com/c/8422202: message.h was updated in the
        # Chromium tree, but the corresponding .patch file is missing.
        upstream_directory, chromium_directory, _ = (
            _CreateUpstreamAndChromiumRepositories(
                self.temporary_directory,
                upstream_files={
                    'src/google/protobuf/message.h':
                    ('const EnumValueDescriptor* GetEnum() const;\n'),
                },
                chromium_files={
                    'src/google/protobuf/message.h':
                    ('ABSL_DEPRECATED("Use field") '
                     'const EnumValueDescriptor* GetEnum() const;\n'),
                },
            ))

        verification_result = roll_protobuf.CheckTreeMatchesPatchedUpstream(
            protobuf_directory=chromium_directory,
            upstream_repository_path=upstream_directory,
        )

        self.assertEqual(('src/google/protobuf/message.h', ),
                         verification_result.modified_files)

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
        upstream_directory, chromium_directory, _ = (
            _CreateUpstreamAndChromiumRepositories(
                self.temporary_directory,
                upstream_files={
                    'src/google/protobuf/message.h':
                    ('int Value() { return 1; }\n'),
                    'src/google/protobuf/port.cc':
                    ('int Port() { return 0; }\n'),
                },
                chromium_files={
                    'patches/0001-update-value.patch':
                    patch_content,
                    'src/google/protobuf/message.h':
                    ('int Value() { return 2; }\n'),
                    'src/google/protobuf/port.cc':
                    ('int Port() { return 99; }\n'),
                },
            ))

        verification_result = roll_protobuf.CheckTreeMatchesPatchedUpstream(
            protobuf_directory=chromium_directory,
            upstream_repository_path=upstream_directory,
        )

        self.assertEqual(('src/google/protobuf/port.cc', ),
                         verification_result.modified_files)

    def testCheckTreeDetectsUnexpectedExtraChromiumFile(self):
        upstream_directory, chromium_directory, _ = (
            _CreateUpstreamAndChromiumRepositories(
                self.temporary_directory,
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

        self.assertEqual(('src/google/protobuf/stray_local_file.cc', ),
                         verification_result.chromium_only_files)

    def testCheckTreeDetectsMissingUpstreamFileNotInExceptionList(self):
        upstream_directory, chromium_directory, _ = (
            _CreateUpstreamAndChromiumRepositories(
                self.temporary_directory,
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

        self.assertEqual(('src/google/protobuf/extension_set.h', ),
                         verification_result.upstream_only_files)

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
        upstream_directory, chromium_directory, _ = (
            _CreateUpstreamAndChromiumRepositories(
                self.temporary_directory,
                upstream_files={
                    'src/google/protobuf/message.h':
                    ('int Value() { return 1; }\n'),
                },
                chromium_files={
                    'patches/0015-broken-context.patch':
                    conflicting_patch,
                    'src/google/protobuf/message.h':
                    ('int Value() { return 1; }\n'),
                },
            ))

        verification_result = roll_protobuf.CheckTreeMatchesPatchedUpstream(
            protobuf_directory=chromium_directory,
            upstream_repository_path=upstream_directory,
        )

        self.assertEqual('0015-broken-context.patch',
                         verification_result.failing_patch)

    def testCheckTreeIgnoresChromiumOwnedAndBootstrapGeneratedFiles(self):
        upstream_directory, chromium_directory, _ = (
            _CreateUpstreamAndChromiumRepositories(
                self.temporary_directory,
                upstream_files={
                    'src/google/protobuf/arena.h':
                    '// arena\n',
                    'src/google/protobuf/descriptor.pb.h':
                    ('// upstream unpatched descriptor.pb.h\n'),
                    'src/google/protobuf/descriptor.pb.cc':
                    ('// upstream unpatched descriptor.pb.cc\n'),
                    'src/google/protobuf/compiler/plugin.pb.h':
                    ('// upstream unpatched plugin.pb.h\n'),
                    'src/google/protobuf/compiler/plugin.pb.cc':
                    ('// upstream unpatched plugin.pb.cc\n'),
                },
                chromium_files={
                    'BUILD.gn':
                    '# Chromium GN build\n',
                    'DEPS':
                    'deps = {}\n',
                    'DIR_METADATA':
                    'monorail {}\n',
                    'OWNERS':
                    'file://OWNERS\n',
                    'PRESUBMIT.py':
                    'PRESUBMIT_VERSION = "2.0.0"\n',
                    'PRESUBMIT_test.py':
                    '# test\n',
                    'check_file_lists.py':
                    '# check_file_lists\n',
                    'check_file_lists_test.py':
                    '# test\n',
                    'gen_extra_chromium_files.py':
                    '# gen\n',
                    'proto_library.gni':
                    '# gni\n',
                    'proto_sources.gni':
                    '# gni\n',
                    'proto_wkt.gni':
                    '# gni\n',
                    'roll_protobuf.py':
                    '# roll\n',
                    'roll_protobuf_test.py':
                    '# test\n',
                    'patches/0044-trim-protoc-main.md':
                    '# note\n',
                    'src/google/protobuf/arena.h':
                    '// arena\n',
                    'src/google/protobuf/descriptor.pb.h':
                    ('// Chromium regenerated descriptor.pb.h\n'),
                    'src/google/protobuf/descriptor.pb.cc':
                    ('// Chromium regenerated descriptor.pb.cc\n'),
                    'src/google/protobuf/compiler/plugin.pb.h':
                    ('// Chromium regenerated plugin.pb.h\n'),
                    'src/google/protobuf/compiler/plugin.pb.cc':
                    ('// Chromium regenerated plugin.pb.cc\n'),
                    'python/google/protobuf/descriptor_pb2.py':
                    ('# generated descriptor_pb2\n'),
                    'python/google/protobuf/compiler/plugin_pb2.py':
                    ('# generated plugin_pb2\n'),
                    'python/google/protobuf/internal/'
                    'python_edition_defaults.py':
                    ('# generated edition defaults\n'),
                },
            ))

        verification_result = roll_protobuf.CheckTreeMatchesPatchedUpstream(
            protobuf_directory=chromium_directory,
            upstream_repository_path=upstream_directory,
        )

        self.assertTrue(verification_result.IsClean())

    def testCheckTreeIgnoresCompatibilityAndWellKnownTypeGeneratedFiles(self):
        upstream_directory, chromium_directory, _ = (
            _CreateUpstreamAndChromiumRepositories(
                self.temporary_directory,
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

        self.assertTrue(verification_result.IsClean())

    def testCheckTreeIgnoresUpstreamFilesMatchedByGitignore(self):
        upstream_directory, chromium_directory, _ = (
            _CreateUpstreamAndChromiumRepositories(
                self.temporary_directory,
                upstream_files={
                    '.gitignore':
                    '*.sln\n*.targets\n*.xcodeproj/\n',
                    'src/google/protobuf/arena.h':
                    '// arena\n',
                    'csharp/src/Google.Protobuf.sln':
                    'Visual Studio\n',
                    'csharp/Google.Protobuf.Tools.targets':
                    '<Project/>\n',
                    'objectivec/ProtocolBuffers_OSX.xcodeproj/'
                    'project.pbxproj':
                    '// !$*UTF8*$!\n',
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

        self.assertTrue(verification_result.IsClean())

    def testCheckTreeRaisesErrorWhenProtobufDirectoryIsOutsideGitWorkTree(self):
        upstream_directory = os.path.join(self.temporary_directory, 'upstream')
        commit_hash = _InitializeGitRepositoryWithFiles(
            upstream_directory,
            {'src/google/protobuf/arena.h': '// arena\n'},
        )
        non_git_directory = os.path.join(self.temporary_directory, 'plain_dir')
        _WriteFilesToDirectory(
            non_git_directory,
            {
                'README.chromium': f'Revision: {commit_hash}\n',
                'src/google/protobuf/arena.h': '// arena\n',
            },
        )

        with mock.patch.dict(
                os.environ,
            {'GIT_CEILING_DIRECTORIES': self.temporary_directory}):
            with self.assertRaises(roll_protobuf.RollProtobufError):
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
            ('//third_party/protobuf does not match upstream abc1234 plus '
             'patches/*.patch:\n\n'
             'Chromium-only files not in known exceptions (1):\n'
             '  + src/google/protobuf/stray.cc\n\n'
             'Upstream files missing from //third_party/protobuf (1):\n'
             '  - src/google/protobuf/missing.cc'), report_text)

    def testMainCheckExitsZeroAndPrintsSummaryWhenTreeIsClean(self):
        upstream_directory, chromium_directory, commit_hash = (
            _CreateUpstreamAndChromiumRepositories(
                self.temporary_directory,
                upstream_files={
                    'src/google/protobuf/arena.h': '// arena\n',
                },
                chromium_files={
                    'src/google/protobuf/arena.h': '// arena\n',
                },
            ))

        exit_code, stdout, _ = _RunMain(
            'check',
            '--protobuf-dir',
            chromium_directory,
            '--upstream-repo',
            upstream_directory,
        )

        self.assertEqual((
            roll_protobuf.EXIT_CODE_SUCCESS,
            f'Verified //third_party/protobuf matches upstream '
            f'{commit_hash} plus 0 patch(es).',
        ), (exit_code, stdout))

    def testMainCheckExitsOneAndListsModifiedFileWhenPatchIsMissing(self):
        upstream_directory, chromium_directory, commit_hash = (
            _CreateUpstreamAndChromiumRepositories(
                self.temporary_directory,
                upstream_files={
                    'src/google/protobuf/message.h':
                    ('int Value() { return 1; }\n'),
                },
                chromium_files={
                    'src/google/protobuf/message.h':
                    ('int Value() { return 2; }\n'),
                },
            ))

        exit_code, stdout, _ = _RunMain(
            'check',
            '--protobuf-dir',
            chromium_directory,
            '--upstream-repo',
            upstream_directory,
        )

        self.assertEqual((
            roll_protobuf.EXIT_CODE_ERROR,
            f'//third_party/protobuf does not match upstream '
            f'{commit_hash} plus patches/*.patch:\n\n'
            'Modified files not explained by patches/ (1):\n'
            '  M src/google/protobuf/message.h',
        ), (exit_code, stdout))

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
        upstream_directory, chromium_directory, commit_hash = (
            _CreateUpstreamAndChromiumRepositories(
                self.temporary_directory,
                upstream_files={
                    'src/google/protobuf/message.h':
                    ('int Value() { return 1; }\n'),
                },
                chromium_files={
                    'patches/0099-broken.patch':
                    conflicting_patch,
                    'src/google/protobuf/message.h':
                    ('int Value() { return 1; }\n'),
                },
            ))

        _, stdout, _ = _RunMain(
            'check',
            '--protobuf-dir',
            chromium_directory,
            '--upstream-repo',
            upstream_directory,
        )

        self.assertEqual((f'Patch failed to apply to upstream {commit_hash}: '
                          '0099-broken.patch'),
                         stdout.splitlines()[0])

    def testMainCheckOutputsMachineReadableJsonWhenFlagIsSet(self):
        upstream_directory, chromium_directory, commit_hash = (
            _CreateUpstreamAndChromiumRepositories(
                self.temporary_directory,
                upstream_files={
                    'src/google/protobuf/arena.h': '// arena\n',
                },
                chromium_files={
                    'src/google/protobuf/arena.h': '// arena suffix\n',
                },
            ))

        _, stdout, _ = _RunMain(
            'check',
            '--protobuf-dir',
            chromium_directory,
            '--upstream-repo',
            upstream_directory,
            '--json',
        )

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
                'patch_inventory_errors': [],
                'hygiene_errors': [],
                'stale_bootstrap_triggers': [],
                'file_lists': None,
                'compile_size': None,
                'compile_size_error': None,
            }, json.loads(stdout))

    def testMainCheckIncludesJsonErrorMessageWhenRevisionIsMissingFromUpstream(
            self):
        missing_revision = '0' * 40
        upstream_directory, chromium_directory, _ = (
            _CreateUpstreamAndChromiumRepositories(
                self.temporary_directory,
                upstream_files={
                    'src/google/protobuf/arena.h': '// arena\n',
                },
                chromium_files={
                    'README.chromium': f'Revision: {missing_revision}\n',
                    'src/google/protobuf/arena.h': '// arena\n',
                },
            ))

        _, stdout, _ = _RunMain(
            'check',
            '--protobuf-dir',
            chromium_directory,
            '--upstream-repo',
            upstream_directory,
            '--json',
        )

        self.assertEqual(
            {
                'is_clean':
                False,
                'error': (f'Revision {missing_revision} not found in '
                          f'upstream repository {upstream_directory}.'),
            }, json.loads(stdout))

    def testMainCheckOutputsJsonErrorWhenReadmeCannotBeRead(self):
        expected_readme_path = os.path.join(
            os.path.abspath(self.temporary_directory), 'README.chromium')

        _, stdout, _ = _RunMain(
            'check',
            '--protobuf-dir',
            self.temporary_directory,
            '--upstream-repo',
            self.temporary_directory,
            '--json',
        )

        self.assertEqual(
            {
                'is_clean': False,
                'error': f'Missing README.chromium at {expected_readme_path}.',
            }, json.loads(stdout))

    def testMainCheckExitsOneAndPrintsErrorToStderrWhenReadmeIsMissing(self):
        expected_readme_path = os.path.join(
            os.path.abspath(self.temporary_directory), 'README.chromium')

        exit_code, _, stderr = _RunMain(
            'check',
            '--protobuf-dir',
            self.temporary_directory,
            '--upstream-repo',
            self.temporary_directory,
        )

        self.assertEqual((
            roll_protobuf.EXIT_CODE_ERROR,
            f'Error: Missing README.chromium at {expected_readme_path}.',
        ), (exit_code, stderr))

    def testMainCheckClonesCacheUnderBuildDirectoryWhenDashCFlagIsSet(self):
        build_directory = os.path.join(self.temporary_directory, 'out_release')
        upstream_directory, chromium_directory, _ = (
            _CreateUpstreamAndChromiumRepositories(
                self.temporary_directory,
                upstream_files={
                    'src/google/protobuf/arena.h': '// arena\n',
                },
                chromium_files={
                    'src/google/protobuf/arena.h': '// arena\n',
                },
            ))
        expected_cache_directory = os.path.join(build_directory,
                                                '.protobuf_roll',
                                                'upstream-protobuf')

        _RunMain(
            'check',
            '-C',
            build_directory,
            '--protobuf-dir',
            chromium_directory,
            '--upstream-url',
            upstream_directory,
        )

        self.assertTrue(
            os.path.isfile(os.path.join(expected_cache_directory, 'HEAD')))

    def testCheckTreeFlagsUntrackedNonIgnoredFileInChromiumWorkingTree(self):
        upstream_directory, chromium_directory, _ = (
            _CreateUpstreamAndChromiumRepositories(
                self.temporary_directory,
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

        self.assertEqual(('src/google/protobuf/untracked_scratch.h', ),
                         verification_result.chromium_only_files)

    def testCheckTreeDoesNotMaskMissingUpstreamFileMatchedByUserExcludesFile(
            self):
        upstream_directory, chromium_directory, _ = (
            _CreateUpstreamAndChromiumRepositories(
                self.temporary_directory,
                upstream_files={
                    'src/google/protobuf/arena.h': '// arena\n',
                    'src/google/protobuf/user_ignored.h': '// missing\n',
                },
                chromium_files={
                    'src/google/protobuf/arena.h': '// arena\n',
                },
            ))
        user_excludes_file = os.path.join(self.temporary_directory,
                                          'global_gitignore')
        with open(user_excludes_file, 'w', encoding='utf-8') as handle:
            handle.write('*user_ignored*\n')

        with mock.patch.dict(
                os.environ, {
                    'GIT_CONFIG_COUNT': '1',
                    'GIT_CONFIG_KEY_0': 'core.excludesFile',
                    'GIT_CONFIG_VALUE_0': user_excludes_file,
                }):
            verification_result = (
                roll_protobuf.CheckTreeMatchesPatchedUpstream(
                    protobuf_directory=chromium_directory,
                    upstream_repository_path=upstream_directory,
                ))

        self.assertEqual(('src/google/protobuf/user_ignored.h', ),
                         verification_result.upstream_only_files)

    def testCheckTreeDoesNotSkipUntrackedChromiumFileMatchedByUserExcludesFile(
            self):
        upstream_directory, chromium_directory, _ = (
            _CreateUpstreamAndChromiumRepositories(
                self.temporary_directory,
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
        user_excludes_file = os.path.join(self.temporary_directory,
                                          'global_gitignore')
        with open(user_excludes_file, 'w', encoding='utf-8') as handle:
            handle.write('*user_excluded*\n')

        with mock.patch.dict(
                os.environ, {
                    'GIT_CONFIG_COUNT': '1',
                    'GIT_CONFIG_KEY_0': 'core.excludesFile',
                    'GIT_CONFIG_VALUE_0': user_excludes_file,
                }):
            verification_result = (
                roll_protobuf.CheckTreeMatchesPatchedUpstream(
                    protobuf_directory=chromium_directory,
                    upstream_repository_path=upstream_directory,
                ))

        self.assertEqual(('src/google/protobuf/user_excluded.h', ),
                         verification_result.chromium_only_files)

    def testCheckTreeDetectsDeletedTrackedFileMatchingGitignore(self):
        upstream_directory, chromium_directory, _ = (
            _CreateUpstreamAndChromiumRepositories(
                self.temporary_directory,
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
            os.path.join(chromium_directory, 'src', 'solaris', 'libstdc++.la'))

        verification_result = roll_protobuf.CheckTreeMatchesPatchedUpstream(
            protobuf_directory=chromium_directory,
            upstream_repository_path=upstream_directory,
        )

        self.assertEqual(('src/solaris/libstdc++.la', ),
                         verification_result.upstream_only_files)

    def testCheckTreeRaisesErrorWhenProtobufDirectoryIsGitIgnored(self):
        upstream_directory, chromium_directory, commit_hash = (
            _CreateUpstreamAndChromiumRepositories(
                self.temporary_directory,
                upstream_files={
                    'src/google/protobuf/arena.h': '// arena\n',
                },
                chromium_files={
                    '.gitignore': 'ignored_subdir/\n',
                    'src/google/protobuf/arena.h': '// arena\n',
                },
            ))
        ignored_protobuf_directory = os.path.join(chromium_directory,
                                                  'ignored_subdir')
        _WriteFilesToDirectory(
            ignored_protobuf_directory,
            {
                'README.chromium': f'Revision: {commit_hash}\n',
                'src/google/protobuf/arena.h': '// arena\n',
            },
        )

        with self.assertRaisesRegex(
                roll_protobuf.RollProtobufError,
                r'git ls-files returned no README\.chromium'):
            roll_protobuf.CheckTreeMatchesPatchedUpstream(
                protobuf_directory=ignored_protobuf_directory,
                upstream_repository_path=upstream_directory,
            )

    @_SkipOnWindows('os.symlink requires elevated privileges on Windows.')
    def testListRegularFilesInDirectorySkipsGitPycachePycAndSymlinks(self):
        _WriteFilesToDirectory(
            self.temporary_directory,
            {
                '.git/HEAD': 'ref: refs/heads/main\n',
                '__pycache__/cached.cpython-311.pyc': 'bytecode\n',
                'src/stale.pyc': 'bytecode\n',
                'src/google/protobuf/arena.h': '// arena\n',
            },
        )
        os.symlink(
            os.path.join(self.temporary_directory, 'src', 'google', 'protobuf',
                         'arena.h'),
            os.path.join(self.temporary_directory, 'src', 'symlink_arena.h'),
        )

        listed_files = roll_protobuf._ListRegularFilesInDirectory(
            self.temporary_directory)

        self.assertEqual({'src/google/protobuf/arena.h'}, listed_files)

    def testListTopLevelPatchPathsReturnsPatchesInReplayOrder(self):
        _WriteFilesToDirectory(
            self.temporary_directory,
            {
                'patches/0010-second.patch': '// second\n',
                'patches/0009-first.patch': '// first\n',
                'patches/0009-first.md': '# note\n',
                'patches/protobuf_v25/0001-upstream.patch': '// v25\n',
            },
        )

        patch_paths = roll_protobuf.ListTopLevelPatchPaths(
            self.temporary_directory)

        self.assertEqual([
            os.path.join(self.temporary_directory, 'patches',
                         '0009-first.patch'),
            os.path.join(self.temporary_directory, 'patches',
                         '0010-second.patch'),
        ], patch_paths)

    def testListTopLevelPatchPathsReturnsEmptyListWhenPatchesDirectoryIsMissing(
            self):
        patch_paths = roll_protobuf.ListTopLevelPatchPaths(
            self.temporary_directory)

        self.assertEqual([], patch_paths)

    def testIsChromiumOwnedFileClassifiesTrackedChromiumAndUpstreamPaths(self):
        classification_by_path = {
            path: roll_protobuf.IsChromiumOwnedFile(path)
            for path in (
                'BUILD.gn',
                'roll_protobuf.py',
                'patches/0004-fix-shared-library-exports.patch',
                'patches/0044-trim-protoc-main.md',
                'patches/protobuf_v25/0001-bazel.patch',
                'patches/protobuf_v25/README.md',
                'src/google/protobuf/arena.h',
            )
        }

        self.assertEqual(
            {
                'BUILD.gn': True,
                'roll_protobuf.py': True,
                'patches/0004-fix-shared-library-exports.patch': True,
                'patches/0044-trim-protoc-main.md': True,
                'patches/protobuf_v25/0001-bazel.patch': False,
                'patches/protobuf_v25/README.md': False,
                'src/google/protobuf/arena.h': False,
            }, classification_by_path)

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
            }, classification_by_path)


def _NumberedLines(name: str, **overrides: str) -> str:
    """Returns 12 lines 'name line N', replacing any `lineN='...'` overrides."""
    return ''.join(f'{overrides.get(f"line{n}", f"{name} line {n}")}\n'
                   for n in range(1, 13))


_PLAN_SOURCE_DIRECTORY = 'src/google/protobuf'
# '[1]' is a glob character class; git must treat it literally.
_PLAN_CLEAN_PATH = f'{_PLAN_SOURCE_DIRECTORY}/clean[1].cc'
_PLAN_EMPTY_PATH = f'{_PLAN_SOURCE_DIRECTORY}/empty.cc'
_PLAN_CONFLICT_PATH = f'{_PLAN_SOURCE_DIRECTORY}/conflict.cc'
_PLAN_RENAMED_PATH = f'{_PLAN_SOURCE_DIRECTORY}/renamed.cc'
_PLAN_MOVED_PATH = f'{_PLAN_SOURCE_DIRECTORY}/moved.cc'
_PLAN_ADDED_PATH = f'{_PLAN_SOURCE_DIRECTORY}/added.cc'
_PLAN_PATCH_ADDED_PATH = f'{_PLAN_SOURCE_DIRECTORY}/chromium_added.cc'

_PLAN_CLEAN_PATCH = '0001-clean.patch'
_PLAN_EMPTY_PATCH = '0002-empty.patch'
_PLAN_CONFLICT_PATCH = '0003-conflict.patch'
_PLAN_MISSING_FILE_PATCH = '0004-missing-file.patch'
_PLAN_STACKED_PATCH = '0005-stacked-on-clean.patch'

# Upstream's own .gitignore ignores every fixture source file, so scratch
# repositories only see them if files are added with --force.
_PLAN_UPSTREAM_GITIGNORE = '*.cc\n'


def _FileListsCmake(*relative_paths: str) -> str:
    """Returns a src/file_lists.cmake listing relative_paths in libprotobuf."""
    entries = ''.join(f'  ${{protobuf_SOURCE_DIR}}/{relative_path}\n'
                      for relative_path in relative_paths)
    return f'set(libprotobuf_srcs\n{entries})\n'


_PLAN_OLD_UPSTREAM_FILES = {
    _PLAN_CLEAN_PATH: _NumberedLines('clean'),
    _PLAN_EMPTY_PATH: _NumberedLines('empty'),
    _PLAN_CONFLICT_PATH: _NumberedLines('conflict'),
    _PLAN_RENAMED_PATH: _NumberedLines('renamed'),
    'src/file_lists.cmake': _FileListsCmake(_PLAN_CLEAN_PATH),
    '.gitignore': _PLAN_UPSTREAM_GITIGNORE,
}

_PLAN_CLEAN_PATCHED_CONTENT = _NumberedLines(
    'clean',
    line6='clean line 6 (chromium)',
    line10='clean line 10 (upstreamed)',
)

# Patches 0001-0004 each edit line 6 of their own file, against old upstream.
# 0001 also edits line 10 the way new upstream does, so it shrinks on replay.
# Patch 0005 edits line 7 of clean.cc on top of 0001, so it only applies
# cleanly if the replay kept 0001, and adds chromium_added.cc, a file only
# Chromium has, with trailing whitespace that `apply.whitespace` would reject.
_PLAN_PATCHED_FILES_BY_PATCH_NAME = {
    _PLAN_CLEAN_PATCH: {
        _PLAN_CLEAN_PATH: _PLAN_CLEAN_PATCHED_CONTENT
    },
    _PLAN_EMPTY_PATCH: {
        _PLAN_EMPTY_PATH:
        _NumberedLines('empty', line6='empty line 6 (upstreamed)'),
    },
    _PLAN_CONFLICT_PATCH: {
        _PLAN_CONFLICT_PATH:
        _NumberedLines('conflict', line6='conflict line 6 (chromium)'),
    },
    _PLAN_MISSING_FILE_PATCH: {
        _PLAN_RENAMED_PATH:
        _NumberedLines('renamed', line6='renamed line 6 (chromium)'),
    },
}

# New upstream:
# - clean.cc changes line 3, inside 0001's context but not its change, so a
#   plain `git apply` fails and only the three-way merge applies it cleanly,
#   and makes 0001's line 10 change itself;
# - empty.cc already contains 0002's change;
# - conflict.cc changes the line 0003 changes, differently;
# - renamed.cc moves to moved.cc unchanged, which `git apply -3` cannot follow;
# - added.cc is new and listed in src/file_lists.cmake.
_PLAN_NEW_UPSTREAM_FILES = {
    _PLAN_CLEAN_PATH:
    _NumberedLines(
        'clean',
        line3='clean line 3 (upstream)',
        line10='clean line 10 (upstreamed)',
    ),
    _PLAN_EMPTY_PATH:
    _NumberedLines('empty', line6='empty line 6 (upstreamed)'),
    _PLAN_CONFLICT_PATH:
    _NumberedLines('conflict', line6='conflict line 6 (upstream)'),
    _PLAN_MOVED_PATH:
    _PLAN_OLD_UPSTREAM_FILES[_PLAN_RENAMED_PATH],
    _PLAN_ADDED_PATH:
    '// added upstream\n',
    'src/file_lists.cmake':
    _FileListsCmake(_PLAN_CLEAN_PATH, _PLAN_ADDED_PATH),
    '.gitignore':
    _PLAN_UPSTREAM_GITIGNORE,
}


@dataclasses.dataclass(frozen=True)
class _PlanFixture:
    """Paths and commits of a fake upstream and Chromium protobuf directory."""

    upstream_directory: str
    protobuf_directory: str
    old_commit: str
    new_commit: str

    def PatchPaths(self) -> list[str]:
        return roll_protobuf.ListTopLevelPatchPaths(self.protobuf_directory)


def _MakePatch(old_files: Mapping[str, str],
               patched_files: Mapping[str, str]) -> str:
    """Returns the unified diff that turns old_files into patched_files."""
    return ''.join(
        line for path, new_text in patched_files.items()
        for line in difflib.unified_diff(
            old_files.get(path, '').splitlines(keepends=True),
            new_text.splitlines(keepends=True),
            fromfile=f'a/{path}' if path in old_files else '/dev/null',
            tofile=f'b/{path}',
        ))


def _CreatePlanFixture(temporary_directory: str) -> _PlanFixture:
    """Builds an upstream repository with two versions and five patches.

    Against new upstream the patches come out, in order, CLEAN, EMPTY,
    CONFLICT, MISSING_FILE and CLEAN.
    """
    upstream_directory = os.path.join(temporary_directory, 'upstream')
    old_commit = _InitializeGitRepositoryWithFiles(upstream_directory,
                                                   _PLAN_OLD_UPSTREAM_FILES)
    _RunGitCommand(upstream_directory,
                   ['mv', _PLAN_RENAMED_PATH, _PLAN_MOVED_PATH])
    _WriteFilesToDirectory(upstream_directory, _PLAN_NEW_UPSTREAM_FILES)
    _RunGitCommand(upstream_directory, ['add', '-A', '.'])
    _RunGitCommand(upstream_directory, ['commit', '--quiet', '-m', 'New'])
    new_commit = _RunGitCommand(upstream_directory, ['rev-parse', 'HEAD'])

    chromium_files = {
        'README.chromium':
        f'Revision: {old_commit}\n',
        'proto_sources.gni': ('sources = [\n'
                              f'  "{_PLAN_CLEAN_PATH}",\n'
                              f'  "{_PLAN_PATCH_ADDED_PATH}",\n'
                              f'  "{_PLAN_RENAMED_PATH}",\n'
                              ']\n'),
        'BUILD.gn':
        '',
        **{
            f'patches/{patch_name}':
            _MakePatch(_PLAN_OLD_UPSTREAM_FILES, patched_files)
            for patch_name, patched_files in _PLAN_PATCHED_FILES_BY_PATCH_NAME.items(
            )
        },
        f'patches/{_PLAN_STACKED_PATCH}':
        _MakePatch(
            {
                **_PLAN_OLD_UPSTREAM_FILES, _PLAN_CLEAN_PATH:
                _PLAN_CLEAN_PATCHED_CONTENT
            },
            {
                _PLAN_CLEAN_PATH:
                _NumberedLines(
                    'clean',
                    line6='clean line 6 (chromium)',
                    line7='clean line 7 (chromium)',
                    line10='clean line 10 (upstreamed)',
                ),
                _PLAN_PATCH_ADDED_PATH:
                '// added by a Chromium patch   \n',
            },
        ),
    }
    protobuf_directory = os.path.join(temporary_directory, 'chromium')
    _InitializeGitRepositoryWithFiles(protobuf_directory, chromium_files)
    return _PlanFixture(
        upstream_directory=upstream_directory,
        protobuf_directory=protobuf_directory,
        old_commit=old_commit,
        new_commit=new_commit,
    )


class RollProtobufPlanTest(unittest.TestCase):
    """Proves `plan` classifies patches like a real roll would, read-only."""

    def setUp(self):
        super().setUp()
        self.enterContext(
            mock.patch.dict(
                os.environ,
                {
                    'GIT_CONFIG_GLOBAL': os.devnull,
                    'GIT_CONFIG_NOSYSTEM': '1',
                },
            ))
        self.temporary_directory = self.enterContext(
            tempfile.TemporaryDirectory())
        self.fixture = _CreatePlanFixture(self.temporary_directory)

    def _BuildPatchChain(self) -> roll_protobuf.PatchChain:
        return roll_protobuf.BuildPatchChain(
            upstream_repository_path=self.fixture.upstream_directory,
            old_upstream_commit=self.fixture.old_commit,
            patch_paths=self.fixture.PatchPaths(),
            chain_directory=os.path.join(self.temporary_directory, 'chain'),
        )

    def testBuildPatchChainExportsFullPreimageAndPostimageBlobIds(self):
        patch_chain = self._BuildPatchChain()

        self.assertRegex(patch_chain.patches[0].diff.decode(),
                         r'(?m)^index [0-9a-f]{40}\.\.[0-9a-f]{40} 100644$')

    def testBuildPatchChainRaisesWhenPatchDoesNotApplyToOldUpstream(self):
        broken_patch_path = os.path.join(self.fixture.protobuf_directory,
                                         'patches', '0005-broken.patch')
        _WriteFilesToDirectory(
            self.fixture.protobuf_directory,
            {
                'patches/0005-broken.patch': (f'--- a/{_PLAN_CLEAN_PATH}\n'
                                              f'+++ b/{_PLAN_CLEAN_PATH}\n'
                                              '@@ -1 +1 @@\n'
                                              '-no such line\n'
                                              '+replacement\n'),
            },
        )

        with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                    r'0005-broken\.patch does not apply'):
            roll_protobuf.BuildPatchChain(
                upstream_repository_path=self.fixture.upstream_directory,
                old_upstream_commit=self.fixture.old_commit,
                patch_paths=[*self.fixture.PatchPaths(), broken_patch_path],
                chain_directory=os.path.join(self.temporary_directory,
                                             'chain'),
            )

    def testBuildPatchChainRecordsPreimageBlobsAndChangedLineCountPerPatch(
            self):
        patch_chain = self._BuildPatchChain()

        self.assertEqual(
            {
                _PLAN_CLEAN_PATCH: ((_PLAN_CLEAN_PATH, ), 4),
                _PLAN_EMPTY_PATCH: ((_PLAN_EMPTY_PATH, ), 2),
                _PLAN_CONFLICT_PATCH: ((_PLAN_CONFLICT_PATH, ), 2),
                _PLAN_MISSING_FILE_PATCH: ((_PLAN_RENAMED_PATH, ), 2),
                _PLAN_STACKED_PATCH: ((_PLAN_CLEAN_PATH, ), 3),
            }, {
                chain_patch.patch_name: (
                    tuple(chain_patch.preimage_blobs),
                    chain_patch.changed_line_count,
                )
                for chain_patch in patch_chain.patches
            })

    def testBuildPatchChainTakesPreimageFromEarlierPatchesNotOldUpstream(self):
        patch_chain = self._BuildPatchChain()
        clean_after_first_patch_blob_id = subprocess.run(
            ['git', 'hash-object', '--stdin'],
            input=_PLAN_CLEAN_PATCHED_CONTENT.encode(),
            capture_output=True,
            check=True,
        ).stdout.decode().strip()

        self.assertEqual(
            clean_after_first_patch_blob_id,
            patch_chain.patches[4].preimage_blobs[_PLAN_CLEAN_PATH])

    def _CreateNewUpstreamCheckout(self, prefix: str = '') -> str:
        checkout_directory = os.path.join(self.temporary_directory, 'replay')
        _InitializeGitRepositoryWithFiles(
            checkout_directory,
            {
                f'{prefix}/{path}' if prefix else path: content
                for path, content in _PLAN_NEW_UPSTREAM_FILES.items()
            },
        )
        return checkout_directory

    def testCopyPreimageBlobsCopiesOnlyBlobsTheDestinationLacks(self):
        patch_chain = self._BuildPatchChain()
        replay_directory = self._CreateNewUpstreamCheckout()
        old_upstream_preimage_blob_ids = [
            blob_id for chain_patch in patch_chain.patches[:4]
            for blob_id in chain_patch.preimage_blobs.values()
        ]

        first_copied_blob_ids = roll_protobuf.CopyPreimageBlobs(
            patch_chain.repository_directory,
            replay_directory,
            [
                *reversed(old_upstream_preimage_blob_ids),
                *old_upstream_preimage_blob_ids,
            ],
        )
        second_copied_blob_ids = roll_protobuf.CopyPreimageBlobs(
            patch_chain.repository_directory,
            replay_directory,
            old_upstream_preimage_blob_ids,
        )

        expected_copied_blob_ids = tuple(
            sorted(blob_id for chain_patch in patch_chain.patches[:3]
                   for blob_id in chain_patch.preimage_blobs.values()))
        self.assertEqual((expected_copied_blob_ids, ()),
                         (first_copied_blob_ids, second_copied_blob_ids))

    def testApplyChainPatchThreeWayClassifiesCleanEmptyAndConflictingPatches(
            self):
        patch_chain = self._BuildPatchChain()
        replay_directory = self._CreateNewUpstreamCheckout()

        patch_results = []
        for chain_patch in patch_chain.patches[:3]:
            roll_protobuf.CopyPreimageBlobs(
                patch_chain.repository_directory,
                replay_directory,
                chain_patch.preimage_blobs.values(),
            )
            patch_results.append(
                roll_protobuf.ApplyChainPatchThreeWay(replay_directory,
                                                      chain_patch))
            _RunGitCommand(replay_directory,
                           ['reset', '--quiet', '--hard', 'HEAD'])

        self.assertEqual([
            roll_protobuf.PatchReplayResult(
                patch_name=_PLAN_CLEAN_PATCH,
                status=roll_protobuf.PatchReplayStatus.CLEAN,
                changed_line_count_before=4,
                changed_line_count_after=2,
            ),
            roll_protobuf.PatchReplayResult(
                patch_name=_PLAN_EMPTY_PATCH,
                status=roll_protobuf.PatchReplayStatus.EMPTY,
                changed_line_count_before=2,
            ),
            roll_protobuf.PatchReplayResult(
                patch_name=_PLAN_CONFLICT_PATCH,
                status=roll_protobuf.PatchReplayStatus.CONFLICT,
                changed_line_count_before=2,
                conflicted_files=(roll_protobuf.ConflictedFile(
                    path=_PLAN_CONFLICT_PATH, conflict_region_count=1), ),
            ),
        ], patch_results)

    def testApplyChainPatchThreeWayAppliesUnderDirectoryPrefix(self):
        patch_chain = self._BuildPatchChain()
        checkout_directory = self._CreateNewUpstreamCheckout(
            'third_party/protobuf')
        clean_patch = patch_chain.patches[0]
        roll_protobuf.CopyPreimageBlobs(
            patch_chain.repository_directory,
            checkout_directory,
            clean_patch.preimage_blobs.values(),
        )

        patch_result = roll_protobuf.ApplyChainPatchThreeWay(
            checkout_directory,
            clean_patch,
            directory_prefix='./third_party/protobuf/',
        )

        self.assertEqual((roll_protobuf.PatchReplayStatus.CLEAN,
                          f'third_party/protobuf/{_PLAN_CLEAN_PATH}'),
                         (patch_result.status,
                          _RunGitCommand(checkout_directory,
                                         ['diff', '--cached', '--name-only'])))

    def testApplyChainPatchThreeWayNamesUpstreamRelativePathWhenMissing(self):
        patch_chain = self._BuildPatchChain()
        checkout_directory = self._CreateNewUpstreamCheckout(
            'third_party/protobuf')

        patch_result = roll_protobuf.ApplyChainPatchThreeWay(
            checkout_directory,
            patch_chain.patches[3],
            directory_prefix='./third_party/protobuf/',
        )

        self.assertEqual((roll_protobuf.PatchReplayStatus.MISSING_FILE,
                          (_PLAN_RENAMED_PATH, )),
                         (patch_result.status, patch_result.missing_files))

    def testApplyChainPatchThreeWayReportsErrorWithGitDiagnostics(self):
        unparseable_patch = roll_protobuf.ChainPatch(
            patch_name='0009-unparseable.patch',
            diff=b'this is not a diff\n',
            preimage_blobs={},
            changed_line_count=0,
        )
        replay_directory = self._CreateNewUpstreamCheckout()

        patch_result = roll_protobuf.ApplyChainPatchThreeWay(
            replay_directory, unparseable_patch)

        self.assertEqual((roll_protobuf.PatchReplayStatus.ERROR, True),
                         (patch_result.status, 'No valid patches in input'
                          in (patch_result.error_message or '')))

    def _PlanRoll(self,
                  to_revision: str | None = None) -> roll_protobuf.RollPlan:
        return roll_protobuf.PlanRoll(
            to_revision=to_revision or self.fixture.new_commit,
            protobuf_directory=self.fixture.protobuf_directory,
            upstream_repository_path=self.fixture.upstream_directory,
        )

    def testPlanRollReplaysPatchChainOntoNewUpstream(self):
        roll_plan = self._PlanRoll()

        self.assertEqual(3, roll_plan.copied_blob_count)
        self.assertEqual(
            (
                roll_protobuf.PatchReplayResult(
                    patch_name=_PLAN_CLEAN_PATCH,
                    status=roll_protobuf.PatchReplayStatus.CLEAN,
                    changed_line_count_before=4,
                    changed_line_count_after=2,
                ),
                roll_protobuf.PatchReplayResult(
                    patch_name=_PLAN_EMPTY_PATCH,
                    status=roll_protobuf.PatchReplayStatus.EMPTY,
                    changed_line_count_before=2,
                ),
                roll_protobuf.PatchReplayResult(
                    patch_name=_PLAN_CONFLICT_PATCH,
                    status=roll_protobuf.PatchReplayStatus.CONFLICT,
                    changed_line_count_before=2,
                    conflicted_files=(roll_protobuf.ConflictedFile(
                        path=_PLAN_CONFLICT_PATH, conflict_region_count=1), ),
                ),
                roll_protobuf.PatchReplayResult(
                    patch_name=_PLAN_MISSING_FILE_PATCH,
                    status=roll_protobuf.PatchReplayStatus.MISSING_FILE,
                    changed_line_count_before=2,
                    missing_files=(_PLAN_RENAMED_PATH, ),
                ),
                roll_protobuf.PatchReplayResult(
                    patch_name=_PLAN_STACKED_PATCH,
                    status=roll_protobuf.PatchReplayStatus.CLEAN,
                    changed_line_count_before=3,
                    changed_line_count_after=3,
                ),
            ),
            roll_plan.patch_results,
        )

    def testPlanRollReportsUpstreamFileAddedToFileLists(self):
        roll_plan = self._PlanRoll()

        self.assertEqual(
            {
                'added_files': [{
                    'path': _PLAN_ADDED_PATH,
                    'upstream_lists': ['libprotobuf_srcs'],
                    'chromium_sibling_count': 3,
                }],
                'removed_files': [_PLAN_RENAMED_PATH],
            }, roll_plan.file_lists_diff.ToDictionary())

    def testPlanRollSkipsFileListsReportWhenNewUpstreamLacksFileListsCmake(
            self):
        _RunGitCommand(self.fixture.upstream_directory,
                       ['rm', '--quiet', 'src/file_lists.cmake'])
        _RunGitCommand(self.fixture.upstream_directory,
                       ['commit', '--quiet', '-m', 'Drop file_lists.cmake'])
        to_revision = _RunGitCommand(self.fixture.upstream_directory,
                                     ['rev-parse', 'HEAD'])

        roll_plan = self._PlanRoll(to_revision)

        self.assertIsNone(roll_plan.file_lists_diff)

    def testPlanRollLeavesProtobufDirectoryAndUpstreamRepositoryUntouched(
            self):

        def SnapshotRepository(repository_directory: str) -> tuple[str, str]:
            return (
                _RunGitCommand(repository_directory,
                               ['status', '--porcelain', '-uall']),
                _RunGitCommand(repository_directory, ['for-each-ref']),
            )

        protobuf_before = SnapshotRepository(self.fixture.protobuf_directory)
        upstream_before = SnapshotRepository(self.fixture.upstream_directory)

        self._PlanRoll()

        self.assertEqual((protobuf_before, upstream_before),
                         (SnapshotRepository(self.fixture.protobuf_directory),
                          SnapshotRepository(self.fixture.upstream_directory)))

    @_SkipOnWindows('Git hooks need an executable shell script.')
    def testPlanRollIgnoresDeveloperGlobalGitConfiguration(self):
        hooks_directory = os.path.join(self.temporary_directory, 'hooks')
        for hook_name in ('pre-commit', 'commit-msg', 'pre-applypatch'):
            hook_path = os.path.join(hooks_directory, hook_name)
            _WriteFilesToDirectory(hooks_directory,
                                   {hook_name: '#!/bin/sh\nexit 1\n'})
            os.chmod(hook_path, 0o755)
        global_config_path = os.path.join(self.temporary_directory,
                                          'gitconfig')
        _WriteFilesToDirectory(
            self.temporary_directory,
            {
                'gitconfig': ('[core]\n'
                              f'\thooksPath = {hooks_directory}\n'
                              '[commit]\n\tgpgsign = true\n'
                              '[diff]\n\tnoprefix = true\n'
                              '[merge]\n\tconflictStyle = diff3\n'
                              '[apply]\n\twhitespace = error-all\n'),
            },
        )
        baseline_plan = self._PlanRoll()

        with mock.patch.dict(
                os.environ, {
                    'GIT_CONFIG_GLOBAL': global_config_path,
                    'GIT_CONFIG_COUNT': '1',
                    'GIT_CONFIG_KEY_0': 'core.autocrlf',
                    'GIT_CONFIG_VALUE_0': 'true',
                    'GIT_CONFIG_PARAMETERS': "'diff.noprefix'='true'",
                }):
            configured_plan = self._PlanRoll()

        self.assertEqual(baseline_plan, configured_plan)

    def testFormatRollPlanReportDescribesEachPatchOnItsOwnLine(self):
        report_lines = roll_protobuf.FormatRollPlanReport(
            self._PlanRoll()).splitlines()

        self.assertEqual([
            f'  CLEAN         {_PLAN_CLEAN_PATCH}: 4 -> 2 changed lines',
            f'  EMPTY         {_PLAN_EMPTY_PATCH}: already upstream',
            f'  CONFLICT      {_PLAN_CONFLICT_PATCH}: '
            f'{_PLAN_CONFLICT_PATH} (1 region(s))',
            f'  MISSING_FILE  {_PLAN_MISSING_FILE_PATCH}: not in new '
            f'upstream (renamed or deleted?): {_PLAN_RENAMED_PATH}',
            f'  CLEAN         {_PLAN_STACKED_PATCH}: 3 -> 3 changed lines',
        ], report_lines[2:7])

    def testFormatRollPlanReportFlagsConflictWithoutMarkersAsBinary(self):
        roll_plan = roll_protobuf.RollPlan(
            old_revision='v1',
            old_commit='1' * 40,
            new_revision='v2',
            new_commit='2' * 40,
            patch_results=(roll_protobuf.PatchReplayResult(
                patch_name='0001-binary.patch',
                status=roll_protobuf.PatchReplayStatus.CONFLICT,
                changed_line_count_before=0,
                conflicted_files=(roll_protobuf.ConflictedFile(
                    path='image.png', conflict_region_count=0), ),
            ), ),
            copied_blob_count=0,
            file_lists_diff=None,
        )

        self.assertIn(
            '0001-binary.patch: image.png (no conflict markers, binary?)',
            roll_protobuf.FormatRollPlanReport(roll_plan))

    def testFormatRollPlanReportSaysWhyFileListReportIsMissing(self):
        roll_plan = roll_protobuf.RollPlan(
            old_revision='v1',
            old_commit='1' * 40,
            new_revision='v2',
            new_commit='2' * 40,
            patch_results=(),
            copied_blob_count=0,
            file_lists_diff=None,
        )

        self.assertIn(
            'No patches; copied 0 preimage blob(s).\n\n'
            'Skipped the upstream file-list report: src/file_lists.cmake is '
            'missing from the old or new version.',
            roll_protobuf.FormatRollPlanReport(roll_plan))

    def testMainPlanPrintsStatusCountsAndCopiedBlobCount(self):
        exit_code, stdout, _ = _RunMain(
            'plan',
            '--to',
            self.fixture.new_commit,
            '--protobuf-dir',
            self.fixture.protobuf_directory,
            '--upstream-repo',
            self.fixture.upstream_directory,
        )

        self.assertEqual(roll_protobuf.EXIT_CODE_SUCCESS, exit_code)
        self.assertIn(
            '2 CLEAN, 1 EMPTY, 1 CONFLICT, 1 MISSING_FILE; copied 3 '
            'preimage blob(s).\n\n'
            'Upstream files added in src/file_lists.cmake',
            stdout,
        )

    def testMainPlanOutputsPatchCountsAsJson(self):
        _, stdout, _ = _RunMain(
            'plan',
            '--to',
            self.fixture.new_commit,
            '--protobuf-dir',
            self.fixture.protobuf_directory,
            '--upstream-repo',
            self.fixture.upstream_directory,
            '--json',
        )

        self.assertEqual(
            {
                'CLEAN': 2,
                'EMPTY': 1,
                'CONFLICT': 1,
                'MISSING_FILE': 1,
            },
            json.loads(stdout)['patch_counts'],
        )

    def testMainPlanExitsOneAndNamesRevisionWhenTargetIsUnknown(self):
        exit_code, _, stderr = _RunMain(
            'plan',
            '--to',
            'v999.0',
            '--protobuf-dir',
            self.fixture.protobuf_directory,
            '--upstream-repo',
            self.fixture.upstream_directory,
        )

        self.assertEqual(roll_protobuf.EXIT_CODE_ERROR, exit_code)
        self.assertIn('Revision v999.0 not found', stderr)


def _WriteFakeBootstrapGeneratorScript(protobuf_directory: str,
                                       *,
                                       omit_path: str | None = None,
                                       exit_code: int = 0) -> None:
    """Writes a hermetic gen_extra_chromium_files.py into protobuf_directory."""
    outputs = sorted(p for p in roll_protobuf.BOOTSTRAP_GENERATED_FILES
                     if p != omit_path)
    script = (
        'import argparse, os, pathlib, shutil, sys\n'
        'parser = argparse.ArgumentParser()\n'
        'parser.add_argument("-C", "--output-directory", required=True)\n'
        'args = parser.parse_args()\n'
        'root = os.path.dirname(os.path.abspath(__file__))\n'
        'shutil.rmtree(os.path.join(root, "compatibility"), ignore_errors=True)\n'
        f'if {exit_code}:\n'
        '    print("protoc build boom", file=sys.stderr)\n'
        f'    sys.exit({exit_code})\n'
        f'for rel in {outputs!r}:\n'
        '    target = pathlib.Path(root, *rel.split("/"))\n'
        '    target.parent.mkdir(parents=True, exist_ok=True)\n'
        '    target.write_text(\n'
        '        f"# generated via {os.path.basename(args.output_directory)}\\n",\n'
        '        encoding="utf-8", newline="\\n")\n')
    _WriteFilesToDirectory(protobuf_directory,
                           {'gen_extra_chromium_files.py': script})


def _WriteFakeCompileSizeProbeScript(protobuf_directory: str,
                                     *,
                                     raw_stdout: str | None = None,
                                     stderr_only_whitespace: bool = False,
                                     exit_code: int = 0) -> None:
    """Writes a hermetic compile_size_probe.py into protobuf_directory."""
    script = (
        'import argparse, json, sys\n'
        'parser = argparse.ArgumentParser()\n'
        'parser.add_argument("-C", required=True)\n'
        'parser.add_argument("--compare", required=True)\n'
        'parser.add_argument("--json", action="store_true")\n'
        'args = parser.parse_args()\n'
        f'if {exit_code}:\n'
        f'    if {stderr_only_whitespace!r}:\n'
        '        sys.stderr.write("   \\n")\n'
        '        print(f"stdout error for {args.compare}")\n'
        '    else:\n'
        '        print(f"probe failed for {args.compare}", file=sys.stderr)\n'
        f'    sys.exit({exit_code})\n'
        f'if {raw_stdout is not None!r}:\n'
        f'    print({raw_stdout or ""!r})\n'
        '    sys.exit(0)\n'
        'print(json.dumps({\n'
        '    "baseline_name": args.compare,\n'
        '    "before_total_bytes": 1000,\n'
        '    "after_total_bytes": 1250,\n'
        '    "delta_total_bytes": 250,\n'
        '    "before_total_symbol_bytes": 400,\n'
        '    "after_total_symbol_bytes": 440,\n'
        '    "delta_total_symbol_bytes": 40,\n'
        '    "translation_units": [{\n'
        '        "translation_unit": "gen/duration.pb.cc",\n'
        '        "before_bytes": 1000,\n'
        '        "after_bytes": 1250,\n'
        '        "delta_bytes": 250,\n'
        '        "added_headers": [{\n'
        '            "path": "absl/container/btree_map.h",\n'
        '            "size_bytes": 200,\n'
        '            "include_chain": ["gen/duration.pb.cc", "map.h",\n'
        '                              "absl/container/btree_map.h"],\n'
        '        }],\n'
        '        "removed_headers": [{"path": "old.h", "size_bytes": 10}],\n'
        '        "resized_files": [{"path": "arena.h", "delta_bytes": 60}],\n'
        '    }],\n'
        '}))')
    _WriteFilesToDirectory(protobuf_directory,
                           {'compile_size_probe.py': script + '\n'})


class RollProtobufStartTest(unittest.TestCase):
    """Proves checkout-modifying roll commands (prepare, start, status, abort)."""

    def setUp(self):
        super().setUp()
        self.enterContext(
            mock.patch.dict(
                os.environ,
                {
                    'GIT_CONFIG_GLOBAL': os.devnull,
                    'GIT_CONFIG_NOSYSTEM': '1',
                },
            ))
        self.temporary_directory = self.enterContext(
            tempfile.TemporaryDirectory())
        seed = _CreatePlanFixture(os.path.join(self.temporary_directory, 's'))
        _WriteFilesToDirectory(
            seed.upstream_directory,
            {
                '.gitignore': '*.Swp\n',
                'version.json': '{"37.x": {"protoc_version": "37.0"}}\n',
            },
        )
        _RunGitCommand(seed.upstream_directory, ['add', '-A', '.'])
        _RunGitCommand(seed.upstream_directory,
                       ['commit', '--quiet', '--amend', '--no-edit'])
        new_commit = _RunGitCommand(seed.upstream_directory,
                                    ['rev-parse', 'HEAD'])
        _RunGitCommand(seed.upstream_directory, ['tag', 'v37.0', new_commit])
        self.repository_root = os.path.join(self.temporary_directory, 'repo')
        protobuf_directory = os.path.join(self.repository_root, 'third_party',
                                          'protobuf')
        shutil.copytree(seed.protobuf_directory,
                        protobuf_directory,
                        ignore=shutil.ignore_patterns('.git'))
        _RunGitCommand(self.repository_root, ['init', '--quiet'])
        _WriteFilesToDirectory(
            self.repository_root,
            {
                '.gitignore':
                '*.targets\n*.orig\n*.rej\n',
                'OTHER_CHROMIUM_FILE':
                'outside third_party/protobuf\n',
                'third_party/protobuf/src/file_lists.cmake':
                _PLAN_OLD_UPSTREAM_FILES['src/file_lists.cmake'],
                'third_party/protobuf/README.chromium':
                ('Name: Protocol Buffers\n'
                 'Version: 36.0\n'
                 'CPEPrefix: cpe:/a:google:protobuf:36.0\n'
                 f'Revision: {seed.old_commit}\n'),
            },
        )
        _RunGitCommand(self.repository_root, ['add', '-f', '.'])
        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '-m', 'Initial checkout'])
        self.fixture = dataclasses.replace(
            seed,
            protobuf_directory=protobuf_directory,
            new_commit=new_commit,
        )

    def testPrepareRollWritesChainPatchesPreimageBlobsAndStateJson(self):
        start_head = _RunGitCommand(self.repository_root,
                                    ['rev-parse', 'HEAD'])

        state = roll_protobuf.PrepareRoll(
            to_revision='v37.0',
            protobuf_directory=self.fixture.protobuf_directory,
            upstream_repository_path=self.fixture.upstream_directory,
        )
        second_state = roll_protobuf.PrepareRoll(
            to_revision='v37.0',
            protobuf_directory=self.fixture.protobuf_directory,
            upstream_repository_path=self.fixture.upstream_directory,
        )
        loaded_state = roll_protobuf.LoadRollState(
            self.fixture.protobuf_directory)

        self.assertEqual((state, state), (second_state, loaded_state))
        self.assertEqual(
            ('prepared', start_head, 'v37.0', self.fixture.new_commit, 5, 5),
            (
                state.phase,
                state.start_commit,
                state.new_revision,
                state.new_commit,
                len(state.patches),
                state.copied_blob_count,
            ),
        )
        for chain_patch in state.patches:
            for blob_id in chain_patch.preimage_blobs.values():
                self.assertEqual(
                    'blob',
                    _RunGitCommand(self.repository_root,
                                   ['cat-file', '-t', blob_id]),
                )

    def testPrepareRollRejectsActiveRollAndLoadRollStateRoundTripsResults(
            self):
        state = roll_protobuf.PrepareRoll(
            to_revision='v37.0',
            protobuf_directory=self.fixture.protobuf_directory,
            upstream_repository_path=self.fixture.upstream_directory,
        )
        conflict_state = dataclasses.replace(
            state,
            phase='conflict',
            current_index=2,
            conflict_files=(f'third_party/protobuf/{_PLAN_CONFLICT_PATH}', ),
            patch_results=(roll_protobuf.PatchReplayResult(
                patch_name=_PLAN_CONFLICT_PATCH,
                status=roll_protobuf.PatchReplayStatus.CONFLICT,
                changed_line_count_before=2,
                conflicted_files=(roll_protobuf.ConflictedFile(
                    path=f'third_party/protobuf/{_PLAN_CONFLICT_PATH}',
                    conflict_region_count=1,
                ), ),
            ), ),
        )
        checkout = roll_protobuf._ResolveCheckoutPaths(
            self.fixture.protobuf_directory)
        roll_protobuf._SaveRollState(checkout, conflict_state)

        self.assertEqual(
            conflict_state,
            roll_protobuf.LoadRollState(self.fixture.protobuf_directory))
        with self.assertRaisesRegex(
                roll_protobuf.RollProtobufError,
                r'already in progress \(phase: conflict\)'):
            roll_protobuf.PrepareRoll(
                to_revision='v37.0',
                protobuf_directory=self.fixture.protobuf_directory,
                upstream_repository_path=self.fixture.upstream_directory,
            )
        os.remove(os.path.join(checkout.chain_directory, _PLAN_CLEAN_PATCH))
        with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                    r'Corrupt roll state'):
            roll_protobuf.LoadRollState(self.fixture.protobuf_directory)

    def testLoadRollStateReturnsNoneWhenNoRollIsInProgress(self):
        self.assertIsNone(
            roll_protobuf.LoadRollState(self.fixture.protobuf_directory))
        with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                    r'is not inside a git working tree'):
            roll_protobuf.LoadRollState(self.temporary_directory)

    def testLoadRollStateRaisesWhenStateJsonIsCorrupt(self):
        checkout = roll_protobuf._ResolveCheckoutPaths(
            self.fixture.protobuf_directory)
        os.makedirs(checkout.roll_state_directory, exist_ok=True)
        pathlib.Path(checkout.state_file_path).write_text('{not valid json',
                                                          encoding='utf-8')

        with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                    r'Corrupt roll state'):
            roll_protobuf.LoadRollState(self.fixture.protobuf_directory)

    def testImportUpstreamReleaseReplacesUpstreamFilesAndUpdatesReadme(self):
        _WriteFilesToDirectory(
            self.fixture.upstream_directory,
            {
                'compatibility/smoke/Test.java': '// deleted during roll\n',
                'src/google/protobuf/any.pb.cc': '// deleted WKT\n',
                'csharp/Google.Protobuf.targets': '<Project/>\n',
                'version.json': '{"37.x": {"protoc_version": "37.0-dev"}}\n',
            },
        )
        _RunGitCommand(self.fixture.upstream_directory, ['add', '-A', '.'])
        _RunGitCommand(self.fixture.upstream_directory,
                       ['commit', '--quiet', '--amend', '--no-edit'])
        new_commit = _RunGitCommand(self.fixture.upstream_directory,
                                    ['rev-parse', 'HEAD'])
        _WriteFilesToDirectory(
            self.fixture.protobuf_directory,
            {
                'patches/0001-clean.md': '# Notes on 0001\n',
                'patches/protobuf_v25/0001-bazel.patch': '// old subdir\n',
                'python/google/protobuf/descriptor_pb2.py': '# bootstrap\n',
            },
        )

        import_commit = roll_protobuf.ImportUpstreamRelease(
            protobuf_directory=self.fixture.protobuf_directory,
            upstream_repository_path=self.fixture.upstream_directory,
            to_revision='v37.0',
            new_commit=new_commit,
        )

        present_files = roll_protobuf._ListRegularFilesInDirectory(
            self.fixture.protobuf_directory)
        readme_text = pathlib.Path(
            self.fixture.protobuf_directory,
            'README.chromium').read_text(encoding='utf-8')
        commit_subject = _RunGitCommand(self.repository_root,
                                        ['log', '-1', '--format=%s'])

        self.assertEqual(
            _RunGitCommand(self.repository_root, ['rev-parse', 'HEAD']),
            import_commit)
        self.assertEqual('Import upstream protobuf v37.0 (unmodified)',
                         commit_subject)
        self.assertIn(_PLAN_ADDED_PATH, present_files)
        self.assertIn('patches/0001-clean.patch', present_files)
        self.assertIn('patches/0001-clean.md', present_files)
        self.assertIn('python/google/protobuf/descriptor_pb2.py',
                      present_files)
        self.assertNotIn('compatibility/smoke/Test.java', present_files)
        self.assertNotIn('src/google/protobuf/any.pb.cc', present_files)
        self.assertNotIn('patches/protobuf_v25/0001-bazel.patch',
                         present_files)
        self.assertNotIn('csharp/Google.Protobuf.targets', present_files)
        for removed_directory in ('compatibility', 'csharp',
                                  'patches/protobuf_v25'):
            self.assertFalse(
                os.path.exists(
                    os.path.join(self.fixture.protobuf_directory,
                                 removed_directory)))
        self.assertEqual(
            ('Name: Protocol Buffers\n'
             'Version: 37.0\n'
             'CPEPrefix: cpe:/a:google:protobuf:37.0\n'
             f'Revision: {new_commit}\n'),
            readme_text,
        )
        self.assertTrue(
            os.path.isfile(
                os.path.join(self.repository_root, 'OTHER_CHROMIUM_FILE')))

    def testImportUpstreamReleaseReadsProtocVersionFromVersionJsonForCommitSha(
            self):
        roll_protobuf.ImportUpstreamRelease(
            protobuf_directory=self.fixture.protobuf_directory,
            upstream_repository_path=self.fixture.upstream_directory,
            to_revision=self.fixture.new_commit,
            new_commit=self.fixture.new_commit,
        )

        readme_text = pathlib.Path(
            self.fixture.protobuf_directory,
            'README.chromium').read_text(encoding='utf-8')

        self.assertIn('Version: 37.0\nCPEPrefix: cpe:/a:google:protobuf:37.0',
                      readme_text)

    def testStartRollRefusesWhenWorkingTreeHasUncommittedChanges(self):
        _WriteFilesToDirectory(self.repository_root,
                               {'OTHER_CHROMIUM_FILE': 'dirty edit\n'})

        with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                    r'Working tree has uncommitted changes'):
            roll_protobuf.StartRoll(
                to_revision='v37.0',
                protobuf_directory=self.fixture.protobuf_directory,
                upstream_repository_path=self.fixture.upstream_directory,
            )

        self.assertIsNone(
            roll_protobuf.LoadRollState(self.fixture.protobuf_directory))

    def testStartRollCommitsCleanAndEmptyPatchesAndStopsAtFirstConflict(self):
        state = roll_protobuf.StartRoll(
            to_revision='v37.0',
            protobuf_directory=self.fixture.protobuf_directory,
            upstream_repository_path=self.fixture.upstream_directory,
        )

        recent_subjects = _RunGitCommand(
            self.repository_root, ['log', '-3', '--format=%s']).splitlines()
        conflicted_text = pathlib.Path(
            self.fixture.protobuf_directory,
            *_PLAN_CONFLICT_PATH.split('/')).read_text(encoding='utf-8')

        self.assertEqual(('conflict', 2), (state.phase, state.current_index))
        self.assertEqual(
            [
                f'Drop patches/{_PLAN_EMPTY_PATCH} (already upstream)',
                f'Apply patches/{_PLAN_CLEAN_PATCH}',
                'Import upstream protobuf v37.0 (unmodified)',
            ],
            recent_subjects,
        )
        self.assertFalse(
            os.path.exists(
                os.path.join(self.fixture.protobuf_directory, 'patches',
                             _PLAN_EMPTY_PATCH)))
        self.assertIn('<<<<<<< ', conflicted_text)

    def testStartRollRefreshedPatchReverseAppliesToItsOwnCommit(self):
        for patch_name in (_PLAN_CONFLICT_PATCH, _PLAN_MISSING_FILE_PATCH):
            os.remove(
                os.path.join(self.fixture.protobuf_directory, 'patches',
                             patch_name))
        _RunGitCommand(self.repository_root, ['add', '-A', '.'])
        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '-m', 'Keep clean/empty patches'])

        state = roll_protobuf.StartRoll(
            to_revision='v37.0',
            protobuf_directory=self.fixture.protobuf_directory,
            upstream_repository_path=self.fixture.upstream_directory,
        )

        self.assertEqual('replayed', state.phase)
        refreshed_patch_path = os.path.join(self.fixture.protobuf_directory,
                                            'patches', _PLAN_CLEAN_PATCH)
        _RunGitCommand(self.repository_root, ['checkout', '--quiet', 'HEAD~1'])
        _RunGitCommand(
            self.repository_root,
            [
                'apply', '--check', '-R', '--directory=third_party/protobuf',
                refreshed_patch_path
            ],
        )

    def testGetRollStatusReportsIdlePreparedConflictAndReplayedStates(self):
        idle_status = roll_protobuf.GetRollStatus(
            self.fixture.protobuf_directory)
        prepared_state = roll_protobuf.PrepareRoll(
            to_revision='v37.0',
            protobuf_directory=self.fixture.protobuf_directory,
            upstream_repository_path=self.fixture.upstream_directory,
        )
        prepared_status = roll_protobuf.GetRollStatus(
            self.fixture.protobuf_directory)
        conflict_state = roll_protobuf.StartRoll(
            to_revision='v37.0',
            protobuf_directory=self.fixture.protobuf_directory,
            upstream_repository_path=self.fixture.upstream_directory,
        )
        conflict_status = roll_protobuf.GetRollStatus(
            self.fixture.protobuf_directory)
        roll_protobuf._SaveRollState(
            roll_protobuf._ResolveCheckoutPaths(
                self.fixture.protobuf_directory),
            dataclasses.replace(conflict_state, phase='replayed'))
        replayed_status = roll_protobuf.GetRollStatus(
            self.fixture.protobuf_directory)

        self.assertEqual(
            ('idle', 'prepared', 'conflict', 'replayed', 5),
            (idle_status.state, prepared_status.state, conflict_status.state,
             replayed_status.state, replayed_status.index))
        self.assertEqual(
            {
                'state':
                'conflict',
                'index':
                3,
                'total':
                5,
                'patch':
                _PLAN_CONFLICT_PATCH,
                'files': [f'third_party/protobuf/{_PLAN_CONFLICT_PATH}'],
                'next': [
                    'roll_protobuf.py continue',
                    'roll_protobuf.py drop --reason <why>',
                    'roll_protobuf.py abort',
                ],
            },
            conflict_status.ToDictionary(),
        )
        self.assertIn('Prepared roll',
                      roll_protobuf.FormatRollStateReport(prepared_state))
        self.assertIn(f'Stopped at [3/5] patches/{_PLAN_CONFLICT_PATCH}.',
                      roll_protobuf.FormatRollStateReport(conflict_state))
        self.assertIn(f'  third_party/protobuf/{_PLAN_CONFLICT_PATH}\nNext: ',
                      roll_protobuf.FormatRollStatusReport(conflict_status))

    def testAbortRollRestoresExactStartingCommitAndDeletesRollState(self):
        _WriteFilesToDirectory(self.repository_root,
                               {'.gitignore': '*.targets\n*.orig\n'})
        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '-am', 'Ignore .orig'])
        start_commit = _RunGitCommand(self.repository_root,
                                      ['rev-parse', 'HEAD'])
        roll_protobuf.StartRoll(
            to_revision='v37.0',
            protobuf_directory=self.fixture.protobuf_directory,
            upstream_repository_path=self.fixture.upstream_directory,
        )
        _WriteFilesToDirectory(
            self.fixture.protobuf_directory,
            {
                'src/untracked.tmp': 'scratch\n',
                'src/untracked_dir/untracked.orig': 'leftover\n',
            },
        )

        roll_protobuf.AbortRoll(self.fixture.protobuf_directory)

        self.assertEqual(
            (start_commit, '', None),
            (
                _RunGitCommand(self.repository_root, ['rev-parse', 'HEAD']),
                _RunGitCommand(self.repository_root,
                               ['status', '--porcelain', '-uall']),
                roll_protobuf.LoadRollState(self.fixture.protobuf_directory),
            ),
        )
        self.assertFalse(
            os.path.exists(
                os.path.join(self.fixture.protobuf_directory, 'src',
                             'untracked_dir')))

    def testAbortRollRaisesWhenNoRollIsInProgress(self):
        with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                    r'No protobuf roll is in progress'):
            roll_protobuf.AbortRoll(self.fixture.protobuf_directory)

    def testMainStartExitsTwoOnConflictAndStatusAndAbortCleanUp(self):
        start_commit = _RunGitCommand(self.repository_root,
                                      ['rev-parse', 'HEAD'])

        start_code, start_stdout, _ = _RunMain(
            'start',
            '--to',
            'v37.0',
            '--protobuf-dir',
            self.fixture.protobuf_directory,
            '--upstream-repo',
            self.fixture.upstream_directory,
        )
        status_code, status_stdout, _ = _RunMain(
            'status',
            '--protobuf-dir',
            self.fixture.protobuf_directory,
            '--json',
        )
        abort_code, abort_stdout, _ = _RunMain(
            'abort',
            '--protobuf-dir',
            self.fixture.protobuf_directory,
        )

        self.assertEqual(
            (
                roll_protobuf.EXIT_CODE_CONFLICT,
                roll_protobuf.EXIT_CODE_SUCCESS,
                roll_protobuf.EXIT_CODE_SUCCESS,
            ),
            (start_code, status_code, abort_code),
        )
        self.assertIn('Doing it by hand', start_stdout)
        self.assertEqual('conflict', json.loads(status_stdout)['state'])
        self.assertEqual(
            f'Aborted roll to v37.0 and reset to {start_commit}.',
            abort_stdout,
        )

    def testMainPrepareAndCleanStartExitZero(self):
        for patch_name in (_PLAN_CONFLICT_PATCH, _PLAN_MISSING_FILE_PATCH):
            os.remove(
                os.path.join(self.fixture.protobuf_directory, 'patches',
                             patch_name))
        _RunGitCommand(self.repository_root, ['add', '-A', '.'])
        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '-m', 'Drop conflicting patches'])

        prepare_code, prepare_stdout, _ = _RunMain(
            'prepare',
            '--to',
            'v37.0',
            '--protobuf-dir',
            self.fixture.protobuf_directory,
            '--upstream-repo',
            self.fixture.upstream_directory,
        )
        start_code, start_stdout, _ = _RunMain(
            'start',
            '--to',
            'v37.0',
            '--protobuf-dir',
            self.fixture.protobuf_directory,
            '--upstream-repo',
            self.fixture.upstream_directory,
        )
        status_code, status_stdout, _ = _RunMain(
            'status',
            '--protobuf-dir',
            self.fixture.protobuf_directory,
        )

        self.assertEqual(
            (
                roll_protobuf.EXIT_CODE_SUCCESS,
                roll_protobuf.EXIT_CODE_SUCCESS,
                roll_protobuf.EXIT_CODE_SUCCESS,
            ),
            (prepare_code, start_code, status_code),
        )
        self.assertIn('Prepared roll', prepare_stdout)
        self.assertIn('Replayed all 3 patch(es).', start_stdout)
        self.assertIn('State: replayed (3/3 patch(es)).', status_stdout)

    def testContinueRollCommitsResolvedConflictRefreshesPatchAndAdvances(self):
        roll_protobuf.StartRoll(
            to_revision='v37.0',
            protobuf_directory=self.fixture.protobuf_directory,
            upstream_repository_path=self.fixture.upstream_directory,
        )
        _WriteFilesToDirectory(
            self.fixture.protobuf_directory,
            {
                _PLAN_CONFLICT_PATH:
                _NumberedLines('conflict', line6='conflict line 6 (resolved)')
            },
        )

        state = roll_protobuf.ContinueRoll(self.fixture.protobuf_directory)
        refreshed_patch = pathlib.Path(
            self.fixture.protobuf_directory, 'patches',
            _PLAN_CONFLICT_PATCH).read_text(encoding='utf-8')

        self.assertEqual(('conflict', 3), (state.phase, state.current_index))
        self.assertEqual(
            roll_protobuf.PatchReplayResult(
                patch_name=_PLAN_CONFLICT_PATCH,
                status=roll_protobuf.PatchReplayStatus.CLEAN,
                changed_line_count_before=2,
                changed_line_count_after=2,
            ),
            state.patch_results[2],
        )
        self.assertIn('+conflict line 6 (resolved)', refreshed_patch)
        self.assertEqual(
            f'Apply patches/{_PLAN_CONFLICT_PATCH}',
            _RunGitCommand(self.repository_root, ['log', '-1', '--format=%s']),
        )

    def testContinueRollHandlesBinaryConflictsSpacedAddedPathsAndSetextHeadings(
            self):
        spaced_path = 'src/added space_\u00e9.cc'
        binary_path = 'src/blob.bin'
        # Add binary_path to both old and new upstream so a binary patch hunk
        # conflicts on v37.0 without writing text conflict markers.
        _RunGitCommand(self.fixture.upstream_directory,
                       ['checkout', '--quiet', self.fixture.old_commit])
        pathlib.Path(self.fixture.upstream_directory,
                     *binary_path.split('/')).write_bytes(b'\x00\x01old\xff')
        _RunGitCommand(self.fixture.upstream_directory, ['add', binary_path])
        _RunGitCommand(self.fixture.upstream_directory,
                       ['commit', '--quiet', '--amend', '--no-edit'])
        old_commit = _RunGitCommand(self.fixture.upstream_directory,
                                    ['rev-parse', 'HEAD'])

        # Generate a binary + spaced-added-path + text-conflict patch on old_commit.
        _WriteFilesToDirectory(
            self.fixture.upstream_directory,
            {
                _PLAN_CONFLICT_PATH:
                _NumberedLines('conflict', line6='conflict line 6 (chromium)'),
                spaced_path:
                '// added file with space and UTF-8\n',
            },
        )
        pathlib.Path(
            self.fixture.upstream_directory,
            *binary_path.split('/')).write_bytes(b'\x00\x01patched\xff')
        _RunGitCommand(self.fixture.upstream_directory,
                       ['add', '-f', '-A', '.'])
        _RunGitCommand(self.fixture.upstream_directory,
                       ['commit', '--quiet', '-m', 'Patch candidate'])
        patch_bytes = roll_protobuf._RunScratchGit(
            self.fixture.upstream_directory, 'diff', '--binary',
            '--no-renames', 'HEAD~1', 'HEAD').stdout

        _RunGitCommand(self.fixture.upstream_directory,
                       ['checkout', '--quiet', self.fixture.new_commit])
        pathlib.Path(self.fixture.upstream_directory,
                     *binary_path.split('/')).write_bytes(b'\x00\x01new\xff')
        _RunGitCommand(self.fixture.upstream_directory, ['add', binary_path])
        _RunGitCommand(self.fixture.upstream_directory,
                       ['commit', '--quiet', '--amend', '--no-edit'])
        new_commit = _RunGitCommand(self.fixture.upstream_directory,
                                    ['rev-parse', 'HEAD'])
        _RunGitCommand(self.fixture.upstream_directory,
                       ['tag', '-f', 'v37.0', new_commit])

        pathlib.Path(self.fixture.protobuf_directory, 'patches',
                     _PLAN_CONFLICT_PATCH).write_bytes(patch_bytes)
        _WriteFilesToDirectory(
            self.fixture.protobuf_directory,
            {
                'README.chromium': ('Name: Protocol Buffers\n'
                                    'Version: 36.0\n'
                                    'CPEPrefix: cpe:/a:google:protobuf:36.0\n'
                                    f'Revision: {old_commit}\n'),
            },
        )
        _RunGitCommand(self.repository_root, ['add', '-A', '.'])
        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '-m', 'Update conflict fixture'])

        roll_protobuf.StartRoll(
            to_revision='v37.0',
            protobuf_directory=self.fixture.protobuf_directory,
            upstream_repository_path=self.fixture.upstream_directory,
        )
        _WriteFilesToDirectory(
            self.fixture.protobuf_directory,
            {_PLAN_CONFLICT_PATH: '||||||| base\nresolved\n'},
        )
        with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                    r'Unresolved conflict markers remain'):
            roll_protobuf.ContinueRoll(self.fixture.protobuf_directory)

        # Markdown Setext H1 (`=======`) must not false-positive as a marker,
        # while the untouched unmerged binary file must still block continue.
        _WriteFilesToDirectory(
            self.fixture.protobuf_directory,
            {_PLAN_CONFLICT_PATH: 'Heading\n=======\nresolved\n'},
        )
        with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                    r'Unresolved unmerged files remain'):
            roll_protobuf.ContinueRoll(self.fixture.protobuf_directory)

        pathlib.Path(
            self.fixture.protobuf_directory,
            *binary_path.split('/')).write_bytes(b'\x00\x01merged\xff')
        target_protobuf_dir = self.fixture.protobuf_directory
        if sys.platform != 'win32':
            target_protobuf_dir = os.path.join(self.temporary_directory,
                                               'protobuf_link')
            os.symlink(self.fixture.protobuf_directory, target_protobuf_dir)

        state = roll_protobuf.ContinueRoll(target_protobuf_dir)
        refreshed_patch = pathlib.Path(self.fixture.protobuf_directory,
                                       'patches',
                                       _PLAN_CONFLICT_PATCH).read_bytes()

        self.assertEqual(('conflict', 3), (state.phase, state.current_index))
        self.assertTrue(
            os.path.isfile(
                os.path.join(self.fixture.protobuf_directory,
                             *spaced_path.split('/'))))
        self.assertIn(b'GIT binary patch', refreshed_patch)
        roll_protobuf._RunScratchGit(
            self.repository_root,
            'apply',
            '--check',
            '-R',
            '--unidiff-zero',
            '--directory=third_party/protobuf',
            '-',
            input_bytes=refreshed_patch,
        )

    def testContinueRollRefusesMarkersRejectFilesUnexpectedHeadAndExtraFiles(
            self):

        def AssertContinueRaises(pattern: str) -> None:
            with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                        pattern):
                roll_protobuf.ContinueRoll(self.fixture.protobuf_directory)

        AssertContinueRaises(r'No conflicting protobuf patch is waiting')
        roll_protobuf.StartRoll(
            to_revision='v37.0',
            protobuf_directory=self.fixture.protobuf_directory,
            upstream_repository_path=self.fixture.upstream_directory,
        )
        AssertContinueRaises(r'Unresolved conflict markers remain')
        _WriteFilesToDirectory(
            self.fixture.protobuf_directory,
            {
                _PLAN_CONFLICT_PATH:
                _PLAN_NEW_UPSTREAM_FILES[_PLAN_CONFLICT_PATH]
            },
        )
        AssertContinueRaises(r'No changes remain')

        _WriteFilesToDirectory(
            self.fixture.protobuf_directory,
            {
                _PLAN_CONFLICT_PATH:
                _NumberedLines('conflict', line6='conflict line 6 (resolved)'),
                'src/leftover.rej':
                '@@ -1 +1 @@\n',
            },
        )
        AssertContinueRaises(r'Remove \.rej and \.orig files')
        os.remove(
            os.path.join(self.fixture.protobuf_directory, 'src',
                         'leftover.rej'))

        _WriteFilesToDirectory(self.repository_root,
                               {'OTHER_CHROMIUM_FILE': 'staged outside\n'})
        _RunGitCommand(self.repository_root, ['add', 'OTHER_CHROMIUM_FILE'])
        AssertContinueRaises(r'Staged changes outside //third_party/protobuf')
        _RunGitCommand(self.repository_root,
                       ['checkout', 'HEAD', '--', 'OTHER_CHROMIUM_FILE'])

        _WriteFilesToDirectory(self.fixture.protobuf_directory,
                               {'BUILD.gn': '# accidental edit\n'})
        with self.assertRaisesRegex(
                roll_protobuf.RollProtobufError,
                r'Chromium-owned or bootstrap-generated files'):
            roll_protobuf.ContinueRoll(self.fixture.protobuf_directory,
                                       allow_extra_files=True)
        _WriteFilesToDirectory(self.fixture.protobuf_directory,
                               {'BUILD.gn': ''})

        _WriteFilesToDirectory(self.fixture.protobuf_directory,
                               {'src/unrelated.cc': '// extra\n'})
        AssertContinueRaises(r'Modified files not touched by')

        state = roll_protobuf.ContinueRoll(self.fixture.protobuf_directory,
                                           allow_extra_files=True)
        self.assertEqual(('conflict', 3), (state.phase, state.current_index))

        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '--allow-empty', '-m', 'Manual'])
        AssertContinueRaises(r'run `roll_protobuf\.py resume`')

    def testDropCurrentPatchDeletesPatchCommitsReasonAndReplaysRemaining(self):
        _WriteFilesToDirectory(self.repository_root,
                               {'.gitignore': '*.targets\n*.orig\n'})
        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '-am', 'Ignore .orig'])
        roll_protobuf.StartRoll(
            to_revision='v37.0',
            protobuf_directory=self.fixture.protobuf_directory,
            upstream_repository_path=self.fixture.upstream_directory,
        )
        for invalid_reason in ('   ', 'line 1\nline 2', 'line 1\rline 2'):
            with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                        r'non-empty --reason'):
                roll_protobuf.DropCurrentPatch(invalid_reason,
                                               self.fixture.protobuf_directory)

        _WriteFilesToDirectory(
            self.fixture.protobuf_directory,
            {
                'src/leftover.rej': '@@ -1 +1 @@\n',
                'src/new_dir/leftover.orig': '// backup\n',
                'src/untracked.tmp': '// scratch\n',
            },
        )
        _WriteFilesToDirectory(self.repository_root,
                               {'OTHER_CHROMIUM_FILE': 'staged outside\n'})
        _RunGitCommand(self.repository_root, ['add', 'OTHER_CHROMIUM_FILE'])
        with self.assertRaisesRegex(
                roll_protobuf.RollProtobufError,
                r'Staged changes outside //third_party/protobuf.*'
                r'OTHER_CHROMIUM_FILE'):
            roll_protobuf.DropCurrentPatch('obsolete in v37',
                                           self.fixture.protobuf_directory)
        self.assertTrue(
            os.path.exists(
                os.path.join(self.fixture.protobuf_directory, 'src',
                             'untracked.tmp')))
        _RunGitCommand(self.repository_root,
                       ['checkout', 'HEAD', '--', 'OTHER_CHROMIUM_FILE'])

        after_first_drop = roll_protobuf.DropCurrentPatch(
            'obsolete in v37', self.fixture.protobuf_directory)
        final_state = roll_protobuf.DropCurrentPatch(
            'file removed upstream', self.fixture.protobuf_directory)
        recent_subjects = _RunGitCommand(
            self.repository_root, ['log', '-3', '--format=%s']).splitlines()

        self.assertEqual(
            ('conflict', 3),
            (after_first_drop.phase, after_first_drop.current_index))
        self.assertEqual(('replayed', 5),
                         (final_state.phase, final_state.current_index))
        self.assertEqual(
            final_state,
            roll_protobuf.LoadRollState(self.fixture.protobuf_directory))
        self.assertEqual(
            [
                f'Apply patches/{_PLAN_STACKED_PATCH}',
                f'Drop patches/{_PLAN_MISSING_FILE_PATCH} (file removed upstream)',
                f'Drop patches/{_PLAN_CONFLICT_PATCH} (obsolete in v37)',
            ],
            recent_subjects,
        )
        self.assertEqual(
            _PLAN_NEW_UPSTREAM_FILES[_PLAN_CONFLICT_PATH],
            pathlib.Path(
                self.fixture.protobuf_directory,
                *_PLAN_CONFLICT_PATH.split('/')).read_text(encoding='utf-8'),
        )
        for cleaned_entry in ('leftover.rej', 'new_dir', 'untracked.tmp'):
            self.assertFalse(
                os.path.exists(
                    os.path.join(self.fixture.protobuf_directory, 'src',
                                 cleaned_entry)))
        for dropped_patch in (_PLAN_CONFLICT_PATCH, _PLAN_MISSING_FILE_PATCH):
            self.assertFalse(
                os.path.exists(
                    os.path.join(self.fixture.protobuf_directory, 'patches',
                                 dropped_patch)))
        with self.assertRaisesRegex(
                roll_protobuf.RollProtobufError,
                r'No conflicting protobuf patch is waiting for '
                r'`roll_protobuf\.py drop`'):
            roll_protobuf.DropCurrentPatch('already replayed',
                                           self.fixture.protobuf_directory)
        self.assertIn('dropped (obsolete in v37)',
                      roll_protobuf.FormatRollStateReport(final_state))

    def testResumeRollReconcilesManualApplyAndDropCommitsAndFinishesRoll(self):
        roll_protobuf.StartRoll(
            to_revision='v37.0',
            protobuf_directory=self.fixture.protobuf_directory,
            upstream_repository_path=self.fixture.upstream_directory,
        )
        _WriteFilesToDirectory(
            self.fixture.protobuf_directory,
            {
                _PLAN_CONFLICT_PATH:
                _NumberedLines('conflict', line6='conflict line 6 (manual)')
            },
        )
        _RunGitCommand(
            self.repository_root,
            ['add', '--', f'third_party/protobuf/{_PLAN_CONFLICT_PATH}'])
        _RunGitCommand(self.repository_root, [
            'commit', '--quiet', '-m', f'Apply patches/{_PLAN_CONFLICT_PATCH}'
        ])
        refreshed = _RunGitCommand(
            self.repository_root,
            ['diff', 'HEAD~1', 'HEAD', '--relative=third_party/protobuf'])
        pathlib.Path(self.fixture.protobuf_directory, 'patches',
                     _PLAN_CONFLICT_PATCH).write_text(refreshed + '\n',
                                                      encoding='utf-8',
                                                      newline='\n')
        _RunGitCommand(
            self.repository_root,
            [
                'commit', '--quiet', '--amend', '--no-edit',
                f'third_party/protobuf/patches/{_PLAN_CONFLICT_PATCH}'
            ],
        )
        _RunGitCommand(
            self.repository_root,
            [
                'rm', '--quiet',
                f'third_party/protobuf/patches/{_PLAN_MISSING_FILE_PATCH}'
            ],
        )
        _RunGitCommand(
            self.repository_root,
            [
                'commit', '--quiet', '-m',
                f'Drop patches/{_PLAN_MISSING_FILE_PATCH} (file removed upstream)'
            ],
        )

        state = roll_protobuf.ResumeRoll(self.fixture.protobuf_directory)

        self.assertEqual(('replayed', 5), (state.phase, state.current_index))
        self.assertEqual(
            [
                ('CLEAN', 2, None),
                ('EMPTY', None, None),
                ('CLEAN', 2, None),
                ('DROPPED', None, 'file removed upstream'),
                ('CLEAN', 3, None),
            ],
            [(
                result.status.value,
                result.changed_line_count_after,
                result.error_message,
            ) for result in state.patch_results],
        )

    def testResumeRollRejectsDirtyWorkingTreeAndUnexpectedCommitSubjects(self):
        roll_protobuf.StartRoll(
            to_revision='v37.0',
            protobuf_directory=self.fixture.protobuf_directory,
            upstream_repository_path=self.fixture.upstream_directory,
        )
        with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                    r'Working tree has uncommitted changes'):
            roll_protobuf.ResumeRoll(self.fixture.protobuf_directory)

        _RunGitCommand(self.repository_root,
                       ['reset', '--quiet', '--hard', 'HEAD'])
        _RunGitCommand(
            self.repository_root,
            ['commit', '--quiet', '--allow-empty', '-m', 'Wrong subject'])
        with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                    r'Unexpected commit subject'):
            roll_protobuf.ResumeRoll(self.fixture.protobuf_directory)

    def testMainContinueAndDropCompleteRollEndToEnd(self):
        start_code, _, _ = _RunMain('start', '--to', 'v37.0', '--protobuf-dir',
                                    self.fixture.protobuf_directory,
                                    '--upstream-repo',
                                    self.fixture.upstream_directory)
        _WriteFilesToDirectory(
            self.fixture.protobuf_directory, {
                _PLAN_CONFLICT_PATH:
                _NumberedLines('conflict', line6='conflict line 6 (cli)'),
                'src/extra_cli.cc':
                'extra\n'
            })
        continue_code, continue_stdout, _ = _RunMain(
            'continue', '--allow-extra-files', '--protobuf-dir',
            self.fixture.protobuf_directory)
        drop_code, drop_stdout, _ = _RunMain('drop', '--reason',
                                             'no longer needed in v37',
                                             '--protobuf-dir',
                                             self.fixture.protobuf_directory,
                                             '--json')
        drop_payload = json.loads(drop_stdout)

        self.assertEqual((roll_protobuf.EXIT_CODE_CONFLICT,
                          roll_protobuf.EXIT_CODE_CONFLICT,
                          roll_protobuf.EXIT_CODE_SUCCESS),
                         (start_code, continue_code, drop_code))
        self.assertIn(f'Stopped at [4/5] patches/{_PLAN_MISSING_FILE_PATCH}.',
                      continue_stdout)
        self.assertIn('`roll_protobuf.py drop --reason "<why>"`',
                      continue_stdout)
        self.assertEqual(
            ('replayed', 'no longer needed in v37'),
            (drop_payload['phase'], drop_payload['patch_results'][3]['error']))

    def testMainResumeFinishesRollAfterManualCommit(self):
        _RunMain('start', '--to', 'v37.0', '--protobuf-dir',
                 self.fixture.protobuf_directory, '--upstream-repo',
                 self.fixture.upstream_directory)
        _RunGitCommand(self.repository_root,
                       ['reset', '--quiet', '--hard', 'HEAD'])
        resume_outcomes = []
        for patch_name in (_PLAN_CONFLICT_PATCH, _PLAN_MISSING_FILE_PATCH):
            _RunGitCommand(self.repository_root, [
                'rm', '--quiet', f'third_party/protobuf/patches/{patch_name}'
            ])
            _RunGitCommand(self.repository_root, [
                'commit', '--quiet', '-m',
                f'Drop patches/{patch_name} (manual drop)'
            ])
            resume_outcomes.append(
                _RunMain('resume', '--protobuf-dir',
                         self.fixture.protobuf_directory))

        self.assertEqual([
            roll_protobuf.EXIT_CODE_CONFLICT, roll_protobuf.EXIT_CODE_SUCCESS
        ], [code for code, _, _ in resume_outcomes])
        self.assertIn(f'Stopped at [4/5] patches/{_PLAN_MISSING_FILE_PATCH}.',
                      resume_outcomes[0][1])
        self.assertIn('Replayed all 5 patch(es).', resume_outcomes[1][1])

    def testIsBootstrapGeneratorInputClassifiesCompilerAndExactInputs(self):
        self.assertTrue(
            roll_protobuf.IsBootstrapGeneratorInput(
                'src/google/protobuf/compiler/cpp/enum.cc'))
        self.assertTrue(
            roll_protobuf.IsBootstrapGeneratorInput(
                'src/google/protobuf/descriptor.proto'))
        self.assertTrue(
            roll_protobuf.IsBootstrapGeneratorInput(
                'src/google/protobuf/feature_resolver.cc'))
        self.assertTrue(
            roll_protobuf.IsBootstrapGeneratorInput(
                'src/google/protobuf/generated_message_tctable_gen.cc'))
        self.assertTrue(
            roll_protobuf.IsBootstrapGeneratorInput(
                'gen_extra_chromium_files.py'))
        self.assertFalse(
            roll_protobuf.IsBootstrapGeneratorInput(
                'src/google/protobuf/compiler/plugin.pb.h'))
        self.assertFalse(
            roll_protobuf.IsBootstrapGeneratorInput(
                'src/google/protobuf/arena.h'))

    def testGenerateBootstrapFilesForceStagesGitignoredFilesAndUpdatesState(
            self):
        _WriteFilesToDirectory(self.fixture.upstream_directory,
                               {'.gitignore': '*.Swp\n*_pb2.py\n'})
        _RunGitCommand(self.fixture.upstream_directory,
                       ['add', '-f', '--', '.gitignore'])
        _RunGitCommand(self.fixture.upstream_directory,
                       ['commit', '--quiet', '--amend', '--no-edit'])
        _RunGitCommand(self.fixture.upstream_directory,
                       ['tag', '-f', 'v37.0', 'HEAD'])
        for patch_name in (_PLAN_CONFLICT_PATCH, _PLAN_MISSING_FILE_PATCH):
            os.remove(
                os.path.join(self.fixture.protobuf_directory, 'patches',
                             patch_name))
        _WriteFakeBootstrapGeneratorScript(self.fixture.protobuf_directory)
        _WriteFilesToDirectory(
            self.fixture.protobuf_directory,
            {
                '.gitignore': '*.Swp\n*_pb2.py\n',
                'compatibility/legacy.bin': 'stale\n',
            },
        )
        _RunGitCommand(self.repository_root,
                       ['add', '-f', '-A', '--', 'third_party/protobuf'])
        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '-m', 'Add fake generator'])
        out_directory = os.path.join(self.temporary_directory, 'out_release')

        standalone = roll_protobuf.GenerateBootstrapFiles(
            out_directory, self.fixture.protobuf_directory)
        self.assertEqual(
            tuple(
                sorted(roll_protobuf.BOOTSTRAP_GENERATED_FILES
                       | {'compatibility/legacy.bin'})),
            standalone.changed_files)
        self.assertIsNone(
            roll_protobuf.LoadRollState(self.fixture.protobuf_directory))

        roll_protobuf.StartRoll(
            to_revision='v37.0',
            protobuf_directory=self.fixture.protobuf_directory,
            upstream_repository_path=self.fixture.upstream_directory,
        )
        _WriteFilesToDirectory(self.repository_root,
                               {'OTHER_CHROMIUM_FILE': 'local wip\n'})
        out_roll_dir = os.path.join(self.temporary_directory, 'out_roll')

        first = roll_protobuf.GenerateBootstrapFiles(
            out_roll_dir, self.fixture.protobuf_directory)
        _WriteFilesToDirectory(
            self.fixture.protobuf_directory,
            {'src/google/protobuf/compiler/editor.Swp': 'ignored\n'})
        second = roll_protobuf.GenerateBootstrapFiles(
            out_roll_dir, self.fixture.protobuf_directory)

        self.assertEqual(
            tuple(sorted(roll_protobuf.BOOTSTRAP_GENERATED_FILES)),
            first.changed_files)
        self.assertEqual((), second.changed_files)
        self.assertEqual(first.inputs_digest, second.inputs_digest)
        self.assertEqual(
            'Regenerate bootstrap files',
            _RunGitCommand(self.repository_root, ['log', '-1', '--format=%s']))
        self.assertIn(
            f'Bootstrap-Inputs-Hash: {second.inputs_digest}',
            _RunGitCommand(self.repository_root, ['log', '-1', '--format=%B']))
        self.assertEqual(
            second.commit,
            roll_protobuf.LoadRollState(
                self.fixture.protobuf_directory).expected_head)
        self.assertIn('Regenerated ',
                      roll_protobuf.FormatGenerateReport(first))
        self.assertIn('already up to date',
                      roll_protobuf.FormatGenerateReport(second))
        self.assertIn(
            'OTHER_CHROMIUM_FILE',
            _RunGitCommand(self.repository_root, ['diff', '--name-only']))

        # A stale replayed state whose expected_head was garbage-collected
        # does not crash standalone GenerateBootstrapFiles on another branch,
        # and a prepared state (expected_head == start_commit) is not mistaken
        # for an ancestor on unrelated child branches.
        checkout = roll_protobuf._ResolveCheckoutPaths(
            self.fixture.protobuf_directory)
        roll_branch = _RunGitCommand(self.repository_root,
                                     ['rev-parse', '--abbrev-ref', 'HEAD'])
        _RunGitCommand(self.repository_root,
                       ['config', 'log.showSignature', 'true'])
        state = roll_protobuf.LoadRollState(self.fixture.protobuf_directory)
        pruned_head = 'f' * 40
        stale_state = dataclasses.replace(state, expected_head=pruned_head)
        roll_protobuf._SaveRollState(checkout, stale_state)
        _RunGitCommand(self.repository_root, [
            'checkout', '--quiet', '-b', 'standalone_branch',
            state.start_commit
        ])
        _RunGitCommand(self.repository_root,
                       ['checkout', '--', 'OTHER_CHROMIUM_FILE'])
        roll_protobuf.GenerateBootstrapFiles(out_roll_dir,
                                             self.fixture.protobuf_directory)
        self.assertEqual(
            pruned_head,
            roll_protobuf.LoadRollState(
                self.fixture.protobuf_directory).expected_head)
        self.assertTrue(
            roll_protobuf._IsRollBranchHead(checkout, stale_state,
                                            roll_branch))
        prepared_state = dataclasses.replace(state,
                                             phase='prepared',
                                             expected_head=state.start_commit)
        self.assertFalse(
            roll_protobuf._IsRollBranchHead(checkout, prepared_state))
        for off_branch_state in (dataclasses.replace(state, phase='conflict'),
                                 prepared_state):
            roll_protobuf._SaveRollState(checkout, off_branch_state)
            roll_protobuf.GenerateBootstrapFiles(
                out_roll_dir, self.fixture.protobuf_directory)

    def testGenerateBootstrapFilesRejectsInvalidPreconditionsAndFailures(self):
        out_directory = os.path.join(self.temporary_directory, 'out_release')
        with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                    r'Provide -C <out_dir>'):
            roll_protobuf.GenerateBootstrapFiles(
                '  ', self.fixture.protobuf_directory)

        roll_protobuf.PrepareRoll(
            to_revision='v37.0',
            protobuf_directory=self.fixture.protobuf_directory,
            upstream_repository_path=self.fixture.upstream_directory,
        )
        with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                    r"while roll phase is 'prepared'"):
            roll_protobuf.GenerateBootstrapFiles(
                out_directory, self.fixture.protobuf_directory)
        roll_protobuf.StartRoll(
            to_revision='v37.0',
            protobuf_directory=self.fixture.protobuf_directory,
            upstream_repository_path=self.fixture.upstream_directory,
        )
        with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                    r"while roll phase is 'conflict'"):
            roll_protobuf.GenerateBootstrapFiles(
                out_directory, self.fixture.protobuf_directory)
        roll_protobuf.AbortRoll(self.fixture.protobuf_directory)

        _WriteFilesToDirectory(self.repository_root,
                               {'OTHER_CHROMIUM_FILE': 'staged\n'})
        _RunGitCommand(self.repository_root, ['add', 'OTHER_CHROMIUM_FILE'])
        with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                    r'Repository has staged changes'):
            roll_protobuf.GenerateBootstrapFiles(
                out_directory, self.fixture.protobuf_directory)
        _RunGitCommand(self.repository_root,
                       ['checkout', '--', 'OTHER_CHROMIUM_FILE'])
        _RunGitCommand(self.repository_root,
                       ['reset', '--quiet', 'HEAD', 'OTHER_CHROMIUM_FILE'])

        _WriteFilesToDirectory(self.fixture.protobuf_directory,
                               {'src/leftover.orig': 'backup\n'})
        with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                    r'uncommitted changes or \.rej/\.orig'):
            roll_protobuf.GenerateBootstrapFiles(
                out_directory, self.fixture.protobuf_directory)
        os.remove(
            os.path.join(self.fixture.protobuf_directory, 'src',
                         'leftover.orig'))

        _WriteFilesToDirectory(
            self.fixture.protobuf_directory, {
                '.gitignore': '*_pb2.py\n',
                'compatibility/legacy.bin': 'keep on failure\n',
            })
        _WriteFakeBootstrapGeneratorScript(self.fixture.protobuf_directory,
                                           exit_code=1)
        _RunGitCommand(self.repository_root,
                       ['add', '-f', '--', 'third_party/protobuf'])
        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '-m', 'Add failing generator'])
        with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                    r'protoc build boom'):
            roll_protobuf.GenerateBootstrapFiles(
                out_directory, self.fixture.protobuf_directory)
        self.assertEqual(
            '',
            _RunGitCommand(
                self.repository_root,
                ['status', '--porcelain', '--', 'third_party/protobuf']))
        self.assertEqual(
            'OTHER_CHROMIUM_FILE',
            _RunGitCommand(self.repository_root, ['diff', '--name-only']))

        _WriteFakeBootstrapGeneratorScript(
            self.fixture.protobuf_directory,
            omit_path='python/google/protobuf/descriptor_pb2.py')
        _RunGitCommand(self.repository_root,
                       ['add', '--', 'third_party/protobuf'])
        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '-m', 'Add incomplete generator'])
        with self.assertRaisesRegex(
                roll_protobuf.RollProtobufError,
                r'did not produce expected bootstrap files.*descriptor_pb2\.py'
        ):
            roll_protobuf.GenerateBootstrapFiles(
                out_directory, self.fixture.protobuf_directory)
        self.assertEqual(
            '',
            _RunGitCommand(
                self.repository_root,
                ['status', '--porcelain', '--', 'third_party/protobuf']))
        self.assertFalse(
            os.path.exists(
                os.path.join(self.fixture.protobuf_directory, 'python',
                             'google', 'protobuf', 'compiler',
                             'plugin_pb2.py')))

        # Missing Revision: in README.chromium fails before running the
        # generator or staging changes into the index.
        _WriteFakeBootstrapGeneratorScript(self.fixture.protobuf_directory)
        _WriteFilesToDirectory(self.fixture.protobuf_directory,
                               {'README.chromium': 'Name: Protocol Buffers\n'})
        _RunGitCommand(self.repository_root,
                       ['add', '--', 'third_party/protobuf'])
        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '-m', 'Drop Revision line'])
        with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                    r'Revision'):
            roll_protobuf.GenerateBootstrapFiles(
                out_directory, self.fixture.protobuf_directory)
        self.assertTrue(
            os.path.isfile(
                os.path.join(self.fixture.protobuf_directory, 'compatibility',
                             'legacy.bin')))
        self.assertEqual(
            '',
            _RunGitCommand(
                self.repository_root,
                ['status', '--porcelain', '--', 'third_party/protobuf']))

    def testFindPatchInventoryErrorsSharesPresubmitChecks(self):
        patch_names = sorted(name for name in os.listdir(
            os.path.join(self.fixture.protobuf_directory, 'patches'))
                             if name.endswith('.patch'))
        readme_path = pathlib.Path(self.fixture.protobuf_directory,
                                   'README.chromium')
        base_readme = readme_path.read_text(encoding='utf-8')
        bullets = '\n'.join(f'- {name}' for name in patch_names)
        readme_path.write_text(
            f'{base_readme}\nDescription of the patches:\n{bullets}\n',
            encoding='utf-8',
            newline='\n')
        self.assertEqual((),
                         roll_protobuf._FindPatchInventoryErrors(
                             self.fixture.protobuf_directory))

        _WriteFilesToDirectory(
            self.fixture.protobuf_directory, {
                'patches/0099-extra.patch': 'diff\n',
                'patches/0098-orphan.md': 'note\n',
            })
        errors = roll_protobuf._FindPatchInventoryErrors(
            self.fixture.protobuf_directory)
        self.assertTrue(any('0099-extra.patch' in e for e in errors))
        self.assertTrue(any('0098-orphan.md' in e for e in errors))

    def testFindLocalHygieneErrorsDetectsRejectsMarkersAndUnmergedEntries(
            self):
        checkout = roll_protobuf._ResolveCheckoutPaths(
            self.fixture.protobuf_directory)
        self.assertEqual((), roll_protobuf._FindLocalHygieneErrors(checkout))

        marker = '<' * 7 + ' HEAD\n'
        _WriteFilesToDirectory(self.fixture.protobuf_directory, {
            'src/leftover.rej': 'reject\n',
            _PLAN_CLEAN_PATH: marker + 'body\n',
        })
        blob_sha = _RunGitCommand(
            self.repository_root,
            ['rev-parse', 'HEAD:third_party/protobuf/README.chromium'])
        roll_protobuf._RunScratchGit(
            self.repository_root,
            'update-index',
            '--index-info',
            input_bytes=(
                f'100644 {blob_sha} 1\tthird_party/protobuf/src/bin.dat\n'.
                encode('utf-8')))

        self.assertEqual(
            (
                'src/leftover.rej: leftover patch .rej/.orig file',
                f'{_PLAN_CLEAN_PATH}: unresolved conflict markers',
                'src/bin.dat: unresolved unmerged index entry',
            ),
            roll_protobuf._FindLocalHygieneErrors(checkout),
        )

    def testFindStaleBootstrapTriggersDetectsRollCompilerEditsAndHashDrift(
            self):
        _WriteFilesToDirectory(self.fixture.upstream_directory,
                               {'.gitignore': '*.Swp\n*_pb2.py\n'})
        _RunGitCommand(self.fixture.upstream_directory,
                       ['add', '-f', '--', '.gitignore'])
        _RunGitCommand(self.fixture.upstream_directory,
                       ['commit', '--quiet', '--amend', '--no-edit'])
        _RunGitCommand(self.fixture.upstream_directory,
                       ['tag', '-f', 'v37.0', 'HEAD'])
        for patch_name in (_PLAN_CONFLICT_PATCH, _PLAN_MISSING_FILE_PATCH):
            os.remove(
                os.path.join(self.fixture.protobuf_directory, 'patches',
                             patch_name))
        _WriteFilesToDirectory(self.fixture.protobuf_directory,
                               {'.gitignore': '*.Swp\n*_pb2.py\n'})
        _WriteFakeBootstrapGeneratorScript(self.fixture.protobuf_directory)
        _RunGitCommand(self.repository_root,
                       ['add', '-f', '-A', '--', 'third_party/protobuf'])
        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '-m', 'Add fake generator'])
        out_directory = os.path.join(self.temporary_directory, 'out_release')
        roll_protobuf.GenerateBootstrapFiles(out_directory,
                                             self.fixture.protobuf_directory)
        _RunGitCommand(self.repository_root,
                       ['update-ref', 'refs/remotes/origin/main', 'HEAD'])
        checkout = roll_protobuf._ResolveCheckoutPaths(
            self.fixture.protobuf_directory)
        self.assertEqual((),
                         roll_protobuf._FindStaleBootstrapTriggers(checkout))

        # Manual C++ compiler patch that also updates README.chromium's patch
        # inventory and regenerates C++ descriptor.pb.h is recognized as clean.
        compiler_path = 'src/google/protobuf/compiler/cpp/enum.cc'
        _WriteFilesToDirectory(
            self.fixture.protobuf_directory, {
                compiler_path:
                '// pre-roll fix\n',
                'src/google/protobuf/descriptor.pb.h':
                '// manual regen\n',
                'README.chromium':
                pathlib.Path(self.fixture.protobuf_directory,
                             'README.chromium').read_text(encoding='utf-8') +
                '\n- 0099-fix.patch\n',
            })
        _RunGitCommand(self.repository_root,
                       ['add', '-A', '--', 'third_party/protobuf'])
        _RunGitCommand(
            self.repository_root,
            ['commit', '--quiet', '-m', 'Manual C++ compiler patch'])
        self.assertEqual((),
                         roll_protobuf._FindStaleBootstrapTriggers(checkout))
        _RunGitCommand(self.repository_root,
                       ['reset', '--quiet', '--hard', 'origin/main'])

        roll_protobuf.StartRoll(
            to_revision='v37.0',
            protobuf_directory=self.fixture.protobuf_directory,
            upstream_repository_path=self.fixture.upstream_directory,
        )
        self.assertEqual(('upstream revision changed', ),
                         roll_protobuf._FindStaleBootstrapTriggers(checkout))

        # Running gen_extra_chromium_files.py manually without `git add -f` on
        # untracked gitignored *_pb2.py files still flags the missing outputs.
        _RunGitCommand(self.repository_root, [
            'rm',
            '--quiet',
            '--',
            'third_party/protobuf/python/google/protobuf/descriptor_pb2.py',
            'third_party/protobuf/python/google/protobuf/compiler/'
            'plugin_pb2.py',
        ])
        out_manual = os.path.join(self.temporary_directory, 'out_manual')
        subprocess.run([
            sys.executable,
            os.path.join(self.fixture.protobuf_directory,
                         'gen_extra_chromium_files.py'), '-C', out_manual
        ],
                       check=True)
        _RunGitCommand(self.repository_root,
                       ['add', '--', 'third_party/protobuf'])
        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '-m', 'Manual incomplete regen'])
        self.assertEqual((
            'missing bootstrap output: '
            'python/google/protobuf/compiler/plugin_pb2.py',
            'missing bootstrap output: '
            'python/google/protobuf/descriptor_pb2.py',
        ), roll_protobuf._FindStaleBootstrapTriggers(checkout))
        _RunGitCommand(self.repository_root,
                       ['reset', '--quiet', '--hard', 'HEAD~1'])
        roll_protobuf._CleanUntrackedAndBackupFiles(
            self.fixture.protobuf_directory)

        # Squashing before `generate` (without Bootstrap-Inputs-Hash or
        # python/ bootstrap outputs) still flags the revision change.
        pre_gen_tip = _RunGitCommand(self.repository_root,
                                     ['rev-parse', 'HEAD'])
        _RunGitCommand(self.repository_root,
                       ['reset', '--quiet', '--soft', 'origin/main'])
        _RunGitCommand(
            self.repository_root,
            ['commit', '--quiet', '-m', '[protobuf] Roll protobuf to v37.0'])
        self.assertEqual(('upstream revision changed', ),
                         roll_protobuf._FindStaleBootstrapTriggers(checkout))
        _RunGitCommand(self.repository_root,
                       ['reset', '--quiet', '--hard', pre_gen_tip])

        out_roll = os.path.join(self.temporary_directory, 'out_roll')
        gen = roll_protobuf.GenerateBootstrapFiles(
            out_roll, self.fixture.protobuf_directory)
        self.assertEqual((),
                         roll_protobuf._FindStaleBootstrapTriggers(checkout))

        _WriteFilesToDirectory(self.fixture.protobuf_directory,
                               {'BUILD.gn': '# non-compiler edit\n'})
        _RunGitCommand(self.repository_root,
                       ['add', '--', 'third_party/protobuf'])
        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '-m', 'Update BUILD.gn'])
        self.assertEqual((),
                         roll_protobuf._FindStaleBootstrapTriggers(checkout))

        # Squashing the roll after post-`generate` commits stays clean both
        # with an interior Bootstrap-Inputs-Hash trailer and with a clean
        # human CL description (`git cl patch`) that omits tool trailers.
        roll_tip = _RunGitCommand(self.repository_root, ['rev-parse', 'HEAD'])
        _RunGitCommand(self.repository_root,
                       ['reset', '--quiet', '--soft', 'origin/main'])
        _RunGitCommand(self.repository_root, [
            'commit', '--quiet', '-m', 'Roll protobuf to v37.0\n\n'
            '    Regenerate bootstrap files\n\n'
            f'    Bootstrap-Inputs-Hash: {gen.inputs_digest}\n\n'
            'Update BUILD.gn\n\n'
            'Bug: 568074591\n'
            'NOAUTOREVERT=true'
        ])
        self.assertEqual((),
                         roll_protobuf._FindStaleBootstrapTriggers(checkout))
        _RunGitCommand(self.repository_root, [
            'commit', '--quiet', '--amend', '-m',
            '[protobuf] Roll protobuf to v37.0\n\nBug: 568074591'
        ])
        self.assertEqual((),
                         roll_protobuf._FindStaleBootstrapTriggers(checkout))
        _WriteFilesToDirectory(self.fixture.protobuf_directory,
                               {compiler_path: '// edited after squash\n'})
        self.assertEqual((compiler_path, ),
                         roll_protobuf._FindStaleBootstrapTriggers(checkout))

        # Manual README.chromium steps 0-5: unpacking upstream C++ bootstrap
        # files in steps 1-3 (including descriptor.pb.h) must NOT satisfy step
        # 4 if Revision: is bumped before gen_extra_chromium_files.py runs;
        # once step 4 commits python/ bootstrap outputs, step 5 updates
        # Revision: once (uncommitted and committed) without re-triggering.
        _RunGitCommand(self.repository_root,
                       ['config', 'log.showSignature', 'true'])
        _RunGitCommand(self.repository_root,
                       ['reset', '--quiet', '--hard', pre_gen_tip])
        roll_protobuf.AbortRoll(self.fixture.protobuf_directory)
        _RunGitCommand(
            self.repository_root,
            ['checkout', 'origin/main', '--', 'third_party/protobuf'])
        base_readme = pathlib.Path(
            self.fixture.protobuf_directory,
            'README.chromium').read_text(encoding='utf-8')
        step5_readme = re.sub(r'^Revision:\s*\S+',
                              f'Revision: {self.fixture.new_commit}',
                              base_readme,
                              flags=re.MULTILINE)
        _WriteFilesToDirectory(
            self.fixture.protobuf_directory, {
                _PLAN_ADDED_PATH: '// manual import\n',
                'src/google/protobuf/descriptor.pb.h': '// upstream archive\n',
            })
        _RunGitCommand(self.repository_root,
                       ['add', '-f', '--', 'third_party/protobuf'])
        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '-m', 'Manual steps 1-3'])
        _WriteFilesToDirectory(self.fixture.protobuf_directory,
                               {'README.chromium': step5_readme})
        self.assertIn('upstream revision changed',
                      roll_protobuf._FindStaleBootstrapTriggers(checkout))
        _RunGitCommand(
            self.repository_root,
            ['checkout', '--', 'third_party/protobuf/README.chromium'])
        subprocess.run([
            sys.executable,
            os.path.join(self.fixture.protobuf_directory,
                         'gen_extra_chromium_files.py'), '-C', out_manual
        ],
                       check=True)
        _RunGitCommand(self.repository_root,
                       ['add', '-f', '-A', '--', 'third_party/protobuf'])
        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '-m', 'Manual step 4 regen'])
        _WriteFilesToDirectory(self.fixture.protobuf_directory,
                               {'README.chromium': step5_readme})
        self.assertEqual((),
                         roll_protobuf._FindStaleBootstrapTriggers(checkout))
        _RunGitCommand(self.repository_root,
                       ['add', '--', 'third_party/protobuf/README.chromium'])
        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '-m', 'Manual step 5 README'])
        self.assertEqual((),
                         roll_protobuf._FindStaleBootstrapTriggers(checkout))
        second_bump = re.sub(r'^Revision:\s*\S+',
                             'Revision: ' + ('a' * 40),
                             step5_readme,
                             flags=re.MULTILINE)
        _WriteFilesToDirectory(self.fixture.protobuf_directory,
                               {'README.chromium': second_bump})
        self.assertIn('upstream revision changed',
                      roll_protobuf._FindStaleBootstrapTriggers(checkout))
        _RunGitCommand(
            self.repository_root,
            ['checkout', '--', 'third_party/protobuf/README.chromium'])
        _WriteFilesToDirectory(self.fixture.protobuf_directory,
                               {compiler_path: '// edited after step 5\n'})
        self.assertEqual((compiler_path, ),
                         roll_protobuf._FindStaleBootstrapTriggers(checkout))
        _RunGitCommand(self.repository_root,
                       ['reset', '--quiet', '--hard', roll_tip])

        _WriteFilesToDirectory(self.fixture.protobuf_directory,
                               {compiler_path: '// edited\n'})
        self.assertEqual((compiler_path, ),
                         roll_protobuf._FindStaleBootstrapTriggers(checkout))
        _RunGitCommand(self.repository_root,
                       ['add', '--', 'third_party/protobuf'])
        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '-m', 'Edit compiler'])
        self.assertEqual((compiler_path, ),
                         roll_protobuf._FindStaleBootstrapTriggers(checkout))

        roll_protobuf.GenerateBootstrapFiles(out_directory,
                                             self.fixture.protobuf_directory)
        self.assertEqual((),
                         roll_protobuf._FindStaleBootstrapTriggers(checkout))
        clean_gen_tip = _RunGitCommand(self.repository_root,
                                       ['rev-parse', 'HEAD'])

        # Renaming a generator input outside IsBootstrapGeneratorInput still
        # reports the old path via `--no-renames` (staged, committed, and when
        # reference_commit is None).
        _RunGitCommand(self.repository_root, [
            'mv', f'third_party/protobuf/{compiler_path}',
            'third_party/protobuf/src/google/protobuf/moved_enum.cc'
        ])
        self.assertEqual((compiler_path, ),
                         roll_protobuf._FindStaleBootstrapTriggers(checkout))
        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '-m', 'Move compiler file'])
        self.assertEqual((compiler_path, ),
                         roll_protobuf._FindStaleBootstrapTriggers(checkout))
        _RunGitCommand(self.repository_root,
                       ['reset', '--quiet', '--hard', pre_gen_tip])
        _WriteFilesToDirectory(self.fixture.protobuf_directory,
                               {compiler_path: '// pre-gen compiler\n'})
        _RunGitCommand(self.repository_root,
                       ['add', '--', 'third_party/protobuf'])
        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '-m', 'Add compiler file'])
        _RunGitCommand(self.repository_root, [
            'mv', f'third_party/protobuf/{compiler_path}',
            'third_party/protobuf/src/google/protobuf/moved_enum.cc'
        ])
        with unittest.mock.patch.object(roll_protobuf,
                                        '_ResolveRollBaseCommit',
                                        return_value=None):
            self.assertIn(compiler_path,
                          roll_protobuf._FindStaleBootstrapTriggers(checkout))
        _RunGitCommand(self.repository_root,
                       ['reset', '--quiet', '--hard', clean_gen_tip])

        # Simulate an autosquash rebase that folded an input change before the
        # regeneration commit without re-running `generate`.
        stale_hash = '0' * 64
        _RunGitCommand(self.repository_root, [
            'commit', '--quiet', '--amend', '--allow-empty', '-m',
            f'Regenerate bootstrap files\n\nBootstrap-Inputs-Hash: {stale_hash}'
        ])
        self.assertEqual(
            ('generator inputs changed since last `roll_protobuf.py generate`',
             ), roll_protobuf._FindStaleBootstrapTriggers(checkout))

    def testCompileSizeProbeComparisonSelectsBaselineFormatsAndHandlesErrors(
            self):
        checkout = roll_protobuf._ResolveCheckoutPaths(
            self.fixture.protobuf_directory)
        out_dir = os.path.join(self.temporary_directory, 'out_release')
        self.assertEqual(
            (None, None),
            roll_protobuf._RunCompileSizeProbeComparison(checkout, None, None))
        with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                    r'--baseline requires -C'):
            roll_protobuf._RunCompileSizeProbeComparison(
                checkout, None, 'before')
        with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                    r'non-empty baseline name'):
            roll_protobuf._RunCompileSizeProbeComparison(
                checkout, out_dir, '  ')

        # Missing probe script skips cleanly by default, or reports an error
        # when an explicit --baseline was requested.
        self.assertEqual((None, None),
                         roll_protobuf._RunCompileSizeProbeComparison(
                             checkout, out_dir, None))
        _, missing_err = roll_protobuf._RunCompileSizeProbeComparison(
            checkout, out_dir, 'before')
        self.assertIn('Missing compile_size_probe.py', missing_err)

        _WriteFakeCompileSizeProbeScript(self.fixture.protobuf_directory)
        _WriteFilesToDirectory(
            out_dir, {
                '.compile_size_probe/.tmp.json': '{}',
                '.compile_size_probe/custom.artifacts.json': '{}',
                '.compile_size_probe/foo..bar.json': '{}',
            })
        self.assertEqual((None, None),
                         roll_protobuf._RunCompileSizeProbeComparison(
                             checkout, out_dir, None))

        _WriteFilesToDirectory(
            out_dir, {
                '.compile_size_probe/zeta.json': '{}',
                '.compile_size_probe/alpha.json': '{}',
            })
        os.utime(os.path.join(out_dir, '.compile_size_probe', 'zeta.json'),
                 (100, 100))
        os.utime(os.path.join(out_dir, '.compile_size_probe', 'alpha.json'),
                 (200, 200))
        newest_payload, _ = roll_protobuf._RunCompileSizeProbeComparison(
            checkout, out_dir, None)
        self.assertEqual('alpha', newest_payload['baseline_name'])

        _WriteFilesToDirectory(out_dir,
                               {'.compile_size_probe/pre_roll.json': '{}'})
        os.utime(os.path.join(out_dir, '.compile_size_probe', 'pre_roll.json'),
                 (50, 50))
        payload, error = roll_protobuf._RunCompileSizeProbeComparison(
            checkout, out_dir, None)
        self.assertIsNone(error)
        self.assertEqual('pre_roll', payload['baseline_name'])

        _WriteFilesToDirectory(out_dir,
                               {'.compile_size_probe/before.json': '{}'})
        os.utime(os.path.join(out_dir, '.compile_size_probe', 'before.json'),
                 (10, 10))
        stale_before_payload, _ = roll_protobuf._RunCompileSizeProbeComparison(
            checkout, out_dir, None)
        self.assertEqual('pre_roll', stale_before_payload['baseline_name'])
        os.utime(os.path.join(out_dir, '.compile_size_probe', 'before.json'),
                 (60, 60))
        before_payload, _ = roll_protobuf._RunCompileSizeProbeComparison(
            checkout, out_dir, None)
        self.assertEqual('before', before_payload['baseline_name'])

        dash_payload, _ = roll_protobuf._RunCompileSizeProbeComparison(
            checkout, out_dir, '-v36')
        self.assertEqual('-v36', dash_payload['baseline_name'])

        summary = roll_protobuf._FormatCompileSizeSummary(payload)
        self.assertIn('baseline "pre_roll": 1,000 B -> 1,250 B (+250 B)',
                      summary)
        self.assertIn('Compiled symbol bytes: 400 B -> 440 B (+40 B)', summary)
        self.assertIn(
            'via: gen/duration.pb.cc -> map.h -> '
            'absl/container/btree_map.h', summary)
        self.assertIn('-10 B  old.h', summary)
        self.assertIn('+60 B  arena.h (resized)', summary)

        _WriteFakeCompileSizeProbeScript(self.fixture.protobuf_directory,
                                         exit_code=1)
        self.assertEqual((None, 'probe failed for custom'),
                         roll_protobuf._RunCompileSizeProbeComparison(
                             checkout, out_dir, 'custom'))
        _WriteFakeCompileSizeProbeScript(self.fixture.protobuf_directory,
                                         stderr_only_whitespace=True,
                                         exit_code=1)
        self.assertEqual((None, 'stdout error for custom'),
                         roll_protobuf._RunCompileSizeProbeComparison(
                             checkout, out_dir, 'custom'))
        _WriteFakeCompileSizeProbeScript(self.fixture.protobuf_directory,
                                         raw_stdout='not-json')
        _, json_error = roll_protobuf._RunCompileSizeProbeComparison(
            checkout, out_dir, 'custom')
        self.assertIn('invalid JSON', json_error)
        _WriteFakeCompileSizeProbeScript(self.fixture.protobuf_directory,
                                         raw_stdout='[]')
        _, list_error = roll_protobuf._RunCompileSizeProbeComparison(
            checkout, out_dir, 'custom')
        self.assertIn('expected object', list_error)

    def testCheckRollFailsOnInventoryHygieneAndStaleBootstrapErrors(self):
        clean_tree = roll_protobuf.TreeVerificationResult('abc', ())
        dirty_tree = dataclasses.replace(clean_tree, modified_files=('a.h', ))
        base_check = roll_protobuf.RollCheckResult(tree_result=clean_tree)
        self.assertTrue(base_check.IsClean())
        self.assertFalse(
            dataclasses.replace(base_check, tree_result=dirty_tree).IsClean())
        self.assertFalse(
            dataclasses.replace(base_check,
                                patch_inventory_errors=('err', )).IsClean())
        self.assertFalse(
            dataclasses.replace(base_check,
                                hygiene_errors=('err', )).IsClean())
        self.assertFalse(
            dataclasses.replace(base_check,
                                stale_bootstrap_triggers=('err', )).IsClean())

        for patch_name in (_PLAN_CONFLICT_PATCH, _PLAN_MISSING_FILE_PATCH):
            os.remove(
                os.path.join(self.fixture.protobuf_directory, 'patches',
                             patch_name))
        _WriteFakeBootstrapGeneratorScript(self.fixture.protobuf_directory)
        _RunGitCommand(self.repository_root,
                       ['add', '-A', '--', 'third_party/protobuf'])
        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '-m', 'Add fake generator'])
        _RunGitCommand(self.repository_root,
                       ['update-ref', 'refs/remotes/origin/main', 'HEAD'])
        roll_protobuf.StartRoll(
            to_revision='v37.0',
            protobuf_directory=self.fixture.protobuf_directory,
            upstream_repository_path=self.fixture.upstream_directory,
        )
        _WriteFilesToDirectory(self.fixture.protobuf_directory,
                               {'src/leftover.orig': 'backup\n'})

        result = roll_protobuf.CheckRoll(
            protobuf_directory=self.fixture.protobuf_directory,
            upstream_repository_path=self.fixture.upstream_directory,
        )
        report = roll_protobuf.FormatRollCheckReport(result)

        self.assertTrue(result.tree_result.IsClean())
        self.assertFalse(result.IsClean())
        self.assertTrue(result.patch_inventory_errors)
        self.assertEqual(
            ('src/leftover.orig: leftover patch .rej/.orig file', ),
            result.hygiene_errors)
        self.assertIn('upstream revision changed',
                      result.stale_bootstrap_triggers)
        self.assertIn('Patch inventory / README.chromium errors:', report)
        self.assertIn('Local hygiene errors:', report)
        self.assertIn('Stale bootstrap files', report)
        with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                    r'non-empty baseline name'):
            roll_protobuf.CheckRoll(
                protobuf_directory=self.fixture.protobuf_directory,
                upstream_repository_path=self.fixture.upstream_directory,
                baseline_name='  ',
            )
        with self.assertRaisesRegex(roll_protobuf.RollProtobufError,
                                    r'--baseline requires -C'):
            roll_protobuf.CheckRoll(
                protobuf_directory=self.fixture.protobuf_directory,
                upstream_repository_path=self.fixture.upstream_directory,
                baseline_name='before',
            )

        # Unborn HEAD repository still runs CheckRoll without crashing.
        unborn = os.path.join(self.temporary_directory, 'unborn_repo')
        _WriteFilesToDirectory(
            unborn, {
                'README.chromium': f'Revision: {self.fixture.old_commit}\n\n'
                'Description of the patches:\n',
                **_PLAN_OLD_UPSTREAM_FILES,
            })
        _RunGitCommand(unborn, ['init', '--quiet'])
        self.assertTrue(
            roll_protobuf.CheckRoll(
                protobuf_directory=unborn,
                upstream_repository_path=self.fixture.upstream_directory,
            ).IsClean())

    def testCheckRollReportsFileListsAndCompileSizeProbeWithoutFailing(self):
        for patch_name in (_PLAN_CONFLICT_PATCH, _PLAN_MISSING_FILE_PATCH):
            os.remove(
                os.path.join(self.fixture.protobuf_directory, 'patches',
                             patch_name))
        _WriteFakeBootstrapGeneratorScript(self.fixture.protobuf_directory)
        _WriteFakeCompileSizeProbeScript(self.fixture.protobuf_directory)
        _RunGitCommand(self.repository_root,
                       ['add', '-A', '--', 'third_party/protobuf'])
        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '-m', 'Add fake scripts'])
        _RunGitCommand(self.repository_root,
                       ['update-ref', 'refs/remotes/origin/main', 'HEAD'])
        roll_protobuf.StartRoll(
            to_revision='v37.0',
            protobuf_directory=self.fixture.protobuf_directory,
            upstream_repository_path=self.fixture.upstream_directory,
        )
        out_dir = os.path.join(self.temporary_directory, 'out_release')
        _WriteFilesToDirectory(out_dir,
                               {'.compile_size_probe/before.json': '{}'})
        roll_protobuf.GenerateBootstrapFiles(out_dir,
                                             self.fixture.protobuf_directory)
        patch_names = sorted(name for name in os.listdir(
            os.path.join(self.fixture.protobuf_directory, 'patches'))
                             if name.endswith('.patch'))
        readme_path = pathlib.Path(self.fixture.protobuf_directory,
                                   'README.chromium')
        readme_path.write_text(
            readme_path.read_text(encoding='utf-8') +
            '\nDescription of the patches:\n' +
            ''.join(f'- {name}\n' for name in patch_names),
            encoding='utf-8',
            newline='\n',
        )
        _RunGitCommand(self.repository_root,
                       ['add', '--', 'third_party/protobuf'])
        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '-m', 'Sync README patches'])

        result = roll_protobuf.CheckRoll(
            protobuf_directory=self.fixture.protobuf_directory,
            upstream_repository_path=self.fixture.upstream_directory,
            build_directory=out_dir,
        )
        report = roll_protobuf.FormatRollCheckReport(result)
        payload = result.ToDictionary()

        self.assertTrue(result.IsClean())
        self.assertTrue(payload['is_clean'])
        self.assertEqual(
            [_PLAN_ADDED_PATH],
            [e['path'] for e in payload['file_lists']['added_files']])
        self.assertEqual('before', payload['compile_size']['baseline_name'])
        self.assertIn('Upstream files added in src/file_lists.cmake', report)
        self.assertIn('Compile-size comparison against baseline "before"',
                      report)

        _WriteFakeCompileSizeProbeScript(self.fixture.protobuf_directory,
                                         exit_code=1)
        _RunGitCommand(self.repository_root,
                       ['add', '--', 'third_party/protobuf'])
        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '-m', 'Failing probe'])
        err_result = roll_protobuf.CheckRoll(
            protobuf_directory=self.fixture.protobuf_directory,
            upstream_repository_path=self.fixture.upstream_directory,
            build_directory=out_dir,
        )
        self.assertTrue(err_result.IsClean())
        self.assertIn('Compile-size probe skipped due to error:',
                      roll_protobuf.FormatRollCheckReport(err_result))

    def testMainGenerateAndCheckCompletePostRollVerification(self):
        for patch_name in (_PLAN_CONFLICT_PATCH, _PLAN_MISSING_FILE_PATCH):
            os.remove(
                os.path.join(self.fixture.protobuf_directory, 'patches',
                             patch_name))
        _WriteFakeBootstrapGeneratorScript(self.fixture.protobuf_directory)
        _WriteFakeCompileSizeProbeScript(self.fixture.protobuf_directory)
        _RunGitCommand(self.repository_root,
                       ['add', '-A', '--', 'third_party/protobuf'])
        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '-m', 'Add fake scripts'])
        _RunGitCommand(self.repository_root,
                       ['update-ref', 'refs/remotes/origin/main', 'HEAD'])
        _RunMain('start', '--to', 'v37.0', '--protobuf-dir',
                 self.fixture.protobuf_directory, '--upstream-repo',
                 self.fixture.upstream_directory)

        _, status_before, _ = _RunMain('status', '--protobuf-dir',
                                       self.fixture.protobuf_directory,
                                       '--json')
        stale_code, stale_out, _ = _RunMain('check', '--protobuf-dir',
                                            self.fixture.protobuf_directory,
                                            '--upstream-repo',
                                            self.fixture.upstream_directory)
        missing_c_code, _, missing_c_err = _RunMain(
            'generate', '--protobuf-dir', self.fixture.protobuf_directory)
        out_dir = os.path.join(self.temporary_directory, 'out_release')
        _WriteFilesToDirectory(
            out_dir, {
                '.compile_size_probe/before.json': '{}',
                '.compile_size_probe/pre_roll.json': '{}',
            })
        os.utime(os.path.join(out_dir, '.compile_size_probe', 'before.json'),
                 (200, 200))
        os.utime(os.path.join(out_dir, '.compile_size_probe', 'pre_roll.json'),
                 (100, 100))
        gen_code, gen_json, _ = _RunMain('generate', '-C', out_dir,
                                         '--protobuf-dir',
                                         self.fixture.protobuf_directory,
                                         '--json')
        _, status_after, _ = _RunMain('status', '--protobuf-dir',
                                      self.fixture.protobuf_directory,
                                      '--json')

        patch_names = sorted(name for name in os.listdir(
            os.path.join(self.fixture.protobuf_directory, 'patches'))
                             if name.endswith('.patch'))
        readme_path = pathlib.Path(self.fixture.protobuf_directory,
                                   'README.chromium')
        readme_path.write_text(
            readme_path.read_text(encoding='utf-8') +
            '\nDescription of the patches:\n' +
            ''.join(f'- {name}\n' for name in patch_names),
            encoding='utf-8',
            newline='\n',
        )
        _RunGitCommand(self.repository_root,
                       ['add', '--', 'third_party/protobuf'])
        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '-m', 'Sync README patches'])
        check_code, check_json, _ = _RunMain(
            'check', '-C', out_dir, '--baseline', 'pre_roll', '--protobuf-dir',
            self.fixture.protobuf_directory, '--upstream-repo',
            self.fixture.upstream_directory, '--json')

        self.assertEqual([
            'roll_protobuf.py generate -C <out_dir>', 'roll_protobuf.py abort'
        ],
                         json.loads(status_before)['next'])
        self.assertEqual(roll_protobuf.EXIT_CODE_ERROR, stale_code)
        self.assertIn('Stale bootstrap files', stale_out)
        self.assertEqual(roll_protobuf.EXIT_CODE_ERROR, missing_c_code)
        self.assertIn('-C <out_dir>', missing_c_err)
        self.assertEqual(roll_protobuf.EXIT_CODE_SUCCESS, gen_code)
        self.assertEqual(len(roll_protobuf.BOOTSTRAP_GENERATED_FILES),
                         len(json.loads(gen_json)['changed_files']))
        self.assertEqual(
            ['roll_protobuf.py check -C <out_dir>', 'roll_protobuf.py abort'],
            json.loads(status_after)['next'])
        self.assertEqual(roll_protobuf.EXIT_CODE_SUCCESS, check_code)
        check_payload = json.loads(check_json)
        self.assertTrue(check_payload['is_clean'])
        self.assertEqual('pre_roll',
                         check_payload['compile_size']['baseline_name'])

        roll_branch = _RunGitCommand(self.repository_root,
                                     ['rev-parse', '--abbrev-ref', 'HEAD'])
        active_prep_code, _, active_prep_err = _RunMain(
            'prepare', '--to', 'v37.0', '--protobuf-dir',
            self.fixture.protobuf_directory, '--upstream-repo',
            self.fixture.upstream_directory)
        self.assertEqual(roll_protobuf.EXIT_CODE_ERROR, active_prep_code)
        self.assertIn('already in progress (phase: replayed)', active_prep_err)

        # Switching to an unrelated branch off origin/main treats stale
        # replayed state.json as idle, refuses abort while roll_branch exists,
        # clears orphaned state once roll_branch is deleted, and allows prepare.
        _RunGitCommand(
            self.repository_root,
            ['checkout', '--quiet', '-b', 'unrelated', 'origin/main'])
        _WriteFilesToDirectory(self.repository_root,
                               {'OTHER_CHROMIUM_FILE': 'unrelated work\n'})
        _RunGitCommand(self.repository_root, ['add', 'OTHER_CHROMIUM_FILE'])
        _RunGitCommand(self.repository_root,
                       ['commit', '--quiet', '-m', 'Unrelated branch commit'])
        unrelated_head = _RunGitCommand(self.repository_root,
                                        ['rev-parse', 'HEAD'])
        _, unrelated_status, _ = _RunMain('status', '--protobuf-dir',
                                          self.fixture.protobuf_directory,
                                          '--json')
        abort_code, _, abort_err = _RunMain('abort', '--protobuf-dir',
                                            self.fixture.protobuf_directory)
        resume_code, _, resume_err = _RunMain('resume', '--protobuf-dir',
                                              self.fixture.protobuf_directory)
        self.assertEqual('idle', json.loads(unrelated_status)['state'])
        self.assertEqual(roll_protobuf.EXIT_CODE_ERROR, abort_code)
        self.assertIn('not on the v37.0 roll branch', abort_err)
        self.assertIn(f'git checkout {roll_branch}', abort_err)
        self.assertEqual(roll_protobuf.EXIT_CODE_ERROR, resume_code)
        self.assertIn(f'git checkout {roll_branch}', resume_err)

        checkout = roll_protobuf._ResolveCheckoutPaths(
            self.fixture.protobuf_directory)
        replayed_state = roll_protobuf.LoadRollState(
            self.fixture.protobuf_directory)
        roll_protobuf._SaveRollState(
            checkout,
            dataclasses.replace(
                replayed_state,
                phase='conflict',
                current_index=0,
                conflict_files=('third_party/protobuf/foo.cc', )))
        _RunGitCommand(self.repository_root,
                       ['tag', roll_branch, unrelated_head])
        _, off_conflict_status, _ = _RunMain('status', '--protobuf-dir',
                                             self.fixture.protobuf_directory,
                                             '--json')
        cont_code, _, cont_err = _RunMain('continue', '--protobuf-dir',
                                          self.fixture.protobuf_directory)
        off_conflict = json.loads(off_conflict_status)
        self.assertEqual('conflict', off_conflict['state'])
        self.assertEqual([], off_conflict['files'])
        self.assertEqual([f'git checkout {roll_branch}'], off_conflict['next'])
        self.assertEqual(roll_protobuf.EXIT_CODE_ERROR, cont_code)
        self.assertIn(f'git checkout {roll_branch}', cont_err)

        _RunGitCommand(self.repository_root, ['tag', '-d', roll_branch])
        _RunGitCommand(self.repository_root, ['branch', '-D', roll_branch])
        orphan_cont_code, _, orphan_cont_err = _RunMain(
            'continue', '--protobuf-dir', self.fixture.protobuf_directory)
        orphan_res_code, _, orphan_res_err = _RunMain(
            'resume', '--protobuf-dir', self.fixture.protobuf_directory)
        self.assertEqual(roll_protobuf.EXIT_CODE_ERROR, orphan_cont_code)
        self.assertIn('No conflicting protobuf patch is waiting',
                      orphan_cont_err)
        self.assertEqual(roll_protobuf.EXIT_CODE_ERROR, orphan_res_code)
        self.assertIn('No active protobuf roll to resume', orphan_res_err)
        stale_abort_code, stale_abort_out, _ = _RunMain(
            'abort', '--protobuf-dir', self.fixture.protobuf_directory)
        self.assertEqual(roll_protobuf.EXIT_CODE_SUCCESS, stale_abort_code)
        self.assertEqual('Removed stale roll state for v37.0.',
                         stale_abort_out)
        self.assertEqual(
            unrelated_head,
            _RunGitCommand(self.repository_root, ['rev-parse', 'HEAD']))

        prep_code, _, _ = _RunMain('prepare', '--to', 'v37.0',
                                   '--protobuf-dir',
                                   self.fixture.protobuf_directory,
                                   '--upstream-repo',
                                   self.fixture.upstream_directory)
        self.assertEqual(roll_protobuf.EXIT_CODE_SUCCESS, prep_code)

        # Aborting a `prepared` state removes GITDIR/protobuf-roll/ without
        # resetting switched branches or wiping dirty/untracked files.
        _RunGitCommand(self.repository_root,
                       ['checkout', '--quiet', '-b', 'unrelated_two'])
        _RunGitCommand(
            self.repository_root,
            ['commit', '--quiet', '--allow-empty', '-m', 'Second branch'])
        second_head = _RunGitCommand(self.repository_root,
                                     ['rev-parse', 'HEAD'])
        _WriteFilesToDirectory(self.repository_root,
                               {'OTHER_CHROMIUM_FILE': 'dirty outside\n'})
        _WriteFilesToDirectory(self.fixture.protobuf_directory, {
            'BUILD.gn': '# dirty inside\n',
            'src/untracked.tmp': 'keep\n',
        })
        prep_abort_code, prep_abort_out, _ = _RunMain(
            'abort', '--protobuf-dir', self.fixture.protobuf_directory)
        self.assertEqual(roll_protobuf.EXIT_CODE_SUCCESS, prep_abort_code)
        self.assertEqual('Aborted prepared roll to v37.0.', prep_abort_out)
        self.assertEqual(
            second_head,
            _RunGitCommand(self.repository_root, ['rev-parse', 'HEAD']))
        self.assertIsNone(
            roll_protobuf.LoadRollState(self.fixture.protobuf_directory))
        self.assertEqual(
            'dirty outside\n',
            pathlib.Path(self.repository_root,
                         'OTHER_CHROMIUM_FILE').read_text(encoding='utf-8'))
        self.assertEqual(
            '# dirty inside\n',
            pathlib.Path(self.fixture.protobuf_directory,
                         'BUILD.gn').read_text(encoding='utf-8'))
        self.assertTrue(
            os.path.isfile(
                os.path.join(self.fixture.protobuf_directory, 'src',
                             'untracked.tmp')))

        # If `start` fails during patch replay after importing upstream, `abort`
        # still resets HEAD back to start_commit.
        _RunGitCommand(self.repository_root,
                       ['reset', '--quiet', '--hard', second_head])
        os.remove(
            os.path.join(self.fixture.protobuf_directory, 'src',
                         'untracked.tmp'))
        error_result = roll_protobuf.PatchReplayResult(
            patch_name=_PLAN_CLEAN_PATCH,
            status=roll_protobuf.PatchReplayStatus.ERROR,
            changed_line_count_before=1,
            error_message='corrupt patch',
        )
        with unittest.mock.patch.object(roll_protobuf,
                                        'ApplyChainPatchThreeWay',
                                        return_value=error_result):
            err_start_code, _, err_start_err = _RunMain(
                'start', '--to', 'v37.0', '--protobuf-dir',
                self.fixture.protobuf_directory, '--upstream-repo',
                self.fixture.upstream_directory)
        self.assertEqual(roll_protobuf.EXIT_CODE_ERROR, err_start_code)
        self.assertIn('corrupt patch', err_start_err)
        self.assertNotEqual(
            second_head,
            _RunGitCommand(self.repository_root, ['rev-parse', 'HEAD']))
        reset_abort_code, reset_abort_out, _ = _RunMain(
            'abort', '--protobuf-dir', self.fixture.protobuf_directory)
        self.assertEqual(roll_protobuf.EXIT_CODE_SUCCESS, reset_abort_code)
        self.assertEqual(f'Aborted roll to v37.0 and reset to {second_head}.',
                         reset_abort_out)
        self.assertEqual(
            second_head,
            _RunGitCommand(self.repository_root, ['rev-parse', 'HEAD']))


if __name__ == '__main__':
    unittest.main()
