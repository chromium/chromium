# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the info implementation of Command."""

from __future__ import print_function

import cr


class InfoCommand(cr.Command):
  """The cr info command implementation."""

  def __init__(self):
    super(InfoCommand, self).__init__()
    self.help = 'Print information about the cr environment'

  def AddArguments(self, subparsers):
    parser = super(InfoCommand, self).AddArguments(subparsers)
    parser.add_argument(
        '-s', '--short', dest='_short',
        action='store_true', default=False,
        help='Short form results, useful for scripting.'
    )
    self.ConsumeArgs(parser, 'the environment')
    return parser

  def EarlyArgProcessing(self):
    if getattr(cr.context.args, '_short', False):
      self.requires_build_dir = False
    cr.Command.EarlyArgProcessing(self)

  def Run(self):
    if cr.context.remains:
      for var in cr.context.remains:
        if getattr(cr.context.args, '_short', False):
          val = cr.context.Find(var)
          if val is None:
            val = ''
          print(val)
        else:
          print(var, '=', cr.context.Find(var))
    else:
      cr.base.client.PrintInfo()
