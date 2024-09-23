# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def memoize(fn):
  '''Decorates |fn| to memoize.
  '''
  memory = {}

  def impl(*args, **optargs):
    full_args = args + tuple(optargs.items())
    if full_args not in memory:
      memory[full_args] = fn(*args, **optargs)
    return memory[full_args]

  return impl
