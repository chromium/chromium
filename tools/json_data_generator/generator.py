# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import importlib.util
import os
import re
import sys
from typing import Dict, List

from json_data_generator.util import (GetDirNameFromPath, GetFileNameFromPath,
                                      GetFileNameWithoutExtensionFromPath,
                                      JoinPath)

_FILE_PATH = os.path.dirname(os.path.realpath(__file__))

_JSON5_PATH = os.path.join(_FILE_PATH, os.pardir, os.pardir, 'third_party',
                           'pyjson5', 'src')
sys.path.insert(1, _JSON5_PATH)
import json5

_JINJA2_PATH = os.path.join(_FILE_PATH, os.pardir, os.pardir, 'third_party')
sys.path.insert(1, _JINJA2_PATH)
import jinja2


class JSONDataGenerator(object):
    '''A generic json data generator.'''

    def __init__(self, out_dir: str):
        self.out_dir = out_dir
        self.model: Dict = {}
        # Store all json sources used in the generator.
        self.sources: List[str] = list()

    def AddJSONFilesToModel(self, paths: List[str]):
        '''Adds one or more JSON files to the model.'''
        for path in paths:
            try:
                with open(path, 'r', encoding='utf-8') as f:
                    self.AddJSONToModel(path, f.read())
                    self.sources.append(path)
            except ValueError as err:
                raise ValueError('\n%s:\n    %s' % (path, err))

    def AddJSONToModel(self, json_path: str, json_string: str):
        '''Adds a |json_string| with data to the model.
        Every json file is added to |self.model| with the original file
        name as the key.
        '''
        data = json5.loads(json_string,
                           object_pairs_hook=collections.OrderedDict)
        # Use the json file name as the key of the loaded json data.
        key = GetFileNameWithoutExtensionFromPath(json_path)
        self.model[key] = data

    def GetGlobals(self, template_path: str):
        file_name_without_ext = GetFileNameWithoutExtensionFromPath(
            template_path)
        out_file_path = JoinPath(self.out_dir, file_name_without_ext)
        return {
            'model': self.model,
            'source_json_files': self.sources,
            'out_file_path': out_file_path,
        }

    def GetFilters(self):
        return {
            'to_header_guard': self._ToHeaderGuard,
        }

    def RenderTemplate(self,
                       path_to_template: str,
                       path_to_template_helper: str = None):
        template_dir = GetDirNameFromPath(path_to_template)
        template_name = GetFileNameFromPath(path_to_template)
        jinja_env = jinja2.Environment(
            loader=jinja2.FileSystemLoader(template_dir),
            keep_trailing_newline=True)
        jinja_env.globals.update(self.GetGlobals(path_to_template))
        jinja_env.filters.update(self.GetFilters())
        if path_to_template_helper:
            template_helper_module = self._LoadTemplateHelper(
                path_to_template_helper)
            jinja_env.globals.update(
                template_helper_module.get_custom_globals(self.model))
            jinja_env.filters.update(
                template_helper_module.get_custom_filters(self.model))
        template = jinja_env.get_template(template_name)
        return template.render()

    def _LoadTemplateHelper(self, path_to_template_helper: str):
        template_helper_dir = GetDirNameFromPath(path_to_template_helper)
        try:
            sys.path.append(template_helper_dir)
            spec = importlib.util.spec_from_file_location(
                path_to_template_helper, path_to_template_helper)
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            return module
        finally:
            # Restore sys.path to what it was before.
            sys.path.remove(template_helper_dir)

    def _ToHeaderGuard(self, path: str):
        return re.sub(r'[\\\/\.\-]+', '_', path.upper())
