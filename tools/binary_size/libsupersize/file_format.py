# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Deals with loading & saving .size and .sizediff files.

The .size file is written in the following format. There are no section
delimiters, instead the end of a section is usually determined by a row count on
the first line of a section, followed by that number of rows. In other cases,
the sections have a known size.

Header
------
4 lines long.
Line 0 of the file is a header comment.
Line 1 is the serialization version of the file.
Line 2 is the number of characters in the header fields string.
Line 3 is the header fields string, a stringified JSON object.

Path list
---------
A list of paths. The first line is the size of the list,
and the next N lines that follow are items in the list. Each item is a tuple
of (object_path, source_path) where the two parts are tab separated.

Component list
--------------
A list of components. The first line is the size of the list,
and the next N lines that follow are items in the list. Each item is a unique
COMPONENT which is referenced later.
This section is only present if 'has_components' is True in header fields.

Symbol counts
-------------
2 lines long.
The first line is a tab separated list of section names.
The second line is a tab separated list of symbol group lengths, in the same
order as the previous line.

Numeric values
--------------
In each section, the number of rows is the same as the number of section names
in Symbol counts. The values on a row are space separated, in the order of the
symbols in each group.

Addresses
~~~~~~~~~~
Symbol start addresses which are delta-encoded.

Sizes
~~~~~
The number of bytes this symbol takes up.

Padding
~~~~~~~
The number of padding bytes this symbol has.
This section is only present if 'has_padding' is True in header fields.

Path indices
~~~~~~~~~~~~~
Indices that reference paths in the prior Path list section. Delta-encoded.

Component indices
~~~~~~~~~~~~~~~~~~
Indices that reference components in the prior Component list section.
Delta-encoded.
This section is only present if 'has_components' is True in header fields.

Symbols
-------
The final section contains details info on each symbol. Each line represents
a single symbol. Values are tab separated and follow this format:
symbol.full_name, symbol.num_aliases, symbol.flags
|num_aliases| will be omitted if the aliases of the symbol are the same as the
previous line. |flags| will be omitted if there are no flags.



The .sizediff file stores a sparse representation of a difference between .size
files. Each .sizediff file stores two sparse .size files, before and after,
containing only symbols that differed between "before" and "after". They can
be rendered via the Tiger viewer. .sizediff files use the following format:

Header
------
3 lines long.
Line 0 of the file is a header comment.
Line 1 is the number of characters in the header fields string.
Line 2 is the header fields string, a stringified JSON object. This currently
contains two fields, 'before_length' (the length in bytes of the 'before'
section) and 'version', which is always 1.

Before
------
The next |header.before_length| bytes are a valid gzipped sparse .size file
containing the "before" snapshot.

After
-----
All remaining bytes are a valid gzipped sparse .size file containing the
"after" snapshot.
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


def SortSymbols(raw_symbols):
  logging.debug('Sorting %d symbols', len(raw_symbols))
  # TODO(agrieve): Either change this sort so that it's only sorting by section
  #     (and not using .sort()), or have it specify a total ordering (which must
  #     also include putting padding-only symbols before others of the same
  #     address). Note: The sort as-is takes ~1.5 seconds.
  # s.size_without_padding > 0 prevents CalculatePadding() from incorrectly
  #   detecting duplicate symbols.
  # (s.full_name, s.object_path) are important for sort stability when called by
  #   _ExpandSparseSymbols().
  def sort_key(s):
    return (s.IsPak(), s.IsBss(), s.section_name, s.address,
            s.size_without_padding > 0, s.full_name, s.object_path)

  raw_symbols.sort(key=sort_key)
  seen_aliases = set()
  for s in raw_symbols:
    if s.aliases:
      if s.aliases[0] not in seen_aliases:
        s.aliases.sort(key=sort_key)
        seen_aliases.add(s.aliases[0])

  logging.info('Processed %d symbols', len(raw_symbols))


def CalculatePadding(raw_symbols):
  """Populates the |padding| field based on symbol addresses. """
  logging.info('Calculating padding')

  # Padding not really required, but it is useful to check for large padding and
  # log a warning.
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


def _ExpandSparseSymbols(sparse_symbols):
  """Expands a symbol list with all aliases of all symbols in the list.

  Args:
    sparse_symbols: A list or SymbolGroup to expand.
  """
  representative_symbols = set()
  raw_symbols = []
  logging.debug('Expanding sparse_symbols with aliases of included symbols')
  for sym in sparse_symbols:
    if sym.aliases:
      num_syms = len(representative_symbols)
      representative_symbols.add(sym.aliases[0])
      if num_syms < len(representative_symbols):
        raw_symbols.extend(sym.aliases)
    else:
      raw_symbols.append(sym)
  logging.debug('Done expanding sparse_symbols')
  return models.SymbolGroup(raw_symbols)


