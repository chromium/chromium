# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

import search_engine_icons_utils


def generate_icon_path_map(output_filename, engine_keywords):
  """Generates the `kSearchEngineIconPathMap` map.

  The code is generated in `search_engine_choice/generated_icon_utils-inc.cc`.
  """
  with open(output_filename, 'w', encoding='utf-8', newline='') as utils_file:
    utils_file.write('constexpr auto kSearchEngineIconPathMap =\n')
    utils_file.write(
        '\tbase::MakeFixedFlatMap<std::u16string_view, std::string_view>({\n')

    for engine_keyword in engine_keywords:
      resource_name = search_engine_icons_utils.keyword_to_resource_name(
          engine_keyword)
      utils_file.write('\t\t{')
      utils_file.write(f'u"{engine_keyword}", "chrome://theme/{resource_name}"')
      utils_file.write('},\n')

    # Add Google to the map
    utils_file.write('\t\t{u"google.com",\n')
    utils_file.write('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)\n')
    utils_file.write('\t\t "chrome://theme/IDR_GOOGLE_COM_PNG"\n')
    utils_file.write('#else\n')
    utils_file.write('\t\t "chrome://theme/IDR_DEFAULT_FAVICON"\n')
    utils_file.write('#endif\n')
    utils_file.write('\t}});\n')


def generate_icon_resource_id_map(output_filename, engine_keywords):
  """Generates the `kSearchEngineResourceIdMap` map.

  The code is generated in
  `components/search_engines/generated_search_engine_resource_ids-inc.cc`.
  """
  with open(output_filename, 'w', encoding='utf-8', newline='') as utils_file:
    utils_file.write('constexpr auto kSearchEngineResourceIdMap =\n')
    utils_file.write('\tbase::MakeFixedFlatMap<std::u16string_view, int>({\n')

    for engine_keyword in engine_keywords:
      resource_name = search_engine_icons_utils.keyword_to_resource_name(
          engine_keyword)
      utils_file.write('\t\t{')
      utils_file.write(f'u"{engine_keyword}", {resource_name}')
      utils_file.write('},\n')

    # Add Google to the map
    utils_file.write('\t\t{u"google.com",\n')
    utils_file.write('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)\n')
    utils_file.write('\t\t IDR_GOOGLE_COM_PNG\n')
    utils_file.write('#else\n')
    utils_file.write('\t\t IDR_DEFAULT_FAVICON\n')
    utils_file.write('#endif\n')
    utils_file.write('\t}});\n\n')


if len(sys.argv) >= 3:
  src_dir = sys.argv[1]
  generated_search_engine_resource_ids_file = sys.argv[2]
  generated_icon_utils_file = sys.argv[3]

engine_keywords = {
    keyword
    for (engine, keyword
         ) in search_engine_icons_utils.get_used_engines_with_keywords(src_dir)
}
# Sort the engines so that the order of the engines in the generated files is
# deterministic.
engine_keywords = sorted(engine_keywords)
generate_icon_path_map(generated_icon_utils_file, engine_keywords)
generate_icon_resource_id_map(generated_search_engine_resource_ids_file,
                              engine_keywords)
