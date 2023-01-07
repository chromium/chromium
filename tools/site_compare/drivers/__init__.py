# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Imports a set of drivers appropriate to the current OS."""

import sys

platform_dir = sys.platform

keyboard  = __import__(platform_dir+".keyboard",  globals(), locals(), [''])
mouse     = __import__(platform_dir+".mouse",     globals(), locals(), [''])
windowing = __import__(platform_dir+".windowing", globals(), locals(), [''])
