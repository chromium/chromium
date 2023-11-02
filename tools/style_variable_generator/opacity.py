# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import sys


class Opacity:
    '''A representation of a single color value.

    This opacity can be of the following formats:
    - 0.5
    - $named_opacity
      '''

    def __init__(self, value=None):
        self.var = None
        self.a = -1
        if value is not None:
            self.Parse(value)
            if not self.var and self.a == -1:
                raise ValueError('Malformed opacity value:' + value)

    def Parse(self, value):
        if isinstance(value, str):
            match = re.match(r'^\$([a-z0-9_\-\.]+)$', value)
            if match:
                self.var = match.group(1)
                return

        self.a = float(value)
        if not (0 <= self.a <= 1):
            raise ValueError('Alpha expected to be between 0 and 1')

    def GetReadableStr(self):
        return 'var(--%s)' % self.var if self.var else '%g%%' % (
            float(self.a) * 100)

    def __repr__(self):
        return 'var(--%s)' % self.var if self.var else '%g' % self.a