def _SaveSizeInfoToFile(size_info,
                        file_obj,
                        include_padding=False,
                        sparse_symbols=None):
  """Saves size info to a .size file.

  Args:
    size_info: Data to write to the file
    file_obj: File opened for writing.
    include_padding: Whether to save padding data, useful if adding a subset of
      symbols.
    sparse_symbols: If present, only save these symbols to the file.
  """
  if sparse_symbols is not None:
    # Any aliases of sparse symbols must also be included, or else file
    # parsing will attribute symbols that happen to follow an incomplete alias
    # group to that alias group.
    raw_symbols = _ExpandSparseSymbols(sparse_symbols)
  else:
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
      'has_padding': include_padding,
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
    } for c in size_info.containers]
  else:
    # Write using old format.
    fields['metadata'] = size_info.metadata_legacy
    fields['section_sizes'] = size_info.containers[0].section_sizes

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
  if include_padding:
    write_groups(lambda s: s.padding)
    w.LogSize('paddings')  # For libchrome, adds 300kb

  write_groups(
      lambda s: path_tuples[(s.object_path, s.source_path)], delta=True)
  w.LogSize('path indices')  # For libchrome: adds 125kb.

  write_groups(lambda s: components[s.component], delta=True)
  w.LogSize('component indices')

  prev_aliases = None
  for group in symbol_group_by_segment:
    for symbol in group:
      w.WriteString(symbol.full_name)
      if symbol.aliases and symbol.aliases is not prev_aliases:
        w.WriteString('\t0%x' % symbol.num_aliases)
      prev_aliases = symbol.aliases
      if symbol.flags:
        w.WriteString('\t%x' % symbol.flags)
      w.WriteBytes(b'\n')
  w.LogSize('names (final)')  # For libchrome: adds 3.5mb.


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


def _LoadSizeInfoFromFile(file_obj, size_path):
  """Loads a size_info from the given file.

  See _SaveSizeInfoToFile() for details on the .size file format.

  Args:
    file_obj: File to read, should be a GzipFile
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
                           section_sizes=cfield['section_sizes'])
      containers.append(c)
  else:  # Old format.
    build_config = {}
    metadata = fields.get('metadata')
    if metadata:
      for key in models.BUILD_CONFIG_KEYS:
        if key in metadata:
          build_config[key] = metadata[key]
          del metadata[key]
    section_sizes = fields['section_sizes']
    containers.append(
        models.Container(name='',
                         metadata=metadata,
                         section_sizes=section_sizes))
  models.Container.AssignShortNames(containers)

  has_components = fields.get('has_components', False)
  has_padding = fields.get('has_padding', False)

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
    return models.SizeInfo(build_config, containers, [], size_path=size_path)

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

  return models.SizeInfo(build_config,
                         containers,
                         raw_symbols,
                         size_path=size_path)


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


def SaveSizeInfo(size_info,
                 path,
                 file_obj=None,
                 include_padding=False,
                 sparse_symbols=None):
  """Saves |size_info| to |path|."""
  if os.environ.get('SUPERSIZE_MEASURE_GZIP') == '1':
    # Doing serialization and Gzip together.
    with _OpenGzipForWrite(path, file_obj=file_obj) as f:
      _SaveSizeInfoToFile(
          size_info,
          f,
          include_padding=include_padding,
          sparse_symbols=sparse_symbols)
  else:
    # Doing serizliation and Gzip separately.
    # This turns out to be faster. On Python 3: 40s -> 14s.
    bytesio = io.BytesIO()
    _SaveSizeInfoToFile(
        size_info,
        bytesio,
        include_padding=include_padding,
        sparse_symbols=sparse_symbols)

    logging.debug('Serialization complete. Gzipping...')
    with _OpenGzipForWrite(path, file_obj=file_obj) as f:
      f.write(bytesio.getvalue())


def LoadSizeInfo(filename, file_obj=None):
  """Returns a SizeInfo loaded from |filename|."""
  with gzip.GzipFile(filename=filename, fileobj=file_obj) as f:
    return _LoadSizeInfoFromFile(f, filename)


def SaveDeltaSizeInfo(delta_size_info, path, file_obj=None):
  """Saves |delta_size_info| to |path|."""

  if not file_obj:
    with open(path, 'wb') as f:
      return SaveDeltaSizeInfo(delta_size_info, path, f)

  changed_symbols = delta_size_info.raw_symbols \
      .WhereDiffStatusIs(models.DIFF_STATUS_UNCHANGED).Inverted()
  before_symbols = models.SymbolGroup(
      [sym.before_symbol for sym in changed_symbols if sym.before_symbol])
  after_symbols = models.SymbolGroup(
      [sym.after_symbol for sym in changed_symbols if sym.after_symbol])

  before_size_file = io.BytesIO()
  after_size_file = io.BytesIO()

  after_promise = parallel.CallOnThread(
      SaveSizeInfo,
      delta_size_info.after,
      '',
      file_obj=after_size_file,
      include_padding=True,
      sparse_symbols=after_symbols)
  SaveSizeInfo(
      delta_size_info.before,
      '',
      file_obj=before_size_file,
      include_padding=True,
      sparse_symbols=before_symbols)

  w = _Writer(file_obj)
  w.WriteBytes(_COMMON_HEADER + _SIZEDIFF_HEADER)
  # JSON header fields
  fields = {
      'version': _SIZEDIFF_VERSION,
      'before_length': before_size_file.tell(),
  }
  fields_str = json.dumps(fields, indent=2, sort_keys=True)

  w.WriteLine(str(len(fields_str)))
  w.WriteLine(fields_str)

  w.WriteBytes(before_size_file.getvalue())
  after_promise.get()
  w.WriteBytes(after_size_file.getvalue())


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
  json_str = file_obj.read(json_len + 1)  # + 1 for \n
  fields = json.loads(json_str)

  assert fields['version'] == _SIZEDIFF_VERSION
  after_pos = file_obj.tell() + fields['before_length']

  before_size_info = LoadSizeInfo(path, file_obj)
  file_obj.seek(after_pos)
  after_size_info = LoadSizeInfo(path, file_obj)

  return before_size_info, after_size_info
