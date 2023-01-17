# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Tool to create a new, empty .grd file with all the basic sections.
'''


import getopt
import sys

from grit.tool import interface
from grit import constants
from grit import util

# The contents of the new .grd file
_FILE_CONTENTS = '''\
<?xml version="1.0" encoding="UTF-8"?>
<grit base_dir="." latest_public_release="0" current_release="1"
      source_lang_id="en" enc_check="%s">
  <outputs>
    <!-- TODO add each of your output files.  Modify the three below, and add
    your own for your various languages.  See the user's guide for more
    details.
    Note that all output references are relative to the output directory
    which is specified at build time. -->
    <output filename="resource.h" type="rc_header" />
    <output filename="en_resource.rc" type="rc_all" />
    <output filename="fr_resource.rc" type="rc_all" />
  </outputs>
  <translations>
    <!-- TODO add references to each of the XTB files (from the Translation
    Console) that contain translations of messages in your project.  Each
    takes a form like <file path="english.xtb" />.  Remember that all file
    references are relative to this .grd file. -->
  </translations>
  <release seq="1">
    <includes>
      <!-- TODO add a list of your included resources here, e.g. BMP and GIF
      resources. -->
    </includes>
    <structures>
      <!-- TODO add a list of all your structured resources here, e.g. HTML
      templates, menus, dialogs etc.  Note that for menus, dialogs and version
      information resources you reference an .rc file containing them.-->
    </structures>
    <messages>
      <!-- TODO add all of your "string table" messages here.  Remember to
      change nontranslateable parts of the messages into placeholders (using the
      <ph> element).  You can also use the 'grit add' tool to help you identify
      nontranslateable parts and create placeholders for them. -->
    </messages>
  </release>
</grit>''' % constants.ENCODING_CHECK


class NewGrd(interface.Tool):
  '''Usage: grit newgrd OUTPUT_FILE

Creates a new, empty .grd file OUTPUT_FILE with comments about what to put
where in the file.'''

  def ShortDescription(self):
    return 'Create a new empty .grd file.'

  def ParseOptions(self, args):
    """Set this objects and return all non-option arguments."""
    own_opts, args = getopt.getopt(args, '', ('help',))
    for key, val in own_opts:
      if key == '--help':
        self.ShowUsage()
        sys.exit(0)
    return args

  def Run(self, opts, args):
    args = self.ParseOptions(args)
    if len(args) != 1:
      print('This tool requires exactly one argument, the name of the output '
            'file.')
      return 2
    filename = args[0]
    with util.WrapOutputStream(open(filename, 'wb'), 'utf-8') as out:
      out.write(_FILE_CONTENTS)
    print("Wrote file %s" % filename)
