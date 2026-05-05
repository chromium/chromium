#!/usr/bin/env vpython3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Detect strings in `.grd(p)` files that are likely unused.

If unused strings are found and not fixed, write their IDs to stdout (one per
line) and exit with code 1. Otherwise, exit successfully.
"""

import argparse
from collections.abc import Collection, Iterator, Mapping, MutableSet
from concurrent.futures import ProcessPoolExecutor
import dataclasses
import itertools
import pathlib
import re
import subprocess
import sys
import xml.sax

_ANDROID_MANIFEST_PATTERN = re.compile('[\'"]@string/(?P<name>\\w*)[\'"]')
_CPP_ID_PATTERN = re.compile(r'\bIDS_\w*\b')
_JAVA_ID_PATTERN = re.compile(
    r'\bR\s*\.\s*(string|plurals)\s*\.\s*(?P<name>\w*)\b')

_GRD_SKIP_LIST = {
    # These have special generated usage that static analysis can't detect, so
    # just skip them.
    pathlib.Path('components', 'printing_component_strings.grdp'),
    pathlib.Path('components', 'privacy_sandbox_chrome_strings.grdp'),
    pathlib.Path('third_party', 'blink', 'public', 'strings',
                 'permission_element_strings.grd'),
    pathlib.Path('third_party', 'libaddressinput', 'chromium',
                 'address_input_strings.grdp'),
    # These strings should naturally age out.
    pathlib.Path('ios', 'chrome', 'browser', 'whats_new', 'ui', 'strings'),
    # This example is not shipped.
    pathlib.Path('ui', 'views', 'examples', 'views_examples_resources.grd'),
}

_EMPTY_IF_PATTERN = re.compile(r'<if[^>]*>\s*</if>\s*', flags=re.DOTALL)


def _should_analyze(path: pathlib.Path) -> bool:
  if set(path.parts) & {'testdata', 'web_tests'}:
    return False
  return not any(path.is_relative_to(skip_path) for skip_path in _GRD_SKIP_LIST)


def _list_paths(include_paths: list[pathlib.Path]) -> Iterator[pathlib.Path]:
  repo_root = subprocess.check_output(['git', 'rev-parse', '--show-toplevel'],
                                      text=True)
  repo_root = pathlib.Path(repo_root.strip())
  with subprocess.Popen(
      ['git', 'ls-files', '--', *include_paths],
      stdout=subprocess.PIPE,
      cwd=repo_root,
      text=True,
  ) as ls_proc:
    for line in ls_proc.stdout:
      path_from_root = pathlib.Path(line.strip())
      if _should_analyze(path_from_root):
        yield repo_root / path_from_root


@dataclasses.dataclass
class GrdParseResult:
  """The result of parsing a `.grd(p)` file.

  Attributes:
    ids: A set of `IDS_...` message IDs.
    parts: Relative paths to `.grdp` files to include.
    generates_runtime_strings: True iff this `.grdp` generates a non-`.pak`
        format that might be consumed at runtime. In that case, it's difficult
        to detect statically whether a string is unused, so skip such `.grd`s
        and their constituent parts.
  """
  ids: MutableSet[str] = dataclasses.field(default_factory=set)
  parts: MutableSet[str] = dataclasses.field(default_factory=set)
  generates_runtime_strings: bool = False


class _GrdParser(xml.sax.handler.ContentHandler):

  def __init__(self):
    super().__init__()
    self.result = GrdParseResult()

  def startElement(self, name, attrs):
    # Some `<message>`s don't start with the conventional `IDS_`. Checking
    # whether such strings are orphaned could be inefficient because we would
    # either need to store all constants used or make additional passes through
    # C++/Java files. Just skip them and accept some false negatives.
    if name == 'message' and (msg_id := attrs['name']).startswith('IDS_'):
      self.result.ids.add(msg_id)
    elif name == 'output' and attrs['type'] in {
        'chrome_messages_json',
        'chrome_messages_json_gzip',
    }:
      self.result.generates_runtime_strings = True
    elif name == 'part':
      self.result.parts.add(attrs['file'])


# Mapping of `.grd(p)` paths (relative to the repo root) to parsed results.
GrdResultsByPath = Mapping[pathlib.Path, GrdParseResult]


def parse_grd(path: pathlib.Path) -> GrdParseResult:
  parser = _GrdParser()
  with path.open() as stream:
    xml.sax.parse(stream, parser)
    return parser.result


def filter_grds(results_by_path: GrdResultsByPath) -> GrdResultsByPath:
  filtered_results_by_path = dict(results_by_path)
  for path, result in results_by_path.items():
    if not result.generates_runtime_strings:
      continue
    # `.grdp` files can include other `.grdp`, so do DFS.
    paths = [path]
    while paths:
      path = paths.pop()
      if result := filtered_results_by_path.pop(path, None):
        for part in result.parts:
          paths.append(path.parent / part)
  return filtered_results_by_path


def parse_used_ids(path: pathlib.Path) -> set[str]:
  if path.suffix in {'.h', '.cc', '.mm', '.swift', '.plist', '.py'}:
    # Special cases:
    # * `.py` build scripts and XML `.plist` entries can generate string
    #   references.
    # * Ignore binary `.plist` files.
    errors = 'ignore' if path.suffix == '.plist' else 'strict'
    with path.open(errors=errors) as stream:
      content = stream.read()
      used_ids = set(_CPP_ID_PATTERN.findall(content))
      # For embedded RC files on Windows, a `<message name="IDS_FOO">` generates
      # `IDS_FOO_BASE` [0]. However, because there's no way to distinguish this
      # case from a regular ID ending in `_BASE`, just say that both IDs are
      # possibly used.
      #
      # [0]: https://chromium.googlesource.com/chromium/src/+/main/base/win/embedded_i18n/create_string_rc.py
      return used_ids | {used_id.removesuffix('_BASE') for used_id in used_ids}
  elif path.suffix in {'.java', '.xml'}:
    if path.suffix == '.java':
      pattern = _JAVA_ID_PATTERN
    else:
      pattern = _ANDROID_MANIFEST_PATTERN
    with path.open() as stream:
      content = stream.read()
      names = {match['name'] for match in pattern.finditer(content)}
      return {f'IDS_{name.upper()}' for name in names}
  else:
    return set()


def remove_strings(grd_path: pathlib.Path,
                   ids_to_remove: Collection[str]) -> None:
  with grd_path.open() as grd_file:
    grd_content = grd_file.read()
  # Use regex instead of structured parsing because comments don't survive
  # round-tripping.
  names_pattern = '|'.join(map(re.escape, ids_to_remove))
  pattern = (r'<message\s+[^>]*name\s*=\s*'
             f'[\'"]({names_pattern})[\'"].*?</message>\\s*')
  grd_content = re.sub(pattern, '', grd_content, flags=re.DOTALL)
  grd_content = _EMPTY_IF_PATTERN.sub('', grd_content)
  with grd_path.open('w') as grd_file:
    grd_file.write(grd_content)

  screenshots_dir = grd_path.parent / grd_path.name.replace('.', '_')
  for msg_id in ids_to_remove:
    try:
      (screenshots_dir / f'{msg_id}.png.sha1').unlink()
    except OSError:
      # Some very old strings may not have screenshots.
      pass


def main() -> int:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--max-workers',
                      type=int,
                      default=8,
                      help='Maximum number of workers to use.')
  parser.add_argument(
      '--fix',
      action='store_true',
      help=('Try removing orphaned strings from `.grd(p)` files. You should '
            'review the output because of imperfect formatting and comments '
            "that can't be checked for validity."))
  parser.add_argument(
      'include_paths',
      nargs='*',
      help=('Paths relative to the repo root to analyze. If no paths are '
            'given, analyze all source files.'))
  args = parser.parse_args()

  include_paths = list(map(pathlib.Path, args.include_paths))
  grd_paths, source_paths = [], []
  for path in _list_paths(include_paths):
    if path.suffix in {'.grd', '.grdp'}:
      grd_paths.append(path)
    else:
      source_paths.append(path)

  # Parsing each file's contents is compute-bound. Parallelize work among
  # processes, not threads, so that the critical path isn't bottlenecked by
  # Python's Global Interpreter Lock (GIL).
  with ProcessPoolExecutor(max_workers=args.max_workers) as pool:
    grd_results_by_path = dict(
        zip(grd_paths, pool.map(parse_grd, grd_paths, chunksize=200)))
    grd_results_by_path = filter_grds(grd_results_by_path)

    used_ids = set()
    for file_ids in pool.map(parse_used_ids, source_paths, chunksize=200):
      used_ids.update(file_ids)

    declared_ids = set(
        itertools.chain.from_iterable(
            result.ids for result in grd_results_by_path.values()))
    orphaned_ids = declared_ids - used_ids
    for orphan in sorted(orphaned_ids):
      print(orphan)

    if args.fix:
      futures = []
      for grd_path, result in grd_results_by_path.items():
        ids_to_remove = orphaned_ids & result.ids
        futures.append(pool.submit(remove_strings, grd_path, ids_to_remove))
      for future in futures:
        future.result()
      return 0

  return 1 if orphaned_ids else 0


if __name__ == '__main__':
  sys.exit(main())
