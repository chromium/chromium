# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Checks that collected symbols are not missing things."""

import logging
import models
import os


class QualityCheckError(Exception):
  def __init__(self, msg):
    super(QualityCheckError,
          self).__init__('--check-data-quality assertion failed: ' + msg)


def CheckDataQuality(size_info, track_string_literals):
  logging.debug('Grouping symbols')
  grouped = size_info.raw_symbols.GroupedByContainerAndSectionName()
  section_sizes = size_info.section_sizes
  logging.debug('computing')
  for symbols in grouped:
    container = symbols[0].container
    section_name = symbols[0].section_name
    segment_size = container.section_sizes[section_name]
    is_other = section_name == models.SECTION_OTHER
    is_native = section_name in models.NATIVE_SECTIONS
    is_dex = section_name in models.DEX_SECTIONS

    logging.debug('checking section %s<%s>', section_name, container.name)

    actual_size = 0.0
    actual_padding = 0.0
    placeholder_size = 0.0
    no_name_size = 0.0
    no_source_path_size = 0.0
    no_attribution_size = 0.0
    no_component_size = 0.0
    string_literal_size = 0.0
    alias_count = 0
    generated_count = 0
    unlikely_count = 0
    startup_count = 0

    for sym in symbols:
      pss = sym.pss
      actual_size += pss
      actual_padding += sym.padding_pss
      if sym.full_name.startswith('**'):
        placeholder_size += pss
      if not sym.full_name:
        no_name_size += pss
      if not sym.source_path:
        no_source_path_size += pss
      if not (sym.full_name or sym.source_path or sym.object_path):
        no_attribution_size += pss
      if not sym.component:
        no_component_size += pss
      if sym.IsStringLiteral():
        string_literal_size += pss
      alias_count += int(bool(sym.aliases and sym is sym.aliases[0]))
      generated_count += int(bool(sym.flags & models.FLAG_GENERATED_SOURCE))
      unlikely_count += int(bool(sym.flags & models.FLAG_UNLIKELY))
      startup_count += int(bool(sym.flags & models.FLAG_STARTUP))

      if os.path.isabs(sym.source_path):
        raise QualityCheckError('Abs path found in source_path: ' + repr(sym))
      if os.path.isabs(sym.object_path):
        raise QualityCheckError('Abs path found in object_path: ' + repr(sym))

    def raise_error(msg, *args):
      header = ('Within section {} of container "{}", '
                'which has {} symbols totalling {} bytes: ').format(
                    section_name, container.name, len(symbols), segment_size)
      raise QualityCheckError(header + msg.format(*args))

    def raise_size_error(kind, size, limit_fraction):
      raise_error(
          'Abnormally high number of bytes attributed to {}: {:.0f} '
          '({:.0%}, limit was {:.0%}).', kind, size, size / segment_size,
          limit_fraction)

    def check_size(kind, size, limit_fraction):
      limit = limit_fraction * segment_size
      if size > limit:
        raise_size_error(kind, size, limit_fraction)

    def check_some_exist(kind, count, limit=1):
      if count < limit:
        raise_error(
            'Expected at least {} {} to exist. '
            'Found only {} out of {} symbols.', limit, kind, count,
            len(symbols))

    if not isinstance(segment_size, int):
      raise_error('Section size should be a whole number.')
    if segment_size < 1:
      raise_error('Section size should not greater than zero.')
    if round(actual_size) != segment_size:
      raise_error('Sum of symbols sizes do not match section size. Sum={}',
                  round(actual_size))

    check_size('padding', actual_padding, (0.05 if is_other else 0.01))

    # One bad symbol can mess up small containers.
    is_small_section = (len(symbols) < 10
                        or segment_size / section_sizes[section_name] < .1)
    if not is_small_section:
      # Dex string tables show up as placeholders.
      check_size('placeholders', placeholder_size, (0.2 if is_dex else 0.01))

      check_size('symbols without names', no_name_size, 0.01)
      check_size('symbols without source paths', no_source_path_size, 0.1)
      check_size('symbols without name or path', no_attribution_size, 0.01)
      check_size('symbols without component', no_component_size, 0.20)

      if track_string_literals and section_name == models.SECTION_RODATA:
        if string_literal_size / segment_size < .05:
          raise_error(
              'Expected more size from string literals. Found only {} ({:.1%})',
              string_literal_size, string_literal_size / segment_size)

      if is_native:
        check_some_exist('symbol aliases', alias_count)
      if is_native or is_dex:
        check_some_exist('generated symbols', generated_count)
      if section_name == models.SECTION_TEXT:
        check_some_exist('symbols annotated by AFDO profile', unlikely_count)
        check_some_exist('static initializers', startup_count)


