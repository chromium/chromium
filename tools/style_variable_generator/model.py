# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
from style_variable_generator.color import Color
from style_variable_generator.opacity import Opacity
import copy


class Modes:
    LIGHT = 'light'
    DARK = 'dark'
    DEBUG = 'debug'
    # The mode that colors will fallback to when not specified in a
    # non-default mode. An error will be raised if a color in any mode is
    # not specified in the default mode.
    DEFAULT = LIGHT
    ALL = [LIGHT, DARK, DEBUG]


class VariableType:
    COLOR = 'color'
    OPACITY = 'opacity'
    TYPOGRAPHY = 'typography'
    UNTYPED_CSS = 'untyped_css'
    ALL = [
        COLOR,
        OPACITY,
        TYPOGRAPHY,
        UNTYPED_CSS,
    ]


class ModeKeyedModel(object):
    def __init__(self, generator):
        self.variables = collections.OrderedDict()
        self.generator = generator

    def Add(self, name, value_obj, context):
        self.generator.SetVariableContext(name, context)
        if name not in self.variables:
            self.variables[name] = {}

        if isinstance(value_obj, dict):
            for mode in value_obj:
                value = self._CreateValue(value_obj[mode])
                if mode == 'default':
                    mode = Modes.DEFAULT
                assert mode in Modes.ALL and mode not in self.variables[name]
                self.variables[name][mode] = value
        else:
            self.variables[name][Modes.DEFAULT] = self._CreateValue(value_obj)

    # Returns the value that |name| will have in |mode|. Resolves to the default
    # mode's value if the a value for |mode| isn't specified. Always returns a
    # value.
    def Resolve(self, name, mode):
        if mode in self.variables[name]:
            return self.variables[name][mode]

        return self.variables[name][Modes.DEFAULT]

    def Flatten(self, resolve_missing=False):
        '''Builds a name to variable dictionary for each mode.
        If |resolve_missing| is true, colors that aren't specified in |mode|
        will be resolved to their default mode value.'''
        flattened = {}
        for mode in Modes.ALL:
            variables = collections.OrderedDict()
            for name, mode_values in self.items():
                if resolve_missing:
                    variables[name] = self.Resolve(name, mode)
                else:
                    if mode in mode_values:
                        variables[name] = mode_values[mode]
            flattened[mode] = variables

        return flattened

    def keys(self):
        return self.variables.keys()

    def items(self):
        return self.variables.items()

    def __getitem__(self, key):
        return self.variables[key]


class OpacityModel(ModeKeyedModel):
    '''A dictionary of opacity names to their values in each mode.
       e.g OpacityModel['disabled_opacity'][Modes.LIGHT] = Opacity(...)
    '''

    def __init__(self, generator):
        super(OpacityModel, self).__init__(generator)

    # Returns a float from 0-1 representing the concrete value of |opacity|.
    def ResolveOpacity(self, opacity, mode):
        if opacity.a != -1:
            return opacity

        return self.ResolveOpacity(self.Resolve(opacity.var, mode), mode)

    def _CreateValue(self, value):
        return Opacity(value)


