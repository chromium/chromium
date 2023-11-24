# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Handles the download of the search engine favicons.

For all search engines referenced in template_url_prepopulate_data.cc,
downloads their Favicon, scales it and puts it as resource into the repository
for display, e.g. in the search engine choice UI and settings.

This should be run whenever template_url_prepopulate_data.cc changes the list of
search engines used per country, or whenever prepopulated_engines.json changes
a favicon.

To run, `apt-get install python3-commentjson`, then
`python3 tools/search_engine_choice/generate_search_engine_icons.py`.
"""

import hashlib
import os
import re
import shutil
import sys
import commentjson
import requests


def get_image_hash(image_path):
  """Gets the hash of the image that's passed as argument.

  This is needed to check whether the downloaded image was already added in the
  repo or not.

  Args:
    image_path: The path of the image for which we want to get the hash.

  Returns:
    The hash of the image that's passed as argument.
  """
  with open(image_path, 'rb') as image:
    return hashlib.sha256(image.read()).hexdigest()


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


def populate_used_engines():
  """Populates the `used_engines` set.

  Populates the `used_engines` set by checking which engines are used in
  `template_url_prepopulate_data.cc`.
  """
  print('Populating used engines set')
  SE_NAME_REGEX = re.compile(r'.*SearchEngineTier::[A-Za-z]+, &(.+)},')
  with open('../search_engines/template_url_prepopulate_data.cc',
            'r',
            encoding='utf-8') as file:
    lines = file.readlines()
    for line in lines:
      match = SE_NAME_REGEX.match(line)
      if match:
        used_engines.add(match.group(1))


def delete_files_in_directory(directory_path):
  """Deletes previously generated icons.

  Deletes the icons that were previously created and added to directory_path.

  Args:
    directory_path: The path of the directory where the icons live.

  Raises:
    OSError: Error occurred while deleting files in {directory_path}
  """
  try:
    files = os.listdir(directory_path)
    for file in files:
      file_path = os.path.join(directory_path, file)

      # Only remove pngs.
      filename = os.path.basename(file_path)
      if filename.endswith('.png') and os.path.isfile(file_path):
        os.remove(file_path)
    print('All files deleted successfully from ' + directory_path)
  except OSError:
    print('Error occurred while deleting files in ' + directory_path)


def download_icons_from_android_search():
  """Downloads icons from the android_search gstatic directory.

  Goes through all search engines in `prepopulated_engines.json` and downloads
  the corresponding 96x96 icon from the appropriate subfolder of
  https://www.gstatic.com/android_search/search_providers/. Because there is no
  way to list the contents of a directory on gstatic and because some
  subfolders are not named exactly the same as in `prepopulated_engines.json`,
  this function loads a config file `generate_search_engine_icons_config.json`
  with the extra information needed to locate the icons.

  The Google Search icon is not downloaded because it already exists in the
  repo. Search engines not relevant to the to the default search engine choice
  screen are ignored.
  """
  config_file_path = '../../tools/search_engine_choice/generate_search_engine_icons_config.json'
  prepopulated_engines_file_path = '../search_engines/prepopulated_engines.json'
  image_destination_path = './default_100_percent/search_engine_choice/'

  delete_files_in_directory(image_destination_path)

  with open(config_file_path, 'r', encoding='utf-8') as config_json, open(
      prepopulated_engines_file_path, 'r', encoding='utf-8') as engines_json:
    config_data = commentjson.loads(config_json.read())
    engine_data = commentjson.loads(engines_json.read())

    icon_hash_to_name = {}

    for engine in engine_data['elements']:
      if engine not in used_engines or engine in config_data['ignored_engines']:
        continue

      search_engine_keyword = engine_data['elements'][engine]['keyword']
      icon_name = keyword_to_identifer(search_engine_keyword)
      icon_full_path = image_destination_path + f'{icon_name}.png'
      if engine in config_data['engine_aliases']:
        engine = config_data['engine_aliases'][engine]

      directory_url = f'https://www.gstatic.com/android_search/search_providers/{engine}/'
      try_filenames = [
          config_data['non_default_icon_filenames'][engine]
      ] if engine in config_data['non_default_icon_filenames'] else [
          'xxxhdpi.png', f'{engine}_xxxhdpi.png'
      ]
      any_found = False
      for filename in try_filenames:
        icon_url = directory_url + filename
        try:
          img_data = requests.get(icon_url)
        except requests.exceptions.RequestException as e:
          print('Error when loading URL {icon_url}: {e}')
          continue
        if img_data.status_code == 200:
          with open(icon_full_path, 'wb') as icon_file:
            icon_file.write(img_data.content)
          any_found = True
          break
      if not any_found:
        print('WARNING: no icon found for search engine: %s' % engine)
        continue

      icon_hash = get_image_hash(icon_full_path)
      if icon_hash in icon_hash_to_name:
        # We already have this icon.
        engine_keyword_to_icon_name[search_engine_keyword] = icon_hash_to_name[
            icon_hash]
        os.remove(icon_full_path)
        continue

      engine_keyword_to_icon_name[search_engine_keyword] = icon_name
      icon_hash_to_name[icon_hash] = icon_name
  print('Finished downloading icons')
  os.system('../../tools/resources/optimize-png-files.sh ' +
            image_destination_path)


def generate_icon_resource_code():
  """Links the downloaded icons to their respective resource id.

  Generates the code to link the icons to a resource ID in
  `search_engine_choice_scaled_resources.grdp`
  """
  print('Writing to search_engine_choice_scaled_resources.grdp...')
  with open('./search_engine_choice_scaled_resources.grdp',
            'w',
            encoding='utf-8',
            newline='') as grdp_file:
    grdp_file.write('<?xml version="1.0" encoding="utf-8"?>\n')
    grdp_file.write(
        '<!-- This file is generated using generate_search_engine_icons.py'
        ' -->\n')
    grdp_file.write("<!-- Don't modify it manually -->\n")
    grdp_file.write('<grit-part>\n')

    # Add the google resource id.
    grdp_file.write('  <if expr="_google_chrome">\n')
    grdp_file.write('    <structure type="chrome_scaled_image"'
                    ' name="IDR_GOOGLE_COM_PNG"'
                    ' file="google_chrome/google_search_logo.png" />\n')
    grdp_file.write('  </if>\n')

    # Add the remaining resource ids.
    for engine_keyword, icon_name in engine_keyword_to_icon_name.items():
      resource_id = 'IDR_' + keyword_to_identifer(
          engine_keyword).upper() + '_PNG'
      grdp_file.write('  <structure type="chrome_scaled_image" name="' +
                      resource_id + '" file="search_engine_choice/' +
                      icon_name + '.png" />\n')

    grdp_file.write('</grit-part>\n')


def create_adding_icons_to_source_function():
  """Generates the `AddGeneratedIconResources` in
  `search_engine_choice/generated_icon_utils.cc`.

  Generates the function that will be used to populate the `WebUIDataSource`
  with the generated icons.
  """
  print('Creating `AddGeneratedIconResources` function...')

  with open(
      '../../chrome/browser/ui/webui/search_engine_choice/generated_icon_utils.cc',
      'w',
      encoding='utf-8',
      newline='') as utils_file:

    # Add the copyright notice.
    utils_file.write('// Copyright 2023 The Chromium Authors\n')
    utils_file.write('// Use of this source code is governed by a BSD-style'
                     ' license that can be\n')
    utils_file.write('// found in the LICENSE file.\n\n')

    # Include the required header files.
    utils_file.write(
        '#include "chrome/browser/ui/webui/search_engine_choice/icon_utils.h"\n\n'
    )
    utils_file.write('#include "base/check_op.h"\n')
    utils_file.write('#include "build/branding_buildflags.h"\n')
    utils_file.write(
        '#include "components/grit/components_scaled_resources.h"\n')
    utils_file.write(
        '#include "content/public/browser/web_ui_data_source.h"\n\n')

    # Create the function name.
    utils_file.write(
        '// This code is generated using `generate_search_engine_icons.py`.'
        " Don't modify it manually.\n")
    utils_file.write('void AddGeneratedIconResources(content::WebUIDataSource*'
                     ' source, const std::string& directory) {\n')
    utils_file.write('\tCHECK(source);\n')
    utils_file.write("\tCHECK_EQ(directory.back(), '/');\n")

    # Add google to the source
    utils_file.write('\t#if BUILDFLAG(GOOGLE_CHROME_BRANDING)\n')
    utils_file.write('\tsource->AddResourcePath(directory + "google_com.png",'
                     ' IDR_GOOGLE_COM_PNG);\n')
    utils_file.write('\t#endif\n')

    for engine_keyword in engine_keyword_to_icon_name:
      engine_name = keyword_to_identifer(engine_keyword)
      local_image_path = engine_name + '.png'
      image_resource_id = 'IDR_' + engine_name.upper() + '_PNG'
      utils_file.write('\tsource->AddResourcePath(directory + "' +
                       local_image_path + '", ' + image_resource_id + ');\n')

    utils_file.write('}\n')


if sys.platform != 'linux':
  print(
      'Warning: This script has not been tested outside of the Linux platform')

# Move to working directory to `src/components/resources/`.
current_file_path = os.path.dirname(__file__)
os.chdir(current_file_path)
os.chdir('../../components/resources')

# A set of search engines that are used in `template_url_prepopulate_data.cc`
used_engines = set()

# This is a dictionary of engine keyword to corresponding icon name. Have an
# empty icon name would mean that we weren't able to download the favicon for
# that engine.
engine_keyword_to_icon_name = {}

populate_used_engines()
download_icons_from_android_search()
generate_icon_resource_code()
create_adding_icons_to_source_function()
# Format the generated code
os.system('git cl format')
print('Icon and code generation completed.')
