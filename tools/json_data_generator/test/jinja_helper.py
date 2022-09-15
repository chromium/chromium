# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def _custom_upper(name: str):
    return name.upper()


def get_custom_globals(model):
    return {
        'name_with_title': 'Mr ' + model['test_data1']['name'],
    }


def get_custom_filters(model):
    return {
        'custom_upper': _custom_upper,
    }
