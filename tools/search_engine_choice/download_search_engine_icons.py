# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Handles the download of the search engine favicons.

For all search engines referenced in search_engine_countries-inc.cc,
downloads their Favicon, scales it and puts it as resource into the repository
for display, e.g. in the search engine choice UI and settings.

This should be run whenever search_engine_countries-inc.cc changes the list of
search engines used per country, or whenever prepopulated_engines.json changes
a favicon.

To run:
`python3 tools/search_engine_choice/download_search_engine_icons.py`.
"""

import hashlib
import json
import os
import sys
import requests

import search_engine_icons_utils


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
  for percent in ['100', '200', '300']:
    delete_files_in_directory(
        f'components/resources/default_{percent}_percent/search_engine_choice')

  with open(search_engine_icons_utils.config_file_path, 'r',
            encoding='utf-8') as config_json:
    config_data = json.loads(json_comment_eater.Nom(config_json.read()))
    icon_hash_to_name = {}

    for (engine, keyword) in sorted(
        search_engine_icons_utils.get_used_engines_with_keywords("")):
      icon_name = search_engine_icons_utils.keyword_to_identifer(keyword)
      icon_full_path = f'components/resources/default_100_percent/search_engine_choice/{icon_name}.png'
      if engine in config_data['engine_aliases']:
        engine = config_data['engine_aliases'][engine]

      directory_url = f'https://www.gstatic.com/android_search/search_providers/{engine}/'
      try_filenames = []
      if engine in config_data['non_default_icon_filenames']:
        try_filenames = [
            config_data['non_default_icon_filenames'][engine] + 'mdpi.png'
        ]
      try_filenames = try_filenames + [
          f'{engine}_icon_mdpi.png',
          f'{engine}_mdpi.png',
          'mdpi.png',
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
        print('WARNING: no icon found for search engine: ' + engine)
        continue

      icon_hash = get_image_hash(icon_full_path)
      if icon_hash in icon_hash_to_name:
        # We already have this icon.
        engine_keyword_to_icon_name[keyword] = icon_hash_to_name[icon_hash]
        os.remove(icon_full_path)
        continue

      # Download hidpi versions
      # If the low dpi version is basename_mdpi.png, download basename_xhdpi.png
      # and basename_xxhdpi.png.
      for (resource_path, hidpi) in [('default_200_percent', 'xhdpi'),
                                     ('default_300_percent', 'xxhdpi')]:
        # Replace the substring "mdpi" by "xhdpi" or "xxhdpi" from the end.
        (basename, mdpi_suffix, png_extension) = icon_url.rpartition('mdpi')
        hidpi_url = basename + hidpi + png_extension
        hidpi_path = f'components/resources/{resource_path}/search_engine_choice/{icon_name}.png'
        try:
          img_data = requests.get(hidpi_url)
        except requests.exceptions.RequestException as e:
          print('Error when loading URL {hidpi_url}: {e}')
          continue
        if img_data.status_code == 200:
          with open(hidpi_path, 'wb') as icon_file:
            icon_file.write(img_data.content)
        else:
          print('WARNING: no %s icon found for search engine: %s' %
                (hidpi, engine))

      engine_keyword_to_icon_name[keyword] = icon_name
      icon_hash_to_name[icon_hash] = icon_name
  print('Finished downloading icons')
  os.system('tools/resources/optimize-png-files.sh search_engine_choice')


def generate_icon_resource_code():
  """Links the downloaded icons to their respective resource id.

  Generates the code to link the icons to a resource ID in
  `search_engine_choice_scaled_resources.grdp`
  """
  print('Writing to search_engine_choice_scaled_resources.grdp...')
  with open('components/resources/search_engine_choice_scaled_resources.grdp',
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

    # Add the remaining resource ids, sorted alphabetically.
    resources = []
    for engine_keyword, icon_name in engine_keyword_to_icon_name.items():
      resource_id = search_engine_icons_utils.keyword_to_resource_name(
          engine_keyword)
      resources.append((resource_id, icon_name))

    for resource_id, icon_name in sorted(resources):
      grdp_file.write(
          f'  <structure type="chrome_scaled_image" name="{resource_id}" file="search_engine_choice/{icon_name}.png" />\n'
      )

    grdp_file.write('</grit-part>\n')


if sys.platform != 'linux':
  print(
      'Warning: This script has not been tested outside of the Linux platform')

# Move to working directory to `src/`.
current_file_path = os.path.dirname(__file__)
os.chdir(current_file_path)
os.chdir('../../')

sys.path.insert(0,
                os.path.normpath(current_file_path + "/../json_comment_eater"))
try:
  import json_comment_eater
finally:
  sys.path.pop(0)

# This is a dictionary of engine keyword to corresponding icon name. Have an
# empty icon name would mean that we weren't able to download the favicon for
# that engine.
engine_keyword_to_icon_name = {}

download_icons_from_android_search()
generate_icon_resource_code()
print('Icon download completed.')
