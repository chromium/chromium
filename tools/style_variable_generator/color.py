#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import textwrap
from style_variable_generator.opacity import Opacity
from abc import ABC, abstractmethod


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


# Attempts to parse special variables, returns the Color if successful.
def from_white_black(var):
    if var == 'white':
        return ColorRGB([255, 255, 255])

    if var == 'black':
        return ColorRGB([0, 0, 0])

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

        value = match.group(1)
        if len(value) != 6:
            raise ValueError('Expected #RRGGBB')

        return ColorRGB([int(x, 16) for x in textwrap.wrap(value, 2)],
                        Opacity(1))

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
            return ColorRGB([int(x) for x in values], Opacity(1))

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
            return ColorRGB([int(x) for x in values[0:3]], Opacity(values[3]))

        raise ValueError('rgba() expected to have either'
                         '1 reference + alpha, or 3 ints + alpha')

    def ParseBlend(value):
        match = re.match(r'^blend\((.*)\)$', value)
        if not match:
            return None

        values = list(split_args(match.group(1)))
        if len(values) == 2:
            # blend(color1, color2)
            return ColorBlend([ParseColor(values[0]), ParseColor(values[1])])
        elif len(values) == 3:
            # blend(color1, blendPercentage%, color2)
            blendPercentage = int(re.match(r'(\d+)%', values[1]).group(1))
            return ColorBlend([ParseColor(values[0]),
                               ParseColor(values[2])], blendPercentage)

        raise ValueError('Unexpected number of arguments for blend()')

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
    if not isinstance(parsed, Color):
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

    @abstractmethod
    def GetFormula(self):
        pass

    @abstractmethod
    def __repr__(self):
        pass


class ColorRGB(Color):
    def __init__(self, rgb=None, opacity=None):
        super().__init__()
        if rgb is None:
            (self.r, self.g, self.b) = [-1, -1, -1]
        else:
            if not all([(0 <= v <= 255) for v in rgb]):
                raise ValueError(f'RGB value out of bounds: {rgb}')
            (self.r, self.g, self.b) = rgb
        self.opacity = opacity

    def GetFormula(self):
        a = repr(self.opacity)
        return 'rgba(%d, %d, %d, %s)' % (self.r, self.g, self.b, a)

    def __repr__(self):
        a = repr(self.opacity)
        return 'rgba(%d, %d, %d, %s)' % (self.r, self.g, self.b, a)


class ColorRGBVar(Color):
    def __init__(self, rgb_var=None, opacity=None):
        super().__init__()
        if not re.match(r'^[\w\.\-]+\.rgb$', rgb_var):
            raise ValueError(f'"{rgb_var}" is not a valid RGBVar value')
        self.rgb_var = rgb_var
        self.opacity = opacity

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
    blended_colors[1]. The mix percentace is `opacity`. If `opacity` is not
    provided, the mix percentage may be taken from A's opacity.
    '''
    def __init__(self, colors=[], blendPercentage=None):
        super().__init__()
        if len(colors) not in [0, 2]:
            raise ValueError(
                f'Can only color-mix 2 colors. Found: {len(colors)}')
        if not all(isinstance(c, Color) for c in colors):
            raise ValueError(f'Non-Color found in {colors}')

        self.blended_colors = colors
        self.blendPercentage = blendPercentage

    def GetFormula(self):
        if self.blendPercentage is None:
            return 'blend(%s, %s)' % (self.blended_colors[0].GetFormula(),
                                      self.blended_colors[1].GetFormula())
        return 'blend(%s, %s, %s)' % (self.blended_colors[0].GetFormula(),
                                      self.blendPercentage,
                                      self.blended_colors[1].GetFormula())

    def __repr__(self):
        return (
            f'blend({repr(self.blended_colors)}, {repr(self.blendPercentage)})'
        )
