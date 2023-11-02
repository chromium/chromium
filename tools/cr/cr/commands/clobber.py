# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the clobber command."""

from __future__ import print_function

import os

import cr


class ClobberCommand(cr.Command):
  """The implementation of the clobber command.

  The clobber command removes all generated files from the output directory.
  """

  def __init__(self):
    super(ClobberCommand, self).__init__()
    self.help = 'Clobber the current output directory'
    self.description = ("""
        This deletes all generated files from the output directory.
        """)

  def Run(self):
    self.Clobber()

  @classmethod
  def Clobber(cls):
    """Performs the clobber."""
    build_dir = cr.context.Get('CR_BUILD_DIR')
    clobber_path = os.path.join('{CR_SRC}', 'build', 'clobber.py')
    print('Clobbering output directory %s...' % build_dir)
    cr.Host.Execute(clobber_path, build_dir)
    print('Done')
