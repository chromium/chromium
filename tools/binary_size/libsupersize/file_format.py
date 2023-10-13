# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Deals with loading & saving .size and .sizediff files.

See docs/file_format.md for a specification of the file formats.
"""

import contextlib
import gzip
import io
import itertools
import json
import logging
import os
import sys

import models
import parallel


_COMMON_HEADER = b'# Created by //tools/binary_size\n'

# File format version for .size files.
_SIZE_HEADER_SINGLE_CONTAINER = b'Size File Format v1\n'
_SIZE_HEADER_MULTI_CONTAINER = b'Size File Format v1.1\n'

# Header for .sizediff files
_SIZEDIFF_HEADER = b'DIFF\n'
_SIZEDIFF_VERSION = 1

# Native sections are sorted by address.
_SECTION_SORT_ORDER = {
    models.SECTION_DATA: 0,
    models.SECTION_DATA_REL_RO_LOCAL: 0,
    models.SECTION_DATA_REL_RO: 0,
    models.SECTION_RODATA: 0,
    models.SECTION_TEXT: 0,
    models.SECTION_BSS: 1,
    models.SECTION_BSS_REL_RO: 1,
    models.SECTION_RELRO_PADDING: 1,
    models.SECTION_PART_END: 1,
    models.SECTION_DEX: 2,
    models.SECTION_DEX_METHOD: 3,
    models.SECTION_PAK_NONTRANSLATED: 4,
    models.SECTION_PAK_TRANSLATIONS: 5,
    models.SECTION_ARSC: 6,
    models.SECTION_OTHER: 7,
}

# Keys in build config for old .size files.
_LEGACY_METADATA_BUILD_CONFIG_KEYS = (models.BUILD_CONFIG_GIT_REVISION,
                                      models.BUILD_CONFIG_GN_ARGS,
                                      models.BUILD_CONFIG_OUT_DIRECTORY)

# Ensure each |models.SECTION_*| (except |SECTION_MULTIPLE|) has an entry.
assert len(_SECTION_SORT_ORDER) + 1 == len(models.SECTION_NAME_TO_SECTION)


class _Writer:
  """Helper to format and write data to a file object."""

  def __init__(self, file_obj):
    self.file_obj_ = file_obj

  def WriteBytes(self, b):
    # Direct write of raw bytes.
    self.file_obj_.write(b)

  def WriteString(self, s):
    self.file_obj_.write(s.encode('ascii'))

  def WriteLine(self, s):
    self.file_obj_.write(s.encode('ascii'))
    self.file_obj_.write(b'\n')

  def WriteNumberList(self, gen):
    """Writes numbers from |gen| separated by space, in one line."""
    sep = b''
    for num in gen:
      self.WriteBytes(sep)
      self.WriteString(str(num))
      sep = b' '
    self.WriteBytes(b'\n')

  def LogSize(self, desc):
    self.file_obj_.flush()
    size = self.file_obj_.tell()
    logging.debug('File size with %s: %d' % (desc, size))


def _SortKey(s):
  # size_without_padding so that "** symbol gap" sorts before other symbols
  # with same address (necessary for correctness within CalculatePadding()).
  return (
      _SECTION_SORT_ORDER[s.section_name],
      s.IsOverhead(),
      s.address,
      # Only use size_without_padding for native symbols (that have
      # addresses) since padding-only symbols must come first for
      # correctness.
      # DEX also has 0-size symbols (for nested classes, not sure why)
      # and we don't want to sort them differently since they don't have
      # any padding either.
      s.address and s.size_without_padding > 0,
      s.full_name.startswith('**'),
      s.full_name,
      s.object_path)


def _DescribeSymbolSortOrder(syms):
  return ''.join('%r: %r\n' % (_SortKey(s), s) for s in syms)


def SortSymbols(raw_symbols):
  """Sorts the given symbols in the order that they should be archived in.

  The sort order is chosen such that:
    * Padding can be discarded.
    * Ordering is deterministic (total ordering).

  Also sorts |aliases| such that they match the order within |raw_symbols|.

  Args:
    raw_symbols: List of symbols to sort.
  """
  logging.debug('Sorting %d symbols', len(raw_symbols))

  # Sort aliases first to make raw_symbols quicker to sort.
  # Although sorting is done when aliases are first created, aliases that differ
  # only by path can later become out-of-order due to path normalization.
  i = 0
  count = len(raw_symbols)
  while i < count:
    s = raw_symbols[i]
    num_aliases = s.num_aliases
    if s.aliases:
      expected = raw_symbols[i:i + num_aliases]
      assert s.aliases == expected, 'Aliases out of order:\n{}\n{}'.format(
          _DescribeSymbolSortOrder(s.aliases),
          _DescribeSymbolSortOrder(expected))

      s.aliases.sort(key=_SortKey)
      raw_symbols[i:i + num_aliases] = s.aliases
      i += num_aliases
    else:
      i += 1

  # Python's sort() is faster when the input list is already mostly sorted.
  raw_symbols.sort(key=_SortKey)


def CalculatePadding(raw_symbols):
  """Populates the |padding| field based on symbol addresses. """
  logging.info('Calculating padding')

  seen_container_and_sections = set()
  for i, symbol in enumerate(raw_symbols[1:]):
    prev_symbol = raw_symbols[i]
    if symbol.IsOverhead():
      # Overhead symbols are not actionable so should be padding-only.
      symbol.padding = symbol.size
    if (prev_symbol.container.name != symbol.container.name
        or prev_symbol.section_name != symbol.section_name):
      container_and_section = (symbol.container.name, symbol.section_name)
      assert container_and_section not in seen_container_and_sections, """\
