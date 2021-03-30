#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates several files used by the size trybot to monitor size regressions."""

import argparse
import collections
import json
import logging
import os
import re
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), 'libsupersize'))
import archive
import diagnose_bloat
import diff
import describe
import file_format
import models

_RESOURCE_SIZES_LOG = 'resource_sizes_log'
_BASE_RESOURCE_SIZES_LOG = 'base_resource_sizes_log'
_MUTABLE_CONSTANTS_LOG = 'mutable_contstants_log'
_FOR_TESTING_LOG = 'for_test_log'
_DEX_SYMBOLS_LOG = 'dex_symbols_log'
_SIZEDIFF_FILENAME = 'supersize_diff.sizediff'
_HTML_REPORT_URL = (
    'https://chrome-supersize.firebaseapp.com/viewer.html?load_url={{' +
    _SIZEDIFF_FILENAME + '}}')
_MAX_DEX_METHOD_COUNT_INCREASE = 50
_MAX_NORMALIZED_INCREASE = 16 * 1024
_MAX_PAK_INCREASE = 1024


_PROGUARD_CLASS_MAPPING_RE = re.compile(r'(?P<original_name>[^ ]+)'
                                        r' -> '
                                        r'(?P<obfuscated_name>[^:]+):')
_PROGUARD_FIELD_MAPPING_RE = re.compile(r'(?P<type>[^ ]+) '
                                        r'(?P<original_name>[^ (]+)'
                                        r' -> '
                                        r'(?P<obfuscated_name>[^:]+)')
_PROGUARD_METHOD_MAPPING_RE = re.compile(
    # line_start:line_end: (optional)
    r'((?P<line_start>\d+):(?P<line_end>\d+):)?'
    r'(?P<return_type>[^ ]+)'  # original method return type
    # original method class name (if exists)
    r' (?:(?P<original_method_class>[a-zA-Z_\d.$]+)\.)?'
    r'(?P<original_method_name>[^.\(]+)'
    r'\((?P<params>[^\)]*)\)'  # original method params
    r'(?:[^ ]*)'  # original method line numbers (ignored)
    r' -> '
    r'(?P<obfuscated_name>.+)')  # obfuscated method name


class _SizeDelta(collections.namedtuple(
    'SizeDelta', ['name', 'units', 'expected', 'actual'])):

  @property
  def explanation(self):
    ret = '{}: {} {} (max is {} {})'.format(
        self.name, self.actual, self.units, self.expected, self.units)
    return ret

  def IsAllowable(self):
    return self.actual <= self.expected

  def IsLargeImprovement(self):
    return (self.actual * -1) >= self.expected

  def __lt__(self, other):
    return self.name < other.name


def _SymbolDiffHelper(title_fragment, symbols):
  added = symbols.WhereDiffStatusIs(models.DIFF_STATUS_ADDED)
  removed = symbols.WhereDiffStatusIs(models.DIFF_STATUS_REMOVED)
  both = (added + removed).SortedByName()
  lines = []
  if len(both) > 0:
    for group in both.GroupedByContainer():
      counts = group.CountsByDiffStatus()
      lines += [
          '===== {} Added & Removed ({}) ====='.format(
              title_fragment, group.full_name),
          'Added: {}'.format(counts[models.DIFF_STATUS_ADDED]),
          'Removed: {}'.format(counts[models.DIFF_STATUS_REMOVED]),
          ''
      ]
      lines.extend(describe.GenerateLines(group, summarize=False))
      lines += ['']

  return lines, len(added) - len(removed)


def _CreateMutableConstantsDelta(symbols):
  symbols = symbols.WhereInSection('d').WhereNameMatches(r'\bk[A-Z]|\b[A-Z_]+$')
  lines, net_added = _SymbolDiffHelper('Mutable Constants', symbols)

  return lines, _SizeDelta('Mutable Constants', 'symbols', 0, net_added)


