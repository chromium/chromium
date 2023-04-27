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


def from_rgb(rgb):
    for v in rgb:
        if not (0 <= v <= 255):
            raise ValueError('RGB value out of bounds')

    color = Color()
    (color.r, color.g, color.b) = rgb
    return color


# Attempts to parse special variables, returns the Color if successful.
def from_white_black(var):
    if var == 'white':
        return from_rgb([255, 255, 255])

    if var == 'black':
        return from_rgb([0, 0, 0])

    return None


def from_rgb_ref(rgb_ref):
    match = re.match(r'^\$([a-z0-9_\.\-]+)\.rgb$', rgb_ref)
    if not match:
        raise ValueError(f'Expected a reference to an RGB variable: {rgb_ref}')

    rgb_var = match.group(1)

    color = from_white_black(rgb_var)
    if color is None:
        return ColorRGBVar(rgb_var + '.rgb')

    return color


def ParseColor(value):
    def ParseHex(value):
        match = re.match(r'^#([0-9a-f]*)$', value)
        if not match:
            return None
        color = Color()

        value = match.group(1)
        if len(value) != 6:
            raise ValueError('Expected #RRGGBB')

        color = from_rgb([int(x, 16) for x in textwrap.wrap(value, 2)])
        color.opacity = Opacity(1)

        return color

    def ParseRGB(value):
        match = re.match(r'^rgb\((.*)\)$', value)
        if not match:
            return None

        values = match.group(1).split(',')
        if len(values) == 1:
            color = from_rgb_ref(values[0])
            color.opacity = Opacity(1)
            return color

        if len(values) == 3:
            color = from_rgb([int(x) for x in values])
            color.opacity = Opacity(1)
            return color

        raise ValueError('rgb() expected to have either 1 reference or 3 ints')

    def ParseRGBA(value):
        match = re.match(r'^rgba\((.*)\)$', value)
        if not match:
            return None

        values = [x.strip() for x in match.group(1).split(',')]
        if len(values) == 2:
            color = from_rgb_ref(values[0])
            color.opacity = Opacity(values[1])
            return color

        if len(values) == 4:
            color = from_rgb([int(x) for x in values[0:3]])
            color.opacity = Opacity(values[3])
            return color

        raise ValueError('rgba() expected to have either'
                         '1 reference + alpha, or 3 ints + alpha')

    def ParseBlend(value):
        match = re.match(r'^blend\((.*)\)$', value)
        if not match:
            return None
        color = ColorBlend()

        values = list(split_args(match.group(1)))
        if len(values) == 2:
            color.blended_colors.append(ParseColor(values[0]))
            color.blended_colors.append(ParseColor(values[1]))
            return color

        raise ValueError('blend() expected to have 2 colors')

    def ParseVariableReference(value):
        match = re.match(r'^\$([\w\.\-]+)$', value)
        if not match:
            return None

        var = match.group(1)

        color = from_white_black(var)
        if color is not None:
            color.opacity = Opacity(1)
            return color

        if value.endswith('.rgb'):
            raise ValueError(
                'color reference cannot resolve to an rgb reference')

        return ColorVar(var)

    parsers = [
        ParseHex,
        ParseRGB,
        ParseRGBA,
        ParseBlend,
        ParseVariableReference,
    ]

    value = re.sub(r'_rgb\b', '.rgb', value)

    parsed = None
    for p in parsers:
        parsed = p(value)
        if parsed is not None:
            break

    if parsed is None:
        raise ValueError('Malformed color value')
    if not parsed.opacity and not isinstance(parsed, (ColorBlend, ColorVar)):
        raise ValueError(repr(parsed))

    return parsed


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

    def __init__(self):
        self.r = -1
        self.g = -1
        self.b = -1

        self.opacity = None

    def GetFormula(self):
        a = repr(self.opacity)
        return 'rgba(%d, %d, %d, %s)' % (self.r, self.g, self.b, a)

    def __repr__(self):
        a = repr(self.opacity)
        return 'rgba(%d, %d, %d, %s)' % (self.r, self.g, self.b, a)


class ColorRGBVar(Color):
    def __init__(self, rgb_var=None):
        super().__init__()
        if not re.match(r'^[\w\.\-]+\.rgb$', rgb_var):
            raise ValueError(f'"{rgb_var}" is not a valid RGBVar value')
        self.rgb_var = rgb_var

    def ToVar(self):
        assert (self.rgb_var)
        return self.rgb_var.replace('.rgb', '')

    def GetFormula(self):
        a = self.opacity.GetReadableStr()
        return '%s @ %s' % (self.rgb_var, a)

    def __repr__(self):
        a = repr(self.opacity)
        return 'rgba(var(--%s), %s)' % (self.rgb_var, a)


class ColorVar(Color):
    def __init__(self, var=None):
        super().__init__()
        if not re.match(r'^[\w\.\-]+$', var):
            raise ValueError(f'{var} is not a valid var value')
        self.var = var

    def GetFormula(self):
        return self.var

    def __repr__(self):
        return 'var(--%s)' % self.var


class ColorBlend(Color):
    '''This color is the result of blending two other colors

    It uses the "A over B" operation, where A is blended_colors[0] and B is
    blended_colors[1].
    '''
    def __init__(self):
        super().__init__()
        self.blended_colors = []

    def GetFormula(self):
        return 'blend(%s, %s)' % (self.blended_colors[0].GetFormula(),
                                  self.blended_colors[1].GetFormula())

    def __repr__(self):
        return 'blend(' + repr(self.blended_colors) + ")"
