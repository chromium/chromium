# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import inspect
import os
import sys

EXPECTATIONS_FILE = 'expectations.txt'


def _MaybeAddTypToPath():
    src_dir = os.path.join(
        os.path.dirname(inspect.getfile(CheckChangeOnUpload)),
        os.pardir, os.pardir, os.pardir, os.pardir, os.pardir)
    typ_dir = os.path.join(src_dir, 'third_party', 'catapult',
                           'third_party', 'typ')
    if typ_dir not in sys.path:
        sys.path.append(typ_dir)


def CheckChangeOnUpload(input_api, output_api):

    results = []
    if any(EXPECTATIONS_FILE in f.LocalPath()
           for f in input_api.AffectedFiles() if f.Action() != 'D'):
        _MaybeAddTypToPath()
        from typ.expectations_parser import TestExpectations
        test_expectations = TestExpectations()
        with open(EXPECTATIONS_FILE, 'r') as exp:
            ret, errors = test_expectations.parse_tagged_list(exp.read())

        if ret:
            results.append(output_api.PresubmitError(
                'Expectations file had the following errors: \n' + errors))
    return results
