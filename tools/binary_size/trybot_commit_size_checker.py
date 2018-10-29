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

_MAX_DEX_METHOD_COUNT_INCREASE = 50
_MAX_NORMALIZED_INCREASE = 16 * 1024
_MAX_PAK_INCREASE = 1024

_NORMALIZED_APK_SIZE_DETAILS = (
    'See https://chromium.googlesource.com/chromium/src/+/master/docs/speed/'
    'binary_size/metrics.md#Normalized-APK-Size '
    'for an explanation of Normalized APK Size')

_FAILURE_GUIDANCE = """
Please look at the symbol diffs from the "Show Resource Sizes Diff",
"Show Supersize Diff", and "Dex Method Count", and "Supersize HTML Report" bot
steps. Try and understand the growth and see if it can be mitigated.

There is guidance at:

https://chromium.googlesource.com/chromium/src/+/master/docs/speed/apk_size_regressions.md#Debugging-Apk-Size-Increase

If the growth is expected / justified, then you can bypass this bot failure by
adding "Binary-Size: $JUSTIFICATION" to your commit description. Here are some
examples:

Binary-Size: Increase is due to translations and so cannot be avoided.
Binary-Size: Increase is due to new images, which are already optimally encoded.
Binary-Size: Increase is temporary due to a "new way" / "old way" refactoring.
    It should go away once the "old way" is removed.
Binary-Size: Increase is temporary and will be reverted before next branch cut.
Binary-Size: Increase needed to reduce RAM of a common user flow.
Binary-Size: Increase needed to reduce runtime of a common user flow.
Binary-Size: Increase needed to implement a feature, and I've already spent a
    non-trivial amount of time trying to reduce its size.
"""


class _SizeDelta(collections.namedtuple(
    'SizeDelta', ['name', 'units', 'expected', 'actual', 'details'])):

  @property
  def explanation(self):
    return '{}: expected max: {} {}, got {} {}\n{}'.format(
        self.name, self.expected, self.units, self.actual, self.units,
        self.details)

  def _IsAllowable(self):
    return self.actual <= self.expected

  def __cmp__(self, other):
    return cmp(self.name, other.name)


def _CreateAndWriteMethodCountDelta(symbols, output_path):
  dex_symbols = symbols.WhereInSection(models.SECTION_DEX_METHOD)
  dex_added = dex_symbols.WhereDiffStatusIs(models.DIFF_STATUS_ADDED)
  dex_removed = dex_symbols.WhereDiffStatusIs(models.DIFF_STATUS_REMOVED)
  dex_added_count, dex_removed_count = len(dex_added), len(dex_removed)
  dex_net_added = dex_added_count - dex_removed_count

  dex_expl = 'Added: {}\n{}\n\nRemoved: {}\n{}'.format(
      dex_added_count,
      '\n'.join(s.name for s in dex_added.SortedByName()),
      dex_removed_count,
      '\n'.join(s.name for s in dex_removed.SortedByName()))

  with open(output_path, 'w') as f:
    f.writelines(dex_expl)

  return _SizeDelta('Dex Methods', 'methods', _MAX_DEX_METHOD_COUNT_INCREASE,
                    dex_net_added, dex_expl)


def _CreateAndWriteResourceSizesDelta(apk_name, before_dir, after_dir,
                                     output_path):
  sizes_diff = diagnose_bloat.ResourceSizesDiff(apk_name)
  sizes_diff.ProduceDiff(before_dir, after_dir)

  with open(output_path, 'w') as f:
    f.writelines(l + '\n' for l in sizes_diff.Summary())

  return _SizeDelta('Normalized APK Size', 'bytes', _MAX_NORMALIZED_INCREASE,
                    sizes_diff.summary_stat.value, _NORMALIZED_APK_SIZE_DETAILS)


def _CreateAndWriteSupersizeDiff(apk_name, before_dir, after_dir, output_path):
  before_size_path = os.path.join(before_dir, apk_name + '.size')
  after_size_path = os.path.join(after_dir, apk_name + '.size')
  before = archive.LoadAndPostProcessSizeInfo(before_size_path)
  after = archive.LoadAndPostProcessSizeInfo(after_size_path)
  size_info_delta = diff.Diff(before, after, sort=True)

  with open(output_path, 'w') as f:
    f.writelines(l + '\n' for l in describe.GenerateLines(size_info_delta))

  return size_info_delta


