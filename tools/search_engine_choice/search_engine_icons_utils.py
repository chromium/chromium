# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import re
import sys

config_file_path = 'tools/search_engine_choice/generate_search_engine_icons_config.json'
search_engines_countries_src_path = 'components/search_engines/search_engine_countries-inc.cc'
prepopulated_engines_file_path = 'components/search_engines/prepopulated_engines.json'


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


def get_used_engines(src_dir):
  """Returns the set of used engines.

  Returns the set of used engines. by checking which engines are used in
  `search_engine_countries-inc.cc`.
  """
  used_engines = set()
  SE_NAME_REGEX = re.compile(r'.*SearchEngineTier::[A-Za-z]+, &(.+)},')
  with open(src_dir + config_file_path, 'r',
            encoding='utf-8') as config_json, open(
                src_dir + search_engines_countries_src_path,
                'r',
                encoding='utf-8') as file:
    config_data = json.loads(json_comment_eater.Nom(config_json.read()))
    lines = file.readlines()
    for line in lines:
      match = SE_NAME_REGEX.match(line)
      if match:
        engine = match.group(1)
        if not engine in config_data['ignored_engines']:
          used_engines.add(engine)
  return used_engines


def get_used_engines_with_keywords(src_dir):
  """Returns the set of used engines with their keyword.

  Reads the keyword from `components/search_engines/prepopulated_engines.json`.
  Returns a set of pairs (engine, keyword).
  """
  used_engines_with_keywords = set()
  with open(src_dir + prepopulated_engines_file_path, 'r',
            encoding='utf-8') as engines_json:
    engine_data = json.loads(json_comment_eater.Nom(engines_json.read()))
    used_engines = get_used_engines(src_dir)
    for used_engine in used_engines:
      used_engines_with_keywords.add(
          (used_engine, engine_data['elements'][used_engine]['keyword']))
  return used_engines_with_keywords


current_file_path = os.path.dirname(__file__)
sys.path.insert(0,
                os.path.normpath(current_file_path + "/../json_comment_eater"))
try:
  import json_comment_eater
finally:
  sys.path.pop(0)
