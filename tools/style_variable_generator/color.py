#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import textwrap
from style_variable_generator.opacity import Opacity


def split_args(arg_str):
    '''Splits a string of args by comma, taking into account brackets.
    '''
    num_unmatched = 0
    prev_index = 0
    for i, c in enumerate(arg_str):
        if c == '(':
            num_unmatched += 1
        elif c == ')':
            num_unmatched -= 1
            if (num_unmatched < 0):
                raise ValueError('too many ")"')
        elif c == ',' and num_unmatched == 0:
            yield arg_str[prev_index:i].strip()
            prev_index = i + 1
    if num_unmatched > 0:
        raise ValueError('too many "("')
    yield arg_str[prev_index:].strip()


class Color:
    '''A representation of a single color value.

    This color can be of the following formats:
    - #rrggbb
    - rgb(r, g, b)
    - rgba(r, g, b, a)
    - rgba(r, g, b, $named_opacity)
    - $other_color
    - rgb($other_color.rgb)
    - rgba($other_color.rgb, a)
    - rgba($other_color.rgb, $named_opacity)
    - blend(color1, color2)

    NB: The color components that refer to other colors' RGB values must end
    with '.rgb'.
    '''

    def __init__(self, value_str=None):
        self.var = None
        self.rgb_var = None
        self.r = -1
        self.g = -1
        self.b = -1
        # If non-empty, this color is the result of blending two other
        # colors using the "A over B" operation, where A is blended_colors[0]
        # and B is blended_colors[1].
        self.blended_colors = []

        self.opacity = None

        if value_str is not None:
            # Legacy support for old '_rgb'-style RGB refs.
            value_str = re.sub(r'_rgb\b', '.rgb', value_str)

            self.Parse(value_str)
            if not self.var and not self.blended_colors and not self.opacity:
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
        match = re.match(r'^\$([a-z0-9_\.\-]+)\.rgb$', rgb_ref)
        if not match:
            raise ValueError(
                f'Expected a reference to an RGB variable: {rgb_ref}')

        rgb_var = match.group(1)

        if not self._ParseWhiteBlack(rgb_var):
            self.rgb_var = rgb_var + '.rgb'

    def RGBVarToVar(self):
        assert (self.rgb_var)
        return self.rgb_var.replace('.rgb', '')

    def Parse(self, value):
        def ParseHex(value):
            match = re.match(r'^#([0-9a-f]*)$', value)
            if not match:
                return False

            value = match.group(1)
            if len(value) != 6:
                raise ValueError('Expected #RRGGBB')

            self._AssignRGB([int(x, 16) for x in textwrap.wrap(value, 2)])
            self.opacity = Opacity(1)

            return True

        def ParseRGB(value):
            match = re.match(r'^rgb\((.*)\)$', value)
            if not match:
                return False

            self.opacity = Opacity(1)

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
            match = re.match(r'^rgba\((.*)\)$', value)
            if not match:
                return False

            values = [x.strip() for x in match.group(1).split(',')]
            if len(values) == 2:
                self._ParseRGBRef(values[0])
                self.opacity = Opacity(values[1])
                return True

            if len(values) == 4:
                self._AssignRGB([int(x) for x in values[0:3]])
                self.opacity = Opacity(values[3])
                return True

            raise ValueError('rgba() expected to have either'
                             '1 reference + alpha, or 3 ints + alpha')

        def ParseBlend(value):
            match = re.match(r'^blend\((.*)\)$', value)
            if not match:
                return False

            values = list(split_args(match.group(1)))
            if len(values) == 2:
                self.blended_colors.append(Color(values[0]))
                self.blended_colors.append(Color(values[1]))
                return True

            raise ValueError('blend() expected to have 2 colors')

        def ParseVariableReference(value):
            match = re.match(r'^\$([\w\d_\.\-]+)$', value)
            if not match:
                return False

            var = match.group(1)

            if self._ParseWhiteBlack(var):
                self.opacity = Opacity(1)
                return True

            if value.endswith('.rgb'):
                raise ValueError(
                    'color reference cannot resolve to an rgb reference')

            self.var = var
            return True

        parsers = [
            ParseHex,
            ParseRGB,
            ParseRGBA,
            ParseBlend,
            ParseVariableReference,
        ]

        parsed = False
        for p in parsers:
            parsed = p(value)
            if parsed:
                break

        if not parsed:
            raise ValueError('Malformed color value')

    def GetFormula(self):
        if self.blended_colors:
            return 'blend(%s, %s)' % (self.blended_colors[0].GetFormula(),
                                      self.blended_colors[1].GetFormula())
        if self.var:
            return self.var
        if self.rgb_var:
            a = self.opacity.GetReadableStr()
            return '%s @ %s' % (self.rgb_var, a)
        a = repr(self.opacity)
        return 'rgba(%d, %d, %d, %s)' % (self.r, self.g, self.b, a)

    def __repr__(self):
        a = repr(self.opacity)

        if self.var:
            return 'var(--%s)' % self.var

        if self.rgb_var:
            return 'rgba(var(--%s), %s)' % (self.rgb_var, a)

        return 'rgba(%d, %d, %d, %s)' % (self.r, self.g, self.b, a)
