# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Methods for converting model objects to human-readable formats."""

import abc
import data_quality
import io
import collections
import csv
import datetime
import itertools
import math
import time

import models


def _PrettySize(size):
  # Arbitrarily chosen cut-off.
  if abs(size) < 2000:
    return '%d bytes' % size
  # Always show 3 digits.
  size /= 1024.0
  if abs(size) < 10:
    return '%.2fkb' % size
  if abs(size) < 100:
    return '%.1fkb' % size
  if abs(size) < 1024:
    return '%dkb' % size
  size /= 1024.0
  if abs(size) < 10:
    return '%.2fmb' % size
  # We shouldn't be seeing sizes > 100mb.
  return '%.1fmb' % size


def _FormatPss(pss, force_sign=False):
  # Shows a decimal for small numbers to make it clear that a shared symbol has
  # a non-zero pss.
  if abs(pss) > 10:
    return str(int(pss))
  near_int = abs(pss) % 1 < 0.05
  if near_int and abs(pss) < 1 and pss:
    return '~0'
  if force_sign:
    return ('%+.0f' if near_int else '%+.1f') % pss
  return ('%.0f' if near_int else '%.1f') % pss


def _Divide(a, b):
  return float(a) / b if b else 0


def _GetSectionSizeInfo(unsummed_sections, summed_sections, section_sizes):
  sizes = [v for k, v in section_sizes.items() if k in summed_sections]
  total_bytes = sum(sizes)
  max_bytes = max(sizes)

  maybe_significant_sections = unsummed_sections | summed_sections

  def is_significant_section(name, size):
    # Show all sections containing symbols, plus relocations.
    # As a catch-all, also include any section that comprises > 4% of the
    # largest section. Use largest section rather than total so that it still
    # works out when showing a diff containing +100, -100 (total=0).
    return (name in maybe_significant_sections
            or name in ['.rela.dyn', '.rel.dyn']
            or abs(_Divide(size, max_bytes)) > .04)

  section_names = sorted(
      k for k, v in section_sizes.items() if is_significant_section(k, v))

  return (total_bytes, section_names)


class Histogram:
  BUCKET_NAMES_FOR_SMALL_VALUES = {-1: '(-1,0)', 0: '{0}', 1: '(0,1)'}

  def __init__(self):
    self.data = collections.defaultdict(int)

  # Input:  (-8,-4], (-4,-2], (-2,-1], (-1,0), {0}, (0,1), [1,2), [2,4), [4,8).
  # Output:   -4,      -3,      -2,      -1,    0,    1,     2,     3,     4.
  @staticmethod
  def _Bucket(v):
    absv = abs(v)
    if absv < 1:
      return 0 if v == 0 else (-1 if v < 0 else 1)
    mag = int(math.log(absv, 2.0)) + 2
    return mag if v > 0 else -mag

  @staticmethod
  def _BucketName(k):
    if abs(k) <= 1:
      return Histogram.BUCKET_NAMES_FOR_SMALL_VALUES[k]
    if k < 0:
      return '(-{},-{}]'.format(1 << (-k - 1), 1 << (-k - 2))
    return '[{},{})'.format(1 << (k - 2), 1 << (k - 1))

  def Add(self, v):
    self.data[self._Bucket(v)] += 1

  def Generate(self):
    keys = sorted(self.data.keys())
    bucket_names = [self._BucketName(k) for k in keys]
    bucket_values = [str(self.data[k]) for k in keys]
    num_items = len(keys)
    num_cols = 6
    num_rows = (num_items + num_cols - 1) // num_cols  # Divide and round up.
    # Needed for range() to not throw due to step by 0.
    if num_rows == 0:
      return
    # Spaces needed by items in each column, to align on ':'.
    name_col_widths = []
    value_col_widths = []
    for i in range(0, num_items, num_rows):
      name_col_widths.append(max(len(s) for s in bucket_names[i:][:num_rows]))
      value_col_widths.append(max(len(s) for s in bucket_values[i:][:num_rows]))

    yield 'Histogram of symbols based on PSS:'
    for r in range(num_rows):
      row = list(
          zip(bucket_names[r::num_rows], name_col_widths,
              bucket_values[r::num_rows], value_col_widths))
      line = '    ' + '   '.join('{:>{}}: {:<{}}'.format(*t) for t in row)
      yield line.rstrip()


