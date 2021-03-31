#!/usr/bin/env python
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script is used to analyze #include graphs.

It produces the .js file that accompanies include-analysis.html.

Usage:

$ gn gen --args="show_includes=true" out/Debug
$ autoninja -C out/Debug -v base_unittests | tee /tmp/build_log
$ analyze_includes.py base_unittests $(git rev-parse --short HEAD) \
    /tmp/build_log /tmp/include-analysis.js

(Optionally, add use_goma=true to the gn args.)

The script takes a little under an hour to run on a fast machine for the chrome
build target, which is considered fast enough for batch job purposes for now.
"""

from __future__ import print_function
import argparse
import json
import os
import re
import sys
import unittest
from collections import defaultdict
from datetime import datetime


def parse_build(build_log):
  """Parse the build_log (generated as in the Usage note above) to capture the
  include graph. Returns a (roots, includes) pair, where roots is a list of root
  nodes in the graph (the source files) and includes is a dict from filename to
  list of filenames included by that filename."""
  build_dir = '.'
  file_stack = []
  includes = {}
  roots = set()

  # Note: A file might include different files for different compiler
  # invocations depending on -D flags. For such cases, includes[file] will be
  # the union of those includes.

  # ninja: Entering directory `out/foo'
  ENTER_DIR_RE = re.compile(r'ninja: Entering directory `(.*?)\'$')
  # ...clang... -c foo.cc -o foo.o ...
  COMPILE_RE = re.compile(r'.*clang.* -c (.*?) ')
  # . a.h
  # .. b.h
  # . c.h
  INCLUDE_RE = re.compile(r'(\.+) (.*)$')

  for line in build_log:
    m = ENTER_DIR_RE.match(line)
    if m:
      build_dir = m.group(1)

    m = COMPILE_RE.match(line)
    if m:
      filename = m.group(1)
      roots.add(filename)
      file_stack = [filename]
      includes.setdefault(filename, set())

    m = INCLUDE_RE.match(line)
    if m:
      prev_depth = len(file_stack) - 1
      depth = len(m.group(1))
      filename = m.group(2)
      includes.setdefault(filename, set())

      if depth > prev_depth:
        assert depth == prev_depth + 1
      else:
        for _ in range(prev_depth - depth + 1):
          file_stack.pop()

      includes[file_stack[-1]].add(filename)
      file_stack.append(filename)

  # Normalize paths.
  normalized = {}

  def n(fn):
    if not fn in normalized:
      x = os.path.normpath(os.path.join(build_dir, fn))
      normalized[fn] = x
    return normalized[fn]

  roots = set([n(x) for x in roots])
  includes = dict([(n(k), set([n(x) for x in v])) for k, v in includes.items()])

  return roots, includes


class TestParseBuild(unittest.TestCase):
  def test_basic(self):
    x = [
        'ninja: Entering directory `out/foo\'',
        'clang -c ../../a.cc -o a.o',
        '. ../../a.h',
        'clang -c gen/c.c -o a.o',
    ]
    (roots, includes) = parse_build(x)
    self.assertEquals(roots, set(['a.cc', 'out/foo/gen/c.c']))
    self.assertEquals(set(includes.keys()),
                      set(['a.cc', 'a.h', 'out/foo/gen/c.c']))
    self.assertEquals(includes['a.cc'], set(['a.h']))
    self.assertEquals(includes['a.h'], set())
    self.assertEquals(includes['out/foo/gen/c.c'], set())

  def test_more(self):
    x = [
        'ninja: Entering directory `out/foo\'',
        'clang -c ../../a.cc -o a.o',
        '. ../../a.h',
        '. ../../b.h',
        '.. ../../c.h',
        '... ../../d.h',
        '. ../../e.h',
    ]
    (roots, includes) = parse_build(x)
    self.assertEquals(roots, set(['a.cc']))
    self.assertEquals(includes['a.cc'], set(['a.h', 'b.h', 'e.h']))
    self.assertEquals(includes['b.h'], set(['c.h']))
    self.assertEquals(includes['c.h'], set(['d.h']))
    self.assertEquals(includes['d.h'], set())
    self.assertEquals(includes['e.h'], set())

  def test_multiple(self):
    x = [
        'ninja: Entering directory `out/foo\'',
        'clang -c ../../a.cc -o a.o',
        '. ../../a.h',
        'clang -c ../../b.cc -o b.o',
        '. ../../b.h',
    ]
    (roots, includes) = parse_build(x)
    self.assertEquals(roots, set(['a.cc', 'b.cc']))
    self.assertEquals(includes['a.cc'], set(['a.h']))
    self.assertEquals(includes['b.cc'], set(['b.h']))


def post_order_nodes(root, child_nodes):
  """Generate the nodes reachable from root (including root itself) in
  post-order traversal order. child_nodes maps each node to its children."""
  visited = set()

  def walk(n):
    if n in visited:
      return
    visited.add(n)

    for c in child_nodes[n]:
      for x in walk(c):
        yield x
    yield n

  return walk(root)


def compute_doms(root, includes):
  """Compute the dominators for all nodes reachable from root. Node A dominates
  node B if all paths from the root to B go through A. Returns a dict from
  filename to the set of dominators of that filename (including itself)."""
  rpo = list(reversed(list(post_order_nodes(root, includes))))
  assert rpo[0] == root

  # Initialization.
  # The data-flow analysis starts with full dom sets,
  # except for root, where we know the end result already.
  # preds[n] are the direct predecessors of node n.
  # The order is not important here, rpo is just handy as a list of all nodes.
  doms = {}
  preds = defaultdict(set)
  for n in rpo:
    doms[n] = set(rpo)
    for x in includes[n]:
      preds[x].add(n)
  doms[root] = set([root])

  # Iterate to fixed point.
  # This is not the fastest algorithm, but it's simple and fast enough for us.
  # Processing the nodes in reverse post-order leads to faster convergence.
  # rpo[0] (the root) is skipped, because its dominators are known, and its
  # empty preds set would break the set.intersection() invocation.
  changed = True
  while changed:
    changed = False
    for n in rpo[1:]:
      new_set = set.intersection(*[doms[p] for p in preds[n]])
      new_set.add(n)
      if new_set != doms[n]:
        doms[n] = new_set
        changed = True

  return doms


class TestComputeDoms(unittest.TestCase):
  def test_basic(self):
    includes = {}
    includes[1] = [2]
    includes[2] = [1]
    includes[3] = [2]
    includes[4] = [1]
    includes[5] = [4, 3]
    root = 5

    doms = compute_doms(root, includes)

    self.assertEqual(doms[1], set([5, 1]))
    self.assertEqual(doms[2], set([5, 2]))
    self.assertEqual(doms[3], set([5, 3]))
    self.assertEqual(doms[4], set([5, 4]))
    self.assertEqual(doms[5], set([5]))


def trans_size(root, includes, sizes):
  """Compute the transitive size of a file, i.e. the size of the file itself and
  all its transitive includes."""
  return sum([sizes[n] for n in post_order_nodes(root, includes)])


def analyze(target, revision, build_log_file, json_file):
  print('Parsing build log...')
  (roots, includes) = parse_build(build_log_file)

  print('Getting file sizes...')
  sizes = {name: os.path.getsize(name) for name in includes}

  print('Computing transitive sizes...')
  trans_sizes = {n: trans_size(n, includes, sizes) for n in includes}

  print('Counting prevalence...')
  prevalence = {name: 0 for name in includes}
  for r in roots:
    for n in post_order_nodes(r, includes):
      prevalence[n] += 1

  # Map from file to files that include it.
  print('Building reverse include map...')
  included_by = {k: set() for k in includes}
  for k in includes:
    for i in includes[k]:
      included_by[i].add(k)

  build_size = sum([trans_sizes[n] for n in roots])

  print('Computing added sizes...')
  added_sizes = {name: 0 for name in includes}
  for r in roots:
    doms = compute_doms(r, includes)
    for n in doms:
      for d in doms[n]:
        added_sizes[d] += sizes[n]

  # Assign a number to each filename for tighter JSON representation.
  names = []
  name2nr = {}
  for n in sorted(includes.keys()):
    name2nr[n] = len(names)
    names.append(n)

  def nr(name):
    return name2nr[name]

  print('Writing output...')

  # Provide a JS object for convenient inclusion in the HTML file.
  # If someone really wants a proper JSON file, maybe we can reconsider this.
  json_file.write('data = ')

  json.dump(
      {
          'target': target,
          'revision': revision,
          'date': datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S UTC'),
          'files': names,
          'roots': [nr(x) for x in sorted(roots)],
          'includes': [[nr(x) for x in includes[n]] for n in names],
          'included_by': [[nr(x) for x in included_by[n]] for n in names],
          'sizes': [sizes[n] for n in names],
          'tsizes': [trans_sizes[n] for n in names],
          'asizes': [added_sizes[n] for n in names],
          'prevalence': [prevalence[n] for n in names],
      }, json_file)

  print('All done!')


def main():
  result = unittest.main(argv=sys.argv[:1], exit=False, verbosity=2).result
  if len(result.failures) > 0 or len(result.errors) > 0:
    return 1

  parser = argparse.ArgumentParser(description='Analyze an #include graph.')
  parser.add_argument('target', nargs=1, help='The target that was built.')
  parser.add_argument('revision', nargs=1, help='The build revision.')
  parser.add_argument('build_log', nargs=1, help='The build log to analyze.')
  parser.add_argument('output_file', nargs=1, help='The JSON output file.')
  args = parser.parse_args()

  with open(args.build_log[0], 'r') as build_log_file:
    with open(args.output_file[0], 'w') as json_file:
      analyze(args.target[0], args.revision[0], build_log_file, json_file)


if __name__ == '__main__':
  sys.exit(main())
