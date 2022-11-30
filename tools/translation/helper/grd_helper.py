# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import tempfile

here = os.path.dirname(os.path.realpath(__file__))
repo_root = os.path.normpath(os.path.join(here, '..', '..', '..'))

try:
  old_sys_path = sys.path
  sys.path = sys.path + [os.path.join(repo_root, 'tools', 'grit')]
  import grit.grd_reader
  import grit.node.message
  import grit.util
finally:
  sys.path = old_sys_path

TAGS_TO_IGNORE = (
    # <include> tags mostly point to resource files that don't contain UI
    # strings.
    'include',
    # <structure> tags point to image files.
    'structure',
    # <part> tags point to .grdp files. Don't load included part files when
    # loading a .grd or .grdp file. A grd file's contents can refer to deleted
    # grdp files (e.g. if a grdp file was renamed). Trying to load it would
    # fail. It's also unnecessary to load <part> files because grdp files are
    # handled separately in GetGrdpMessagesFromString. Grdp files are also
    # expected to not contain any <part> tags.
    'part')


def GetGrdMessages(grd_path_or_string, dir_path):
  """Load the grd file and return a dict of message ids to messages.

  Ignores non-translateable messages."""
  doc = grit.grd_reader.Parse(grd_path_or_string,
                              dir_path,
                              stop_after=None,
                              first_ids_file=None,
                              debug=False,
                              defines={'_chromium': 1},
                              tags_to_ignore=set(TAGS_TO_IGNORE),
                              skip_validation_checks=True)
  return {
      msg.attrs['name']: msg
      for msg in doc.GetChildrenOfType(grit.node.message.MessageNode)
      if msg.IsTranslateable() and not msg.IsAccessibilityWithNoUI()
  }


def GetGrdpMessagesFromString(grdp_string):
  """Parses the contents of the grdp file given in grdp_string.

    grd_reader can't parse grdp files directly. Instead, this replaces grd
    specific tags in the input string with grdp specific tags, writes the output
    string in a temporary file and loads the grd from the temporary file.

    This code previously created a temporary directory (using grit.util), wrote
    a temporary grd file pointing to another temporary grdp file that contained
    the input string. This was not testable since os.path.exists is overridden
    in tests and grit.util uses mkdirs which in turn calls os.path.exists. This
    new code works because it doesn't need to check the existence of a path.

    It's expected that a grdp file will not refer to another grdp file via a
    <part> tag. Currently, none of the grdp files in the repository does that.
     """
  replaced_string = grdp_string.replace(
      '<grit-part>',
      """<grit base_dir="." latest_public_release="1" current_release="1">
            <release seq="1">
              <messages fallback_to_english="true">
        """)
  replaced_string = replaced_string.replace(
      '</grit-part>', """</messages>
          </release>
        </grit>""")
  with tempfile.NamedTemporaryFile(delete=False) as f:
    f.write(replaced_string.encode('utf-8'))
    f.close()
    messages = GetGrdMessages(f.name, os.path.dirname(f.name))
    os.remove(f.name)
    return messages
