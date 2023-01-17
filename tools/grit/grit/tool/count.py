# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Count number of occurrences of a given message ID.'''


import getopt
import sys

from grit import grd_reader
from grit.tool import interface


class CountMessage(interface.Tool):
  '''Count the number of times a given message ID is used.'''

  def __init__(self):
    pass

  def ShortDescription(self):
    return 'Count the number of times a given message ID is used.'

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
      print('This tool takes a single tool-specific argument, the message '
            'ID to count.')
      return 2
    self.SetOptions(opts)

    id = args[0]
    res_tree = grd_reader.Parse(opts.input, debug=opts.extra_verbose)
    res_tree.OnlyTheseTranslations([])
    res_tree.RunGatherers()

    count = 0
    for c in res_tree.UberClique().AllCliques():
      if c.GetId() == id:
        count += 1

    print("There are %d occurrences of message %s." % (count, id))
