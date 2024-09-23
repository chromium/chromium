#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates several files used by the size trybot to monitor size regressions.

To test locally:
1. Run diagnose_bloat.py to create some entries in out/binary-size-results
2. Run this script with:
HASH1=some hash within out/binary-size-results
HASH2=some hash within out/binary-size-results
mkdir tmp
tools/binary_size/trybot_commit_size_checker.py \
    --author Batman \
    --review-subject "Testing 123" \
    --review-url "https://google.com" \
    --size-config-json-name \
        out/binary-size-build/config/Trichrome_size_config.json \
    --before-dir out/binary-size-results/$HASH1 \
    --after-dir out/binary-size-results/$HASH2 \
    --results-path output.json \
    --staging-dir tmp \
    --local-test
"""

import argparse
import collections
import json
import logging
import os
import pathlib
import re
import sys

sys.path.append(str(pathlib.Path(__file__).parent / 'libsupersize'))
import archive
import diagnose_bloat
import diff
import describe
import dex_disassembly
import file_format
import models
import native_disassembly

_RESOURCE_SIZES_LOG = 'resource_sizes_log'
_RESOURCE_SIZES_64_LOG = 'resource_sizes_64_log'
_MAIN_LOG_NAMES = (_RESOURCE_SIZES_LOG, _RESOURCE_SIZES_64_LOG)
_BASE_RESOURCE_SIZES_LOG = 'base_resource_sizes_log'
_MUTABLE_CONSTANTS_LOG = 'mutable_contstants_log'
_FOR_TESTING_LOG = 'for_test_log'
_DEX_SYMBOLS_LOG = 'dex_symbols_log'
_SIZEDIFF_FILENAME = 'supersize_diff.sizediff'
_HTML_REPORT_URL = (
    'https://chrome-supersize.firebaseapp.com/viewer.html?load_url={{' +
    _SIZEDIFF_FILENAME + '}}')
_MAX_PAK_INCREASE = 1024
_TRYBOT_MD_URL = ('https://chromium.googlesource.com/chromium/src/+/main/docs/'
                  'speed/binary_size/android_binary_size_trybot.md')


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


# See https://crbug.com/1426694
def _MaxSizeIncrease(author, subject):
  if 'AFDO' in subject or 'PGO Profile' in subject:
    return 1024 * 1024
  if 'Update V8' in subject:
    return 100 * 1024
  if 'autoroll' in author:
    return 50 * 1024
  return 16 * 1024


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
  symbols = (
      symbols.WhereInSection('d').WhereNameMatches(r'\bk[A-Z]|\b[A-Z_]+$').
      WhereFullNameMatches('abi:logically_const').Inverted())
  lines, net_added = _SymbolDiffHelper('Mutable Constants', symbols)

  return lines, _SizeDelta('Mutable Constants', 'symbols', 0, net_added)


def _CreateMethodCountDelta(symbols, max_increase):
  symbols = symbols.WhereIsOnDemand(False)
  method_symbols = symbols.WhereInSection(models.SECTION_DEX_METHOD)
  method_lines, net_method_added = _SymbolDiffHelper('Methods', method_symbols)
  class_symbols = symbols.WhereInSection(models.SECTION_DEX).Filter(
      lambda s: not s.IsStringLiteral() and '#' not in s.name)
  class_lines, _ = _SymbolDiffHelper('Classes', class_symbols)
  lines = []
  if class_lines:
    lines.extend(class_lines)
    lines.extend(['', ''])  # empty lines added for clarity
  if method_lines:
    lines.extend(method_lines)

  return lines, _SizeDelta('Dex Methods Count', 'methods', max_increase,
                           net_method_added)


def _CreateResourceSizesDelta(before_dir, after_dir, max_increase):
  sizes_diff = diagnose_bloat.ResourceSizesDiff(
      filename='resource_sizes_32.json')
  sizes_diff.ProduceDiff(before_dir, after_dir)

  return sizes_diff.Summary(), _SizeDelta('Normalized APK Size', 'bytes',
                                          max_increase,
                                          sizes_diff.summary_stat.value)


def _CreateBaseModuleResourceSizesDelta(before_dir, after_dir, max_increase):
  sizes_diff = diagnose_bloat.ResourceSizesDiff(
      filename='resource_sizes_32.json', include_sections=['base'])
  sizes_diff.ProduceDiff(before_dir, after_dir)

  return sizes_diff.DetailedResults(), _SizeDelta(
      'Base Module Size', 'bytes', max_increase,
      sizes_diff.CombinedSizeChangeForSection('base'))


def _CreateResourceSizes64Delta(before_dir, after_dir, max_increase):
  sizes_diff = diagnose_bloat.ResourceSizesDiff(
      filename='resource_sizes_64.json')
  sizes_diff.ProduceDiff(before_dir, after_dir)

  # Allow 4x growth of arm64 before blocking CLs.
  return sizes_diff.Summary(), _SizeDelta('Normalized APK Size (arm64)',
                                          'bytes', max_increase * 4,
                                          sizes_diff.summary_stat.value)


def _CreateSupersizeDiff(before_size_path, after_size_path, review_subject,
                         review_url):
  before = archive.LoadAndPostProcessSizeInfo(before_size_path)
  after = archive.LoadAndPostProcessSizeInfo(after_size_path)
  if review_subject:
    after.build_config[models.BUILD_CONFIG_TITLE] = review_subject
  if review_url:
    after.build_config[models.BUILD_CONFIG_URL] = review_url
  delta_size_info = diff.Diff(before, after, sort=True)

  lines = list(describe.GenerateLines(delta_size_info))
  return lines, delta_size_info


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


def _IsForTestSymbol(value):
  return 'ForTest' in value or 'FOR_TEST' in value


def IterForTestingSymbolsFromMapping(contents):
  current_class_orig = None
  for line in contents.splitlines(keepends=True):
    if line.isspace() or '#' in line:
      continue
    if not line.startswith(' '):
      match = _PROGUARD_CLASS_MAPPING_RE.search(line)
      if match is None:
        raise Exception('Malformed class mapping')
      current_class_orig = match.group('original_name')
      if _IsForTestSymbol(current_class_orig):
        yield current_class_orig
      continue

    assert current_class_orig is not None
    line = line.strip()
    match = _PROGUARD_METHOD_MAPPING_RE.search(line)
    if match:
      method_name = match.group('original_method_name')
      class_name = match.group('original_method_class') or current_class_orig
      if _IsForTestSymbol(method_name) or _IsForTestSymbol(class_name):
        yield f'{class_name}#{method_name}'
      continue

    match = _PROGUARD_FIELD_MAPPING_RE.search(line)
    if match:
      field_name = match.group('original_name')
      if _IsForTestSymbol(field_name) or _IsForTestSymbol(current_class_orig):
        yield f'{current_class_orig}#{field_name}'


def _ExtractForTestingSymbolsFromMappings(mapping_paths):
  symbols = set()
  for mapping_path in mapping_paths:
    with open(mapping_path) as f:
      symbols.update(IterForTestingSymbolsFromMapping(f.read()))
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
    # Give more friendly names to Normalized APK Size metrics.
    name = delta.name
    if log_name == _RESOURCE_SIZES_LOG:
      # The Gerrit plugin looks for this name to put it in the summary.
      name = 'Android Binary Size'
    elif log_name == _RESOURCE_SIZES_64_LOG:
      name = 'Android Binary Size (arm64 high end) (TrichromeLibrary64.apk)'
    listing = {
        'name': name,
        'delta': '{} {}'.format(_FormatNumber(delta.actual), delta.units),
        'limit': '{} {}'.format(_FormatNumber(delta.expected), delta.units),
        'log_name': log_name,
        'allowed': delta.IsAllowable(),
        'large_improvement': delta.IsLargeImprovement(),
    }
    # Always show the Normalized APK Size.
    if log_name in _MAIN_LOG_NAMES or delta.actual != 0:
      binary_size_listings.append(listing)
  binary_size_listings.sort(key=lambda x: x['name'])

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


# TODO(crbug.com/40256106): If missing and file is x32y, return xy; else
# return original filename. Basically allows comparing x_32 targets with x
# targets built under 32bit target_cpu without failing the script due to
# different file names. Remove once migration is complete.
def _UseAlterantiveIfMissing(path):
  if not os.path.isfile(path):
    parent, name = os.path.split(path)
    path = os.path.join(parent, name.replace('32', '', 1))
  return path


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--author', required=True, help='CL author')
  parser.add_argument('--review-subject', help='Review subject')
  parser.add_argument('--review-url', help='Review URL')
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
  parser.add_argument(
      '--local-test',
      action='store_true',
      help='Allow input directories to be diagnose_bloat.py ones.')
  args = parser.parse_args()

  logging.basicConfig(level=logging.INFO,
                      format='%(levelname).1s %(relativeCreated)6d %(message)s')

  before_path = pathlib.Path(args.before_dir)
  after_path = pathlib.Path(args.after_dir)

  before_path_resolver = lambda p: str(before_path / os.path.basename(p))
  after_path_resolver = lambda p: str(after_path / os.path.basename(p))

  if args.local_test:
    config_path = args.size_config_json_name
  else:
    config_path = after_path_resolver(args.size_config_json_name)

  with open(config_path, 'rt') as fh:
    config = json.load(fh)

  if args.local_test:
    size_filename = 'Trichrome.minimal.apks.size'
  else:
    size_filename = config['supersize_input_file'] + '.size'

  before_mapping_paths = [
      _UseAlterantiveIfMissing(before_path_resolver(f))
      for f in config['mapping_files']
  ]
  after_mapping_paths = [
      _UseAlterantiveIfMissing(after_path_resolver(f))
      for f in config['mapping_files']
  ]

  max_size_increase = _MaxSizeIncrease(args.author, args.review_subject)
  # We do not care as much about method count anymore, so this limit is set
  # such that it is very unlikely to be hit.
  max_methods_increase = 200 if '-autoroll' not in args.author else 800

  logging.info('Creating Supersize diff')
  supersize_diff_lines, delta_size_info = _CreateSupersizeDiff(
      _UseAlterantiveIfMissing(before_path_resolver(size_filename)),
      _UseAlterantiveIfMissing(after_path_resolver(size_filename)),
      args.review_subject, args.review_url)

  changed_symbols = delta_size_info.raw_symbols.WhereDiffStatusIs(
      models.DIFF_STATUS_UNCHANGED).Inverted()

  logging.info('Checking dex symbols')
  dex_delta_lines, dex_delta = _CreateMethodCountDelta(changed_symbols,
                                                       max_methods_increase)
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
  testing_symbols_lines, test_symbols_delta = _CreateTestingSymbolsDeltas(
      before_mapping_paths, after_mapping_paths)
  size_deltas.add(test_symbols_delta)
  metrics.add((test_symbols_delta, _FOR_TESTING_LOG))

  # Check for uncompressed .pak file entries being added to avoid unnecessary
  # bloat.
  logging.info('Checking pak symbols')
  size_deltas.update(_CreateUncompressedPakSizeDeltas(changed_symbols))

  # Normalized APK Size is the main metric we use to monitor binary size.
  logging.info('Creating sizes diff')
  resource_sizes_lines, resource_sizes_delta = (_CreateResourceSizesDelta(
      args.before_dir, args.after_dir, max_size_increase))
  size_deltas.add(resource_sizes_delta)
  metrics.add((resource_sizes_delta, _RESOURCE_SIZES_LOG))

  logging.info('Creating base module sizes diff')
  base_resource_sizes_lines, base_resource_sizes_delta = (
      _CreateBaseModuleResourceSizesDelta(args.before_dir, args.after_dir,
                                          max_size_increase))
  size_deltas.add(base_resource_sizes_delta)
  metrics.add((base_resource_sizes_delta, _BASE_RESOURCE_SIZES_LOG))

  config_64 = config.get('to_resource_sizes_py_64')
  if config_64:
    logging.info('Creating 64-bit sizes diff')
    resource_sizes_64_lines, resource_sizes_64_delta = (
        _CreateResourceSizes64Delta(args.before_dir, args.after_dir,
                                    max_size_increase))
    size_deltas.add(resource_sizes_64_delta)
    metrics.add((resource_sizes_64_delta, _RESOURCE_SIZES_64_LOG))

  logging.info('Adding disassembly to dex symbols')
  dex_disassembly.AddDisassembly(delta_size_info, before_path_resolver,
                                 after_path_resolver)
  logging.info('Adding disassembly to native symbols')
  native_disassembly.AddDisassembly(delta_size_info, before_path_resolver,
                                    after_path_resolver)

  # .sizediff can be consumed by the html viewer.
  logging.info('Creating HTML Report')
  sizediff_path = os.path.join(args.staging_dir, _SIZEDIFF_FILENAME)
  file_format.SaveDeltaSizeInfo(delta_size_info, sizediff_path)

  passing_deltas = set(d for d in size_deltas if d.IsAllowable())
  failing_deltas = size_deltas - passing_deltas

  failing_checks_text = '\n'.join(d.explanation for d in sorted(failing_deltas))
  passing_checks_text = '\n'.join(d.explanation for d in sorted(passing_deltas))
  checks_text = """\
