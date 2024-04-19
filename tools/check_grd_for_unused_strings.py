#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Without any args, this simply loads the IDs out of a bunch of the Chrome GRD
files, and then checks the subset of the code that loads the strings to try
and figure out what isn't in use any more.
You can give paths to GRD files and source directories to control what is
check instead.
"""

from __future__ import print_function

import os
import re
import sys
import xml.sax

# Extra messages along the way
# 1 - Print ids that are found in sources but not in the found id set
# 2 - Files that aren't processes (don't match the source name regex)
DEBUG = 0


class GrdIDExtractor(xml.sax.handler.ContentHandler):
  """Extracts the IDs from messages in GRIT files"""
  def __init__(self):
    self.id_set_ = set()

  def startElement(self, name, attrs):
    if name == 'message':
      self.id_set_.add(attrs['name'])

  def allIDs(self):
    """Return all the IDs found"""
    return self.id_set_.copy()


def CheckForUnusedGrdIDsInSources(grd_files, src_dirs):
  """Will collect the message ids out of the given GRD files and then scan
  the source directories to try and figure out what ids are not currently
  being used by any source.

  grd_files:
    A list of GRD files to collect the ids from.
  src_dirs:
    A list of directories to walk looking for source files.
  """
  # Collect all the ids into a large map
  all_ids = set()
  file_id_map = {}
  for y in grd_files:
    handler = GrdIDExtractor()
    xml.sax.parse(y, handler)
    files_ids = handler.allIDs()
    file_id_map[y] = files_ids
    all_ids |= files_ids


  # The regex that will be used to check sources
  id_regex = re.compile('IDS_[A-Z0-9_]+')

  # Make sure the regex matches every id found.
  got_err = False
  for x in all_ids:
    match = id_regex.search(x)
    if match is None:
      print('ERROR: "%s" did not match our regex' % x)
      got_err = True
    if not match.group(0) is x:
      print('ERROR: "%s" did not fully match our regex' % x)
      got_err = True
  if got_err:
    return 1

  # The regex for deciding what is a source file
  src_regex = re.compile('\.(([chm])|(mm)|(cc)|(cp)|(cpp)|(xib)|(py))$')

  ids_left = all_ids.copy()

  # Scanning time.
  for src_dir in src_dirs:
    for root, dirs, files in os.walk(src_dir):
      # Remove svn directories from recursion
      if '.svn' in dirs:
        dirs.remove('.svn')
      for file in files:
        if src_regex.search(file.lower()):
          full_path = os.path.join(root, file)
          src_file_contents = open(full_path).read()
          for match in sorted(set(id_regex.findall(src_file_contents))):
            if match in ids_left:
              ids_left.remove(match)
            if DEBUG:
              if not match in all_ids:
                print('%s had "%s", which was not in the found IDs' % \
                  (full_path, match))
        elif DEBUG > 1:
          full_path = os.path.join(root, file)
          print('Skipping %s.' % full_path)

  # Anything left?
  if len(ids_left) > 0:
    print('The following ids are in GRD files, but *appear* to be unused:')
    for file_path, file_ids in file_id_map.iteritems():
      missing = ids_left.intersection(file_ids)
      if len(missing) > 0:
        print('  %s:' % file_path)
        print('\n'.join('    %s' % (x) for x in sorted(missing)))

  return 0


def main():
  # script lives in src/tools
  tools_dir = os.path.dirname(os.path.abspath(sys.argv[0]))
  src_dir = os.path.dirname(tools_dir)

  # Collect the args into the right buckets
  src_dirs = []
  grd_files = []
  for arg in sys.argv[1:]:
    if arg.lower().endswith('.grd') or arg.lower().endswith('.grdp'):
      grd_files.append(arg)
    else:
      src_dirs.append(arg)

  # If no GRD files were given, default them:
  if len(grd_files) == 0:
    ash_base_dir = os.path.join(src_dir, 'ash')
    chrome_dir = os.path.join(src_dir, 'chrome')
    chrome_app_dir = os.path.join(chrome_dir, 'app')
    chrome_app_res_dir = os.path.join(chrome_app_dir, 'resources')
    device_base_dir = os.path.join(src_dir, 'device')
    services_dir = os.path.join(src_dir, 'services')
    ui_dir = os.path.join(src_dir, 'ui')
    ui_strings_dir = os.path.join(ui_dir, 'strings')
    ui_chromeos_dir = os.path.join(ui_dir, 'chromeos')
    grd_files = [
        os.path.join(ash_base_dir, 'ash_strings.grd'),
        os.path.join(chrome_app_dir, 'chromium_strings.grd'),
        os.path.join(chrome_app_dir, 'generated_resources.grd'),
        os.path.join(chrome_app_dir, 'google_chrome_strings.grd'),
        os.path.join(chrome_app_res_dir, 'locale_settings.grd'),
        os.path.join(chrome_app_res_dir, 'locale_settings_chromiumos.grd'),
        os.path.join(chrome_app_res_dir, 'locale_settings_google_chromeos.grd'),
        os.path.join(chrome_app_res_dir, 'locale_settings_linux.grd'),
        os.path.join(chrome_app_res_dir, 'locale_settings_mac.grd'),
        os.path.join(chrome_app_res_dir, 'locale_settings_win.grd'),
        os.path.join(chrome_app_dir, 'theme', 'theme_resources.grd'),
        os.path.join(chrome_dir, 'browser', 'browser_resources.grd'),
        os.path.join(chrome_dir, 'common', 'common_resources.grd'),
        os.path.join(chrome_dir, 'renderer', 'resources',
                     'renderer_resources.grd'),
        os.path.join(device_base_dir, 'bluetooth', 'bluetooth_strings.grd'),
        os.path.join(device_base_dir, 'fido', 'fido_strings.grd'),
        os.path.join(services_dir, 'services_strings.grd'),
        os.path.join(src_dir, 'chromeos', 'chromeos_strings.grd'),
        os.path.join(src_dir, 'extensions', 'strings',
                     'extensions_strings.grd'),
        os.path.join(src_dir, 'ui', 'resources', 'ui_resources.grd'),
        os.path.join(src_dir, 'ui', 'webui', 'resources',
                     'webui_resources.grd'),
        os.path.join(ui_strings_dir, 'app_locale_settings.grd'),
        os.path.join(ui_strings_dir, 'ax_strings.grd'),
        os.path.join(ui_strings_dir, 'ui_strings.grd'),
        os.path.join(ui_chromeos_dir, 'ui_chromeos_strings.grd'),
    ]

  # If no source directories were given, default them:
  if len(src_dirs) == 0:
    src_dirs = [
      os.path.join(src_dir, 'app'),
      os.path.join(src_dir, 'ash'),
      os.path.join(src_dir, 'chrome'),
      os.path.join(src_dir, 'components'),
      os.path.join(src_dir, 'content'),
      os.path.join(src_dir, 'device'),
      os.path.join(src_dir, 'extensions'),
      os.path.join(src_dir, 'ui'),
      # nsNSSCertHelper.cpp has a bunch of ids
      os.path.join(src_dir, 'third_party', 'mozilla_security_manager'),
      os.path.join(chrome_dir, 'installer'),
    ]

  return CheckForUnusedGrdIDsInSources(grd_files, src_dirs)


if __name__ == '__main__':
  sys.exit(main())
