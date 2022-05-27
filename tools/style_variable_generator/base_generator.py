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
from style_variable_generator.model import Modes, VariableType, ModeKeyedModel, OpacityModel, ColorModel, TypographyModel

_FILE_PATH = os.path.dirname(os.path.realpath(__file__))

_JSON5_PATH = os.path.join(_FILE_PATH, os.pardir, os.pardir, 'third_party',
                           'pyjson5', 'src')
sys.path.insert(1, _JSON5_PATH)
import json5

_JINJA2_PATH = os.path.join(_FILE_PATH, os.pardir, os.pardir, 'third_party')
sys.path.insert(1, _JINJA2_PATH)
import jinja2

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

        RESERVED_SUFFIXES = ['_' + s for s in Modes.ALL + ['rgb', 'inverted']]

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
