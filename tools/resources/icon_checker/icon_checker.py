# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os


def FetchValidIconNames():
  """Reads the list of valid Material Symbol names from a local JSON file.

  Returns:
    A set of valid icon names (using underscores), or None if the file cannot
    be read.
  """
  script_dir = os.path.dirname(os.path.abspath(__file__))
  icon_list_path = os.path.join(script_dir, 'icon_list.json')

  with open(icon_list_path, 'r', encoding='utf-8') as f:
    data = json.load(f)
    return set(data.get('icons', []))


def ExtractIconsFromHtml(input_api):
  """Extracts icon names from HTML files in the affected files.

  Args:
    input_api: The presubmit input API.

  Returns:
    A list of (file_path, icon_name, line_num) tuples.
  """
  affected_icons = []
  for f in input_api.AffectedFiles(include_deletes=False):
    basename = input_api.os_path.basename(f.LocalPath())
    if not (basename.endswith('_icons.html') or basename == 'icons.html' or
            basename.endswith('_icons.html.ts') or basename == 'icons.html.ts'):
      continue
    # Only check files that appear to be iconsets.
    if not any('<cr-iconset' in line for line in f.NewContents()):
      continue
    for line_num, line in f.ChangedContents():
      # Look for <g id="icon-name">
      for match in input_api.re.finditer(r'<g [^>]*id="([^"]+)"', line):
        icon_name = match.group(1)
        affected_icons.append((f.LocalPath(), icon_name, line_num))
  return affected_icons


def CheckIcons(input_api, output_api, affected_icons):
  """Checks if icons match Google Fonts naming.

  Args:
    input_api: The presubmit input API.
    output_api: The presubmit output API.
    affected_icons: A list of (file_path, icon_name, line_num) tuples.

  Returns:
    A list of presubmit results.
  """
  if not affected_icons:
    return []

  valid_names = FetchValidIconNames()
  assert valid_names is not None, \
    'Could not read tools/resources/icon_list.json.'

  results = []
  for file_path, icon_name, line_num in affected_icons:
    # Convert hyphens to underscores for comparison.
    name_to_check = icon_name.replace('-', '_')
    if name_to_check not in valid_names:
      msg = (
          f'File {file_path}:{line_num if line_num else ""}\n'
          f'Icon "{icon_name}" does not match the name of any known icon. '
          f'If you are adding a new icon into the code base, please consider '
          f'matching the name of the .icon file to its appropriate name in '
          f'icon_list.json. Doing so allows developers and UX partners to '
          f'quickly cross reference the icons when it comes time to update '
          f'them. If this icon is intended to be custom please disregard this '
          'warning.')
      results.append(output_api.PresubmitPromptWarning(msg))

  return results
