#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates several files used by the size trybot to monitor size regressions."""

import argparse
import collections
import json
import logging
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), 'libsupersize'))
import archive
import diagnose_bloat
import diff
import describe
import html_report
import models

_NDJSON_FILENAME = 'supersize_diff.ndjson'
_TEXT_FILENAME = 'supersize_diff.txt'
_HTML_REPORT_BASE_URL = (
    'https://storage.googleapis.com/chrome-supersize/viewer.html?load_url=')
_MAX_DEX_METHOD_COUNT_INCREASE = 50
_MAX_NORMALIZED_INCREASE = 16 * 1024
_MAX_PAK_INCREASE = 1024


class _SizeDelta(collections.namedtuple(
    'SizeDelta', ['name', 'units', 'expected', 'actual'])):

  @property
  def explanation(self):
    ret = '{}: {} {} (max is {} {})'.format(
        self.name, self.actual, self.units, self.expected, self.units)
    return ret

  def IsAllowable(self):
    return self.actual <= self.expected

  def __cmp__(self, other):
    return cmp(self.name, other.name)


def _SymbolDiffHelper(symbols):
  added = symbols.WhereDiffStatusIs(models.DIFF_STATUS_ADDED)
  removed = symbols.WhereDiffStatusIs(models.DIFF_STATUS_REMOVED)
  both = (added + removed).SortedByName()
  lines = None
  if len(both) > 0:
    lines = [
        'Added: {}'.format(len(added)),
        'Removed: {}'.format(len(removed)),
    ]
    lines.extend(describe.GenerateLines(both, summarize=False))

  return lines, len(added) - len(removed)


def _CreateMutableConstantsDelta(symbols):
  symbols = symbols.WhereInSection('d').WhereNameMatches(r'\bk[A-Z]|\b[A-Z_]+$')
  lines, net_added = _SymbolDiffHelper(symbols)

  return lines, _SizeDelta('Mutable Constants', 'symbols', 0, net_added)


def _CreateMethodCountDelta(symbols):
  method_symbols = symbols.WhereInSection(models.SECTION_DEX_METHOD)
  method_lines, net_method_added = _SymbolDiffHelper(method_symbols)
  class_symbols = symbols.WhereInSection(
      models.SECTION_DEX).WhereNameMatches('#').Inverted()
  class_lines, _ = _SymbolDiffHelper(class_symbols)
  lines = []
  if class_lines:
    lines.append('===== Classes Added & Removed =====')
    lines.extend(class_lines)
    lines.extend(['', ''])  # empty lines added for clarity
  if method_lines:
    lines.append('===== Methods Added & Removed =====')
    lines.extend(method_lines)

  return lines, _SizeDelta('Dex Methods Count', 'methods',
                           _MAX_DEX_METHOD_COUNT_INCREASE, net_method_added)


def _CreateResourceSizesDelta(apk_name, before_dir, after_dir):
  sizes_diff = diagnose_bloat.ResourceSizesDiff(apk_name)
  sizes_diff.ProduceDiff(before_dir, after_dir)

  return sizes_diff.Summary(), _SizeDelta(
      'Normalized APK Size', 'bytes', _MAX_NORMALIZED_INCREASE,
      sizes_diff.summary_stat.value)


def _CreateSupersizeDiff(apk_name, before_dir, after_dir):
  before_size_path = os.path.join(before_dir, apk_name + '.size')
  after_size_path = os.path.join(after_dir, apk_name + '.size')
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


def _CreateTestingSymbolsDeltas(symbols):
  testing_symbols = symbols.WhereIsDex().WhereNameMatches(
      'ForTest').WhereDiffStatusIs(models.DIFF_STATUS_ADDED)
  lines = None
  if len(testing_symbols):
    lines = list(describe.GenerateLines(testing_symbols, summarize=False))
  return lines, _SizeDelta('Added symbols named "ForTest"', 'symbols', 0,
                           len(testing_symbols))


