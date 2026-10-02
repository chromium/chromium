#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for //third_party/protobuf/PRESUBMIT.py.

Exercises the patch-inventory, companion-markdown, leftover-reject/backup, and
source-without-patch presubmit checks against a hermetic in-memory model of
//third_party/protobuf built on top of //PRESUBMIT_test_mocks.py
(https://crbug.com/568001675).
"""

import os
import sys
import unittest

_PROTOBUF_DIRECTORY = os.path.dirname(os.path.abspath(__file__))
_REPOSITORY_ROOT = os.path.abspath(
    os.path.join(_PROTOBUF_DIRECTORY, '..', '..'))
if _PROTOBUF_DIRECTORY not in sys.path:
    sys.path.insert(0, _PROTOBUF_DIRECTORY)
if _REPOSITORY_ROOT not in sys.path:
    sys.path.append(_REPOSITORY_ROOT)

import PRESUBMIT
from PRESUBMIT_test_mocks import (
    MockAffectedFile,
    MockInputApi,
    MockOutputApi,
)

_BASELINE_README_LINES = (
    'Name: Protocol Buffers',
    'Version: 36.0',
    '',
    'Description of the patches:',
    '',
    '- 0004-fix-shared-library-exports.patch',
    '',
    '  Exports symbols on Linux component builds.',
    '',
    '- 0044-trim-protoc-main.patch',
    '',
    '  Trims unused generators. See `patches/0044-trim-protoc-main.md`',
    '  for details.',
    '',
    '- 0052-remove-dynamic-annotations.patch',
    '',
    '  Removes dynamic annotations.',
    '',
    '- 0070-fix-deprecated-get-enum-in-message.patch',
    '',
    '  Removes [[deprecated]] from GetEnum.',
)

_BASELINE_TOP_LEVEL_PATCH_FILES = (
    '0004-fix-shared-library-exports.patch',
    '0044-trim-protoc-main.patch',
    '0044-trim-protoc-main.md',
    '0052-remove-dynamic-annotations.patch',
    '0070-fix-deprecated-get-enum-in-message.patch',
)


def _RepositoryRelativeProtobufPath(relative_path_in_protobuf):
    """Returns a repository-root-relative path under third_party/protobuf/."""
    return os.path.join('third_party', 'protobuf', relative_path_in_protobuf)


class ProtobufPresubmitTest(unittest.TestCase):
    """Verifies the presubmit invariants for //third_party/protobuf.

    Each test configures a hermetic in-memory directory tree rooted at
    _PROTOBUF_DIRECTORY so that tests never depend on uncommitted state in the
    developer's working tree. Upstream's own 'patches/protobuf_v25/'
    subdirectory is always populated in the fixture to prove that every check
    ignores upstream's nested patch directories.
    """

    def setUp(self):
        self.input_api = MockInputApi()
        self.input_api.presubmit_local_path = _PROTOBUF_DIRECTORY
        self.output_api = MockOutputApi()
        self._ConfigureProtobufTree(
            readme_lines=list(_BASELINE_README_LINES),
            top_level_patches_files=set(_BASELINE_TOP_LEVEL_PATCH_FILES),
            additional_tree_files=set(),
        )

    def _ConfigureProtobufTree(
            self,
            readme_lines,
            top_level_patches_files,
            additional_tree_files=frozenset()):
        """Populates MockInputApi filesystem hooks from an in-memory tree.

        Args:
          readme_lines: Lines of README.chromium, or None to simulate a missing
            README.chromium file.
          top_level_patches_files: Filenames present directly under
            //third_party/protobuf/patches/.
          additional_tree_files: Additional paths relative to
            //third_party/protobuf (such as 'foo.rej') present on disk.
        """
        file_lines_by_relative_path = {}
        if readme_lines is not None:
            file_lines_by_relative_path['README.chromium'] = list(readme_lines)
        for filename in top_level_patches_files:
            file_lines_by_relative_path[f'patches/{filename}'] = ['diff']
        # Upstream protobuf ships Bazel module patches in subdirectories under
        # patches/; include one unconditionally to verify checks ignore it.
        file_lines_by_relative_path[
            'patches/protobuf_v25/0001-Add-MODULE.bazel.patch'] = ['upstream']
        file_lines_by_relative_path[
            'patches/protobuf_v25/README.md'] = ['upstream notes']
        for relative_path in additional_tree_files:
            file_lines_by_relative_path[relative_path] = ['content']

        files_by_absolute_path = {
            os.path.normpath(os.path.join(_PROTOBUF_DIRECTORY, relative_path)):
            lines
            for relative_path, lines in file_lines_by_relative_path.items()
        }
        directories_by_absolute_path = {
            os.path.normpath(_PROTOBUF_DIRECTORY),
            os.path.normpath(os.path.join(_PROTOBUF_DIRECTORY, 'patches')),
            os.path.normpath(
                os.path.join(_PROTOBUF_DIRECTORY, 'patches', 'protobuf_v25')),
        }
        for absolute_file_path in files_by_absolute_path:
            parent_directory = os.path.dirname(absolute_file_path)
            while parent_directory.startswith(_PROTOBUF_DIRECTORY):
                directories_by_absolute_path.add(parent_directory)
                parent_directory = os.path.dirname(parent_directory)

        self.input_api.os_path.isfile = (
            lambda path: os.path.normpath(path) in files_by_absolute_path)
        self.input_api.os_path.isdir = (
            lambda path: os.path.normpath(path) in directories_by_absolute_path)
        self.input_api.os_path.exists = (
            lambda path: os.path.normpath(path) in files_by_absolute_path
            or os.path.normpath(path) in directories_by_absolute_path)

        def list_directory(directory_path):
            normalized_directory = os.path.normpath(directory_path)
            child_names = set()
            for file_path in files_by_absolute_path:
                if os.path.dirname(file_path) == normalized_directory:
                    child_names.add(os.path.basename(file_path))
            for subdirectory_path in directories_by_absolute_path:
                if (subdirectory_path != normalized_directory
                        and os.path.dirname(subdirectory_path)
                        == normalized_directory):
                    child_names.add(os.path.basename(subdirectory_path))
            return sorted(child_names)

        def walk_directory_tree(top_directory):
            normalized_top = os.path.normpath(top_directory)
            for directory_path in sorted(directories_by_absolute_path):
                if not directory_path.startswith(normalized_top):
                    continue
                subdirectories = sorted(
                    os.path.basename(candidate)
                    for candidate in directories_by_absolute_path
                    if candidate != directory_path
                    and os.path.dirname(candidate) == directory_path)
                filenames = sorted(
                    os.path.basename(candidate)
                    for candidate in files_by_absolute_path
                    if os.path.dirname(candidate) == directory_path)
                yield directory_path, subdirectories, filenames

        def read_file(file_item, mode='r'):
            del mode
            if hasattr(file_item, 'AbsoluteLocalPath'):
                file_item = file_item.AbsoluteLocalPath()
            normalized_path = os.path.normpath(file_item)
            if normalized_path in files_by_absolute_path:
                return '\n'.join(files_by_absolute_path[normalized_path])
            raise IOError(f'No such file: {file_item}')

        self.input_api.os_listdir = list_directory
        self.input_api.os_walk = walk_directory_tree
        self.input_api.ReadFile = read_file

    def testMatchingPatchesAndReadmeProduceNoErrors(self):
        results = PRESUBMIT._RunAllProtobufChecks(
            self.input_api, self.output_api)
        self.assertEqual(
            [],
            results,
            msg='Expected no presubmit findings when patches/ and '
            'README.chromium are in sync and no .rej/.orig files exist.',
        )

    def testUndocumentedPatchFileInPatchesDirectoryReportsError(self):
        patches_with_undocumented_file = (
            set(_BASELINE_TOP_LEVEL_PATCH_FILES) | {'0099-undocumented.patch'})
        self._ConfigureProtobufTree(
            _BASELINE_README_LINES, patches_with_undocumented_file)

        results = PRESUBMIT.CheckPatchInventoryInSync(
            self.input_api, self.output_api)

        self.assertEqual(
            ['patches/0099-undocumented.patch: present in patches/ but '
             'missing from README.chromium'],
            [item for result in results for item in result.items],
            msg='Expected CheckPatchInventoryInSync to report a top-level '
            '.patch file that is missing from README.chromium.',
        )

    def testListedPatchInReadmeMissingFromPatchesDirectoryReportsError(self):
        patches_missing_committed_file = (
            set(_BASELINE_TOP_LEVEL_PATCH_FILES)
            - {'0070-fix-deprecated-get-enum-in-message.patch'})
        self._ConfigureProtobufTree(
            _BASELINE_README_LINES, patches_missing_committed_file)

        results = PRESUBMIT.CheckPatchInventoryInSync(
            self.input_api, self.output_api)

        self.assertEqual(
            ['0070-fix-deprecated-get-enum-in-message.patch: listed in '
             'README.chromium but missing from patches/'],
            [item for result in results for item in result.items],
            msg='Expected CheckPatchInventoryInSync to report a patch entry in '
            'README.chromium whose .patch file is missing from patches/.',
        )

    def testMisspelledPatchEntryInReadmeReportsBothSides(self):
        readme_with_typo = [
            line.replace(
                '- 0052-remove-dynamic-annotations.patch',
                '- 0052-remove-dynanmic-annotations.patch',
            ) for line in _BASELINE_README_LINES
        ]
        self._ConfigureProtobufTree(
            readme_with_typo, _BASELINE_TOP_LEVEL_PATCH_FILES)

        results = PRESUBMIT.CheckPatchInventoryInSync(
            self.input_api, self.output_api)

        self.assertEqual(
            [
                'patches/0052-remove-dynamic-annotations.patch: present in '
                'patches/ but missing from README.chromium',
                '0052-remove-dynanmic-annotations.patch: listed in '
                'README.chromium but missing from patches/',
            ],
            [item for result in results for item in result.items],
            msg='Expected a misspelled README.chromium patch entry to report '
            'both the undocumented file on disk and the missing entry name.',
        )

    def testDuplicatePatchEntryInReadmeReportsError(self):
        readme_with_duplicate = list(_BASELINE_README_LINES) + [
            '- 0052-remove-dynamic-annotations.patch'
        ]
        self._ConfigureProtobufTree(
            readme_with_duplicate, _BASELINE_TOP_LEVEL_PATCH_FILES)

        results = PRESUBMIT.CheckPatchInventoryInSync(
            self.input_api, self.output_api)

        self.assertEqual(
            ['0052-remove-dynamic-annotations.patch: listed more than once in '
             'README.chromium'],
            [item for result in results for item in result.items],
            msg='Expected duplicate bullet entries in README.chromium to be '
            'flagged as an error.',
        )

    def testReferencedCompanionMarkdownMissingFromDiskReportsError(self):
        readme_with_missing_markdown_reference = list(_BASELINE_README_LINES) + [
            '  See `patches/0004-fix-shared-library-exports.md` for details.'
        ]
        self._ConfigureProtobufTree(
            readme_with_missing_markdown_reference,
            _BASELINE_TOP_LEVEL_PATCH_FILES,
        )

        results = PRESUBMIT.CheckCompanionMarkdownFiles(
            self.input_api, self.output_api)

        self.assertEqual(
            ['patches/0004-fix-shared-library-exports.md: referenced in '
             'README.chromium but does not exist'],
            [item for result in results for item in result.items],
            msg='Expected a companion patches/*.md referenced in '
            'README.chromium to fail when the file does not exist.',
        )

    def testCompanionMarkdownWithoutMatchingPatchFileReportsError(self):
        patches_with_orphan_markdown = (
            set(_BASELINE_TOP_LEVEL_PATCH_FILES) | {'0099-orphan-note.md'})
        self._ConfigureProtobufTree(
            _BASELINE_README_LINES, patches_with_orphan_markdown)

        results = PRESUBMIT.CheckCompanionMarkdownFiles(
            self.input_api, self.output_api)

        self.assertEqual(
            ['patches/0099-orphan-note.md: has no matching '
             'patches/0099-orphan-note.patch file'],
            [item for result in results for item in result.items],
            msg='Expected a top-level patches/*.md without a matching .patch '
            'file to fail.',
        )

    def testInstructionalPlaceholderMarkdownInReadmeIsIgnored(self):
        readme_with_placeholder = [
            'See patches/NNNN-name.md where one exists.',
        ] + list(_BASELINE_README_LINES)
        self._ConfigureProtobufTree(
            readme_with_placeholder, _BASELINE_TOP_LEVEL_PATCH_FILES)

        results = PRESUBMIT.CheckCompanionMarkdownFiles(
            self.input_api, self.output_api)

        self.assertEqual(
            [],
            results,
            msg='Expected the literal instructional placeholder '
            'patches/NNNN-name.md in README.chromium prose to be ignored.',
        )

    def testLeftoverRejectAndBackupFilesReportError(self):
        self._ConfigureProtobufTree(
            _BASELINE_README_LINES,
            _BASELINE_TOP_LEVEL_PATCH_FILES,
            additional_tree_files={
                'foo.rej',
                'src/google/protobuf/message.h.orig',
            },
        )

        results = PRESUBMIT.CheckNoPatchRejectOrOrigFiles(
            self.input_api, self.output_api)

        self.assertEqual(
            ['foo.rej', 'src/google/protobuf/message.h.orig'],
            [item for result in results for item in result.items],
            msg='Expected leftover .rej and .orig files anywhere under '
            '//third_party/protobuf to be reported as errors.',
        )

    def testModifyingProtobufSourceWithoutTopLevelPatchPromptsWarning(self):
        self.input_api.files = [
            MockAffectedFile(
                _RepositoryRelativeProtobufPath('src/google/protobuf/message.h'),
                ['// edited'],
                action='M',
            ),
            MockAffectedFile(
                _RepositoryRelativeProtobufPath(
                    'python/google/protobuf/message.py'),
                ['# edited'],
                action='M',
            ),
        ]

        results = PRESUBMIT.CheckSourceModifiedWithoutPatch(
            self.input_api, self.output_api)

        self.assertEqual(
            [
                ('warning', [
                    'python/google/protobuf/message.py',
                    'src/google/protobuf/message.h',
                ]),
            ],
            [(result.type, result.items) for result in results],
            msg='Expected modifying src/ or python/ files without touching '
            'patches/ to emit a prompt warning listing those files.',
        )

    def testModifyingProtobufSourceWithTopLevelPatchProducesNoWarning(self):
        self.input_api.files = [
            MockAffectedFile(
                _RepositoryRelativeProtobufPath('src/google/protobuf/message.h'),
                ['// edited'],
                action='M',
            ),
            MockAffectedFile(
                _RepositoryRelativeProtobufPath(
                    'patches/0070-fix-deprecated-get-enum-in-message.patch'),
                ['diff --git a/src/google/protobuf/message.h'],
                action='M',
            ),
        ]

        results = PRESUBMIT.CheckSourceModifiedWithoutPatch(
            self.input_api, self.output_api)

        self.assertEqual(
            [],
            results,
            msg='Expected no warning when a src/ change is accompanied by a '
            'top-level patches/*.patch update.',
        )

    def testModifyingProtobufSourceWithOnlyUpstreamSubdirectoryPatchWarns(self):
        self.input_api.files = [
            MockAffectedFile(
                _RepositoryRelativeProtobufPath('src/google/protobuf/message.h'),
                ['// edited'],
                action='M',
            ),
            MockAffectedFile(
                _RepositoryRelativeProtobufPath(
                    'patches/protobuf_v25/0001-Add-MODULE.bazel.patch'),
                ['diff --git a/MODULE.bazel b/MODULE.bazel'],
                action='M',
            ),
        ]

        results = PRESUBMIT.CheckSourceModifiedWithoutPatch(
            self.input_api, self.output_api)

        self.assertEqual(
            [('warning', ['src/google/protobuf/message.h'])],
            [(result.type, result.items) for result in results],
            msg='Expected changes in upstream patches/protobuf_vNN/ '
            'subdirectories not to satisfy the top-level patch requirement.',
        )

    def testModifyingBootstrapGeneratedFilesWithoutPatchProducesNoWarning(self):
        self.input_api.files = [
            MockAffectedFile(
                _RepositoryRelativeProtobufPath(generated_path),
                ['// regenerated by gen_extra_chromium_files.py'],
                action='M',
            )
            for generated_path in sorted(PRESUBMIT._BOOTSTRAP_GENERATED_FILES)
        ]

        results = PRESUBMIT.CheckSourceModifiedWithoutPatch(
            self.input_api, self.output_api)

        self.assertEqual(
            [],
            results,
            msg='Expected bootstrap files generated by '
            'gen_extra_chromium_files.py not to trigger a missing-patch '
            'warning.',
        )

    def testFilesOutsideProtobufDirectoryAreIgnored(self):
        self.input_api.files = [
            MockAffectedFile('src/other_component/foo.cc', ['// outside']),
            MockAffectedFile('base/foo.rej', ['// outside']),
        ]

        results = PRESUBMIT._RunAllProtobufChecks(
            self.input_api, self.output_api)

        self.assertEqual(
            [],
            results,
            msg='Expected files outside //third_party/protobuf to be ignored.',
        )


if __name__ == '__main__':
    unittest.main()
