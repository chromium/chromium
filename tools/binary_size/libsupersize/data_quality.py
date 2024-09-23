# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Checks that collected symbols are not missing things."""

import logging
import models
import os


class QualityCheckError(Exception):
  pass


def _Divide(a, b):
  return float(a) / b if b else 0


def CheckDataQuality(size_info, track_string_literals):
  logging.debug('Grouping symbols')
  grouped = size_info.raw_symbols.GroupedByContainerAndSectionName()
  section_sizes = size_info.section_sizes
  logging.debug('computing')
  errors = []
  for symbols in grouped:
    segment_has_error = []  # List so can be mutated from nested function.
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
        errors.append('Abs path found in source_path: ' + repr(sym))
      if os.path.isabs(sym.object_path):
        errors.append('Abs path found in object_path: ' + repr(sym))

    def report_error(msg, *args):
      if not segment_has_error:
        segment_has_error.append(True)
        errors.append(('Error(s) found in container "{}", section "{}", '
                       'which has {} symbols totalling {} bytes: ').format(
                           container.name, section_name, len(symbols),
                           segment_size))
      full_msg = msg.format(*args)
      errors.append('    ' + full_msg)

    def report_size_error(kind, size, limit_fraction):
      report_error(
          'Abnormally high number of bytes attributed to {}: {:.0f} '
          '({:.0%}, limit was {:.0%}).', kind, size,
          _Divide(size, segment_size), limit_fraction)

    def check_size(kind, size, limit_fraction):
      limit = limit_fraction * segment_size
      if size > limit:
        report_size_error(kind, size, limit_fraction)

    def check_some_exist(kind, count, limit=1):
      if count < limit:
        report_error(
            'Expected at least {} {} to exist. '
            'Found only {} out of {} symbols.', limit, kind, count,
            len(symbols))

    if not isinstance(segment_size, int):
      report_error('Section size should be a whole number.')
      continue
    if segment_size < 1:
      report_error('Section size less than one.')
      continue
    if round(actual_size) != segment_size:
      report_error('Sum of symbols sizes do not match section size. Sum={}',
                   round(actual_size))
      continue

    check_size('padding', actual_padding, (0.05 if is_other else 0.01))

    # One bad symbol can mess up small containers.
    is_small_section = (len(symbols) < 10 or
                        _Divide(segment_size, section_sizes[section_name]) < .1)
    if not is_small_section:
      # Dex string tables show up as placeholders.
      check_size('placeholders', placeholder_size, (0.2 if is_dex else 0.01))

      check_size('symbols without names', no_name_size, 0.01)
      check_size('symbols without source paths', no_source_path_size, 0.1)
      check_size('symbols without name or path', no_attribution_size, 0.01)
      check_size('symbols without component', no_component_size, 0.20)

      if track_string_literals and section_name == models.SECTION_RODATA:
        if _Divide(string_literal_size, segment_size) < .05:
          report_error(
              'Expected more size from string literals. Found only {} ({:.1%})',
              string_literal_size, _Divide(string_literal_size, segment_size))

      if is_native:
        check_some_exist('symbol aliases', alias_count)
      if is_native or is_dex:
        check_some_exist('generated symbols', generated_count)
      if section_name == models.SECTION_TEXT:
        check_some_exist('symbols annotated by AFDO profile', unlikely_count)
        check_some_exist('static initializers', startup_count)

  if errors:
    # Cap the number of log messages.
    MAX_ERRORS = 40
    logging.error('--check-data-quality Found %d errors:', len(errors))
    for msg in errors[:MAX_ERRORS]:
      logging.error('Failed: %s', msg)
    if len(errors) > MAX_ERRORS:
      logging.error('... and %d more.', len(errors) - MAX_ERRORS)
    raise QualityCheckError()


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

    syms = in_section.WhereIsPlaceholder()
    if syms:
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
    if syms:
      uniques = sum(1 for s in syms.IterUniqueSymbols())
      saved = sum(s.size_without_padding * (s.num_aliases - 1)
                  for s in syms.IterUniqueSymbols())
      yield ('* {} aliases exist, mapped to {} unique addresses '
             '({} bytes saved)').format(len(syms), uniques, saved)

    syms = in_section.WhereObjectPathMatches('{shared}')
    if syms:
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
      if syms:
        yield '* {} symbols are {}. {}'.format(len(syms), desc, size_msg(syms))

    spam_counter = 0
    i = 1
    count = len(in_section)
    while i < count:
      prev_sym = in_section[i - 1]
      sym = in_section[i]
      if (not sym.full_name.startswith('*')
          # Assembly symbol are iffy.
          and not prev_sym.source_path.endswith('.S') and
          not sym.source_path.endswith('.S')
          # String literal symbol creation is imperfect.
          and not prev_sym.IsStringLiteral() and not sym.IsStringLiteral()
          # Thresholds found by experimenting with arm32 Chrome.
          # E.g.: Set to 0 and see what warnings appear, then take max value.
          and ((sym.section in 'rd' and sym.padding >= 256) or
               (sym.section in 't' and sym.padding >= 64))):
        # TODO(crbug.com/40626114): We should synthesize symbols for these gaps
        #     rather than attribute them as padding.
        spam_counter += 1
        if spam_counter > 5:
          break
        yield 'Large padding of {} between:'.format(sym.padding)
        yield '  A) ' + repr(in_section[i - 1])
        yield '  B) ' + repr(sym)
      # All aliases will have the same padding.
      i += sym.num_aliases


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
