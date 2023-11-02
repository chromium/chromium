# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A package for all the built in commands.

This package has all the standard commands built in to the cr tool.
Most commands use actions to perform the real work.
"""

import cr

cr.Import(__name__, 'command')
cr.Import(__name__, 'prepare')
cr.Import(__name__, 'init')
