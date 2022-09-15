#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Prints out du-style information about the files that will be deployed
for a given target
"""

import argparse
import os
import re
import sys
from typing import Iterable
from enum import Enum


def format_size(bytesize: float) -> str:
  """Convert bytes to human readable format.

  Args:
    bytesize: Number to humanize

  Returns:
    Size as string in human-readable format (e.g. 1.8MiB)
  """
  if bytesize < 1024:
    return f'{bytesize}B'

  for suffix in 'BKMGTPEZY':
    if bytesize < 1024:
      break
    bytesize /= 1024

  return f'{bytesize:.1f}{suffix}iB'  # pylint: disable=undefined-loop-variable


class FilesystemNode:
  def __init__(self, path: str) -> None:
    self.path = path
    self.descendant_count = 0
    try:
      self.size = 0 if os.path.isdir(self.path) else os.path.getsize(self.path)
    except FileNotFoundError:
      print(f'{path} not found, please check that you have compiled '
            'the target that generates this manifest.')
      exit(1)


class Analysis(Enum):
  FILE_COUNT = 'file_count'
  SIZE = 'size'

  def __str__(self):
    return self.value


class SortOrder(Enum):
  ASCENDING = 'ascending'
  DESCENDING = 'descending'

  def __str__(self):
    return self.value


def compute_prefix_paths(path: str) -> Iterable[str]:
  prefix = path.rpartition('/')[0]
  while prefix:
    yield prefix
    prefix = prefix.rpartition('/')[0]


class ManifestAnalyzer:
  def __init__(self) -> None:
    self.path_map: dict[str, FilesystemNode] = dict()

  def parse_manifest(self, manifest_path: str) -> None:
    out_dir = re.match('out\/[^\/]+', manifest_path).group()

    with open(manifest_path, 'r') as manifest:
      for line in manifest:
        relative_path = line.strip().partition('=')[2]
        self.register_file(f'{out_dir}/{relative_path}')

  def register_file(self, path: str) -> None:
    if path in self.path_map:
      return

    leaf_node = FilesystemNode(path)
    self.path_map[path] = leaf_node

    for prefix in compute_prefix_paths(path):
      if prefix in self.path_map:
        parent_node = self.path_map[prefix]
      else:
        parent_node = FilesystemNode(prefix)
        self.path_map[prefix] = parent_node

      parent_node.descendant_count += 1
      parent_node.size += leaf_node.size

  def print_file_count(self, max_depth: int, sort_order: SortOrder) -> None:
    sorted_nodes = sorted(self.path_map.values(),
                          key=lambda node: node.descendant_count,
                          reverse=sort_order == SortOrder.DESCENDING)
    for node in sorted_nodes:
      if node.descendant_count <= 0:
        continue
      depth = node.path.count('/')
      if depth > max_depth:
        continue
      print(f'{node.descendant_count: >10}\t{node.path}')

  def print_byte_size(self, max_depth: int, sort_order: SortOrder) -> None:
    sorted_nodes = sorted(self.path_map.values(),
                          key=lambda node: node.size,
                          reverse=sort_order == SortOrder.DESCENDING)
    for node in sorted_nodes:
      depth = node.path.count('/')
      if depth > max_depth:
        continue
      print(f'{format_size(node.size): >10}\t{node.path}')


def main():
  parser = argparse.ArgumentParser(
      description='Launches a long-running emulator that can '
      'be re-used for multiple test runs.')
  parser.add_argument(
      'manifest_path',
      type=str,
      help='path to the .manifest '
      'file. For example, the manifest for chrome/test:browser_tests can be '
      'found at <out_dir>/gen/chrome/test/browser_tests/browser_tests.manifest')
  parser.add_argument('--analysis',
                      type=Analysis,
                      choices=list(Analysis),
                      default=Analysis.SIZE,
                      help='which type of analysis to print')
  parser.add_argument('--max-depth',
                      type=int,
                      default=sys.maxsize,
                      help='only print directories to the provided depth')
  parser.add_argument(
      '--sort-order',
      type=SortOrder,
      choices=list(SortOrder),
      default=SortOrder.ASCENDING,
      help='which order to use for sorting, defualts to ascending')
  args = parser.parse_args()

  analyzer = ManifestAnalyzer()
  analyzer.parse_manifest(args.manifest_path)
  if args.analysis == Analysis.FILE_COUNT:
    analyzer.print_file_count(args.max_depth, args.sort_order)
  else:
    analyzer.print_byte_size(args.max_depth, args.sort_order)


if __name__ == '__main__':
  main()
