# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A package for the base supporting classes of the cr tool."""

import cr

cr.Import(__name__, 'platform')
cr.Import(__name__, 'buildtype')
cr.Import(__name__, 'client')
