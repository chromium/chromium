# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates an html report that allows you to view binary size by component."""

from __future__ import print_function

import codecs
import collections
import itertools
import json
import logging
import os
import uuid

import archive
import diff
import models
import path_util


# These must match |_KEYS| in shared.js.
_COMPACT_FILE_COMPONENT_INDEX_KEY = 'c'
_COMPACT_FILE_PATH_KEY = 'p'
_COMPACT_FILE_SYMBOLS_KEY = 's'
_COMPACT_SYMBOL_BYTE_SIZE_KEY = 'b'
_COMPACT_SYMBOL_COUNT_KEY = 'u'
_COMPACT_SYMBOL_FLAGS_KEY = 'f'
_COMPACT_SYMBOL_NAME_KEY = 'n'
_COMPACT_SYMBOL_NUM_ALIASES_KEY = 'a'
_COMPACT_SYMBOL_TYPE_KEY = 't'

# Always emit this many distict symbols (if present), even when small.
# No need to optimize file size at this point).
_MIN_SYMBOL_COUNT = 1000
# Small symbols grouped into "other" symbols may not comprise more than this
# fraction of total size.
_MAX_OTHER_SYMBOL_COVERAGE = .05
# Don't insert "other" symbols smaller than this (just noise at this point).
_MIN_OTHER_PSS = 1


def _GetOrAddFileNode(path, component, file_nodes, components):
  file_node = file_nodes.get(path)
  if file_node is None:
    component_index = components.GetOrAdd(component)
    file_node = {
      _COMPACT_FILE_PATH_KEY: path,
      _COMPACT_FILE_COMPONENT_INDEX_KEY: component_index,
      _COMPACT_FILE_SYMBOLS_KEY: [],
    }
    file_nodes[path] = file_node
  return file_node


class IndexedSet(object):
  """Set-like object where values are unique and indexed.

  Values must be immutable.
  """

  def __init__(self):
    self._index_dict = {}  # Value -> Index dict
    self.value_list = []  # List containing all the set items

  def GetOrAdd(self, value):
    """Get the index of the value in the list. Append it if not yet present."""
    index = self._index_dict.get(value)
    if index is None:
      self.value_list.append(value)
      index = len(self.value_list) - 1
      self._index_dict[value] = index
    return index


def _PartitionSymbols(symbols):
  # Dex methods are whitelisted for the method_count mode in the UI.
  # Vtable entries are interesting to query for, so whitelist them as well.
  interesting_symbols = symbols.Filter(
      lambda s: s.IsDex() or '[vtable]' in s.name)
  ordered_symbols = interesting_symbols.Inverted().Sorted()

  abs_pss_target = (1 - _MAX_OTHER_SYMBOL_COVERAGE) * sum(
      abs(s.pss) for s in ordered_symbols)
  running_abs_pss = 0
  ordered_count = 0
  for ordered_count, s in enumerate(ordered_symbols):
    running_abs_pss += abs(s.pss)
    if running_abs_pss > abs_pss_target and ordered_count >= _MIN_SYMBOL_COUNT:
      break

  main_symbols = itertools.chain(interesting_symbols,
                                 ordered_symbols[:ordered_count])
  extra_symbols = ordered_symbols[ordered_count:]

  logging.info('Found %d large symbols, %s small symbols',
               len(interesting_symbols) + ordered_count, len(extra_symbols))
  return main_symbols, extra_symbols


def _MakeTreeViewList(symbols, include_all_symbols):
  """Builds JSON data of the symbols for the tree view HTML report.

  As the tree is built on the client-side, this function creates a flat list
  of files, where each file object contains symbols that have the same path.

  Args:
    symbols: A SymbolGroup containing all symbols.
    include_all_symbols: If true, include all symbols in the data file.
  """
  file_nodes = {}
  components = IndexedSet()
  # Dict of path -> type -> accumulated pss.
  small_symbol_pss = collections.defaultdict(
      lambda: collections.defaultdict(float))

  # For delta symbols, most counts should 0, so use that as default. Else use 1.
  default_symbol_count = 0 if symbols.IsDelta() else 1

  if include_all_symbols:
    main_symbols, extra_symbols = symbols, []
  else:
    logging.info('Partitioning symbols...')
    main_symbols, extra_symbols = _PartitionSymbols(symbols)

  # Bundle symbols by the file they belong to.
  # Add all the file buckets into file_nodes.
  for symbol in main_symbols:
    symbol_size = round(symbol.pss, 2)
    if symbol_size.is_integer():
      symbol_size = int(symbol_size)
    symbol_count = default_symbol_count
    if symbol.IsDelta():
      symbol_count = models.DIFF_COUNT_DELTA[symbol.diff_status]

    path = symbol.source_path or symbol.object_path
    file_node = _GetOrAddFileNode(
        path, symbol.component, file_nodes, components)

    name = symbol.full_name if symbol.IsDex() else symbol.template_name
    symbol_entry = {
      _COMPACT_SYMBOL_BYTE_SIZE_KEY: symbol_size,
      _COMPACT_SYMBOL_NAME_KEY: name,
      _COMPACT_SYMBOL_TYPE_KEY: symbol.section,
    }
    if symbol.num_aliases != 1:
      symbol_entry[_COMPACT_SYMBOL_NUM_ALIASES_KEY] = symbol.num_aliases

    # We use symbol count for the method count mode in the diff mode report.
    # Negative values are used to indicate a symbol was removed, so it should
    # count as -1 rather than the default, 1.
    # We don't care about accurate counts for other symbol types currently,
    # so this data is only included for methods.
    is_dex_method = symbol.section_name == models.SECTION_DEX_METHOD
    if is_dex_method and symbol_count != default_symbol_count:
      symbol_entry[_COMPACT_SYMBOL_COUNT_KEY] = symbol_count
    if symbol.flags:
      symbol_entry[_COMPACT_SYMBOL_FLAGS_KEY] = symbol.flags
    file_node[_COMPACT_FILE_SYMBOLS_KEY].append(symbol_entry)

  # Collect small symbols into a per-path dict.
  for symbol in extra_symbols:
    path = symbol.source_path or symbol.object_path
    tup = (path, symbol.component)
    small_symbol_pss[tup][symbol.section_name] += symbol.pss

  # Insert small symbols.
  inserted_smalls_count = 0
  inserted_smalls_abs_pss = 0
  skipped_smalls_count = 0
  skipped_smalls_abs_pss = 0
  for tup, type_to_pss in small_symbol_pss.iteritems():
    path, component = tup
    for section_name, pss in type_to_pss.iteritems():
      if abs(pss) < _MIN_OTHER_PSS:
        skipped_smalls_count += 1
        skipped_smalls_abs_pss += abs(pss)
      else:
        inserted_smalls_count += 1
        inserted_smalls_abs_pss += abs(pss)
        file_node = _GetOrAddFileNode(path, component, file_nodes, components)
        file_node[_COMPACT_FILE_SYMBOLS_KEY].append({
          _COMPACT_SYMBOL_NAME_KEY: 'Other ' + section_name,
          _COMPACT_SYMBOL_TYPE_KEY:
              models.SECTION_NAME_TO_SECTION[section_name],
          _COMPACT_SYMBOL_BYTE_SIZE_KEY: pss,
        })
  logging.debug(
      'Created %d "other" symbols with PSS=%.1f. Omitted %d with PSS=%.1f',
      inserted_smalls_count, inserted_smalls_abs_pss, skipped_smalls_count,
      skipped_smalls_abs_pss)

  meta = {
    'components': components.value_list,
    'total': symbols.pss,
  }
  return meta, file_nodes.values()