Input symbols must be sorted by container, section, then address.
Found: {}
Then: {}
""".format(prev_symbol, symbol)
      seen_container_and_sections.add(container_and_section)
      continue
    if (symbol.address <= 0 or prev_symbol.address <= 0
        or not symbol.IsNative() or not prev_symbol.IsNative()):
      continue

    if symbol.address == prev_symbol.address:
      if symbol.aliases and symbol.aliases is prev_symbol.aliases:
        symbol.padding = prev_symbol.padding
        symbol.size = prev_symbol.size
        continue
      # Padding-only symbols happen for ** symbol gaps.
      assert prev_symbol.size_without_padding == 0, (
          'Found duplicate symbols:\n%r\n%r' % (prev_symbol, symbol))

    padding = symbol.address - prev_symbol.end_address
    symbol.padding = padding
    symbol.size += padding
    assert symbol.size >= 0, (
        'Symbol has negative size (likely not sorted propertly): '
        '%r\nprev symbol: %r' % (symbol, prev_symbol))


def _SaveSizeInfoToFile(size_info, file_obj):
  """Saves size info to a .size file.

  Args:
    size_info: Data to write to the file
    file_obj: File opened for writing.
  """
  raw_symbols = size_info.raw_symbols

  num_containers = len(size_info.containers)
  has_multi_containers = (num_containers > 1)

  file_obj.write(_COMMON_HEADER)
  if has_multi_containers:
    file_obj.write(_SIZE_HEADER_MULTI_CONTAINER)
  else:
    file_obj.write(_SIZE_HEADER_SINGLE_CONTAINER)

  # JSON header fields
  fields = {
      'has_components': True,
      'has_padding': size_info.is_sparse,
      'has_disassembly': True
  }

  if has_multi_containers:
    # Write using new format.
    assert len(set(c.name for c in size_info.containers)) == num_containers, (
        'Container names must be distinct.')
    fields['build_config'] = size_info.build_config
    fields['containers'] = [{
        'name': c.name,
        'metadata': c.metadata,
        'section_sizes': c.section_sizes,
        'metrics_by_file': c.metrics_by_file,
    } for c in size_info.containers]
  else:
    # Write using old format.
    fields['metadata'] = size_info.metadata_legacy
    fields['section_sizes'] = size_info.containers[0].section_sizes
    fields['metrics_by_file'] = size_info.containers[0].metrics_by_file

  fields_str = json.dumps(fields, indent=2, sort_keys=True)

  w = _Writer(file_obj)
  w.WriteLine(str(len(fields_str)))
  w.WriteLine(fields_str)
  w.LogSize('header')  # For libchrome: 570 bytes.

  # Store a single copy of all paths and reference them by index.
  unique_path_tuples = sorted(
      set((s.object_path, s.source_path) for s in raw_symbols))
  path_tuples = {tup: i for i, tup in enumerate(unique_path_tuples)}
  w.WriteLine(str(len(unique_path_tuples)))
  for pair in unique_path_tuples:
    w.WriteLine('%s\t%s' % pair)
  w.LogSize('paths')  # For libchrome, adds 200kb.

  # Store a single copy of all components and have them referenced by index.
  unique_components = sorted(set(s.component for s in raw_symbols))
  components = {comp: i for i, comp in enumerate(unique_components)}
  w.WriteLine(str(len(unique_components)))
  for comp in unique_components:
    w.WriteLine(comp)
  w.LogSize('components')

  # Symbol counts by "segments", defined as (container, section) tuples.
  symbol_group_by_segment = raw_symbols.GroupedByContainerAndSectionName()
  if has_multi_containers:
    container_name_to_index = {
        c.name: i
        for i, c in enumerate(size_info.containers)
    }
    w.WriteLine('\t'.join('<%d>%s' %
                          (container_name_to_index[g.name[0]], g.name[1])
                          for g in symbol_group_by_segment))
  else:
    w.WriteLine('\t'.join(g.name[1] for g in symbol_group_by_segment))
  w.WriteLine('\t'.join(str(len(g)) for g in symbol_group_by_segment))

  def gen_delta(gen, prev_value=0):
    """Adapts a generator of numbers to deltas."""
    for value in gen:
      yield value - prev_value
      prev_value = value

  def write_groups(func, delta=False):
    """Write func(symbol) for each symbol in each symbol group.

    Each line written represents one symbol group in |symbol_group_by_segment|.
    The values in each line are space separated and are the result of calling
    |func| with the Nth symbol in the group.

    If |delta| is True, the differences in values are written instead."""
    for group in symbol_group_by_segment:
      gen = map(func, group)
      w.WriteNumberList(gen_delta(gen) if delta else gen)

  write_groups(lambda s: s.address, delta=True)
  w.LogSize('addresses')  # For libchrome, adds 300kb.

  write_groups(lambda s: s.size if s.IsOverhead() else s.size_without_padding)
  w.LogSize('sizes')  # For libchrome, adds 300kb

  # Padding for non-padding-only symbols is recalculated from addresses on
  # load, so we only need to write it if we're writing a subset of symbols.
  if size_info.is_sparse:
    write_groups(lambda s: s.padding)
    w.LogSize('paddings')  # For libchrome, adds 300kb

  write_groups(
      lambda s: path_tuples[(s.object_path, s.source_path)], delta=True)
  w.LogSize('path indices')  # For libchrome: adds 125kb.

  write_groups(lambda s: components[s.component], delta=True)
  w.LogSize('component indices')

  prev_aliases = None
  symbols_with_disassembly = []
  disassembly_idx = 0
  for group in symbol_group_by_segment:
    for symbol in group:
      if symbol.disassembly:
        symbols_with_disassembly.append((disassembly_idx, symbol.disassembly))
      w.WriteString(symbol.full_name)
      if symbol.aliases and symbol.aliases is not prev_aliases:
        w.WriteString('\t0%x' % symbol.num_aliases)
      prev_aliases = symbol.aliases
      if symbol.flags:
        w.WriteString('\t%x' % symbol.flags)
      w.WriteBytes(b'\n')
      disassembly_idx += 1
  w.LogSize('names (final)')  # For libchrome: adds 3.5mb.

  w.WriteNumberList(x[0] for x in symbols_with_disassembly)
  for _, disassembly in symbols_with_disassembly:
    disassembly_bytes = disassembly.encode('utf-8', errors='surrogatepass')
    w.WriteBytes(b'%d\n' % len(disassembly_bytes))
    w.WriteBytes(disassembly_bytes)


def _ReadLine(file_iter):
  """Read a line from a file object iterator and remove the newline character.

  Args:
    file_iter: File object iterator

  Returns:
    String
  """
  # str[:-1] removes the last character from a string, specifically the newline
  return next(file_iter)[:-1]


def _ReadValuesFromLine(file_iter, split):
  """Read a list of values from a line in a file object iterator.

  Args:
    file_iter: File object iterator
    split: Splits the line with the given string

  Returns:
    List of string values
  """
  return _ReadLine(file_iter).split(split)


def _LoadSizeInfoFromFile(file_obj, size_path, is_sparse):
  """Loads a size_info from the given file.

  See _SaveSizeInfoToFile() for details on the .size file format.

  Args:
    file_obj: File to read, should be a GzipFile.
    size_path: Path to the file to read.
    is_sparse: Whether the size file is a sparse, e.g., created from diff.
  """
  # Split lines on '\n', since '\r' can appear in some lines!
  lines = io.TextIOWrapper(file_obj, newline='\n')
  header_line = _ReadLine(lines).encode('ascii')
  assert header_line == _COMMON_HEADER[:-1], 'was ' + str(header_line)
  header_line = _ReadLine(lines).encode('ascii')
  if header_line == _SIZE_HEADER_SINGLE_CONTAINER[:-1]:
    has_multi_containers = False
  elif header_line == _SIZE_HEADER_MULTI_CONTAINER[:-1]:
    has_multi_containers = True
  else:
    raise ValueError('Version mismatch. Need to write some upgrade code.')

  # JSON header fields
  json_len = int(_ReadLine(lines))
  json_str = lines.read(json_len)

  fields = json.loads(json_str)
  assert ('containers' in fields) == has_multi_containers
  assert ('build_config' in fields) == has_multi_containers
  assert ('containers' in fields) == has_multi_containers
  assert ('metadata' not in fields) == has_multi_containers
  assert ('section_sizes' not in fields) == has_multi_containers

  containers = []
  if has_multi_containers:  # New format.
    build_config = fields['build_config']
    for cfield in fields['containers']:
      c = models.Container(name=cfield['name'],
                           metadata=cfield['metadata'],
                           section_sizes=cfield['section_sizes'],
                           metrics_by_file=cfield.get('metrics_by_file', {}))
      containers.append(c)
  else:  # Old format.
    build_config = {}
    metadata = fields.get('metadata')
    if metadata:
      for key in _LEGACY_METADATA_BUILD_CONFIG_KEYS:
        if key in metadata:
          build_config[key] = metadata[key]
          del metadata[key]
    section_sizes = fields['section_sizes']
    containers.append(
        models.Container(name='',
                         metadata=metadata,
                         section_sizes=section_sizes,
                         metrics_by_file=fields.get('metrics_by_file', {})))
  models.BaseContainer.AssignShortNames(containers)

  has_components = fields.get('has_components', False)
  has_padding = fields.get('has_padding', False)
  has_disassembly = fields.get('has_disassembly', False)

  # Eat empty line.
  _ReadLine(lines)

  # Path list.
  num_path_tuples = int(_ReadLine(lines))  # Number of paths in list.
  # Read the path list values and store for later.
  path_tuples = [
      _ReadValuesFromLine(lines, split='\t') for _ in range(num_path_tuples)
  ]

  if num_path_tuples == 0:
    logging.warning('File contains no symbols: %s', size_path)
    return models.SizeInfo(build_config,
                           containers, [],
                           size_path=size_path,
                           is_sparse=is_sparse)

  # Component list.
  if has_components:
    num_components = int(_ReadLine(lines))  # Number of components in list.
    components = [_ReadLine(lines) for _ in range(num_components)]

  # Symbol counts by "segments", defined as (container, section) tuples.
  segment_names = _ReadValuesFromLine(lines, split='\t')
  symbol_counts = [int(c) for c in _ReadValuesFromLine(lines, split='\t')]

  # Addresses, sizes, paddings, path indices, component indices.
  def read_numeric(delta=False):
    """Read numeric values, where each line corresponds to a symbol group.

    The values in each line are space separated.
    If |delta| is True, the numbers are read as a value to add to the sum of the
    prior values in the line, or as the amount to change by.
    """
    ret = []
    delta_multiplier = int(delta)
    for _ in symbol_counts:
      value = 0
      fields = []
      for f in _ReadValuesFromLine(lines, split=' '):
        value = value * delta_multiplier + int(f)
        fields.append(value)
      ret.append(fields)
    return ret

  addresses = read_numeric(delta=True)
  sizes = read_numeric(delta=False)
  if has_padding:
    paddings = read_numeric(delta=False)
  else:
    paddings = [None] * len(segment_names)
  path_indices = read_numeric(delta=True)
  if has_components:
    component_indices = read_numeric(delta=True)
  else:
    component_indices = [None] * len(segment_names)

  raw_symbols = [None] * sum(symbol_counts)
  symbol_idx = 0
  for (cur_segment_name, cur_symbol_count, cur_addresses, cur_sizes,
       cur_paddings, cur_path_indices,
       cur_component_indices) in zip(segment_names, symbol_counts, addresses,
                                     sizes, paddings, path_indices,
                                     component_indices):
    if has_multi_containers:
      # Extract '<cur_container_idx_str>cur_section_name'.
      assert cur_segment_name.startswith('<')
      cur_container_idx_str, cur_section_name = (cur_segment_name[1:].split(
          '>', 1))
      cur_container = containers[int(cur_container_idx_str)]
    else:
      cur_section_name = cur_segment_name
      cur_container = containers[0]
    alias_counter = 0
    for i in range(cur_symbol_count):
      parts = _ReadValuesFromLine(lines, split='\t')
      full_name = parts[0]
      flags_part = None
      aliases_part = None

      # aliases_part or flags_part may have been omitted.
      if len(parts) == 3:
        # full_name  aliases_part  flags_part
        aliases_part = parts[1]
        flags_part = parts[2]
      elif len(parts) == 2:
        if parts[1][0] == '0':
          # full_name  aliases_part
          aliases_part = parts[1]
        else:
          # full_name  flags_part
          flags_part = parts[1]

      # Use a bit less RAM by using the same instance for this common string.
      if full_name == models.STRING_LITERAL_NAME:
        full_name = models.STRING_LITERAL_NAME
      flags = int(flags_part, 16) if flags_part else 0
      num_aliases = int(aliases_part, 16) if aliases_part else 0

      # Skip the constructor to avoid default value checks.
      new_sym = models.Symbol.__new__(models.Symbol)
      new_sym.container = cur_container
      new_sym.section_name = cur_section_name
      new_sym.full_name = full_name
      new_sym.address = cur_addresses[i]
      new_sym.size = cur_sizes[i]
      paths = path_tuples[cur_path_indices[i]]
      new_sym.object_path, new_sym.source_path = paths
      component = components[cur_component_indices[i]] if has_components else ''
      new_sym.component = component
      new_sym.flags = flags
      new_sym.disassembly = ''
      # Derived.
      if cur_paddings:
        new_sym.padding = cur_paddings[i]
        if not new_sym.IsOverhead():
          new_sym.size += new_sym.padding
      else:
        new_sym.padding = 0  # Computed below.
      new_sym.template_name = ''
      new_sym.name = ''

      if num_aliases:
        assert alias_counter == 0
        new_sym.aliases = [new_sym]
        alias_counter = num_aliases - 1
      elif alias_counter > 0:
        new_sym.aliases = raw_symbols[symbol_idx - 1].aliases
        new_sym.aliases.append(new_sym)
        alias_counter -= 1
      else:
        new_sym.aliases = None

      raw_symbols[symbol_idx] = new_sym
      symbol_idx += 1

  if not has_padding:
    CalculatePadding(raw_symbols)

  # Get disassmebly if it exists.
  if has_disassembly:
    idx_disassembly = _ReadValuesFromLine(lines, split=' ')
    if len(idx_disassembly) > 0 and idx_disassembly[0] != '':
      for elem in idx_disassembly:
        elem = int(elem)
        diss_len = int(_ReadLine(lines))
        diss_text = lines.read(diss_len)
        raw_symbols[elem].disassembly = diss_text
  return models.SizeInfo(build_config,
                         containers,
                         raw_symbols,
                         size_path=size_path,
                         is_sparse=is_sparse)


@contextlib.contextmanager
def _OpenGzipForWrite(path, file_obj=None):
  # Open in a way that doesn't set any gzip header fields.
  if file_obj:
    with gzip.GzipFile(filename='', mode='wb', fileobj=file_obj, mtime=0) as fz:
      yield fz
  else:
    with open(path, 'wb') as f:
      with gzip.GzipFile(filename='', mode='wb', fileobj=f, mtime=0) as fz:
        yield fz


def _SaveCompressedStringList(string_list, file_obj):
  with _OpenGzipForWrite('', file_obj=file_obj) as f:
    w = _Writer(f)
    w.WriteLine(str(len(string_list)))
    for s in string_list:
      w.WriteLine(s)


def _LoadCompressedStringList(file_obj, size):
  bytesio = io.BytesIO()
  bytesio.write(file_obj.read(size))
  bytesio.seek(0)
  with gzip.GzipFile(filename='', fileobj=bytesio) as f:
    toks = f.read().decode('utf-8', errors='surrogatepass').splitlines()
    assert int(toks[0]) == len(toks) - 1
    return toks[1:]


def SaveSizeInfo(size_info, path, file_obj=None):
  """Saves |size_info| to |path|."""
  if os.environ.get('SUPERSIZE_MEASURE_GZIP') == '1':
    # Doing serialization and Gzip together.
    with _OpenGzipForWrite(path, file_obj=file_obj) as f:
      _SaveSizeInfoToFile(size_info, f)
  else:
    # Doing serizliation and Gzip separately.
    # This turns out to be faster. On Python 3: 40s -> 14s.
    bytesio = io.BytesIO()
    _SaveSizeInfoToFile(size_info, bytesio)

    logging.debug('Serialization complete. Gzipping...')
    with _OpenGzipForWrite(path, file_obj=file_obj) as f:
      f.write(bytesio.getvalue())


def LoadSizeInfo(filename, file_obj=None, is_sparse=False):
  """Returns a SizeInfo loaded from |filename|."""
  with gzip.GzipFile(filename=filename, fileobj=file_obj) as f:
    return _LoadSizeInfoFromFile(f, filename, is_sparse)


def SaveDeltaSizeInfo(delta_size_info, path, file_obj=None, make_sparse=True):
  """Saves |delta_size_info| to |path|."""
  if make_sparse and not delta_size_info.is_sparse:
    delta_size_info.MakeSparse()

  if not file_obj:
    with open(path, 'wb') as f:
      return SaveDeltaSizeInfo(delta_size_info, path, f)

  before_size_file = io.BytesIO()
  after_size_file = io.BytesIO()

  after_promise = parallel.CallOnThread(SaveSizeInfo,
                                        delta_size_info.after,
                                        '',
                                        file_obj=after_size_file)
  SaveSizeInfo(delta_size_info.before, '', file_obj=before_size_file)

  removed_sources_file = None
  if delta_size_info.removed_sources:
    removed_sources_file = io.BytesIO()
    _SaveCompressedStringList(delta_size_info.removed_sources,
                              removed_sources_file)

  added_sources_file = None
  if delta_size_info.added_sources:
    added_sources_file = io.BytesIO()
    _SaveCompressedStringList(delta_size_info.added_sources, added_sources_file)

  w = _Writer(file_obj)
  w.WriteBytes(_COMMON_HEADER + _SIZEDIFF_HEADER)
  # JSON header fields
  fields = {
      'version': _SIZEDIFF_VERSION,
      'before_length': before_size_file.tell(),
  }
  if removed_sources_file:
    fields['removed_sources_length'] = removed_sources_file.tell()
  if added_sources_file:
    fields['added_sources_length'] = added_sources_file.tell()

  fields_str = json.dumps(fields, indent=2, sort_keys=True)

  w.WriteLine(str(len(fields_str)))
  w.WriteLine(fields_str)

  if removed_sources_file:
    w.WriteBytes(removed_sources_file.getvalue())
  if added_sources_file:
    w.WriteBytes(added_sources_file.getvalue())

  w.WriteBytes(before_size_file.getvalue())
  after_promise.get()
  w.WriteBytes(after_size_file.getvalue())

  return None


def LoadDeltaSizeInfo(path, file_obj=None):
  """Returns a tuple of size infos (before, after).

  To reconstruct the DeltaSizeInfo, diff the two size infos.
  """
  if not file_obj:
    with open(path, 'rb') as f:
      return LoadDeltaSizeInfo(path, f)

  combined_header = _COMMON_HEADER + _SIZEDIFF_HEADER
  actual_header = file_obj.read(len(combined_header))
  if actual_header != combined_header:
    raise Exception('Bad file header.')

  json_len = int(file_obj.readline())
  json_str = file_obj.read(1 + json_len)  # + 1 for \n
  fields = json.loads(json_str)
  assert fields['version'] == _SIZEDIFF_VERSION
  pos = file_obj.tell()

  removed_sources = []
  removed_sources_length = fields.get('removed_sources_length', 0)
  if removed_sources_length:
    removed_sources = _LoadCompressedStringList(file_obj,
                                                removed_sources_length)
    pos += removed_sources_length

  added_sources = []
  added_sources_length = fields.get('added_sources_length', 0)
  if added_sources_length:
    added_sources = _LoadCompressedStringList(file_obj, added_sources_length)
    pos += added_sources_length

  before_size_info = LoadSizeInfo(path, file_obj, is_sparse=True)
  pos += fields['before_length']
  file_obj.seek(pos)
  after_size_info = LoadSizeInfo(path, file_obj, is_sparse=True)

  return before_size_info, after_size_info, removed_sources, added_sources
