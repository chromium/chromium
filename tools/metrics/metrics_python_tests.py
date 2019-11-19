#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
SRC_DIR = os.path.dirname(os.path.dirname(THIS_DIR))
TYP_DIR = os.path.join(SRC_DIR, 'third_party', 'catapult', 'third_party', 'typ')

if not TYP_DIR in sys.path:
    sys.path.insert(0, TYP_DIR)

import typ


def resolve(*paths):
    return [os.path.join(THIS_DIR, *(p.split('/'))) for p in paths]

sys.exit(typ.main(tests=resolve(
   'actions/extract_actions_test.py',
   'histograms/generate_expired_histograms_array_unittest.py',
   'histograms/pretty_print_test.py',
   'rappor/rappor_model_test.py',
   'ukm/gen_builders_test.py',
   'ukm/ukm_model_test.py',
   'ukm/xml_validations_test.py',
   '../json_comment_eater/json_comment_eater_test.py',
   '../json_to_struct/element_generator_test.py',
   '../json_to_struct/struct_generator_test.py',
   '../variations/fieldtrial_to_struct_unittest.py',
   '../variations/fieldtrial_util_unittest.py',
   '../variations/split_variations_cmd_unittest.py',
   '../../components/variations/service/'
       'generate_ui_string_overrider_unittest.py',
   )))
