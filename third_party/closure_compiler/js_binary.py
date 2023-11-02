# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Used by a js_binary action to compile javascript files.

This script takes in a list of sources and dependencies and compiles them all
together into a single compiled .js file.  The dependencies are ordered in a
post-order, left-to-right traversal order.  If multiple instances of the same
source file are read, only the first is kept. The script can also take in
optional --flags argument which will add custom flags to the compiler.  Any
extern files can also be passed in using the --extern flag.
"""

from __future__ import print_function

import argparse
import os
import sys

import compiler


def ParseDepList(dep):
  """Parses a dependency list, returns |sources, deps, externs|."""
  assert os.path.isfile(dep), (dep +
                               ' is not a js_library target')
  with open(dep, 'r') as dep_list:
    lines = dep_list.read().splitlines()
  assert 'deps:' in lines, dep + ' is not formated correctly, missing "deps:"'
  deps_start = lines.index('deps:')
  assert 'externs:' in lines, dep + ' is not formated correctly, missing "externs:"'
  externs_start = lines.index('externs:')

  return (lines[1:deps_start],
          lines[deps_start+1:externs_start],
          lines[externs_start+1:])


# Cache, to avoid reading the same file twice in the dependency tree and
# processing its dependencies again.
depcache = {}

def AppendUnique(items, new_items):
  """Append items in |new_items| to |items|, avoiding duplicates."""
  # Note this is O(n*n), and assumes |new_items| is already unique, but this is
  # not a bottleneck overall.
  items += [i for i in new_items if i not in items]

def CrawlDepsTree(deps):
  """Parses the dependency tree creating a post-order listing of sources."""
  global depcache

  if len(deps) == 0:
    return ([], [])

  new_sources = []
  new_externs = []
  for dep in deps:
    if dep in depcache:
      cur_sources, cur_externs = depcache[dep]
    else:
      dep_sources, dep_deps, dep_externs = ParseDepList(dep)
      cur_sources, cur_externs = CrawlDepsTree(dep_deps)
      # Add child dependencies of this node before the current node, then cache.
      AppendUnique(cur_sources, dep_sources)
      AppendUnique(cur_externs, dep_externs)
      depcache[dep] = (cur_sources, cur_externs)

    # Add the current node's sources and dedupe.
    AppendUnique(new_sources, cur_sources)
    AppendUnique(new_externs, cur_externs)

  return new_sources, new_externs


def CrawlRootDepsTree(deps, target_sources, target_externs):
  """Parses the dependency tree and adds target sources."""
  sources, externs = CrawlDepsTree(deps)
  AppendUnique(sources, target_sources)
  AppendUnique(externs, target_externs)
  return sources, externs


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('-c', '--compiler', required=True,
                      help='Path to compiler')
  parser.add_argument('-s', '--sources', nargs='*', default=[],
                      help='List of js source files')
  parser.add_argument('-o', '--output', required=False,
                      help='Compile to output')
  parser.add_argument('--chunks', action='store_true',
                      help='Compile each source to its own chunk')
  parser.add_argument('--chunk_suffix', required=False,
                      help='String appended to the source when naming a chunk')
  parser.add_argument('-d', '--deps', nargs='*', default=[],
                      help='List of js_libarary dependencies')
  parser.add_argument('-b', '--bootstrap',
                      help='A file to include before all others')
  parser.add_argument('-cf', '--config', nargs='*', default=[],
                      help='A list of files to include after bootstrap and '
                      'before all others')
  parser.add_argument('-f', '--flags', nargs='*', default=[],
                      help='A list of custom flags to pass to the compiler. '
                      'Do not include leading dashes')
  parser.add_argument('-e', '--externs', nargs='*', default=[],
                      help='A list of extern files to pass to the compiler')
  parser.add_argument('-co', '--checks-only', action='store_true',
                      help='Only performs checks and writes an empty output')

  args = parser.parse_args()

  # If --chunks is used, args.sources will be added later
  sources, externs = CrawlRootDepsTree(args.deps, [] if args.chunks else args.sources, args.externs)
  compiler_args = ['--%s' % flag for flag in args.flags]
  compiler_args += ['--externs=%s' % e for e in externs]

  if not args.chunks:
    compiler_args += [
        '--js_output_file',
        args.output,
    ]

  compiler_args += [
      '--js',
  ]

  if args.bootstrap:
    compiler_args += [args.bootstrap]

  compiler_args += args.config
  compiler_args += sources

  if args.chunks:
    chunk_suffix = args.chunk_suffix
    common_chunk_name = 'common' + chunk_suffix
    compiler_args += [
      '--chunk_output_path_prefix {}'.format(args.output),
      '--chunk {}:auto'.format(common_chunk_name)
    ]

    for s in args.sources:
      # '//path/to/target.js' becomes 'target'
      chunk_name = '{}{}'.format(s.split('/')[-1].split('.')[0], chunk_suffix)
      compiler_args += [
        '--chunk {}:1:{}: {}'.format(chunk_name, common_chunk_name, s)
      ]

  if args.checks_only:
    compiler_args += ['--checks-only']
    open(args.output, 'w').close()

  returncode, errors = compiler.Compiler().run_jar(args.compiler, compiler_args)
  if returncode != 0:
    print(args.compiler, ' '.join(compiler_args))
    print(errors)

  return returncode


if __name__ == '__main__':
  sys.exit(main())
