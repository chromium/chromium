# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import textwrap


class Color:
    '''A representation of a single color value.

    This color can be of the following formats:
    - #RRGGBB
    - rgb(r, g, b)
    - rgba(r, g, b, a)
    - rgba(r, g, b, $named_opacity)
    - $other_color
    - rgb($other_color_rgb)
    - rgba($other_color_rgb, a)
    - rgba($other_color_rgb, $named_opacity)

    NB: The color components that refer to other colors' RGB values must end
    with '_rgb'.
    '''

    def __init__(self, value_str=None):
        self.var = None
        self.rgb_var = None
        self.opacity_var = None
        self.r = -1
        self.g = -1
        self.b = -1
        self.a = -1
        if value_str is not None:
            self.Parse(value_str)
            if not self.var and not self.opacity_var and self.a == -1:
                raise ValueError(repr(self))

    def _AssignRGB(self, rgb):
        for v in rgb:
            if not (0 <= v <= 255):
                raise ValueError('RGB value out of bounds')

        (self.r, self.g, self.b) = rgb

    # Attempts to parse special variables, returns True if successful.
    def _ParseWhiteBlack(self, var):
        if var == 'white':
            self._AssignRGB([255, 255, 255])
            return True

        if var == 'black':
            self._AssignRGB([0, 0, 0])
            return True

        return False

    def _ParseRGBRef(self, rgb_ref):
        match = re.match('^\$([a-z0-9_]+)_rgb$', rgb_ref)
        if not match:
            raise ValueError('Expected a reference to an RGB variable')

        rgb_var = match.group(1)

        if not self._ParseWhiteBlack(rgb_var):
            self.rgb_var = rgb_var + '_rgb'

    def _ParseAlpha(self, alpha_value):
        match = re.match('^\$([a-z0-9_]+_opacity)$', alpha_value)
        if match:
            self.opacity_var = match.group(1)
            return

        self.a = float(alpha_value)
        if not (0 <= self.a <= 1):
            raise ValueError('Alpha expected to be between 0 and 1')

    def RGBVarToVar(self):
        assert (self.rgb_var)
        return self.rgb_var.replace('_rgb', '')

    def Parse(self, value):
        def ParseHex(value):
            match = re.match('^#([0-9a-f]*)$', value)
            if not match:
                return False

            value = match.group(1)
            if len(value) != 6:
                raise ValueError('Expected #RRGGBB')

            self._AssignRGB([int(x, 16) for x in textwrap.wrap(value, 2)])
            self.a = 1

            return True

        def ParseRGB(value):
            match = re.match('^rgb\((.*)\)$', value)
            if not match:
                return False

            self.a = 1

            values = match.group(1).split(',')
            if len(values) == 1:
                self._ParseRGBRef(values[0])
                return True

            if len(values) == 3:
                self._AssignRGB([int(x) for x in values])
                return True

            raise ValueError(
                'rgb() expected to have either 1 reference or 3 ints')

        def ParseRGBA(value):
            match = re.match('^rgba\((.*)\)$', value)
            if not match:
                return False

            values = [x.strip() for x in match.group(1).split(',')]
            if len(values) == 2:
                self._ParseRGBRef(values[0])
                self._ParseAlpha(values[1])
                return True

            if len(values) == 4:
                self._AssignRGB([int(x) for x in values[0:3]])
                self._ParseAlpha(values[3])
                return True

            raise ValueError('rgba() expected to have either'
                             '1 reference + alpha, or 3 ints + alpha')

        def ParseVariableReference(value):
            match = re.match('^\$([\w\d_]+)$', value)
            if not match:
                return False

            var = match.group(1)

            if self._ParseWhiteBlack(var):
                self.a = 1
                return True

            if value.endswith('_rgb'):
                raise ValueError(
                    'color reference cannot resolve to an rgb reference')

            self.var = var
            return True

        parsers = [
            ParseHex,
            ParseRGB,
            ParseRGBA,
            ParseVariableReference,
        ]

        parsed = False
        for p in parsers:
            parsed = p(value)
            if parsed:
                break

        if not parsed:
            raise ValueError('Malformed color value')

    def __repr__(self):
        a = self.opacity_var if self.opacity_var else '%g' % self.a

        if self.var:
            return 'var(--%s)' % self.var

        if self.rgb_var:
            return 'rgba(var(--%s), %s)' % (self.rgb_var, a)

        return 'rgba(%d, %d, %d, %s)' % (self.r, self.g, self.b, a)