class Describer:
  def __init__(self):
    pass

  @abc.abstractmethod
  def _DescribeDeltaSizeInfo(self, diff):
    pass

  @abc.abstractmethod
  def _DescribeSizeInfo(self, size_info):
    pass

  @abc.abstractmethod
  def _DescribeDeltaSymbolGroup(self, delta_group):
    pass

  @abc.abstractmethod
  def _DescribeSymbolGroup(self, group):
    pass

  @abc.abstractmethod
  def _DescribeSymbol(self, sym, single_line=False):
    pass

  def _DescribeIterable(self, obj):
    for i, x in enumerate(obj):
      yield '{}: {!r}'.format(i, x)

  def GenerateLines(self, obj):
    if isinstance(obj, models.DeltaSizeInfo):
      return self._DescribeDeltaSizeInfo(obj)
    if isinstance(obj, models.SizeInfo):
      return self._DescribeSizeInfo(obj)
    if isinstance(obj, models.DeltaSymbolGroup):
      return self._DescribeDeltaSymbolGroup(obj)
    if isinstance(obj, models.SymbolGroup):
      return self._DescribeSymbolGroup(obj)
    if isinstance(obj, (models.Symbol, models.DeltaSymbol)):
      return self._DescribeSymbol(obj)
    if hasattr(obj, '__iter__'):
      return self._DescribeIterable(obj)
    return iter((repr(obj),))