def _CreateMethodCountDelta(symbols):
  symbols = symbols.WhereIsOnDemand(False)
  method_symbols = symbols.WhereInSection(models.SECTION_DEX_METHOD)
  method_lines, net_method_added = _SymbolDiffHelper('Methods', method_symbols)
  class_symbols = symbols.WhereInSection(
      models.SECTION_DEX).WhereNameMatches('#').Inverted()
  class_lines, _ = _SymbolDiffHelper('Classes', class_symbols)
  lines = []
  if class_lines:
    lines.extend(class_lines)
    lines.extend(['', ''])  # empty lines added for clarity
  if method_lines:
    lines.extend(method_lines)

  return lines, _SizeDelta('Dex Methods Count', 'methods',
                           _MAX_DEX_METHOD_COUNT_INCREASE, net_method_added)


def _CreateResourceSizesDelta(before_dir, after_dir):
  sizes_diff = diagnose_bloat.ResourceSizesDiff()
  sizes_diff.ProduceDiff(before_dir, after_dir)

  return sizes_diff.Summary(), _SizeDelta(
      'Normalized APK Size', 'bytes', _MAX_NORMALIZED_INCREASE,
      sizes_diff.summary_stat.value)


def _CreateBaseModuleResourceSizesDelta(before_dir, after_dir):
  sizes_diff = diagnose_bloat.ResourceSizesDiff(include_sections=['base'])
  sizes_diff.ProduceDiff(before_dir, after_dir)

  return sizes_diff.DetailedResults(), _SizeDelta(
      'Base Module Size', 'bytes', _MAX_NORMALIZED_INCREASE,
      sizes_diff.CombinedSizeChangeForSection('base'))


def _CreateSupersizeDiff(main_file_name, before_dir, after_dir):
  before_size_path = os.path.join(before_dir, main_file_name + '.size')
  after_size_path = os.path.join(after_dir, main_file_name + '.size')
  before = archive.LoadAndPostProcessSizeInfo(before_size_path)
  after = archive.LoadAndPostProcessSizeInfo(after_size_path)
  size_info_delta = diff.Diff(before, after, sort=True)

  lines = list(describe.GenerateLines(size_info_delta))
  return lines, size_info_delta


def _CreateUncompressedPakSizeDeltas(symbols):
  pak_symbols = symbols.Filter(lambda s:
      s.size > 0 and
      bool(s.flags & models.FLAG_UNCOMPRESSED) and
      s.section_name == models.SECTION_PAK_NONTRANSLATED)
  return [
      _SizeDelta('Uncompressed Pak Entry "{}"'.format(pak.full_name), 'bytes',
                 _MAX_PAK_INCREASE, pak.after_symbol.size)
      for pak in pak_symbols
  ]


def _ExtractForTestingSymbolsFromSingleMapping(mapping_path):
  with open(mapping_path) as f:
    proguard_mapping_lines = f.readlines()
    current_class_orig = None
    for line in proguard_mapping_lines:
      if line.isspace():
        continue
      if not line.startswith(' '):
        match = _PROGUARD_CLASS_MAPPING_RE.search(line)
        if match is None:
          raise Exception('Malformed class mapping')
        current_class_orig = match.group('original_name')
        continue
      assert current_class_orig is not None
      line = line.strip()
      match = _PROGUARD_METHOD_MAPPING_RE.search(line)
      if (match is not None
          and match.group('original_method_name').find('ForTest') > -1):
        method_symbol = '{}#{}'.format(
            match.group('original_method_class') or current_class_orig,
            match.group('original_method_name'))
        yield method_symbol

      match = _PROGUARD_FIELD_MAPPING_RE.search(line)
      if (match is not None
          and match.group('original_name').find('ForTest') > -1):
        field_symbol = '{}#{}'.format(current_class_orig,
                                      match.group('original_name'))
        yield field_symbol


def _ExtractForTestingSymbolsFromMappings(mapping_paths):
  symbols = set()
  for mapping_path in mapping_paths:
    symbols.update(_ExtractForTestingSymbolsFromSingleMapping(mapping_path))
  return symbols


def _CreateTestingSymbolsDeltas(before_mapping_paths, after_mapping_paths):
  before_symbols = _ExtractForTestingSymbolsFromMappings(before_mapping_paths)
  after_symbols = _ExtractForTestingSymbolsFromMappings(after_mapping_paths)
  added_symbols = list(after_symbols.difference(before_symbols))
  removed_symbols = list(before_symbols.difference(after_symbols))
  lines = []
  if added_symbols:
    lines.append('Added Symbols Named "ForTest"')
    lines.extend(added_symbols)
    lines.extend(['', ''])  # empty lines added for clarity
  if removed_symbols:
    lines.append('Removed Symbols Named "ForTest"')
    lines.extend(removed_symbols)
    lines.extend(['', ''])  # empty lines added for clarity
  return lines, _SizeDelta('Added symbols named "ForTest"', 'symbols', 0,
                           len(added_symbols) - len(removed_symbols))


