# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json

_NONE_PLACEHOLDER = '[[None]]'


def GetNonePlaceholder():
  return _NONE_PLACEHOLDER


def JsonDump(data):
  return json.dumps(data, indent=2, sort_keys=True)
