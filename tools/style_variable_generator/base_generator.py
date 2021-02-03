# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import collections
import re
import textwrap
import path_overrides
from color import Color
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
    ALL = [LIGHT, DARK]


class VariableType:
    COLOR = 'color'
    OPACITY = 'opacity'


class ColorModel:
    '''A dictionary of color names to their values in each mode.
       e.g ColorModel['blue'][Modes.LIGHT] = Color(...)
    '''

    def __init__(self, default_mode, opacity_model):
        self.variables = collections.OrderedDict()
        self._default_mode = default_mode
        self.opacity_model = opacity_model

    def Add(self, mode, name, value):
        if name not in self.variables:
            self.variables[name] = {}
        self.variables[name][mode] = value

    # Returns the Color that |name| will have in |mode|. Resolves to the default
    # mode's Color if the a Color for |mode| isn't specified. Always returns a
    # Color.
    def Resolve(self, name, mode):
        if mode in self.variables[name]:
            return self.variables[name][mode]

        return self.variables[name][self._default_mode]

    # Returns a value from 0-1 representing the final opacity of |color|.
    def ResolveOpacity(self, color):
        if color.a != -1:
            return color.a

        assert (color.opacity_var)
        return self.opacity_model[color.opacity_var]

    # Returns a Color that is the final RGBA value for |name| in |mode|.
    def ResolveToRGBA(self, name, mode):
        c = self.Resolve(name, mode)
        if c.var:
            return self.ResolveToRGBA(c.var, mode)
        result = Color()
        result.a = self.ResolveOpacity(c)

        rgb = c
        if c.rgb_var:
            rgb = self.ResolveToRGBA(c.RGBVarToVar(), mode)

        (result.r, result.g, result.b) = (rgb.r, rgb.g, rgb.b)
        return result

    def keys(self):
        return self.variables.keys()

    def items(self):
        return self.variables.items()

    def __getitem__(self, key):
        return self.variables[key]


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

        # The mode that colors will fallback to when not specified in a
        # non-default mode. An error will be raised if a color in any mode is
        # not specified in the default mode.
        self._default_mode = Modes.LIGHT

        opacity_model = collections.OrderedDict()
        color_model = ColorModel(self._default_mode, opacity_model)

        # A dictionary of |VariableType| to models containing mappings of
        # variable names to values.
        self.model = {
            VariableType.COLOR: color_model,
            VariableType.OPACITY: opacity_model,
        }

        # A dictionary of variable names to objects containing information about
        # how the generator should run for that variable. All variables must
        # populate this dictionary and as such, its keys can be used as a list
        # of all variable names,
        self.context_map = dict()

    def _SetVariableContext(self, name, context):
        if name in self.context_map:
            raise ValueError('Variable name "%s" is reused' % name)
        self.context_map[name] = context or {}

    def GetContextKey(self):
        return self.GetName()

    def AddColor(self, name, value_obj, context=None):
        self._SetVariableContext(name, context)
        try:
            # Python3's unicode class is just 'str'.
            strtype = str if sys.version_info >= (3, ) else basestring

            if isinstance(value_obj, strtype):
                self.model[VariableType.COLOR].Add(self._default_mode, name,
                                                   Color(value_obj))
            elif isinstance(value_obj, dict):
                for mode in Modes.ALL:
                    if mode in value_obj:
                        self.model[VariableType.COLOR].Add(
                            mode, name, Color(value_obj[mode]))
        except ValueError as err:
            raise ValueError('Error parsing color "%s": %s' % (value_obj, err))

    def AddOpacity(self, name, value_obj, context=None):
        self._SetVariableContext(name, context)
        if not isinstance(value_obj, float) and value_obj.startswith('$'):
            raise ValueError('Opacities cannot point to other opacities. '
                             'File a bug if this would be useful for you.')
        self.model[VariableType.OPACITY][name] = float(value_obj)

    def AddJSONFileToModel(self, path):
        try:
            with open(path, 'r') as f:
                return self.AddJSONToModel(f.read(), path)
        except ValueError as err:
            raise ValueError('\n%s:\n    %s' % (path, err))

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
        generator_context = data.get('options',
                                     {}).get(self.GetContextKey(), None)
        self.in_file_to_context[in_file] = generator_context

        for name, value in data.get('colors', {}).items():
            if not re.match('^[a-z0-9_]+$', name):
                raise ValueError(
                    '%s is not a valid variable name (lower case, 0-9, _)' %
                    name)

            self.AddColor(name, value, generator_context)

        for name, value in data.get('opacities', {}).items():
            if not re.match('^[a-z0-9_]+_opacity$', name):
                raise ValueError(
                    name + ' is not a valid opacity name ' +
                    '(lower case, 0-9, _, must end with _opacity)')

            self.AddOpacity(name, value, generator_context)

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
        opacities = self.model[VariableType.OPACITY]

        def CheckColorInDefaultMode(name):
            if (name not in colors.variables
                    or self._default_mode not in colors.variables[name]):
                raise ValueError("%s not defined in default mode '%s'" %
                                 (name, self._default_mode))

        # Check all colors in all modes refer to colors that exist in the
        # default mode.
        for name, mode_values in colors.variables.items():
            for mode, value in mode_values.items():
                CheckColorInDefaultMode(name)
                if value.var:
                    CheckColorInDefaultMode(value.var)
                if value.rgb_var:
                    CheckColorInDefaultMode(value.RGBVarToVar())
                if value.opacity_var and value.opacity_var not in opacities:
                    raise ValueError("Opacity '%s' not defined" %
                                 value.opacity_var)

        # TODO(calamity): Check for circular references.

    # TODO(crbug.com/1053372): Prune unused rgb values.
