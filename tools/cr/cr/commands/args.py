# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the args command."""

from __future__ import print_function

import os

import cr

class ArgsCommand(cr.Command):
  """The implementation of the args command.

  The args command is meant for editing the current build configuration
  in a text editor.
  """

  def __init__(self):
    super(ArgsCommand, self).__init__()
    self.help = 'Edit build configuration in a text editor'
    self.description = ("""
        Opens the configuration for the currently selected out directory in
        a text editor.
        """)

  def Run(self):
    build_config_path = cr.context.Get('CR_BUILD_CONFIG_PATH')
    editor = os.environ.get('EDITOR', 'vi')
    print('Opening %s in a text editor (%s)...' % (build_config_path, editor))
    cr.Host.Execute(editor, build_config_path)
    # TODO(petrcermak): Figure out a way to do this automatically.
    print('Please run \'cr prepare\' if you modified the file')
