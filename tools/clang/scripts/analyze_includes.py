#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script is used to analyze #include graphs.

It produces the .js file that accompanies include-analysis.html.

Usage:

$ gn gen --args="show_includes=true symbol_level=0 enable_precompiled_headers=false" out/Debug
$ autoninja -C out/Debug -v chrome | tee /tmp/build_log
$ analyze_includes.py --target=chrome --revision=$(git rev-parse --short HEAD) \
    --json-out=/tmp/include-analysis.js /tmp/build_log

(If you have reclient access, add use_reclient=true to the gn args, but not on
Windows due to crbug.com/1223741#c9)

The script takes roughly half an hour on a fast machine for the chrome build
target, which is considered fast enough for batch job purposes for now.

If --json-out is not provided, the script exits after printing some statistics
to stdout. This is significantly faster than generating the full JSON data. For
example:

$ autoninja -C out/Debug -v chrome | analyze_includes.py - 2>/dev/null
build_size 270237664463
"""

import argparse
import json
import os
import pathlib
import re
import sys
import unittest
from collections import defaultdict
from datetime import datetime


def parse_build(build_log, root_filter=None):
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

  # Normalize paths.
  normalized = {}

  def norm(fn):
    if fn not in normalized:
      x = fn.replace('\\\\', '\\')
      # Use Path.resolve() rather than path.realpath() to get the canonical
      # upper/lower-case version of the path on Windows.
      p = pathlib.Path(os.path.join(build_dir, x)).resolve()
      x = os.path.relpath(p)
      x = x.replace(os.path.sep, '/')
      normalized[fn] = x
    return normalized[fn]

  # ninja: Entering directory `out/foo'
  ENTER_DIR_RE = re.compile(r'ninja: Entering directory `(.*?)\'$')
  # [M/N] clang... -c foo.cc -o foo.o ...
  # [M/N] .../clang... -c foo.cc -o foo.o ...
  # [M/N] clang-cl.exe /c foo.cc /Fofoo.o ...
  # [M/N] ...\clang-cl.exe /c foo.cc /Fofoo.o ...
  COMPILE_RE = re.compile(r'\[\d+/\d+\] (.*[/\\])?clang.* [/-]c (\S*)')
  # . a.h
  # .. b.h
  # . c.h
  INCLUDE_RE = re.compile(r'(\.+) (.*)$')

  skipping_root = False

  for line in build_log:
    m = INCLUDE_RE.match(line)
    if m:
      if skipping_root:
        continue
      prev_depth = len(file_stack) - 1
      depth = len(m.group(1))
      filename = norm(m.group(2))
      includes.setdefault(filename, set())

      if depth > prev_depth:
        if sys.platform != 'win32':
          # TODO(crbug.com/40187759): Always assert.
          assert depth == prev_depth + 1
        elif depth > prev_depth + 1:
          # Until the bug is fixed, skip these includes.
          print('missing include under', file_stack[0])
          continue
      else:
        for _ in range(prev_depth - depth + 1):
          file_stack.pop()

      includes[file_stack[-1]].add(filename)
      file_stack.append(filename)
      continue

    m = COMPILE_RE.match(line)
    if m:
      skipping_root = False
      filename = norm(m.group(2))
      if root_filter and not root_filter.match(filename):
        skipping_root = True
        continue
      roots.add(filename)
      file_stack = [filename]
      includes.setdefault(filename, set())
      continue

    m = ENTER_DIR_RE.match(line)
    if m:
      build_dir = m.group(1)
      continue

  return roots, includes


class TestParseBuild(unittest.TestCase):
  def test_basic(self):
    x = [
        'ninja: Entering directory `out/foo\'',
        '[1/3] clang -c ../../a.cc -o a.o',
        '. ../../a.h',
        '[2/3] clang -c gen/c.c -o a.o',
    ]
    (roots, includes) = parse_build(x)
    self.assertEqual(roots, set(['a.cc', 'out/foo/gen/c.c']))
    self.assertEqual(set(includes.keys()),
                     set(['a.cc', 'a.h', 'out/foo/gen/c.c']))
    self.assertEqual(includes['a.cc'], set(['a.h']))
    self.assertEqual(includes['a.h'], set())
    self.assertEqual(includes['out/foo/gen/c.c'], set())

  def test_more(self):
    x = [
        'ninja: Entering directory `out/foo\'',
        '[20/99] clang -c ../../a.cc -o a.o',
        '. ../../a.h',
        '. ../../b.h',
        '.. ../../c.h',
        '... ../../d.h',
        '. ../../e.h',
    ]
    (roots, includes) = parse_build(x)
    self.assertEqual(roots, set(['a.cc']))
    self.assertEqual(includes['a.cc'], set(['a.h', 'b.h', 'e.h']))
    self.assertEqual(includes['b.h'], set(['c.h']))
    self.assertEqual(includes['c.h'], set(['d.h']))
    self.assertEqual(includes['d.h'], set())
    self.assertEqual(includes['e.h'], set())

  def test_multiple(self):
    x = [
        'ninja: Entering directory `out/foo\'',
        '[123/234] clang -c ../../a.cc -o a.o',
        '. ../../a.h',
        '[124/234] clang -c ../../b.cc -o b.o',
        '. ../../b.h',
    ]
    (roots, includes) = parse_build(x)
    self.assertEqual(roots, set(['a.cc', 'b.cc']))
    self.assertEqual(includes['a.cc'], set(['a.h']))
    self.assertEqual(includes['b.cc'], set(['b.h']))

  def test_root_filter(self):
    x = [
        'ninja: Entering directory `out/foo\'',
        '[9/100] clang -c ../../a.cc -o a.o',
        '. ../../a.h',
        '[10/100] clang -c ../../b.cc -o b.o',
        '. ../../b.h',
    ]
    (roots, includes) = parse_build(x, re.compile(r'^a.cc$'))
    self.assertEqual(roots, set(['a.cc']))
    self.assertEqual(set(includes.keys()), set(['a.cc', 'a.h']))
    self.assertEqual(includes['a.cc'], set(['a.h']))

  def test_windows(self):
    x = [
        'ninja: Entering directory `out/foo\'',
        '[1/3] path\\clang-cl.exe /c ../../a.cc /Foa.o',
        '. ../../a.h',
        '[2/3] clang-cl.exe /c gen/c.c /Foa.o',
    ]
    (roots, includes) = parse_build(x)
    self.assertEqual(roots, set(['a.cc', 'out/foo/gen/c.c']))
    self.assertEqual(set(includes.keys()),
                     set(['a.cc', 'a.h', 'out/foo/gen/c.c']))
    self.assertEqual(includes['a.cc'], set(['a.h']))
    self.assertEqual(includes['a.h'], set())
    self.assertEqual(includes['out/foo/gen/c.c'], set())


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
  filename to the set of dominators of that filename (including itself).

  The implementation follows the "simple" version of Lengauer & Tarjan "A Fast
  Algorithm for Finding Dominators in a Flowgraph" (TOPLAS 1979).
  """

  parent = {}
  ancestor = {}
  vertex = []
  label = {}
  semi = {}
  pred = defaultdict(list)
  bucket = defaultdict(list)
  dom = {}

  def dfs(v):
    semi[v] = len(vertex)
    vertex.append(v)
    label[v] = v

    for w in includes[v]:
      if w not in semi:
        parent[w] = v
        dfs(w)
      pred[w].append(v)

  def compress(v):
    if ancestor[v] in ancestor:
      compress(ancestor[v])
      if semi[label[ancestor[v]]] < semi[label[v]]:
        label[v] = label[ancestor[v]]
      ancestor[v] = ancestor[ancestor[v]]

  def evaluate(v):
    if v not in ancestor:
      return v
    compress(v)
    return label[v]

  def link(v, w):
    ancestor[w] = v

  # Step 1: Initialization.
  dfs(root)

  for w in reversed(vertex[1:]):
    # Step 2: Compute semidominators.
    for v in pred[w]:
      u = evaluate(v)
      if semi[u] < semi[w]:
        semi[w] = semi[u]

    bucket[vertex[semi[w]]].append(w)
    link(parent[w], w)

    # Step 3: Implicitly define the immediate dominator for each node.
    for v in bucket[parent[w]]:
      u = evaluate(v)
      dom[v] = u if semi[u] < semi[v] else parent[w]
    bucket[parent[w]] = []

  # Step 4: Explicitly define the immediate dominator for each node.
  for w in vertex[1:]:
    if dom[w] != vertex[semi[w]]:
      dom[w] = dom[dom[w]]

  # Get the full dominator set for each node.
  all_doms = {}
  all_doms[root] = {root}

  def dom_set(node):
    if node not in all_doms:
      # node's dominators is itself and the dominators of its immediate
      # dominator.
      all_doms[node] = {node}
      all_doms[node].update(dom_set(dom[node]))

    return all_doms[node]

  return {n: dom_set(n) for n in vertex}


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

  def test_larger(self):
    # Fig. 1 in the Lengauer-Tarjan paper.
    includes = {}
    includes['a'] = ['d']
    includes['b'] = ['a', 'd', 'e']
    includes['c'] = ['f', 'g']
    includes['d'] = ['l']
    includes['e'] = ['h']
    includes['f'] = ['i']
    includes['g'] = ['i', 'j']
    includes['h'] = ['k', 'e']
    includes['i'] = ['k']
    includes['j'] = ['i']
    includes['k'] = ['i', 'r']
    includes['l'] = ['h']
    includes['r'] = ['a', 'b', 'c']
    root = 'r'

    doms = compute_doms(root, includes)

    # Fig. 2 in the Lengauer-Tarjan paper.
    self.assertEqual(doms['a'], set(['a', 'r']))
    self.assertEqual(doms['b'], set(['b', 'r']))
    self.assertEqual(doms['c'], set(['c', 'r']))
    self.assertEqual(doms['d'], set(['d', 'r']))
    self.assertEqual(doms['e'], set(['e', 'r']))
    self.assertEqual(doms['f'], set(['f', 'c', 'r']))
    self.assertEqual(doms['g'], set(['g', 'c', 'r']))
    self.assertEqual(doms['h'], set(['h', 'r']))
    self.assertEqual(doms['i'], set(['i', 'r']))
    self.assertEqual(doms['j'], set(['j', 'g', 'c', 'r']))
    self.assertEqual(doms['k'], set(['k', 'r']))
    self.assertEqual(doms['l'], set(['l', 'd', 'r']))
    self.assertEqual(doms['r'], set(['r']))


def trans_size(root, includes, sizes):
  """Compute the transitive size of a file, i.e. the size of the file itself and
  all its transitive includes."""
  return sum([sizes[n] for n in post_order_nodes(root, includes)])


def log(*args, **kwargs):
  """Log output to stderr."""
  print(*args, file=sys.stderr, **kwargs)


def analyze(target, revision, build_log_file, json_file, root_filter):
  log('Parsing build log...')
  (roots, includes) = parse_build(build_log_file, root_filter)

  log('Getting file sizes...')
  sizes = {name: os.path.getsize(name) for name in includes}

  log('Computing transitive sizes...')
  trans_sizes = {n: trans_size(n, includes, sizes) for n in includes}

  build_size = sum([trans_sizes[n] for n in roots])

  print('build_size', build_size)

  if json_file is None:
    log('--json-out not set; exiting.')
    return 0

  log('Counting prevalence...')
  prevalence = {name: 0 for name in includes}
  for r in roots:
    for n in post_order_nodes(r, includes):
      prevalence[n] += 1

  # Map from file to files that include it.
  log('Building reverse include map...')
  included_by = {k: set() for k in includes}
  for k in includes:
    for i in includes[k]:
      included_by[i].add(k)

  log('Computing added sizes...')

  # Split each src -> dst edge in includes into src -> (src,dst) -> dst, so that
  # we can compute how much each include graph edge adds to the size by doing
  # dominance analysis on the (src,dst) nodes.
  augmented_includes = {}
  for src in includes:
    augmented_includes[src] = set()
    for dst in includes[src]:
      augmented_includes[src].add((src, dst))
      augmented_includes[(src, dst)] = {dst}

  added_sizes = {node: 0 for node in augmented_includes}
  for r in roots:
    doms = compute_doms(r, augmented_includes)
    for node in doms:
      if node not in sizes:
        # Skip the (src,dst) pseudo nodes.
        continue
      for dom in doms[node]:
        added_sizes[dom] += sizes[node]

  # Assign a number to each filename for tighter JSON representation.
  names = []
  name2nr = {}
  for n in sorted(includes.keys()):
    name2nr[n] = len(names)
    names.append(n)

  def nr(name):
    return name2nr[name]

  log('Writing output...')

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
          'includes': [[nr(x) for x in sorted(includes[n])] for n in names],
          'included_by': [[nr(x) for x in included_by[n]] for n in names],
          'sizes': [sizes[n] for n in names],
          'tsizes': [trans_sizes[n] for n in names],
          'asizes': [added_sizes[n] for n in names],
          'esizes': [[added_sizes[(s, d)] for d in sorted(includes[s])]
                     for s in names],
          'prevalence': [prevalence[n] for n in names],
      }, json_file)

  log('All done!')


def main():
  result = unittest.main(argv=sys.argv[:1], exit=False, verbosity=2).result
  if len(result.failures) > 0 or len(result.errors) > 0:
    return 1

  parser = argparse.ArgumentParser(description='Analyze an #include graph.')
  parser.add_argument('build_log',
                      type=argparse.FileType('r', errors='replace'),
                      help='The build log to analyze (- for stdin).')
  parser.add_argument('--target',
                      help='The target that was built (e.g. chrome).')
  parser.add_argument('--revision',
                      help='The revision that was built (e.g. 016588d4ee20).')
  parser.add_argument(
      '--json-out',
      type=argparse.FileType('w'),
      help='Write full analysis data to a JSON file (- for stdout).')
  parser.add_argument('--root-filter',
                      help='Regex to filter which root files are analyzed.')
  args = parser.parse_args()

  if args.json_out and not (args.target and args.revision):
    print('error: --json-out requires both --target and --revision to be set')
    return 1

  try:
    root_filter = re.compile(args.root_filter) if args.root_filter else None
  except Exception:
    print('error: --root-filter is not a valid regex')
    return 1

  analyze(args.target, args.revision, args.build_log, args.json_out,
          root_filter)


if __name__ == '__main__':
  sys.exit(main())
