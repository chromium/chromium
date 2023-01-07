# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Bash auto completion support.

Contains the special mode that returns lists of possible completions for the
current command line.
"""

from __future__ import print_function

import cr


def Complete():
  """Attempts to build a completion list for the current command line.

  COMP_WORD contains the word that is being completed, and COMP_CWORD has
  the index of that word on the command line.
  """

  # TODO(iancottrell): support auto complete of more than just the command
  # try to parse the command line using parser
  print(' '.join(command.name for command in cr.Command.Plugins()))