class DescriberText(Describer):
  def __init__(self, verbose=False, recursive=False, summarize=True):
    super().__init__()
    self.verbose = verbose
    self.recursive = recursive
    self.summarize = summarize

  def _DescribeSectionSizes(self,
                            unsummed_sections,
                            summed_sections,
                            section_sizes,
                            indent=''):
    total_bytes, section_names = _GetSectionSizeInfo(unsummed_sections,
                                                     summed_sections,
                                                     section_sizes)
    yield ''
    yield '{}Section Sizes (Total={} ({} bytes)):'.format(
        indent, _PrettySize(total_bytes), total_bytes)
    for name in section_names:
      size = section_sizes[name]
      if name in unsummed_sections:
        yield '{}    {}: {} ({} bytes) (not included in totals)'.format(
            indent, name, _PrettySize(size), size)
      else:
        notes = ''
        if name not in summed_sections:
          notes = ' (counted in .other)'
        percent = _Divide(size, total_bytes)
        yield '{}    {}: {} ({} bytes) ({:.1%}){}'.format(
            indent, name, _PrettySize(size), size, percent, notes)

    if self.verbose:
      yield ''
      yield '{}Other section sizes:'.format(indent)
      section_names = sorted(
          k for k in section_sizes.keys() if k not in section_names)
      for name in section_names:
        notes = ''
        if name in unsummed_sections:
          notes = ' (not included in totals)'
        elif name not in summed_sections:
          notes = ' (counted in .other)'
        yield '{}    {}: {} ({} bytes){}'.format(
            indent, name, _PrettySize(section_sizes[name]), section_sizes[name],
            notes)

  def _DescribeSymbol(self, sym, single_line=False):
    container_str = sym.container_short_name
    if container_str:
      container_str = '<{}>'.format(container_str)

    address = 'Group' if sym.IsGroup() else hex(sym.address)

    last_field = ''
    if sym.IsGroup():
      last_field = 'count=%d' % len(sym)
    else:
      syms = [sym.before_symbol, sym.after_symbol] if sym.IsDelta() else [sym]
      num_aliases = [s.num_aliases for s in syms if not s is None]
      if num_aliases[0] != num_aliases[-1]:  # If 2 distinct values.
        last_field = 'num_aliases=%d->%d' % tuple(num_aliases)
      elif num_aliases[0] > 1 or self.verbose:
        last_field = 'num_aliases=%d' % num_aliases[0]

    pss_field = _FormatPss(sym.pss, sym.IsDelta())
    if sym.IsDelta():
      b = sum(s.before_symbol.pss_without_padding if s.before_symbol else 0
              for s in sym.IterLeafSymbols())
      a = sum(s.after_symbol.pss_without_padding if s.after_symbol else 0
              for s in sym.IterLeafSymbols())
      pss_field = '{} ({}->{})'.format(pss_field, _FormatPss(b), _FormatPss(a))
    elif sym.num_aliases > 1:
      pss_field = '{} (size={})'.format(pss_field, sym.size)

    if self.verbose:
      if last_field:
        last_field = '  ' + last_field
      if sym.IsDelta():
        yield '{}{}@{:<9s}  {}{}'.format(container_str, sym.section, address,
                                         pss_field, last_field)
      else:
        l = '{}{}@{:<9s}  pss={}  padding={}{}'.format(container_str,
                                                       sym.section, address,
                                                       pss_field, sym.padding,
                                                       last_field)
        yield l
      yield '    source_path={} \tobject_path={}'.format(
          sym.source_path, sym.object_path)
      if sym.name:
        yield '    flags={}  name={}'.format(sym.FlagsString(), sym.name)
        if sym.full_name is not sym.name:
          yield '         full_name={}'.format(sym.full_name)
      elif sym.full_name:
        yield '    flags={}  full_name={}'.format(
            sym.FlagsString(), sym.full_name)
    else:
      if last_field:
        last_field = ' ({})'.format(last_field)
      if sym.IsDelta():
        pss_field = '{:<18}'.format(pss_field)
      else:
        pss_field = '{:<14}'.format(pss_field)
      if single_line:
        yield '{}{}@{:<9s}  {}  {}{}'.format(container_str, sym.section,
                                             address, pss_field, sym.name,
                                             last_field)
      else:
        path = sym.source_path or sym.object_path
        if path and sym.generated_source:
          path = '$root_gen_dir/' + path
        path = path or '{no path}'

        yield '{}{}@{:<9s}  {} {}'.format(container_str, sym.section, address,
                                          pss_field, path)
        if sym.name:
          yield '    {}{}'.format(sym.name, last_field)

  def _DescribeSymbolGroupChildren(self, group, indent=0):
    running_total = 0
    running_percent = 0
    is_delta = group.IsDelta()
    all_groups = all(s.IsGroup() for s in group)

    indent_prefix = '> ' * indent
    diff_prefix = ''
    total = group.pss
    # is_default_sorted ==> sorted by abs(PSS) from largest to smallest.
    if group.is_default_sorted:
      # Skip long tail of small symbols (useful for diffs where aliases change).
      # Long tail is defined as:
      #   * Accounts for < .5% of PSS
      #   * Symbols are smaller than 1.0 byte (by PSS)
      #   * Always show at least 50 symbols.
      min_remaining_pss_to_show = max(1024.0, total / 1000.0 * 5)
      min_symbol_pss_to_show = 1.0
      min_symbols_to_show = 50

    for index, s in enumerate(group):
      if group.is_default_sorted and not self.verbose:
        remaining_pss = total - running_total
        if (index >= min_symbols_to_show and
            abs(remaining_pss) < min_remaining_pss_to_show and
            abs(s.pss) < min_symbol_pss_to_show):
          remaining_count = len(group) - index
          yield '{}Skipping {} tiny symbols comprising {} bytes.'.format(
              indent_prefix, remaining_count, _FormatPss(remaining_pss))
          break

      if group.IsBss() or not s.IsBss():
        running_total += s.pss
        running_percent = _Divide(running_total, total)
      for l in self._DescribeSymbol(s, single_line=all_groups):
        if l[:4].isspace():
          indent_size = 8 + len(indent_prefix) + len(diff_prefix)
          yield '{} {}'.format(' ' * indent_size, l)
        else:
          if is_delta:
            diff_prefix = models.DIFF_PREFIX_BY_STATUS[s.diff_status]
          yield '{}{}{:<4} {:>8} {:7} {}'.format(
              indent_prefix, diff_prefix, str(index) + ')',
              _FormatPss(running_total), '({:.1%})'.format(running_percent), l)

      if self.recursive and s.IsGroup():
        for l in self._DescribeSymbolGroupChildren(s, indent=indent + 1):
          yield l

  @staticmethod
  def _RelevantSections(section_names):
    relevant_sections = [
        s for s in models.SECTION_TO_SECTION_NAME.values() if s in section_names
    ]
    if models.SECTION_MULTIPLE in relevant_sections:
      relevant_sections.remove(models.SECTION_MULTIPLE)
    return relevant_sections

  def _DescribeSymbolGroup(self, group):
    if self.summarize:
      total_size = group.pss
      pss_by_section = collections.defaultdict(float)
      counts_by_section = collections.defaultdict(int)
      for s in group.IterLeafSymbols():
        pss_by_section[s.section_name] += s.pss
        if not s.IsDelta() or s.diff_status is not models.DIFF_STATUS_UNCHANGED:
          counts_by_section[s.section_name] += 1

    # Apply this filter after calcualating size since an alias being removed
    # causes some symbols to be UNCHANGED, yet have pss != 0.
    if group.IsDelta():
      group = group.WhereDiffStatusIs(models.DIFF_STATUS_UNCHANGED).Inverted()

    if self.summarize:
      histogram = Histogram()
      for s in group:
        histogram.Add(s.pss)
      unique_paths = set()
      for s in group.IterLeafSymbols():
        # Ignore paths like foo/{shared}/2
        if '{' not in s.object_path:
          unique_paths.add(s.object_path)

      if group.IsDelta():
        before_unique, after_unique = group.CountUniqueSymbols()
        unique_part = '{:,} -> {:,} unique'.format(before_unique, after_unique)
      else:
        unique_part = '{:,} unique'.format(group.CountUniqueSymbols())

      relevant_sections = self._RelevantSections(pss_by_section)

      size_summary = 'Sizes: ' + ' '.join(
          '{}={:<10}'.format(k, _PrettySize(int(pss_by_section[k])))
          for k in relevant_sections)
      size_summary += ' total={:<10}'.format(_PrettySize(int(total_size)))

      counts_summary = 'Counts: ' + ' '.join(
          '{}={}'.format(k, counts_by_section[k]) for k in relevant_sections)

      section_legend = ', '.join(
          '{}={}'.format(models.SECTION_NAME_TO_SECTION[k], k)
          for k in relevant_sections if k in models.SECTION_NAME_TO_SECTION)

      summary_desc = itertools.chain(
          ['Showing {:,} symbols ({}) with total pss: {} bytes'.format(
              len(group), unique_part, int(total_size))],
          histogram.Generate(),
          [size_summary.rstrip()],
          [counts_summary],
          ['Number of unique paths: {}'.format(len(unique_paths))],
          [''],
          ['Section Legend: {}'.format(section_legend)],
      )
    else:
      summary_desc = ()

    title_parts = ['Index', 'Running Total']
    if group.container_name == '':
      title_parts.append('Section@Address')
    else:
      title_parts.append('<Container>Section@Address')
    if self.verbose:
      title_parts.append('...')
    else:
      if group.IsDelta():
        title_parts.append(u'\u0394 PSS (\u0394 size_without_padding)')
      else:
        title_parts.append('PSS')
      title_parts.append('Path')
    titles = ' | '.join(title_parts)

    header_desc = (titles, '-' * 60)

    children_desc = self._DescribeSymbolGroupChildren(group)
    return itertools.chain(summary_desc, header_desc, children_desc)

  def _DescribeDiffObjectPaths(self, delta_group):
    paths_by_status = [set(), set(), set(), set()]
    for s in delta_group.IterLeafSymbols():
      path = s.source_path or s.object_path
      # Ignore paths like foo/{shared}/2
      if '{' not in path:
        paths_by_status[s.diff_status].add(path)
    # Initial paths sets are those where *any* symbol is
    # unchanged/changed/added/removed.
    unchanged, changed, added, removed = paths_by_status
    # Consider a path with both adds & removes as "changed".
    changed.update(added.intersection(removed))
    # Consider a path added / removed only when all symbols are new/removed.
    added.difference_update(unchanged)
    added.difference_update(changed)
    added.difference_update(removed)
    removed.difference_update(unchanged)
    removed.difference_update(changed)
    removed.difference_update(added)
    yield '{} paths added, {} removed, {} changed'.format(
        len(added), len(removed), len(changed))

    if self.verbose and added:
      yield 'Added files:'
      for p in sorted(added):
        yield '  ' + p
    if self.verbose and removed:
      yield 'Removed files:'
      for p in sorted(removed):
        yield '  ' + p
    if self.verbose and changed:
      yield 'Changed files:'
      for p in sorted(changed):
        yield '  ' + p

  def _DescribeDeltaSymbolGroup(self, delta_group):
    if self.summarize:
      num_inc = 0
      num_dec = 0
      counts_by_section = collections.defaultdict(int)
      for sym in delta_group.IterLeafSymbols():
        if sym.pss > 0:
          num_inc += 1
        elif sym.pss < 0:
          num_dec += 1

        status = sym.diff_status
        if status == models.DIFF_STATUS_ADDED:
          counts_by_section[sym.section_name] += 1
        elif status == models.DIFF_STATUS_REMOVED:
          counts_by_section[sym.section_name] -= 1

      relevant_sections = self._RelevantSections(counts_by_section)
      counts = delta_group.CountsByDiffStatus()
      diff_status_msg = ('{} symbols added (+), {} changed (~), '
                         '{} removed (-), {} unchanged (not shown)').format(
          counts[models.DIFF_STATUS_ADDED],
          counts[models.DIFF_STATUS_CHANGED],
          counts[models.DIFF_STATUS_REMOVED],
          counts[models.DIFF_STATUS_UNCHANGED])
      counts_by_section_msg = 'Added/Removed by section: ' + ' '.join(
          '{}: {:+}'.format(k, counts_by_section[k]) for k in relevant_sections)

      num_unique_before_symbols, num_unique_after_symbols = (
          delta_group.CountUniqueSymbols())
      diff_summary_desc = [
          diff_status_msg,
          counts_by_section_msg,
          'Of changed symbols, {} grew, {} shrank'.format(num_inc, num_dec),
          'Number of unique symbols {} -> {} ({:+})'.format(
              num_unique_before_symbols, num_unique_after_symbols,
              num_unique_after_symbols - num_unique_before_symbols),
          ]
      path_delta_desc = itertools.chain(
          self._DescribeDiffObjectPaths(delta_group),
          ('',))
    else:
      diff_summary_desc = ()
      path_delta_desc = ()

    group_desc = self._DescribeSymbolGroup(delta_group)
    return itertools.chain(diff_summary_desc, path_delta_desc, group_desc)

  def _DescribeDeltaDict(self, data_name, before_dict, after_dict, indent=''):
    common_items = {
        k: v
        for k, v in before_dict.items() if after_dict.get(k) == v
    }
    before_items = {
        k: v
        for k, v in before_dict.items() if k not in common_items
    }
    after_items = {k: v for k, v in after_dict.items() if k not in common_items}
    return itertools.chain(
        (indent + 'Common %s:' % data_name, ),
        (indent + '    %s' % line for line in DescribeDict(common_items)),
        (indent + 'Old %s:' % data_name, ),
        (indent + '    %s' % line for line in DescribeDict(before_items)),
        (indent + 'New %s:' % data_name, ),
        (indent + '    %s' % line for line in DescribeDict(after_items)))

  def _DescribeDeltaSizeInfo(self, diff):
    desc_list = []
    # Describe |build_config| and each container. If there is only one container
    # then support legacy output by reporting |build_config| as part of the
    # first container's metadata.
    if len(diff.containers) > 1:
      desc_list.append(
          self._DescribeDeltaDict('Build config', diff.before.build_config,
                                  diff.after.build_config))
      for c in diff.containers:
        desc_list.append(('', ))
        desc_list.append(('Container<%s>: %s' % (c.short_name, c.name), ))
        desc_list.append(
            self._DescribeDeltaDict('Metadata',
                                    c.before.metadata,
                                    c.after.metadata,
                                    indent='    '))
        unsummed_sections, summed_sections = c.ClassifySections()
        desc_list.append(
            self._DescribeSectionSizes(unsummed_sections,
                                       summed_sections,
                                       c.section_sizes,
                                       indent='    '))
    else:  # Legacy output for single Container case.
      desc_list.append(
          self._DescribeDeltaDict('Metadata', diff.before.metadata_legacy,
                                  diff.after.metadata_legacy))
      c = diff.containers[0]
      unsummed_sections, summed_sections = c.ClassifySections()
      desc_list.append(
          self._DescribeSectionSizes(unsummed_sections, summed_sections,
                                     c.section_sizes))
    desc_list.append(('', ))
    desc_list.append(self.GenerateLines(diff.symbols))
    return itertools.chain.from_iterable(desc_list)

  def _DescribeSizeInfo(self, size_info):
    desc_list = []
    # Describe |build_config| and each container. If there is only one container
    # then support legacy output by reporting |build_config| as part of the
    # first container's metadata.
    if len(size_info.containers) > 1:
      desc_list.append(('Build Configs:', ))
      desc_list.append('    %s' % line
                       for line in DescribeDict(size_info.build_config))
      containers = size_info.containers
    else:
      containers = [
          models.Container(
              name='',
              metadata=size_info.metadata_legacy,
              section_sizes=size_info.containers[0].section_sizes,
              metrics_by_file=size_info.containers[0].metrics_by_file)
      ]
    for c in containers:
      if c.name:
        desc_list.append(('', ))
        desc_list.append(('Container<%s>: %s' % (c.short_name, c.name), ))
      desc_list.append(('Metadata:', ))
      desc_list.append('    %s' % line for line in DescribeDict(c.metadata))
      unsummed_sections, summed_sections = c.ClassifySections()
      desc_list.append(
          self._DescribeSectionSizes(unsummed_sections, summed_sections,
                                     c.section_sizes))

    if self.verbose:
      desc_list.append(('', ))
      desc_list.append(data_quality.DescribeSizeInfoCoverage(size_info))
    desc_list.append(('', ))
    desc_list.append(self.GenerateLines(size_info.symbols))
    return itertools.chain.from_iterable(desc_list)