def BuildReportFromSizeInfo(out_path, size_info, all_symbols=False):
  """Builds a .ndjson report for a .size file.

  Args:
    out_path: Path to save JSON report to.
    size_info: A SizeInfo or DeltaSizeInfo to use for the report.
    all_symbols: If true, all symbols will be included in the report rather
      than truncated.
  """
  logging.info('Reading .size file')
  symbols = size_info.raw_symbols
  is_diff = symbols.IsDelta()
  if is_diff:
    symbols = symbols.WhereDiffStatusIs(models.DIFF_STATUS_UNCHANGED).Inverted()

  meta, tree_nodes = _MakeTreeViewList(symbols, all_symbols)
  logging.info('Created %d tree nodes', len(tree_nodes))
  meta.update({
    'diff_mode': is_diff,
    'section_sizes': size_info.section_sizes,
  })
  if is_diff:
    meta.update({
      'before_metadata': size_info.before.metadata,
      'after_metadata': size_info.after.metadata,
    })
  else:
    meta['metadata'] = size_info.metadata

  # Write newline-delimited JSON file
  logging.info('Serializing JSON')
  # Use separators without whitespace to get a smaller file.
  json_dump_args = {
    'separators': (',', ':'),
    'ensure_ascii': True,
    'check_circular': False,
  }

  with codecs.open(out_path, 'w', encoding='ascii') as out_file:
    json.dump(meta, out_file, **json_dump_args)
    out_file.write('\n')

    for tree_node in tree_nodes:
      json.dump(tree_node, out_file, **json_dump_args)
      out_file.write('\n')


def _MakeDirIfDoesNotExist(rel_path):
  """Ensures a directory exists."""
  abs_path = os.path.abspath(rel_path)
  try:
    os.makedirs(abs_path)
  except OSError:
    if not os.path.isdir(abs_path):
      raise


def AddArguments(parser):
  parser.add_argument('input_size_file',
                      help='Path to input .size file.')
  parser.add_argument('output_report_file',
                      help='Write generated data to the specified '
                           '.ndjson file.')
  parser.add_argument('--all-symbols', action='store_true',
                      help='Include all symbols. Will cause the data file to '
                           'take longer to load.')
  parser.add_argument('--diff-with',
                      help='Diffs the input_file against an older .size file')


def Run(args, parser):
  if not args.input_size_file.endswith('.size'):
    parser.error('Input must end with ".size"')
  if args.diff_with and not args.diff_with.endswith('.size'):
    parser.error('Diff input must end with ".size"')
  if not args.output_report_file.endswith('.ndjson'):
    parser.error('Output must end with ".ndjson"')

  size_info = archive.LoadAndPostProcessSizeInfo(args.input_size_file)
  if args.diff_with:
    before_size_info = archive.LoadAndPostProcessSizeInfo(args.diff_with)
    size_info = diff.Diff(before_size_info, size_info)

  BuildReportFromSizeInfo(
      args.output_report_file, size_info, all_symbols=args.all_symbols)

  logging.warning('Done!')
  msg = [
      'View using a local server via: ',
      '    {0} start_server {1}',
      'or run:',
      '    gsutil.py cp -a public-read {1} gs://chrome-supersize/oneoffs/'
      '{2}.ndjson',
      '  to view at:',
      '    https://storage.googleapis.com/chrome-supersize/viewer.html'
      '?load_url=oneoffs/{2}.ndjson',
  ]
  supersize_path = os.path.relpath(os.path.join(
      path_util.SRC_ROOT, 'tools', 'binary_size', 'supersize'))
  # Use a random UUID as the filename so user can copy-and-paste command
  # directly without a name collision.
  upload_id = uuid.uuid4()
  print('\n'.join(msg).format(supersize_path, args.output_report_file,
                              upload_id))
