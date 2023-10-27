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

      # Only remove pngs and don't remove the default icon (globe)
      filename = os.path.basename(file_path)
      if not filename.endswith('.png') or filename == 'default_favicon.png':
        continue

      if os.path.isfile(file_path):
        os.remove(file_path)
    print('All files deleted successfully from ' + directory_path)
  except OSError:
    print('Error occurred while deleting files in ' + directory_path)


def get_largest_icon_index_and_size(icon_path, name):
  """Fetches the index and size of largest icon in the .ico file.

  Some .ico files contain more than 1 icon. The function finds the largest icon
  by comparing the icon dimensions and returns its index.
  We get the index of the largest icon because scaling an image down is better
  than scaling up.

  Returns:
    A tuple with index of the largest icon in the .ico file and its size.

  Args:
    icon_path: The path to the .ico file.
    name: Name/keyword of the search engine.
  """
  images_stream = os.popen('identify ' + icon_path).read()
  images_stream_strings = images_stream.splitlines()

  max_image_size = 0
  max_image_size_index = 0
  for index, string in enumerate(images_stream_strings):
    # Search for the string dimension example 16x16
    image_dimensions = re.search(r'[0-9]+x[0-9]+', string).group()
    # The image size is the integer before the 'x' character.
    sizes = image_dimensions.split('x')
    if sizes[0] != sizes[1]:
      print('Warning: Icon for %s is not square' % name)
    image_size = int(sizes[0])
    if image_size > max_image_size:
      max_image_size = image_size
      max_image_size_index = index

  return (max_image_size_index, max_image_size)


def create_icons_from_json_file():
  """Downloads the icons that are referenced in the json file.

  Reads the json file and downloads the icons that are referenced in the
  "favicon_url" section of the search_engine.
  Scales those icons down to 48x48 size and converts them to PNG format. After
  finishing the previous step, the function moves the icons to the destination
  directory and runs png optimization.
  The function filters the search engines based on the search engines that are
  used in `template_url_prepopulate_data.cc` so that icons that are never used
  don't get downloaded.
  The Google Search icon is not downloaded because it already exists in the
  repo.

  Raises:
    requests.exceptions.RequestException, FileNotFoundError: Error while loading
      URL {favicon_url}
  """
  print('Creating icons from json file...')
  prepopulated_engines_file_path = '../search_engines/prepopulated_engines.json'
  image_destination_path = './default_100_percent/search_engine_choice/'
  icon_sizes = [48]
  favicon_hash_to_icon_name = {}

  # Delete the previously added search engine icons
  delete_files_in_directory(image_destination_path)

  with open(prepopulated_engines_file_path, 'r',
            encoding='utf-8') as engines_json:
    # Use commentjson to ignore the comments in the json file.
    data = commentjson.loads(engines_json.read())
    for engine in data['elements']:
      # We don't need to download an icon for an engine that's not used.
      if engine not in used_engines:
        continue

      # We don't want to download the google icon because we already have it
      # in the internal repo.
      if engine == 'google':
        continue

      favicon_url = data['elements'][engine]['favicon_url']
      search_engine_keyword = data['elements'][engine]['keyword']

      try:
        # Download the icon and rename it as 'original.ico'
        img_data = requests.get(
            favicon_url,
            headers={
                # Some search engines 403 even requests for favicons if we don't
                # look like a browser
                "User-Agent":
                ("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, "
                 "like Gecko) Chrome/117.0.0.0 Safari/537.36")
            })
        icon_name = keyword_to_identifer(search_engine_keyword)
        with open('original.ico', 'wb') as original_icon:
          original_icon.write(img_data.content)

        icon_hash = get_image_hash('original.ico')
        # We already have the icon stored in the repo.
        if (icon_hash in favicon_hash_to_icon_name):
          engine_keyword_to_icon_name[
              search_engine_keyword] = favicon_hash_to_icon_name[icon_hash]
          os.remove('original.ico')
          continue

        (largest_index, largest_size) = get_largest_icon_index_and_size(
            'original.ico', icon_name)

        # Using ImageMagick command line interface, scale the icons, convert
        # them to PNG format and move them to their corresponding folders.
        last_size = 0
        for desired_size in icon_sizes:
          if largest_size >= last_size:
            last_size = desired_size
            desired_size = min(desired_size, largest_size)
            os.system('convert original.ico[' + str(largest_index) +
                      '] -thumbnail ' + str(desired_size) + 'x' +
                      str(desired_size) + ' ' + icon_name + '.png')

            shutil.move(icon_name + '.png', image_destination_path)

        engine_keyword_to_icon_name[search_engine_keyword] = icon_name
        favicon_hash_to_icon_name[icon_hash] = icon_name
        os.remove('original.ico')

      # `FileNotFoundError` is thrown if we were not able to download the
      #  favicon and we try to move it.
      except (requests.exceptions.RequestException, FileNotFoundError):
        # Favicon URL doesn't load.
        print('Error while loading URL ' + favicon_url)
        # Engine doesn't have a favicon loaded. We use the
        # default icon in that case.
        engine_keyword_to_icon_name[search_engine_keyword] = ''
        continue
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
    for engine_keyword in engine_keyword_to_icon_name:
      icon_name = engine_keyword_to_icon_name[engine_keyword]
      resource_id = 'IDR_' + keyword_to_identifer(
          engine_keyword).upper() + '_PNG'
      # No favicon loaded. Use default_favicon.png
      if not icon_name:
        grdp_file.write(
            '  <structure type="chrome_scaled_image" name="' + resource_id +
            '" file="search_engine_choice/default_favicon.png" />\n')
      else:
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
# that engine. We use the default favicon in that case.
engine_keyword_to_icon_name = {}

populate_used_engines()
create_icons_from_json_file()
generate_icon_resource_code()
create_adding_icons_to_source_function()
# Format the generated code
os.system('git cl format')
print('Icon and code generation completed.')