class DescriberCsv(Describer):
  def __init__(self, verbose=False):
    super().__init__()
    self.verbose = verbose
    self.stringio = io.StringIO()
    self.csv_writer = csv.writer(self.stringio)

  def _RenderCsv(self, data):
    self.stringio.truncate(0)
    self.stringio.seek(0)
    self.csv_writer.writerow(data)
    return self.stringio.getvalue().rstrip()

  def _DescribeSectionSizes(self, unsummed_sections, summed_section,
                            section_sizes):
    _, significant_section_names = _GetSectionSizeInfo(unsummed_sections,
                                                       summed_section,
                                                       section_sizes)
    if self.verbose:
      significant_set = set(significant_section_names)
      section_names = sorted(section_sizes.keys())
      yield self._RenderCsv(['Name', 'Size', 'IsSignificant'])
      for name in section_names:
        size = section_sizes[name]
        yield self._RenderCsv([name, size, int(name in significant_set)])
    else:
      yield self._RenderCsv(['Name', 'Size'])
      for name in significant_section_names:
        size = section_sizes[name]
        yield self._RenderCsv([name, size])

  def _DescribeDeltaSizeInfo(self, diff):
    desc_list = []
    for c in diff.containers:
      unsummed_sections, summed_sections = c.ClassifySections()
      desc_list.append(
          self._DescribeSectionSizes(unsummed_sections, summed_sections,
                                     c.section_sizes))
    desc_list.append(('', ))
    desc_list.append(self.GenerateLines(diff.symbols))
    return itertools.chain.from_iterable(desc_list)

  def _DescribeSizeInfo(self, size_info):
    desc_list = []
    for c in size_info.containers:
      unsummed_sections, summed_sections = c.ClassifySections()
      desc_list.append(
          self._DescribeSectionSizes(unsummed_sections, summed_sections,
                                     c.section_sizes))
    desc_list.append(('', ))
    desc_list.append(self.GenerateLines(size_info.symbols))
    return itertools.chain.from_iterable(desc_list)

  def _DescribeDeltaSymbolGroup(self, delta_group):
    yield self._RenderSymbolHeader(True);
    # Apply filter to remove UNCHANGED groups.
    delta_group = delta_group.WhereDiffStatusIs(
        models.DIFF_STATUS_UNCHANGED).Inverted()
    for sym in delta_group:
      yield self._RenderSymbolData(sym)

  def _DescribeSymbolGroup(self, group):
    yield self._RenderSymbolHeader(False);
    for sym in group:
      yield self._RenderSymbolData(sym)

  def _DescribeSymbol(self, sym, single_line=False):
    yield self._RenderSymbolHeader(sym.IsDelta());
    yield self._RenderSymbolData(sym)

  def _RenderSymbolHeader(self, isDelta):
    fields = []
    fields.append('GroupCount')
    fields.append('Address')
    fields.append('SizeWithoutPadding')
    fields.append('Padding')
    if isDelta:
      fields += ['BeforeNumAliases', 'AfterNumAliases']
    else:
      fields.append('NumAliases')
    fields.append('PSS')
    fields.append('Section')
    if self.verbose:
      fields.append('Flags')
      fields.append('SourcePath')
      fields.append('ObjectPath')
    fields.append('Name')
    if self.verbose:
      fields.append('FullName')
    return self._RenderCsv(fields)

  def _RenderSymbolData(self, sym):
    data = []
    data.append(len(sym) if sym.IsGroup() else None)
    data.append(None if sym.IsGroup() else hex(sym.address))
    data.append(sym.size_without_padding)
    data.append(sym.padding)
    if sym.IsDelta():
      b, a = (None, None) if sym.IsGroup() else (sym.before_symbol,
                                                 sym.after_symbol)
      data.append(b.num_aliases if b else None)
      data.append(a.num_aliases if a else None)
    else:
      data.append(sym.num_aliases)
    data.append(round(sym.pss, 3))
    data.append(sym.section)
    if self.verbose:
      data.append(sym.FlagsString())
      data.append(sym.source_path);
      data.append(sym.object_path);
    data.append(sym.name)
    if self.verbose:
      data.append(sym.full_name)
    return self._RenderCsv(data)


