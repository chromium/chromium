# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A package to hold all the actions for the cr tool.

This package holds the standard actions used by the commands in the cr tool.
These actions are the things that actually perform the work, they are generally
run in sequences by commands.
"""

import cr

cr.Import(__name__, 'action')
cr.Import(__name__, 'runner')
cr.Import(__name__, 'builder')
cr.Import(__name__, 'installer')
