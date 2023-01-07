#!/usr/bin/env python

# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# TODO(509038): Delete this file after generate.py has been deleted, as it will
# no longer be needed. There is a copy of this file in wpt/ that tests the
# generate.py file in that directory.
"""Test that the set of gen-* files is the same as the generated files."""

import fnmatch
import os
import sys
import generate

UPDATE_TIP = (
    'To update the generated tests, run:\n'
    '$ python third_party/blink/web_tests/wpt_internal/bluetooth/generate.py')


def main():
    generated_files = set()
    # Tests data in gen-* files is the same as the data generated.
    for generated_test in generate.GetGeneratedTests():
        generated_files.add(generated_test.path)
        try:
            with open(generated_test.path, 'rb') as f:
                data = f.read().decode('utf-8')
                if data != generated_test.data:
                    print(generated_test.path + ' does not match template.')
                    print(UPDATE_TIP)
                    return -1
        except IOError as e:
            if e.errno == 2:
                print('Missing generated test:\n{}\nFor template:\n{}'.format(
                    generated_test.path, generated_test.template))
                print(UPDATE_TIP)
                return -1

    # Tests that there are no obsolete generated files.
    previous_generated_files = set()
    current_path = os.path.dirname(os.path.realpath(__file__))
    for root, _, filenames in os.walk(current_path):
        for filename in fnmatch.filter(filenames, 'gen-*.html'):
            previous_generated_files.add(os.path.join(root, filename))

    if previous_generated_files != generated_files:
        print('There are extra generated tests. Please remove them.')
        for test_path in previous_generated_files - generated_files:
            print(test_path)
        return -1


if __name__ == '__main__':
    sys.exit(main())
