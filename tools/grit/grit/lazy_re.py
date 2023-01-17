# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''In GRIT, we used to compile a lot of regular expressions at parse
time.  Since many of them never get used, we use lazy_re to compile
them on demand the first time they are used, thus speeding up startup
time in some cases.
'''


import re


class LazyRegexObject:
  '''This object creates a RegexObject with the arguments passed in
  its constructor, the first time any attribute except the several on
  the class itself is accessed.  This accomplishes lazy compilation of
  the regular expression while maintaining a nearly-identical
  interface.
  '''

  def __init__(self, *args, **kwargs):
    self._stash_args = args
    self._stash_kwargs = kwargs
    self._lazy_re = None

  def _LazyInit(self):
    if not self._lazy_re:
      self._lazy_re = re.compile(*self._stash_args, **self._stash_kwargs)

  def __getattribute__(self, name):
    if name in ('_LazyInit', '_lazy_re', '_stash_args', '_stash_kwargs'):
      return object.__getattribute__(self, name)
    else:
      self._LazyInit()
      return getattr(self._lazy_re, name)


def compile(*args, **kwargs):
  '''Creates a LazyRegexObject that, when invoked on, will compile a
  re.RegexObject (via re.compile) with the same arguments passed to
  this function, and delegate almost all of its methods to it.
  '''
  return LazyRegexObject(*args, **kwargs)