def _Divide(a, b):
  return float(a) / b if b else 0


# TODO(agrieve): Have this utilize the stats collected by CheckDataQuality().
def _DescribeSizeInfoContainerCoverage(raw_symbols, container):
  """Yields lines describing how accurate |size_info| is."""
  for section, section_name in models.SECTION_TO_SECTION_NAME.items():
    expected_size = container.section_sizes.get(section_name)
    in_section = raw_symbols.WhereInSection(section_name, container=container)
    actual_size = in_section.size

    if expected_size is None:
      yield 'Section {}: {} bytes from {} symbols.'.format(
          section_name, actual_size, len(in_section))
    else:
      size_fraction = _Divide(actual_size, expected_size)
      yield ('Section {}: has {:.1%} of {} bytes accounted for from '
             '{} symbols. {} bytes are unaccounted for.').format(
                 section_name, size_fraction, actual_size, len(in_section),
                 expected_size - actual_size)

    padding = in_section.padding
    yield '* Padding accounts for {} bytes ({:.1%})'.format(
        padding, _Divide(padding, actual_size))

    def size_msg(syms, show_padding=False):
      size = syms.size if not show_padding else syms.size_without_padding
      size_msg = 'Accounts for {} bytes ({:.1%}).'.format(
          size, _Divide(size, actual_size))
      if show_padding:
        size_msg = size_msg[:-1] + ' padding is {} bytes.'.format(syms.padding)
      return size_msg

    syms = in_section.Filter(lambda s: s.source_path)
    yield '* {} have source paths. {}'.format(len(syms), size_msg(syms))
    syms = in_section.WhereHasComponent()
    yield '* {} have a component assigned. {}'.format(len(syms), size_msg(syms))

    syms = in_section.WhereNameMatches(r'^\*')
    if len(syms):
      yield '* {} placeholders exist (symbols that start with **). {}'.format(
          len(syms), size_msg(syms))

    syms = syms.Inverted().WhereHasAnyAttribution().Inverted()
    if syms:
      yield '* {} symbols have no name or path. {}'.format(
          len(syms), size_msg(syms))

    if section == 'r':
      syms = in_section.Filter(lambda s: s.IsStringLiteral())
      yield '* {} string literals exist. {}'.format(
          len(syms), size_msg(syms, show_padding=True))

    syms = in_section.Filter(lambda s: s.aliases)
    if len(syms):
      uniques = sum(1 for s in syms.IterUniqueSymbols())
      saved = sum(s.size_without_padding * (s.num_aliases - 1)
                  for s in syms.IterUniqueSymbols())
      yield ('* {} aliases exist, mapped to {} unique addresses '
             '({} bytes saved)').format(len(syms), uniques, saved)

    syms = in_section.WhereObjectPathMatches('{shared}')
    if len(syms):
      yield '* {} symbols have shared ownership. {}'.format(
          len(syms), size_msg(syms))
    else:
      yield '* 0 symbols have shared ownership.'

    for flag, desc in ((models.FLAG_HOT, 'marked as "hot"'),
                       (models.FLAG_UNLIKELY, 'marked as "unlikely"'),
                       (models.FLAG_STARTUP,
                        'marked as "startup"'), (models.FLAG_CLONE, 'clones'),
                       (models.FLAG_GENERATED_SOURCE,
                        'from generated sources')):
      syms = in_section.WhereHasFlag(flag)
      if len(syms):
        yield '* {} symbols are {}. {}'.format(len(syms), desc, size_msg(syms))

    # These thresholds were found by experimenting with arm32 Chrome.
    # E.g.: Set them to 0 and see what warnings get logged, then take max value.
    spam_counter = 0
    for i in range(len(in_section) - 1):
      sym = in_section[i + 1]
      if (not sym.full_name.startswith('*')
          and not sym.source_path.endswith('.S')  # Assembly symbol are iffy.
          and not sym.IsStringLiteral()
          and ((sym.section in 'rd' and sym.padding >= 256) or
               (sym.section in 't' and sym.padding >= 64))):
        # TODO(crbug.com/959906): We should synthesize symbols for these gaps
        #     rather than attribute them as padding.
        spam_counter += 1
        if spam_counter > 5:
          break
        yield 'Large padding of {} between:'.format(sym.padding)
        yield '  A) ' + repr(in_section[i])
        yield '  B) ' + repr(sym)


def DescribeSizeInfoCoverage(size_info):
  for i, container in enumerate(size_info.containers):
    if i > 0:
      yield ''
    if container.name:
      yield 'Container <%s>' % container.name
    # TODO(huangs): Change to use "yield from" once linters allow this.
    for line in _DescribeSizeInfoContainerCoverage(size_info.raw_symbols,
                                                   container):
      yield line
