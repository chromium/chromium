#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for //tools/licenses/licenses.py.
"""

import argparse
import contextlib
import csv
import io
import os
import re
import pathlib
import sys
import unittest


REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..'))
sys.path.append(os.path.join(REPOSITORY_ROOT, 'tools', 'licenses'))

import licenses
from test_utils import path_from_root


def construct_absolute_path(path):
  return str(pathlib.PurePosixPath(REPOSITORY_ROOT.replace(os.sep, '/'), path))


class LicensesTest(unittest.TestCase):
  def _get_metadata(self):
    return {
        os.path.join('third_party', 'lib1'): [{
            'Name': 'lib1',
            'Shipped': 'yes',
            'License': 'MIT',
            'License File': [os.path.join('third_party', 'lib1', 'LICENSE')],
        }],
        os.path.join('third_party', 'lib2'): [{
            'Name': 'lib2',
            'Shipped': 'yes',
            'License': 'MIT, Apache 2.0',
            'License File': [
                os.path.join('third_party', 'lib2', 'LICENSE-A'),
                os.path.join('third_party', 'lib2', 'LICENSE-B'),
            ],
        }],
        'ignored': [{
            'Name': 'ignored',
            'Shipped': 'no',
            'License File': [],
        }],
        os.path.join('third_party', 'lib_unshipped'): [{
            'Name': 'lib_unshipped',
            'Shipped': 'no',
            'License': '',
            'License File': [
                os.path.join('third_party', 'lib_unshipped', 'LICENSE')
            ],
        }],
        os.path.join('third_party', 'lib3'): [{
            'Name': 'lib3',
            'Shipped': 'yes',
            'License': '',
            'License File': [os.path.join('third_party', 'lib3', 'LICENSE')],
        }],
        os.path.join('third_party', 'lib3-v1'): [{
            # Test SPDX license file dedup. (different name, same license file)
            'Name': 'lib3-v1',
            'Shipped': 'yes',
            'License': 'Apache 2.0',
            'License File': [
                os.path.join('third_party', 'lib3', 'LICENSE'),
            ],
        }],
        os.path.join('third_party', 'lib3-v2'): [{
            # Test SPDX id dedup. (same name, different license file)
            'Name': 'lib3',
            'Shipped': 'yes',
            'License': 'BSD',
            'License File': [os.path.join('third_party', 'lib3-v2', 'LICENSE')],
        }],
    }

  def test_parse_dir(self):
    # No metadata file found in directory
    test_path = os.path.join('tools', 'licenses', 'test_dir_with_missing_files')
    with self.assertRaisesRegex(
        licenses.LicenseError,
        "missing third party metadata file or licenses.py SPECIAL_CASES"):
      licenses.ParseDir(test_path, REPOSITORY_ROOT)

    test_path = os.path.join('tools', 'licenses', 'test_dir')
    dir_metadata, errors = licenses.ParseDir(test_path, REPOSITORY_ROOT)
    expected = [
        {
            'License File': [
                os.path.join(REPOSITORY_ROOT, test_path, 'LICENSE')
            ],
            'Name': 'License tools directory parsing test',
            'URL': 'https://chromium.tools.licenses.test/src.git',
            'License': 'FAKE',
            'Shipped': 'no',
        },
        {
            'License File': [
                os.path.join(REPOSITORY_ROOT, test_path, 'LICENSE')
            ],
            'Name': 'License tools directory parsing test for multiple',
            'URL': 'https://chromium.tools.licenses.test/multi/src.git',
            'License': 'FAKE',
            'Shipped': 'no',
        },
    ]
    self.assertListEqual(errors, [])
    self.assertListEqual(dir_metadata, expected)

  def test_get_third_party_deps_from_gn_deps_output(self):
    prune_path = next(iter(licenses.PRUNE_PATHS))
    gn_deps = [
        construct_absolute_path('net/BUILD.gn'),
        construct_absolute_path('third_party/zlib/BUILD.gn'),
        construct_absolute_path('third_party/cld_3/src/src/BUILD.gn'),
        construct_absolute_path(prune_path + '/BUILD.gn'),
        construct_absolute_path('external/somelib/BUILD.gn'),
    ]
    third_party_deps = licenses.GetThirdPartyDepsFromGNDepsOutput(
        '\n'.join(gn_deps), None)

    # 'net' is not in the output because it's not a third_party dependency.
    #
    # It must return the direct sub-directory of "third_party". So it should
    # return 'third_party/cld_3', not 'third_party/cld_3/src/src'.
    self.assertEqual(
        third_party_deps,
        set([
            os.path.join('third_party', 'zlib'),
            os.path.join('third_party', 'cld_3'),
        ]))

  def test_get_third_party_deps_from_gn_deps_output_extra_dirs(self):
    prune_path = next(iter(licenses.PRUNE_PATHS))
    gn_deps = [
        construct_absolute_path('net/BUILD.gn'),
        construct_absolute_path('third_party/zlib/BUILD.gn'),
        construct_absolute_path('third_party/cld_3/src/src/BUILD.gn'),
        construct_absolute_path(prune_path + '/BUILD.gn'),
        construct_absolute_path('external/somelib/BUILD.gn'),
    ]
    third_party_deps = licenses.GetThirdPartyDepsFromGNDepsOutput(
        '\n'.join(gn_deps), None, ['external'])

    self.assertEqual(
        third_party_deps,
        set([
            os.path.join('third_party', 'zlib'),
            os.path.join('third_party', 'cld_3'),
            os.path.join('external', 'somelib'),
        ]))

  def test_generate_license_file_csv(self):
    # This is the same for all the links and prevents wildly long strings.
    prefix = ("https://source.chromium.org/chromium/chromium/src/+/main:")

    csv_file = io.StringIO(licenses.GenerateLicenseFileCsv(
        self._get_metadata()))
    csv_rows = [row for row in csv.DictReader(csv_file)]

    expected = [{
        'Library Name': 'Chromium',
        'Link to LICENSE file': f'{prefix}LICENSE',
        'License Name': 'BSD 3-Clause',
        'Binary which uses library': 'Chromium',
        'License text for library included?': 'Yes',
        'Source code for library includes the mirrored source?': 'Yes',
        'Authorization date': 'N/A'
    }, {
        'Library Name': 'lib1',
        'Link to LICENSE file': (
            f'{prefix}tools/licenses/third_party/lib1/LICENSE'),
        'License Name': 'MIT',
        'Binary which uses library': 'Chromium',
        'License text for library included?': 'Yes',
        'Source code for library includes the mirrored source?': 'Yes',
        'Authorization date': 'N/A'
    }, {
        'Library Name': 'lib2',
        'Link to LICENSE file': (
            f'{prefix}tools/licenses/third_party/lib2/LICENSE-A, '
            f'{prefix}tools/licenses/third_party/lib2/LICENSE-B'),
        'License Name': 'MIT, Apache 2.0',
        'Binary which uses library': 'Chromium',
        'License text for library included?': 'Yes',
        'Source code for library includes the mirrored source?': 'Yes',
        'Authorization date': 'N/A'
    }, {
        'Library Name': 'lib3',
        'Link to LICENSE file': (
            f'{prefix}tools/licenses/third_party/lib3/LICENSE'),
        'License Name': 'UNKNOWN',
        'Binary which uses library': 'Chromium',
        'License text for library included?': 'Yes',
        'Source code for library includes the mirrored source?': 'Yes',
        'Authorization date': 'N/A'
    }, {
        'Library Name': 'lib3-v1',
        'Link to LICENSE file': (
            f'{prefix}tools/licenses/third_party/lib3/LICENSE'),
        'License Name': 'Apache 2.0',
        'Binary which uses library': 'Chromium',
        'License text for library included?': 'Yes',
        'Source code for library includes the mirrored source?': 'Yes',
        'Authorization date': 'N/A'
    }, {
        'Library Name': 'lib3',
        'Link to LICENSE file': (
            f'{prefix}tools/licenses/third_party/lib3-v2/LICENSE'),
        'License Name': 'BSD',
        'Binary which uses library': 'Chromium',
        'License text for library included?': 'Yes',
        'Source code for library includes the mirrored source?': 'Yes',
        'Authorization date': 'N/A'
    }]

    expected.sort(key=lambda item: item['Library Name'])
    self.assertEqual(csv_rows, expected)

  def test_generate_license_file_txt(self):
    read_file_vals = [
        'root license text\n',
        'lib1 license text\n',
        'lib2-a license text\n',
        'lib2-b license text\n',
        'lib3 license text\n',
        'lib3 license text\n',
        'lib3-v2 license text\n',
    ]

    license_txt = licenses.GenerateLicenseFilePlainText(
        self._get_metadata(), read_file=lambda _: read_file_vals.pop(0))

    expected = '\n'.join([
        'root license text',
        '',
        '--------------------',
        'lib1',
        '--------------------',
        'lib1 license text',
        '',
        '--------------------',
        'lib2',
        '--------------------',
        'lib2-a license text',
        '',
        'lib2-b license text',
        '',
        '--------------------',
        'lib3',
        '--------------------',
        'lib3 license text',
        '',
        '--------------------',
        'lib3-v1',
        '--------------------',
        'lib3 license text',
        '',
        '--------------------',
        'lib3',
        '--------------------',
        'lib3-v2 license text',
    ]) + '\n'  # extra new line to account for join not adding one to the end
    self.assertEqual(license_txt, expected)

  def test_generate_license_file_sdpx(self):
    read_file_vals = [
        'root\nlicense text\n',
        'lib1\nlicense text\n',
        'lib2-a\nlicense text\n',
        'lib2-b\nlicense text\n',
        'lib3\nlicense text\n',
        'lib3-v2\nlicense text\n',
    ]

    license_txt = licenses.GenerateLicenseFileSpdx(
        self._get_metadata(),
        'http://google.com',
        path_from_root('src'),
        'mydoc',
        'http://google.com',
        repo_root=path_from_root('src'),
        read_file=lambda _: read_file_vals.pop(0))

    expected = '''{
    "spdxVersion": "SPDX-2.2",
    "SPDXID": "SPDXRef-DOCUMENT",
    "name": "mydoc",
    "documentNamespace": "http://google.com",
    "creationInfo": {
        "creators": [
            "Tool: spdx_writer.py"
        ]
    },
    "dataLicense": "CC0-1.0",
    "documentDescribes": [
        "SPDXRef-Package-Chromium"
    ],
    "packages": [
        {
            "SPDXID": "SPDXRef-Package-Chromium",
            "name": "Chromium",
            "licenseConcluded": "LicenseRef-Chromium"
        },
        {
            "SPDXID": "SPDXRef-Package-lib1",
            "name": "lib1",
            "licenseConcluded": "LicenseRef-lib1"
        },
        {
            "SPDXID": "SPDXRef-Package-lib2",
            "name": "lib2",
            "licenseConcluded": "LicenseRef-lib2"
        },
        {
            "SPDXID": "SPDXRef-Package-lib2-1",
            "name": "lib2",
            "licenseConcluded": "LicenseRef-lib2-1"
        },
        {
            "SPDXID": "SPDXRef-Package-lib3",
            "name": "lib3",
            "licenseConcluded": "LicenseRef-lib3"
        },
        {
            "SPDXID": "SPDXRef-Package-lib3-v1",
            "name": "lib3-v1",
            "licenseConcluded": "LicenseRef-lib3"
        },
        {
            "SPDXID": "SPDXRef-Package-lib3-1",
            "name": "lib3",
            "licenseConcluded": "LicenseRef-lib3-1"
        }
    ],
    "hasExtractedLicensingInfos": [
        {
            "name": "Chromium",
            "licenseId": "LicenseRef-Chromium",
            "extractedText": "root\\nlicense text\\n",
            "crossRefs": [
                {
                    "url": "http://google.com/LICENSE"
                }
            ]
        },
        {
            "name": "lib1",
            "licenseId": "LicenseRef-lib1",
            "extractedText": "lib1\\nlicense text\\n",
            "crossRefs": [
                {
                    "url": "http://google.com/third_party/lib1/LICENSE"
                }
            ]
        },
        {
            "name": "lib2",
            "licenseId": "LicenseRef-lib2",
            "extractedText": "lib2-a\\nlicense text\\n",
            "crossRefs": [
                {
                    "url": "http://google.com/third_party/lib2/LICENSE-A"
                }
            ]
        },
        {
            "name": "lib2",
            "licenseId": "LicenseRef-lib2-1",
            "extractedText": "lib2-b\\nlicense text\\n",
            "crossRefs": [
                {
                    "url": "http://google.com/third_party/lib2/LICENSE-B"
                }
            ]
        },
        {
            "name": "lib3",
            "licenseId": "LicenseRef-lib3",
            "extractedText": "lib3\\nlicense text\\n",
            "crossRefs": [
                {
                    "url": "http://google.com/third_party/lib3/LICENSE"
                }
            ]
        },
        {
            "name": "lib3",
            "licenseId": "LicenseRef-lib3-1",
            "extractedText": "lib3-v2\\nlicense text\\n",
            "crossRefs": [
                {
                    "url": "http://google.com/third_party/lib3-v2/LICENSE"
                }
            ]
        }
    ],
    "relationships": [
        {
            "spdxElementId": "SPDXRef-Package-Chromium",
            "relationshipType": "CONTAINS",
            "relatedSpdxElement": "SPDXRef-Package-lib1"
        },
        {
            "spdxElementId": "SPDXRef-Package-Chromium",
            "relationshipType": "CONTAINS",
            "relatedSpdxElement": "SPDXRef-Package-lib2"
        },
        {
            "spdxElementId": "SPDXRef-Package-Chromium",
            "relationshipType": "CONTAINS",
            "relatedSpdxElement": "SPDXRef-Package-lib2-1"
        },
        {
            "spdxElementId": "SPDXRef-Package-Chromium",
            "relationshipType": "CONTAINS",
            "relatedSpdxElement": "SPDXRef-Package-lib3"
        },
        {
            "spdxElementId": "SPDXRef-Package-Chromium",
            "relationshipType": "CONTAINS",
            "relatedSpdxElement": "SPDXRef-Package-lib3-v1"
        },
        {
            "spdxElementId": "SPDXRef-Package-Chromium",
            "relationshipType": "CONTAINS",
            "relatedSpdxElement": "SPDXRef-Package-lib3-1"
        }
    ]
}'''
    self.assertEqual(license_txt, expected)

  def test_generate_notice_file(self):
    read_file_vals = [
        'lib1 license text\n',
        'lib2-a license text\n',
        'lib2-b license text\n',
        'lib3 license text\n',
        'lib3 license text\n',
        'lib3-v2 license text\n',
    ]

    license_txt = licenses.GenerateNoticeFilePlainText(
        self._get_metadata(), read_file=lambda _: read_file_vals.pop(0))

    expected = '\n'.join([
        '--------------------',
        'License notice for lib1',
        '--------------------',
        'lib1 license text',
        '',
        '--------------------',
        'License notice for lib2',
        '--------------------',
        'lib2-a license text',
        '',
        '--------------------',
        'License notice for lib2',
        '--------------------',
        'lib2-b license text',
        '',
        '--------------------',
        'License notice for lib3',
        '--------------------',
        'lib3 license text',
        '',
        '--------------------',
        'License notice for lib3-v1',
        '--------------------',
        'lib3 license text',
        '',
        '--------------------',
        'License notice for lib3',
        '--------------------',
        'lib3-v2 license text',
    ]) + '\n'  # extra new line to account for join not adding one to the end
    self.assertEqual(license_txt, expected)

  def test_configuring_gen_license_file_warnings(self):
    """
    Tests that warnings are silenced in GenerateLicenseFile by default and
    not logged unless enable_warnings is True.
    """
    root_dir = os.path.join(REPOSITORY_ROOT, 'tools', 'licenses')
    args = argparse.Namespace(format="txt",
                              gn_target=None,
                              enable_warnings=True,
                              output_file=None)

    # Warnings enabled
    args.extra_third_party_dirs = ["test_dir_invalid_metadata"]
    with contextlib.redirect_stdout(io.StringIO()) as captured_output:
      licenses.GenerateLicenseFile(args, root_dir=root_dir)
    self.assertRegex(captured_output.getvalue(),
                     r"Errors in test_dir_invalid_metadata")

    args.extra_third_party_dirs = ["test_dir_with_missing_files"]
    with contextlib.redirect_stdout(io.StringIO()) as captured_output:
      licenses.GenerateLicenseFile(args, root_dir=root_dir)
    self.assertRegex(
        captured_output.getvalue(),
        r"Error: missing third party .* test_dir_with_missing_files")

    # # Warnings disabled
    args.enable_warnings = False
    args.extra_third_party_dirs = ["test_dir_invalid_metadata"]
    with contextlib.redirect_stdout(io.StringIO()) as captured_output:
      licenses.GenerateLicenseFile(args, root_dir=root_dir)
    self.assertNotRegex(captured_output.getvalue(),
                        r"Errors in test_dir_invalid_metadata")

    args.extra_third_party_dirs = ["test_dir_with_missing_files"]
    with contextlib.redirect_stdout(io.StringIO()) as captured_output:
      licenses.GenerateLicenseFile(args, root_dir=root_dir)
      self.assertNotRegex(
          captured_output.getvalue(),
          r"Error: missing third party .* test_dir_with_missing_files")


if __name__ == '__main__':
  unittest.main()