class ColorModel(ModeKeyedModel):
    '''A dictionary of color names to their values in each mode.
       e.g ColorModel['blue'][Modes.LIGHT] = Color(...)
    '''

    def __init__(self, generator, opacity_model):
        super(ColorModel, self).__init__(generator)
        self.opacity_model = opacity_model

    def Add(self, name, value_obj, context):
        # If a color has generate_per_mode set, a separate variable will be
        # created for each mode, suffixed by mode name.
        # (e.g my_color_light, my_color_debug)
        generate_per_mode = False

        # If a color has generate_inverted set, a |color_name|_inverted will be
        # generated which uses the dark color for light mode and vice versa.
        generate_inverted = False
        if isinstance(value_obj, dict):
            generate_per_mode = value_obj.pop('generate_per_mode', None)
            generate_inverted = value_obj.pop('generate_inverted', None)
        elif self._CreateValue(value_obj).blended_colors:
            # A blended color could evaluate to different colors in different
            # modes, so add it to all the modes.
            value_obj = {mode: value_obj for mode in Modes.ALL}

        generated_context = dict(context)
        generated_context['generated'] = True

        if generate_per_mode or generate_inverted:
            for mode, value in value_obj.items():
                per_mode_name = name + '_' + mode
                ModeKeyedModel.Add(self, per_mode_name, value,
                                   generated_context)
                value_obj[mode] = '$' + per_mode_name
        if generate_inverted:
            if Modes.LIGHT not in value_obj or Modes.DARK not in value_obj:
                raise ValueError(
                    'generate_inverted requires both dark and light modes to be'
                    ' set')
            ModeKeyedModel.Add(
                self, name + '_inverted', {
                    Modes.LIGHT: '$' + name + '_dark',
                    Modes.DARK: '$' + name + '_light'
                }, generated_context)

        ModeKeyedModel.Add(self, name, value_obj, context)

    # Returns a Color that is the final RGBA value for |name| in |mode|.
    def ResolveToRGBA(self, name, mode):
        return self._ResolveColorToRGBA(self.Resolve(name, mode), mode)

    # Returns a Color that is the final RGBA value for |color| in |mode|.
    def _ResolveColorToRGBA(self, color, mode):
        if color.var:
            return self.ResolveToRGBA(color.var, mode)

        if len(color.blended_colors) == 2:
            return self._BlendColors(color.blended_colors[0],
                                     color.blended_colors[1], mode)

        result = Color()
        assert color.opacity
        result.opacity = self.opacity_model.ResolveOpacity(color.opacity, mode)

        rgb = color
        if color.rgb_var:
            rgb = self.ResolveToRGBA(color.RGBVarToVar(), mode)

        (result.r, result.g, result.b) = (rgb.r, rgb.g, rgb.b)
        return result

    # Returns a Color that is the final RGBA value for |color_a| over |color_b|
    # in |mode|.
    def _BlendColors(self, color_a, color_b, mode):
        # TODO(b/206887565): Check for circular references.
        color_a_res = self._ResolveColorToRGBA(color_a, mode)
        (alpha_a, r_a, g_a, b_a) = (color_a_res.opacity.a, color_a_res.r,
                                    color_a_res.g, color_a_res.b)
        color_b_res = self._ResolveColorToRGBA(color_b, mode)
        (alpha_b, r_b, g_b, b_b) = (color_b_res.opacity.a, color_b_res.r,
                                    color_b_res.g, color_b_res.b)

        # Blend using the formula for "A over B" from
        # https://wikipedia.org/wiki/Alpha_compositing.
        alpha_out = alpha_a + (alpha_b * (1 - alpha_a))
        r_out = round(
            (r_a * alpha_a + r_b * alpha_b * (1 - alpha_a)) / alpha_out)
        g_out = round(
            (g_a * alpha_a + g_b * alpha_b * (1 - alpha_a)) / alpha_out)
        b_out = round(
            (b_a * alpha_a + b_b * alpha_b * (1 - alpha_a)) / alpha_out)

        result = Color()
        (result.r, result.g, result.b) = (r_out, g_out, b_out)
        result.opacity = Opacity(alpha_out)
        return result

    def _CreateValue(self, value):
        return Color(value)


class TypographyModel(object):
    def __init__(self):
        self.font_families = collections.OrderedDict()
        self.typefaces = collections.OrderedDict()

    def AddFontFamily(self, name, value):
        assert name.startswith('font_family_')
        self.font_families[name] = value

    def AddTypeface(self, name, value_obj):
        assert value_obj['font_family']
        assert value_obj['font_size']
        assert value_obj['font_weight']
        assert value_obj['line_height']
        self.typefaces[name] = value_obj
