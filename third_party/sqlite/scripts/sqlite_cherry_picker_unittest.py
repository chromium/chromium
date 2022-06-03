#!/usr/bin/env python3

# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for sqlite_cherry_picker.py.

These tests should be getting picked up by the PRESUBMIT.py in the parent
directory.
"""

from pathlib import Path
import io
import os
import shutil
import tempfile
import unittest
import sqlite_cherry_picker

# pylint: disable=W0212,C0103,C0115,C0116


class CherryPickerUnittest(unittest.TestCase):
    def setUp(self):
        self.test_root = tempfile.mkdtemp()

    def tearDown(self):
        if self.test_root:
            shutil.rmtree(self.test_root)

    def testManifestEntryConstructor(self):
        with self.assertRaises(sqlite_cherry_picker.IncorrectType):
            entry = sqlite_cherry_picker.ManifestEntry('DD', [
                'vsixtest/vsixtest.tcl',
                '6a9a6ab600c25a91a7acc6293828957a386a8a93'
            ])
            del entry  # unused because exception will be raised.

    def testManifestEntryHashType(self):
        entry = sqlite_cherry_picker.ManifestEntry('F', [
            'vsixtest/vsixtest.tcl', '6a9a6ab600c25a91a7acc6293828957a386a8a93'
        ])
        self.assertEqual(entry.get_hash_type(), 'sha1')

        entry = sqlite_cherry_picker.ManifestEntry('F', [
            'tool/warnings-clang.sh',
            'bbf6a1e685e534c92ec2bfba5b1745f34fb6f0bc2a362850723a9ee87c1b31a7'
        ])
        self.assertEqual(entry.get_hash_type(), 'sha3')

        # test a bad hash which is too short and cannot be identified as either
        # a sha1 or sha3 hash.
        with self.assertRaises(sqlite_cherry_picker.UnknownHash):
            entry = sqlite_cherry_picker.ManifestEntry(
                'F', ['file/path.sh', '12345678'])
            entry.get_hash_type()

    def testManifestEntryHashCalc(self):
        data = 'abcdefghijklmnopqrstuvwxyDEFGHIJKLMUVWXYZ0123456789'.encode(
            'utf-8')
        self.assertEqual(
            sqlite_cherry_picker.ManifestEntry.calc_hash(data, 'sha1'),
            'e117bfe4bcb1429cf8a0f72f8f4ea322a9a500eb')
        self.assertEqual(
            sqlite_cherry_picker.ManifestEntry.calc_hash(data, 'sha3'),
            '2cb59ee01402c45e7c56008b7ca33d006dbd980a5e222fac3a584f5639a450d7')

    def testManifestFindEntry(self):
        manifest = sqlite_cherry_picker.Manifest()
        self.assertIsNone(manifest.find_file_entry('nonexistent'))
        manifest.entries = [
            sqlite_cherry_picker.ManifestEntry('F', [
                'tool/warnings-clang.sh',
                'bbf6a1e685e534c92ec2bfba5b1745f34fb6f0bc2a362850723a9ee87c1b31a7'
            ]),
            sqlite_cherry_picker.ManifestEntry('T',
                                               ['+bgcolor', '*', '#d0c0ff'])
        ]

        entry = manifest.find_file_entry('tool/warnings-clang.sh')
        self.assertIsNotNone(entry)
        self.assertEqual(entry.items[0], 'tool/warnings-clang.sh')

        # Should only find files.
        self.assertIsNone(manifest.find_file_entry('+bgcolor'))

    def testManifestDeserialize(self):
        manfest_path = os.path.join(
            os.path.dirname(os.path.realpath(__file__)), 'test_data',
            'test_manifest')
        manifest_data = Path(manfest_path).read_text()
        input_stream = io.StringIO(manifest_data)
        manifest = sqlite_cherry_picker.ManifestSerializer.read_stream(
            input_stream)
        self.assertEqual(len(manifest.entries), 9)

        output_stream = io.StringIO()
        sqlite_cherry_picker.ManifestSerializer.write_stream(
            manifest, output_stream)
        self.assertEqual(output_stream.getvalue(), manifest_data)

    def testGitHash(self):
        valid_git_commit_id = '61c3ca1b1c77bbcaa35f9326decf3658bdb5626a'
        self.assertTrue(
            sqlite_cherry_picker.CherryPicker._is_git_commit_id(
                valid_git_commit_id))

        invalid_git_commit_id = 'f3658bdb5626a'
        self.assertFalse(
            sqlite_cherry_picker.CherryPicker._is_git_commit_id(
                invalid_git_commit_id))

    def testIsBinaryFile(self):
        self.assertTrue(
            sqlite_cherry_picker.CherryPicker._is_binary_file(
                'path/test.data'))
        self.assertTrue(
            sqlite_cherry_picker.CherryPicker._is_binary_file('path/test.db'))
        self.assertTrue(
            sqlite_cherry_picker.CherryPicker._is_binary_file('path/test.ico'))
        self.assertTrue(
            sqlite_cherry_picker.CherryPicker._is_binary_file('path/test.jpg'))
        self.assertTrue(
            sqlite_cherry_picker.CherryPicker._is_binary_file('path/test.png'))

        self.assertFalse(
            sqlite_cherry_picker.CherryPicker._is_binary_file('path/test.c'))
        self.assertFalse(
            sqlite_cherry_picker.CherryPicker._is_binary_file('path/test.h'))


if __name__ == '__main__':
    unittest.main()
