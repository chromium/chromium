# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re


class WebFeature(object):
    def __init__(self, web_feature_path):
        assert os.path.isfile(
            web_feature_path), "%s not found" % (web_feature_path)

        const_def = re.compile(r'\s*k(\w*)\s*=\s*(\d*)\s*,.*')

        self.features = {}

        with open(web_feature_path, 'r') as f:
            for line in f.readlines():
                match = const_def.match(line)
                if match:
                    self.features[match.group(1)] = int(match.group(2))

    def __contains__(self, val):
        return val in self.features

    def __getitem__(self, val):
        assert val in self, "%s not in mojom" % (val)
        return self.features[val]
