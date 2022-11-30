# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections

Tag = collections.namedtuple('Tag', ['name', 'description'])

SMOKE_TEST = Tag(
    'smoke_test',
    'Run by the smoke test. Add this tag to get the feature covered on CQ')
