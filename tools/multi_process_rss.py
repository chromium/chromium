#!/usr/bin/env python
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Counts a resident set size (RSS) of multiple processes without double-counts.
# If they share the same page frame, the page frame is counted only once.
#
# Usage:
# ./multi-process-rss.py <pid>|<pid>r [...]
#
# If <pid> has 'r' at the end, all descendants of the process are accounted.
#
# Example:
# ./multi-process-rss.py 12345 23456r
#
# The command line above counts the RSS of 1) process 12345, 2) process 23456
# and 3) all descendant processes of process 23456.

from __future__ import print_function

import collections
import logging
import os
import psutil
import sys


if sys.platform.startswith('linux'):
  _TOOLS_PATH = os.path.dirname(os.path.abspath(__file__))
  _TOOLS_LINUX_PATH = os.path.join(_TOOLS_PATH, 'linux')
  sys.path.append(_TOOLS_LINUX_PATH)
  import procfs  # pylint: disable=F0401


class _NullHandler(logging.Handler):
  def emit(self, record):
    pass


_LOGGER = logging.getLogger('multi-process-rss')
_LOGGER.addHandler(_NullHandler())


def _recursive_get_children(pid):
  try:
    children = psutil.Process(pid).get_children()
  except psutil.error.NoSuchProcess:
    return []
  descendant = []
  for child in children:
    descendant.append(child.pid)
    descendant.extend(_recursive_get_children(child.pid))
  return descendant


def list_pids(argv):
  pids = []
  for arg in argv[1:]:
    try:
      if arg.endswith('r'):
        recursive = True
        pid = int(arg[:-1])
      else:
        recursive = False
        pid = int(arg)
    except ValueError:
      raise SyntaxError("%s is not an integer." % arg)
    else:
      pids.append(pid)
    if recursive:
      children = _recursive_get_children(pid)
      pids.extend(children)

  pids = sorted(set(pids), key=pids.index)  # uniq: maybe slow, but simple.

  return pids


def count_pageframes(pids):
  pageframes = collections.defaultdict(int)
  pagemap_dct = {}
  for pid in pids:
    maps = procfs.ProcMaps.load(pid)
    if not maps:
      _LOGGER.warning('/proc/%d/maps not found.' % pid)
      continue
    pagemap = procfs.ProcPagemap.load(pid, maps)
    if not pagemap:
      _LOGGER.warning('/proc/%d/pagemap not found.' % pid)
      continue
    pagemap_dct[pid] = pagemap

  for pid, pagemap in pagemap_dct.iteritems():
    for vma in pagemap.vma_internals.itervalues():
      for pageframe, number in vma.pageframes.iteritems():
        pageframes[pageframe] += number

  return pageframes


def count_statm(pids):
  resident = 0
  shared = 0
  private = 0

  for pid in pids:
    statm = procfs.ProcStatm.load(pid)
    if not statm:
      _LOGGER.warning('/proc/%d/statm not found.' % pid)
      continue
    resident += statm.resident
    shared += statm.share
    private += (statm.resident - statm.share)

  return (resident, shared, private)


def main(argv):
  logging_handler = logging.StreamHandler()
  logging_handler.setLevel(logging.WARNING)
  logging_handler.setFormatter(logging.Formatter(
      '%(asctime)s:%(name)s:%(levelname)s:%(message)s'))

  _LOGGER.setLevel(logging.WARNING)
  _LOGGER.addHandler(logging_handler)

  if sys.platform.startswith('linux'):
    logging.getLogger('procfs').setLevel(logging.WARNING)
    logging.getLogger('procfs').addHandler(logging_handler)
    pids = list_pids(argv)
    pageframes = count_pageframes(pids)
  else:
    _LOGGER.error('%s is not supported.' % sys.platform)
    return 1

  # TODO(dmikurube): Classify this total RSS.
  print(len(pageframes) * 4096)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
