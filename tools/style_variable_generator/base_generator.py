# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import collections
import re
import textwrap
from style_variable_generator import path_overrides
from style_variable_generator.color import Color
from style_variable_generator.opacity import Opacity
import copy

_FILE_PATH = os.path.dirname(os.path.realpath(__file__))

_JSON5_PATH = os.path.join(_FILE_PATH, os.pardir, os.pardir, 'third_party',
                           'pyjson5', 'src')
sys.path.insert(1, _JSON5_PATH)
import json5

_JINJA2_PATH = os.path.join(_FILE_PATH, os.pardir, os.pardir, 'third_party')
sys.path.insert(1, _JINJA2_PATH)
import jinja2


class Modes:
    LIGHT = 'light'
    DARK = 'dark'
    DEBUG = 'debug'
    # The mode that colors will fallback to when not specified in a
    # non-default mode. An error will be raised if a color in any mode is
    # not specified in the default mode.
    DEFAULT = LIGHT
    ALL = [LIGHT, DARK, DEBUG]


RESERVED_SUFFIXES = ['_' + s for s in Modes.ALL + ['rgb', 'inverted']]


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


class BaseGenerator:
    '''A generic style variable generator.

    Subclasses should provide format-specific generation templates, filters and
    globals to render their output.
    '''

    @staticmethod
    def GetName():
        return None

    def __init__(self):
        self.out_file_path = None
        # A map of input filepaths to their context object.
        self.in_file_to_context = dict()

        # If specified, only generates the given mode.
        self.generate_single_mode = None

        opacity_model = OpacityModel(self)
        color_model = ColorModel(self, opacity_model)

        # A dictionary of |VariableType| to models containing mappings of
        # variable names to values.
        self.model = {
            VariableType.COLOR:
            color_model,
            VariableType.OPACITY:
            opacity_model,
            VariableType.TYPOGRAPHY:
            TypographyModel(),
            # A dict of client-defined groups to corresponding dicts of variable
            # names to values. This is used to store CSS that doesn't have a
            # dedicated model type. This is used for more freeform variables, or
            # for variable types that haven't been implemented yet.
            # See https://crbug.com/1018654.
            VariableType.UNTYPED_CSS:
            dict(),
        }

        # A dictionary of variable names to objects containing information about
        # how the generator should run for that variable. All variables must
        # populate this dictionary and as such, its keys can be used as a list
        # of all variable names,
        self.context_map = dict()

        # A dictionary of options used to alter generator function. See
        # ./README.md for each generators list of options.
        self.generator_options = {}

    def SetVariableContext(self, name, context):
        if name in self.context_map.keys():
            raise ValueError('Variable name "%s" is reused' % name)
        self.context_map[name] = context or {}

    def GetContextKey(self):
        return self.GetName()

    def AddColor(self, name, value_obj, context=None):
        try:
            self.model[VariableType.COLOR].Add(name, value_obj, context)
        except ValueError as err:
            raise ValueError('Error parsing color "%s": %s' % (value_obj, err))

    # Add all the colors in the data to the model.
    def _AddColors(self, data, generator_context):
        for name, value in data.get('colors', {}).items():
            if not re.match('^[a-z0-9_]+$', name):
                raise ValueError(
                    '%s is not a valid variable name (lower case, 0-9, _)' %
                    name)
            self.AddColor(name, value, generator_context)

    def _ResolveBlendedColors(self):
        # Calculate the final RGBA for all blended colors because the
        # generator's subclasses can't blend yet.
        color_model = self.model[VariableType.COLOR]
        temp_model = {}
        for name, value in color_model.items():
            for mode, color in value.items():
                if color.blended_colors:
                    assert len(color.blended_colors) == 2
                    if name not in temp_model:
                        temp_model[name] = {}
                    temp_model[name][mode] = color_model.ResolveToRGBA(
                        name, mode)
        for name, value in temp_model.items():
            for mode, color in value.items():
                color_model[name][mode] = temp_model[name][mode]

    def AddOpacity(self, name, value_obj, context=None):
        try:
            self.model[VariableType.OPACITY].Add(name, value_obj, context)
        except ValueError as err:
            raise ValueError('Error parsing opacity "%s": %s' %
                             (value_obj, err))

    def AddUntypedCSSGroup(self, group_name, value_obj, context=None):
        for var_name in value_obj.keys():
            self.SetVariableContext(var_name, context)
        self.model[VariableType.UNTYPED_CSS][group_name] = value_obj

    def AddJSONFilesToModel(self, paths):
        '''Adds one or more JSON files to the model.
        '''
        for path in paths:
            try:
                with open(path, 'r') as f:
                    self.AddJSONToModel(f.read(), path)
            except ValueError as err:
                raise ValueError('\n%s:\n    %s' % (path, err))

        # Resolve blended colors after all the files are added because some
        # color dependencies are between different files.
        self._ResolveBlendedColors()

    def AddJSONToModel(self, json_string, in_file=None):
        '''Adds a |json_string| with variable definitions to the model.

        See *test.json5 files for a defacto format reference.

        |in_file| is used to populate a file-to-context map.
        '''
        # TODO(calamity): Add allow_duplicate_keys=False once pyjson5 is
        # rolled.
        data = json5.loads(json_string,
                           object_pairs_hook=collections.OrderedDict)
        # Use the generator's name to get the generator-specific context from
        # the input.
        generator_context = data.get('options', {})
        self.in_file_to_context[in_file] = generator_context

        self._AddColors(data, generator_context)

        for name, value in data.get('opacities', {}).items():
            if not re.match('^[a-z0-9_]+_opacity$', name):
                raise ValueError(
                    name + ' is not a valid opacity name ' +
                    '(lower case, 0-9, _, must end with _opacity)')

            self.AddOpacity(name, value, generator_context)

        typography = data.get('typography')
        if typography:
            typography_model = self.model[VariableType.TYPOGRAPHY]
            for name, value in typography['font_families'].items():
                self.SetVariableContext(name, generator_context)
                typography_model.AddFontFamily(name, value)

            for name, value_obj in typography['typefaces'].items():
                self.SetVariableContext(name, generator_context)
                typography_model.AddTypeface(name, value_obj)

        for name, value in data.get('untyped_css', {}).items():
            self.AddUntypedCSSGroup(name, value, generator_context)

        return generator_context

    def ApplyTemplate(self, style_generator, path_to_template, params):
        loader_root_dir = path_overrides.GetFileSystemLoaderRootDirectory()
        jinja_env = jinja2.Environment(
            loader=jinja2.FileSystemLoader(loader_root_dir),
            keep_trailing_newline=True)
        jinja_env.globals.update(style_generator.GetGlobals())
        jinja_env.filters.update(style_generator.GetFilters())
        template = jinja_env.get_template(
            path_overrides.GetPathToTemplate(path_to_template))
        return template.render(params)

    def Validate(self):
        colors = self.model[VariableType.COLOR]
        color_names = set(colors.keys())
        opacities = self.model[VariableType.OPACITY]
        opacity_names = set(opacities.keys())

        def CheckColorReference(name, referrer):
            if name == referrer:
                raise ValueError("{0} refers to itself".format(name))
            if name not in color_names:
                raise ValueError("Cannot find color %s referenced by %s" %
                                 (name, referrer))

        def CheckOpacityReference(name, referrer):
            if name == referrer:
                raise ValueError("{0} refers to itself".format(name))
            if name not in opacity_names:
                raise ValueError("Cannot find opacity %s referenced by %s" %
                                 (name, referrer))

        # Check all colors in all modes refer to colors that exist in the
        # default mode.
        for name, mode_values in colors.items():
            for suffix in RESERVED_SUFFIXES:
                if not self.context_map[name].get(
                        'generated') and name.endswith(suffix):
                    raise ValueError(
                        'Variable name "%s" uses a reserved suffix: %s' %
                        (name, suffix))
            if Modes.DEFAULT not in mode_values:
                raise ValueError("Color %s not defined for default mode" % name)
            for mode, color in mode_values.items():
                if color.var:
                    CheckColorReference(color.var, name)
                if color.rgb_var:
                    CheckColorReference(color.RGBVarToVar(), name)
                if color.opacity and color.opacity.var:
                    CheckOpacityReference(color.opacity.var, name)
                if color.blended_colors:
                    assert len(color.blended_colors) == 2
                    CheckColorReference(color.blended_colors[0], name)
                    CheckColorReference(color.blended_colors[1], name)

        for name, mode_values in opacities.items():
            for mode, opacity in mode_values.items():
                if opacity.var:
                    CheckOpacityReference(opacity.var, name)

        # TODO(b/206887565): Check for circular references.