def _UtcToLocal(utc):
  epoch = time.mktime(utc.timetuple())
  offset = (datetime.datetime.fromtimestamp(epoch) -
            datetime.datetime.utcfromtimestamp(epoch))
  return utc + offset


def DescribeDict(input_dict):
  display_dict = {}
  for k, v in input_dict.items():
    if k == models.METADATA_ELF_MTIME:
      timestamp_obj = datetime.datetime.utcfromtimestamp(v)
      display_dict[k] = (
          _UtcToLocal(timestamp_obj).strftime('%Y-%m-%d %H:%M:%S'))
    elif isinstance(v, str):
      display_dict[k] = v
    elif isinstance(v, list):
      if v:
        if isinstance(v[0], str):
          display_dict[k] = ' '.join(str(t) for t in v)
        else:
          display_dict[k] = repr(v)
      else:
        display_dict[k] = ''
    else:
      display_dict[k] = repr(v)
  return sorted('%s=%s' % t for t in display_dict.items())


def GenerateLines(obj, verbose=False, recursive=False, summarize=True,
                  format_name='text'):
  """Returns an iterable of lines (without \n) that describes |obj|."""
  if format_name == 'text':
    d = DescriberText(verbose=verbose, recursive=recursive, summarize=summarize)
  elif format_name == 'csv':
    d = DescriberCsv(verbose=verbose)
  else:
    raise ValueError('Unknown format_name \'{}\''.format(format_name));
  return d.GenerateLines(obj)


def WriteLines(lines, func):
  for l in lines:
    func(l)
    func('\n')
