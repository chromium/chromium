# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Logic for diffing two SizeInfo objects."""

import collections
import logging
import re

import models


_STRIP_NUMBER_SUFFIX_PATTERN = re.compile(r'[.0-9]+$')
_NORMALIZE_STAR_SYMBOLS_PATTERN = re.compile(r'\s+\d+( \(.*\))?$')


def _ExactMatchKey(s):
  # Remove numbers and periods for symbols defined by macros that use __line__
  # in names, or for linker symbols like ".L.ref.tmp.2".
  name = _STRIP_NUMBER_SUFFIX_PATTERN.sub('', s.full_name)
  # Use section rather than section_name since clang & gcc use
  # .data.rel.ro vs. .data.rel.ro.local.
  return s.section, name, s.object_path, s.size_without_padding


def _GoodMatchKey(symbol):
  """Returns a tuple that can be used to see if two Symbol are the same.

  Keys are not guaranteed to be unique within a SymbolGroup. When multiple
  symbols have the same key, they will be matched up in order of appearance.
  We do this because the numbering of these generated symbols is not stable.

  Examples of symbols with shared keys:
    "** merge strings"
    "** symbol gap 3", "** symbol gap 5 (end of section)"
    "foo() [clone ##]"
    "CSWTCH.61", "CSWTCH.62"
    ".L.ref.tmp.2", ".L.ref.tmp"
    "._468", "._467"
    ".L__unnamed_1193", ".L__unnamed_712"
  """
  if symbol.IsPak():
    # full_name looks like "about_ui_resources.grdp: IDR_ABOUT_UI_CREDITS_HTML".
    # name is just "IDR_ABOUT_UI_CREDITS_HTML".
    name = symbol.name
  else:
    name = _STRIP_NUMBER_SUFFIX_PATTERN.sub('', symbol.full_name)
    clone_idx = name.find(' [clone ')
    if clone_idx != -1:
      name = name[:clone_idx]
    if name.startswith('*'):
      # "symbol gap 3 (bar)" -> "symbol gaps"
      name = _NORMALIZE_STAR_SYMBOLS_PATTERN.sub('s', name)

  return symbol.section, symbol.object_path, name


def _PoorMatchKey(symbol):
  section, _, name = _GoodMatchKey(symbol)
  return section, name


def _MatchSymbols(before, after, key_func, padding_by_section_name):
  logging.debug('%s: Building symbol index', key_func.__name__)
  before_symbols_by_key = collections.defaultdict(list)
  for s in before:
    before_symbols_by_key[key_func(s)].append(s)

  logging.debug('%s: Creating delta symbols', key_func.__name__)
  unmatched_after = []
  delta_symbols = []
  for after_sym in after:
    before_sym = before_symbols_by_key.get(key_func(after_sym))
    if before_sym:
      before_sym = before_sym.pop(0)
      # Padding tracked in aggregate, except for padding-only symbols.
      if before_sym.size_without_padding != 0:
        padding_by_section_name[before_sym.section_name] += (
            after_sym.padding_pss - before_sym.padding_pss)
      delta_symbols.append(models.DeltaSymbol(before_sym, after_sym))
    else:
      unmatched_after.append(after_sym)

  logging.debug('%s: Matched %d of %d symbols', key_func.__name__,
                len(delta_symbols), len(after))

  unmatched_before = []
  for syms in before_symbols_by_key.itervalues():
    unmatched_before.extend(syms)
  return delta_symbols, unmatched_before, unmatched_after


def _DiffSymbolGroups(before, after):
  # For changed symbols, padding is zeroed out. In order to not lose the
  # information entirely, store it in aggregate.
  padding_by_section_name = collections.defaultdict(int)

  # Usually >90% of symbols are exact matches, so all of the time is spent in
  # this first pass.
  all_deltas, before, after = _MatchSymbols(
      before, after, _ExactMatchKey, padding_by_section_name)
  for key_func in (_GoodMatchKey, _PoorMatchKey):
    delta_syms, before, after = _MatchSymbols(
        before, after, key_func, padding_by_section_name)
    all_deltas.extend(delta_syms)

  logging.debug('Creating %d unmatched symbols', len(after) + len(before))
  for after_sym in after:
    all_deltas.append(models.DeltaSymbol(None, after_sym))
  for before_sym in before:
    all_deltas.append(models.DeltaSymbol(before_sym, None))

  # Create a DeltaSymbol to represent the zero'd out padding of matched symbols.
  for section_name, padding in padding_by_section_name.iteritems():
    if padding != 0:
      after_sym = models.Symbol(section_name, padding,
                                name="** aggregate padding of diff'ed symbols")
      after_sym.padding = padding
      all_deltas.append(models.DeltaSymbol(None, after_sym))

  return models.DeltaSymbolGroup(all_deltas)


def Diff(before, after, sort=False):
  """Diffs two SizeInfo objects. Returns a DeltaSizeInfo."""
  assert isinstance(before, models.SizeInfo)
  assert isinstance(after, models.SizeInfo)
  section_sizes = {k: after.section_sizes.get(k, 0) - v
                   for k, v in before.section_sizes.iteritems()}
  for k, v in after.section_sizes.iteritems():
    if k not in section_sizes:
      section_sizes[k] = v

  symbol_diff = _DiffSymbolGroups(before.raw_symbols, after.raw_symbols)
  ret = models.DeltaSizeInfo(before, after, section_sizes, symbol_diff)

  if sort:
    syms = ret.symbols  # Triggers clustering.
    logging.debug('Grouping')
    # Group path aliases so that functions defined in headers will be sorted
    # by their actual size rather than shown as many small symbols.
    syms = syms.GroupedByAliases(same_name_only=True)
    logging.debug('Sorting')
    ret.symbols = syms.Sorted()
  logging.debug('Diff complete')
  return ret
