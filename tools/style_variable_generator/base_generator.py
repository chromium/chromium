# Copyright 2019 The Chromium Authors
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
from style_variable_generator.model import Model, Modes, VariableType

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

        self.model = Model()

        # A map of input filepaths to their context object.
        self.in_file_to_context = dict()

        # If specified, only generates the given mode.
        self.generate_single_mode = None

        # A dictionary of options used to alter generator function. See
        # ./README.md for each generators list of options.
        self.generator_options = {}


    # If true, will attempt to resolve all blend() colors to the RGBA values at
    # compile time. Note that json5 files can specify "preblend" to override
    # this setting for specific files.
    def DefaultPreblend(self):
        return True

    def GetInputFiles(self):
        return sorted(self.in_file_to_context.keys())

    def AddJSONFilesToModel(self, paths):
        '''Adds one or more JSON files to the model.
        '''
        for path in paths:
            try:
                with open(path, 'r') as f:
                    self.AddJSONToModel(f.read(), path)
            except ValueError as err:
                raise ValueError(f'Could not add {path}') from err

        self.model.PostProcess(default_preblend=self.DefaultPreblend())

    def AddJSONToModel(self, json_string, in_file=None):
        '''Adds a |json_string| with variable definitions to the model.

        See *test.json5 files for a defacto format reference.

        |in_file| is used to populate a file-to-context map.
        '''
        # TODO(calamity): Add allow_duplicate_keys=False once pyjson5 is
        # rolled.
        data = json5.loads(json_string,
                           object_pairs_hook=collections.OrderedDict)

        context = data.get('options', {})
        context['token_namespace'] = data.get('token_namespace', '')
        self.in_file_to_context[in_file] = context

        # Add variables to the model.
        for name, value in data.get('colors', {}).items():
            self.model.Add(VariableType.COLOR, name, value, context)

        for name, value in data.get('opacities', {}).items():
            self.model.Add(VariableType.OPACITY, name, value, context)

        for name, value in data.get('legacy_mappings', {}).items():
            self.model.Add(VariableType.LEGACY_MAPPING, name, value, context)

        typography = data.get('typography')
        if typography:
            for name, value in typography['font_families'].items():
                self.model.Add(VariableType.FONT_FAMILY, name, value, context)

            for name, value in typography['font_faces'].items():
                self.model.Add(VariableType.FONT_FACE, name, value, context)

            for name, value_obj in typography['typefaces'].items():
                self.model.Add(VariableType.TYPEFACE, name, value_obj, context)

        for group_name, value_obj in data.get('untyped_css', {}).items():
            for var_name, value in value_obj.items():
                self.model.Add(VariableType.UNTYPED_CSS, var_name, value,
                               context)

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
