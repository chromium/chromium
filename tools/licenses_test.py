#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for //tools/licenses.py.
"""

import os
import pathlib
import sys
import unittest
from unittest import mock

REPOSITORY_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
sys.path.append(os.path.join(REPOSITORY_ROOT, 'tools'))

import licenses


def construct_absolute_path(path):
  return str(pathlib.PurePosixPath(REPOSITORY_ROOT.replace(os.sep, '/'), path))


class LicensesTest(unittest.TestCase):
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

  def test_generate_license_file_txt(self):
    read_file_vals = [
        'root license text\n',
        'lib1 license text\n',
        'lib2 license text\n',
        'lib3 license text\n',
    ]

    license_txt = licenses.GenerateLicenseFilePlainText(
        {
            'third_party/lib1': {
                'Name': 'lib1',
                'License File': 'third_party/lib1/LICENSE',
            },
            'third_party/lib2': {
                'Name': 'lib2',
                'License File': 'third_party/lib2/LICENSE',
            },
            'ignored': {
                'Name': 'ignored',
                'License File': licenses.NOT_SHIPPED,
            },
            'third_party/lib3': {
                'Name': 'lib3',
                'License File': 'third_party/lib3/LICENSE',
            },
        },
        read_file=lambda _: read_file_vals.pop(0))

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
        'lib2 license text',
        '',
        '--------------------',
        'lib3',
        '--------------------',
        'lib3 license text',
    ]) + '\n'  # extra new line to account for join not adding one to the end
    self.assertEqual(license_txt, expected)

  def test_generate_license_file_sdpx(self):
    read_file_vals = [
        'root\nlicense text\n',
        'lib1\nlicense text\n',
        'lib2\nlicense text\n',
        'lib3\nlicense text\n',
    ]

    license_txt = licenses.GenerateLicenseFileSpdx(
        {
            'third_party/lib1': {
                'Name': 'lib1',
                'License File': 'third_party/lib1/LICENSE',
            },
            'third_party/lib2': {
                'Name': 'lib2',
                'License File': 'third_party/lib2/LICENSE',
            },
            'ignored': {
                'Name': 'ignored',
                'License File': licenses.NOT_SHIPPED,
            },
            'third_party/lib3': {
                'Name': 'lib3',
                'License File': 'third_party/lib3/LICENSE',
            },
        },
        'http://google.com',
        '/src',
        'mydoc',
        'http://google.com',
        repo_root='/src',
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
            "SPDXID": "SPDXRef-Package-lib3",
            "name": "lib3",
            "licenseConcluded": "LicenseRef-lib3"
        }
    ],
    "hasExtractedLicensingInfos": [
        {
            "name": "Chromium License",
            "licenseId": "LicenseRef-Chromium",
            "extractedText": "root\\nlicense text\\n",
            "crossRefs": [
                {
                    "url": "http://google.com/LICENSE"
                }
            ]
        },
        {
            "name": "lib1 License",
            "licenseId": "LicenseRef-lib1",
            "extractedText": "lib1\\nlicense text\\n",
            "crossRefs": [
                {
                    "url": "http://google.com/third_party/lib1/LICENSE"
                }
            ]
        },
        {
            "name": "lib2 License",
            "licenseId": "LicenseRef-lib2",
            "extractedText": "lib2\\nlicense text\\n",
            "crossRefs": [
                {
                    "url": "http://google.com/third_party/lib2/LICENSE"
                }
            ]
        },
        {
            "name": "lib3 License",
            "licenseId": "LicenseRef-lib3",
            "extractedText": "lib3\\nlicense text\\n",
            "crossRefs": [
                {
                    "url": "http://google.com/third_party/lib3/LICENSE"
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
            "relatedSpdxElement": "SPDXRef-Package-lib3"
        }
    ]
}'''
    self.assertEqual(license_txt, expected)


if __name__ == '__main__':
  unittest.main()