def _GenerateBinarySizePluginDetails(metrics):
  binary_size_listings = []
  for delta, log_name in metrics:
    # Only show the base module delta if it is significant.
    if (log_name == _BASE_RESOURCE_SIZES_LOG and delta.IsAllowable()
        and not delta.IsLargeImprovement()):
      continue
    listing = {
        'name': delta.name,
        'delta': '{} {}'.format(_FormatNumber(delta.actual), delta.units),
        'limit': '{} {}'.format(_FormatNumber(delta.expected), delta.units),
        'log_name': log_name,
        'allowed': delta.IsAllowable(),
        'large_improvement': delta.IsLargeImprovement(),
    }
    if log_name == _RESOURCE_SIZES_LOG:
      listing['name'] = 'Android Binary Size'
      binary_size_listings.insert(0, listing)
      continue
    # The main 'binary size' delta is always shown even if unchanged.
    elif delta.actual == 0:
      continue
    binary_size_listings.append(listing)

  binary_size_extras = [
      {
          'text': 'APK Breakdown',
          'url': _HTML_REPORT_URL
      },
  ]

  return {
      'listings': binary_size_listings,
      'extras': binary_size_extras,
  }


def _FormatNumber(number):
  # Adds a sign for positive numbers and puts commas in large numbers
  return '{:+,}'.format(number)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--author', required=True, help='CL author')
  parser.add_argument('--size-config-json-name',
                      required=True,
                      help='Filename of JSON with configs for '
                      'binary size measurement.')
  parser.add_argument(
      '--before-dir',
      required=True,
      help='Directory containing the APK from reference build.')
  parser.add_argument(
      '--after-dir',
      required=True,
      help='Directory containing APK for the new build.')
  parser.add_argument(
      '--results-path',
      required=True,
      help='Output path for the trybot result .json file.')
  parser.add_argument(
      '--staging-dir',
      required=True,
      help='Directory to write summary files to.')
  parser.add_argument('-v', '--verbose', action='store_true')
  args = parser.parse_args()

  if args.verbose:
    logging.basicConfig(level=logging.INFO)

  to_before_path = lambda p: os.path.join(args.before_dir, os.path.basename(p))
  to_after_path = lambda p: os.path.join(args.after_dir, os.path.basename(p))

  with open(to_after_path(args.size_config_json_name), 'rt') as fh:
    config = json.load(fh)
  supersize_input_name = os.path.basename(config['supersize_input_file'])
  before_mapping_paths = [to_before_path(f) for f in config['mapping_files']]
  after_mapping_paths = [to_after_path(f) for f in config['mapping_files']]

  logging.info('Creating Supersize diff')
  supersize_diff_lines, delta_size_info = _CreateSupersizeDiff(
      supersize_input_name, args.before_dir, args.after_dir)

  changed_symbols = delta_size_info.raw_symbols.WhereDiffStatusIs(
      models.DIFF_STATUS_UNCHANGED).Inverted()

  # Monitor dex method count since the "multidex limit" is a thing.
  logging.info('Checking dex symbols')
  dex_delta_lines, dex_delta = _CreateMethodCountDelta(changed_symbols)
  size_deltas = {dex_delta}
  metrics = {(dex_delta, _DEX_SYMBOLS_LOG)}

  # Look for native symbols called "kConstant" that are not actually constants.
  # C++ syntax makes this an easy mistake, and having symbols in .data uses more
  # RAM than symbols in .rodata (at least for multi-process apps).
  logging.info('Checking for mutable constants in native symbols')
  mutable_constants_lines, mutable_constants_delta = (
      _CreateMutableConstantsDelta(changed_symbols))
  size_deltas.add(mutable_constants_delta)
  metrics.add((mutable_constants_delta, _MUTABLE_CONSTANTS_LOG))

  # Look for symbols with 'ForTest' in their name.
  logging.info('Checking for DEX symbols named "ForTest"')
  testing_symbols_lines, test_symbols_delta = (_CreateTestingSymbolsDeltas(
      before_mapping_paths, after_mapping_paths))
  size_deltas.add(test_symbols_delta)
  metrics.add((test_symbols_delta, _FOR_TESTING_LOG))

  # Check for uncompressed .pak file entries being added to avoid unnecessary
  # bloat.
  logging.info('Checking pak symbols')
  size_deltas.update(_CreateUncompressedPakSizeDeltas(changed_symbols))

  # Normalized APK Size is the main metric we use to monitor binary size.
  logging.info('Creating sizes diff')
  resource_sizes_lines, resource_sizes_delta = (_CreateResourceSizesDelta(
      args.before_dir, args.after_dir))
  size_deltas.add(resource_sizes_delta)
  metrics.add((resource_sizes_delta, _RESOURCE_SIZES_LOG))

  logging.info('Creating base module sizes diff')
  base_resource_sizes_lines, base_resource_sizes_delta = (
      _CreateBaseModuleResourceSizesDelta(args.before_dir, args.after_dir))
  size_deltas.add(base_resource_sizes_delta)
  metrics.add((base_resource_sizes_delta, _BASE_RESOURCE_SIZES_LOG))

  # .sizediff can be consumed by the html viewer.
  logging.info('Creating HTML Report')
  sizediff_path = os.path.join(args.staging_dir, _SIZEDIFF_FILENAME)
  file_format.SaveDeltaSizeInfo(delta_size_info, sizediff_path)

  passing_deltas = set(d for d in size_deltas if d.IsAllowable())
  failing_deltas = size_deltas - passing_deltas

  is_roller = '-autoroll' in args.author
  failing_checks_text = '\n'.join(d.explanation for d in sorted(failing_deltas))
  passing_checks_text = '\n'.join(d.explanation for d in sorted(passing_deltas))
  checks_text = """\
FAILING Checks:
{}

PASSING Checks:
{}

To understand what those checks are and how to pass them, see:
https://chromium.googlesource.com/chromium/src/+/master/docs/speed/binary_size/android_binary_size_trybot.md

""".format(failing_checks_text, passing_checks_text)

  status_code = int(bool(failing_deltas))

  # Give rollers a free pass, except for mutable constants.
  # Mutable constants are rare, and other regressions are generally noticed in
  # size graphs and can be investigated after-the-fact.
  if is_roller and mutable_constants_delta not in failing_deltas:
    status_code = 0

  summary = '<br>' + checks_text.replace('\n', '<br>')
  links_json = [
      {
          'name': 'Binary Size Details',
          'lines': resource_sizes_lines,
          'log_name': _RESOURCE_SIZES_LOG,
      },
      {
          'name': 'Base Module Binary Size Details',
          'lines': base_resource_sizes_lines,
          'log_name': _BASE_RESOURCE_SIZES_LOG,
      },
      {
          'name': 'Mutable Constants Diff',
          'lines': mutable_constants_lines,
          'log_name': _MUTABLE_CONSTANTS_LOG,
      },
      {
          'name': 'ForTest Symbols Diff',
          'lines': testing_symbols_lines,
          'log_name': _FOR_TESTING_LOG,
      },
      {
          'name': 'Dex Class and Method Diff',
          'lines': dex_delta_lines,
          'log_name': _DEX_SYMBOLS_LOG,
      },
      {
          'name': 'SuperSize Text Diff',
          'lines': supersize_diff_lines,
      },
      {
          'name': 'SuperSize HTML Diff',
          'url': _HTML_REPORT_URL,
      },
  ]
  # Remove empty diffs (Mutable Constants, Dex Method, ...).
  links_json = [o for o in links_json if o.get('lines') or o.get('url')]

  binary_size_plugin_json = _GenerateBinarySizePluginDetails(metrics)

  results_json = {
      'status_code': status_code,
      'summary': summary,
      'archive_filenames': [_SIZEDIFF_FILENAME],
      'links': links_json,
      'gerrit_plugin_details': binary_size_plugin_json,
  }

  with open(args.results_path, 'w') as f:
    json.dump(results_json, f)


if __name__ == '__main__':
  main()
