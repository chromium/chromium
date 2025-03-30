# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import re
import sys

from typing import Any

Json = dict[str, Any]

config_file_path = 'tools/search_engine_choice/generate_search_engine_icons_config.json'
regional_settings_file_path = 'third_party/search_engines_data/resources/definitions/regional_settings.json'
prepopulated_engines_file_path = 'third_party/search_engines_data/resources/definitions/prepopulated_engines.json'


def _load_json(src_dir: str, file_name: str) -> Json:
  with open(src_dir + file_name, 'r', encoding='utf-8') as json_file:
    return json.loads(json_comment_eater.Nom(json_file.read()))


def keyword_to_identifer(keyword):
  """Sanitized keyword to be used as identifier.

  Replaces characters we find in prepopulates_engines.json's keyword field into
  ones that are valid in file names and variable names.

  Args:
    keyword: the keyword string as in the json file.

  Returns:
    The keyword string with characters replaced that don't work in a variable or
    file name.
  """
  return keyword.replace('.', '_').replace('-', '_')


def keyword_to_resource_name(keyword):
  """Resource name associated with a search engine keyword.

  Args:
    keyword: the keyword string as in the json file.

  Returns:
    The resource name for the icon, e.g. IDR_GOOGLE_COM_PNG.
  """
  icon_filename = keyword_to_identifer(keyword)
  return 'IDR_' + icon_filename.upper() + '_PNG'


def get_used_engines(src_dir) -> set[str]:
  """Returns the set of used engines.

  Returns the set of used engines. by checking which engines are used in
  `regional_settings.json`.
  """
  used_engines = set()
  settings = _load_json(src_dir, regional_settings_file_path)

  for setting in settings['elements'].values():
    used_engines.update(setting['search_engines'])

  # Strip the reference from engine names.
  used_engines = {engine.removeprefix('&') for engine in used_engines}

  # Read the list of engines that should be excluded.
  config = _load_json(src_dir, config_file_path)
  ignored_engines = set(config['ignored_engines'])

  return used_engines - ignored_engines


def get_used_engines_with_keywords(src_dir) -> set[(Json, str)]:
  """Returns the set of used engines with their keyword.

  Reads the keyword from `components/search_engines/prepopulated_engines.json`.
  Returns a set of pairs (engine, keyword).
  """
  engine_data = _load_json(src_dir, prepopulated_engines_file_path)
  used_engines = get_used_engines(src_dir)

  return {(used_engine, engine_data['elements'][used_engine]['keyword'])
          for used_engine in used_engines}


current_file_path = os.path.dirname(__file__)
sys.path.insert(0,
                os.path.normpath(current_file_path + "/../json_comment_eater"))
try:
  import json_comment_eater
finally:
  sys.path.pop(0)
