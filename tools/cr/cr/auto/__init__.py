# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A root package for all the automatically loaded modules.

There are no built in packages below this, it holds a set of packages that
have their search paths modified in order to pick up plugins from directories
that are not known until run-time.
"""
