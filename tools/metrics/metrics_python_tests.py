#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

import setup_modules

import typ

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))

def resolve(*paths):
  return [os.path.join(_THIS_DIR, *(p.split('/'))) for p in paths]

sys.exit(
    typ.main(
        tests=resolve(
            'actions/action_utils_test.py',
            'actions/extract_actions_test.py',
            'actions/actions_model_test.py',
            'actions/PRESUBMIT_test.py',

            # TODO(crbug.com/40772738) - the test ordering is very sensitive due
            # to potential name collisions between ukm/pretty_print.py and
            # histograms/pretty_print.py and the implementation in typ.
            #
            # Until this issue is fixed, best to ensure that the 'histograms'
            # files show up *after* the 'ukm' files (in order for the histograms
            # directory to be added to sys.path *before* ukm), and that we run
            # the tests in a single process (jobs=1, below).
            'common/codegen_shared_test.py',
            'private_metrics/gen_private_metrics_builders_test.py',
            'private_metrics/private_metrics_model_shared_test.py',
            'private_metrics/private_metrics_validations_test.py',
            'ukm/gen_builders_test.py',
            'ukm/ukm_model_test.py',
            'ukm/xml_validations_test.py',
            'histograms/expand_owners_unittest.py',
            'histograms/extract_histograms_test.py',
            'histograms/generate_expired_histograms_array_unittest.py',
            'histograms/generate_allowlist_from_histograms_file_unittest.py',
            'histograms/merge_xml_test.py',
            'histograms/PRESUBMIT_test.py',
            'histograms/pretty_print_test.py',
            'histograms/print_expanded_histograms_test.py',
            'histograms/validate_token_test.py',
            '../json_comment_eater/json_comment_eater_test.py',
            '../json_to_struct/element_generator_test.py',
            '../json_to_struct/struct_generator_test.py',
            '../variations/fieldtrial_to_struct_unittest.py',
            '../variations/fieldtrial_util_unittest.py',
            '../variations/split_variations_cmd_unittest.py',
        ),
        jobs=1))