def _CreateUncompressedPakSizeDeltas(symbols):
  pak_symbols = symbols.Filter(lambda s:
      s.size > 0 and
      bool(s.flags & models.FLAG_UNCOMPRESSED) and
      s.section_name == models.SECTION_PAK_NONTRANSLATED)
  return [_SizeDelta('Uncompressed Pak Entry', 'bytes', _MAX_PAK_INCREASE,
                     pak.after_symbol.size, pak.full_name)
          for pak in pak_symbols]


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--author', required=True, help='CL author')
  parser.add_argument('--apk-name', required=True,
                      help='Name of the apk (ex. Name.apk)')
  parser.add_argument('--before-dir', required=True,
                      help='Directory containing the APK from reference build.')
  parser.add_argument('--after-dir', required=True,
                      help='Directory containing APK for the new build.')
  parser.add_argument('--resource-sizes-diff-path', required=True,
                      help='Output path for the resource_sizes.py diff.')
  parser.add_argument('--supersize-diff-path', required=True,
                      help='Output path for the Supersize diff.')
  parser.add_argument('--dex-method-count-diff-path', required=True,
                      help='Output path for the dex method count diff.')
  parser.add_argument('--ndjson-path', required=True,
                      help='Output path for the Supersize HTML report.')
  parser.add_argument('--results-path', required=True,
                      help='Output path for the trybot result .json file.')
  parser.add_argument('-v', '--verbose', action='store_true')
  args = parser.parse_args()

  if args.verbose:
    logging.basicConfig(level=logging.INFO)

  logging.info('Creating Supersize diff')
  delta_size_info = _CreateAndWriteSupersizeDiff(
      args.apk_name, args.before_dir, args.after_dir, args.supersize_diff_path)
  changed_symbols = delta_size_info.raw_symbols.WhereDiffStatusIs(
      models.DIFF_STATUS_UNCHANGED).Inverted()

  # Monitor dex method growth since this correlates closely with APK size and
  # may affect our dex file structure.
  logging.info('Checking dex symbols')
  size_deltas = set()
  size_deltas.add(
      _CreateAndWriteMethodCountDelta(
          changed_symbols, args.dex_method_count_diff_path))

  # Check for uncompressed .pak file entries being added to avoid unnecessary
  # bloat.
  logging.info('Checking pak symbols')
  size_deltas.update(_CreateUncompressedPakSizeDeltas(changed_symbols))

  # Normalized APK Size is the main metric we use to monitor binary size.
  logging.info('Creating sizes diff')
  resource_sizes_delta = _CreateAndWriteResourceSizesDelta(
          args.apk_name, args.before_dir, args.after_dir,
          args.resource_sizes_diff_path)
  size_deltas.add(resource_sizes_delta)

  # .ndjson can be consumed by the html viewer.
  logging.info('Creating HTML Report')
  html_report.BuildReportFromSizeInfo(
      args.ndjson_path, delta_size_info, all_symbols=True)

  is_roller = '-autoroll' in args.author
  # Useful for bot debugging to have these printed out:
  print 'Is Roller:', is_roller
  for delta in sorted(size_deltas):
    print '{}\n\n'.format(delta.explanation)

  passing_deltas = set(m for m in size_deltas if m._IsAllowable())
  failing_deltas = size_deltas - passing_deltas

  status_code = 0 if is_roller else len(failing_deltas)
  result = 'passed' if status_code == 0 else 'failed'
  message = """

Binary size checks {}.

*******************************************************************************
FAILING:

{}

*******************************************************************************

PASSING:

{}

*******************************************************************************

""".format(result,
           '\n\n'.join(d.explanation for d in sorted(failing_deltas)),
           '\n\n'.join(d.explanation for d in sorted(passing_deltas)))

  if status_code != 0:
    message += _FAILURE_GUIDANCE

  # Make blank lines not blank prevent them from being stripped.
  # https://crbug.com/855671
  message.replace('\n\n', '\n.\n')
  with open(args.results_path, 'w') as f:
    results_json = {
        'details': message,
        'normalized_apk_size': resource_sizes_delta.actual,
        'status_code': status_code
    }
    json.dump(results_json, f)


if __name__ == '__main__':
  main()