def _FormatSign(number):
  if number > 0:
    return '+{}'.format(number)
  return '{}'.format(number)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--author', required=True, help='CL author')
  parser.add_argument(
      '--apk-name', required=True, help='Name of the apk (ex. Name.apk)')
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

  logging.info('Creating Supersize diff')
  supersize_diff_lines, delta_size_info = _CreateSupersizeDiff(
      args.apk_name, args.before_dir, args.after_dir)
  supersize_text_path = os.path.join(args.staging_dir, _TEXT_FILENAME)
  with open(supersize_text_path, 'w') as f:
    describe.WriteLines(supersize_diff_lines, f.write)

  changed_symbols = delta_size_info.raw_symbols.WhereDiffStatusIs(
      models.DIFF_STATUS_UNCHANGED).Inverted()

  # Monitor dex method count since the "multidex limit" is a thing.
  logging.info('Checking dex symbols')
  dex_delta_lines, dex_delta = _CreateMethodCountDelta(changed_symbols)
  size_deltas = {dex_delta}

  # Look for native symbols called "kConstant" that are not actually constants.
  # C++ syntax makes this an easy mistake, and having symbols in .data uses more
  # RAM than symbols in .rodata (at least for multi-process apps).
  logging.info('Checking for mutable constants in native symbols')
  mutable_constants_lines, mutable_constants_delta = (
      _CreateMutableConstantsDelta(changed_symbols))
  size_deltas.add(mutable_constants_delta)

  # Look for symbols with 'ForTesting' in their name.
  logging.info('Checking for symbols named "ForTest"')
  testing_symbols_lines, test_symbols_delta = (
      _CreateTestingSymbolsDeltas(changed_symbols))
  size_deltas.add(test_symbols_delta)

  # Check for uncompressed .pak file entries being added to avoid unnecessary
  # bloat.
  logging.info('Checking pak symbols')
  size_deltas.update(_CreateUncompressedPakSizeDeltas(changed_symbols))

  # Normalized APK Size is the main metric we use to monitor binary size.
  logging.info('Creating sizes diff')
  resource_sizes_lines, resource_sizes_delta = (
      _CreateResourceSizesDelta(args.apk_name, args.before_dir, args.after_dir))
  size_deltas.add(resource_sizes_delta)

  # .ndjson can be consumed by the html viewer.
  logging.info('Creating HTML Report')
  ndjson_path = os.path.join(args.staging_dir, _NDJSON_FILENAME)
  html_report.BuildReportFromSizeInfo(ndjson_path, delta_size_info)

  passing_deltas = set(m for m in size_deltas if m.IsAllowable())
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
          'name': '>>> Binary Size Details <<<',
          'lines': resource_sizes_lines,
      },
      {
          'name': '>>> Mutable Constants Diff <<<',
          'lines': mutable_constants_lines,
      },
      {
          'name': '>>> "ForTest" Symbols Diff <<<',
          'lines': testing_symbols_lines,
      },
      {
          'name': '>>> Dex Class and Method Diff <<<',
          'lines': dex_delta_lines,
      },
      {
          'name': '>>> SuperSize Text Diff <<<',
          'url': '{{' + _TEXT_FILENAME + '}}',
      },
      {
          'name': '>>> SuperSize HTML Diff <<<',
          'url': _HTML_REPORT_BASE_URL + '{{' + _NDJSON_FILENAME + '}}',
      },
  ]
  # Remove empty diffs (Mutable Constants, Dex Method, ...).
  links_json = [o for o in links_json if o.get('lines') or o.get('url')]

  binary_size_listings = []
  for delta in size_deltas:
    if delta.actual == 0:
      continue
    listing = {
        'name': delta.name,
        'delta': '{} {}'.format(_FormatSign(delta.actual), delta.units),
        'limit': '{} {}'.format(_FormatSign(delta.expected), delta.units),
        'allowed': delta.IsAllowable(),
    }
    binary_size_listings.append(listing)

  binary_size_extras = [
      {
          'text': 'SuperSize HTML Diff',
          'url': _HTML_REPORT_BASE_URL + '{{' + _NDJSON_FILENAME + '}}',
      },
      {
          'text': 'SuperSize Text Diff',
          'url': '{{' + _TEXT_FILENAME + '}}',
      },
  ]

  binary_size_plugin_json = {
      'listings': binary_size_listings,
      'extras': binary_size_extras,
  }

  results_json = {
      'status_code': status_code,
      'summary': summary,
      'archive_filenames': [_NDJSON_FILENAME, _TEXT_FILENAME],
      'links': links_json,
      'gerrit_plugin_details': binary_size_plugin_json,
  }

  with open(args.results_path, 'w') as f:
    json.dump(results_json, f)


if __name__ == '__main__':
  main()