FAILING Checks:
{}

PASSING Checks:
{}

To understand what those checks are and how to pass them, see:
{}

""".format(failing_checks_text, passing_checks_text, _TRYBOT_MD_URL)

  status_code = int(bool(failing_deltas))
  see_docs_lines = ['\n', f'For more details: {_TRYBOT_MD_URL}\n']

  summary = '<br>' + checks_text.replace('\n', '<br>')
  links_json = [
      {
          'name': 'Binary Size Details (arm32)',
          'lines': resource_sizes_lines + see_docs_lines,
          'log_name': _RESOURCE_SIZES_LOG,
      },
      {
          'name': 'Base Module Binary Size Details',
          'lines': base_resource_sizes_lines + see_docs_lines,
          'log_name': _BASE_RESOURCE_SIZES_LOG,
      },
      {
          'name': 'Mutable Constants Diff',
          'lines': mutable_constants_lines + see_docs_lines,
          'log_name': _MUTABLE_CONSTANTS_LOG,
      },
      {
          'name': 'ForTest Symbols Diff',
          'lines': testing_symbols_lines + see_docs_lines,
          'log_name': _FOR_TESTING_LOG,
      },
      {
          'name': 'Dex Class and Method Diff',
          'lines': dex_delta_lines + see_docs_lines,
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
  if config_64:
    links_json[2:2] = [
        {
            'name': 'Binary Size Details (arm64)',
            'lines': resource_sizes_64_lines + see_docs_lines,
            'log_name': _RESOURCE_SIZES_64_LOG,
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
