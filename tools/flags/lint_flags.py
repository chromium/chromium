#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Lint `chrome://flags` source code.

These checks aren't run as `AboutFlagsTest.*` because they require access to the
original source code.
"""

import pathlib
import re
import sys

import flags_utils

_ROOT_PATH = pathlib.Path(__file__).parents[2]


def find_unused_flags_in_metadata(
  root_path: pathlib.Path = _ROOT_PATH,
) -> set[str]:
  metadata = flags_utils.load_metadata(root_path=root_path)
  unused_flags = {entry['name'] for entry in metadata}
  # Skip a few permanent debugging flags whose internal names are provided by
  # constants in `**/*switches.{h,cc}` (mostly unrelated to flags).
  unused_flags -= {'enable-benchmarking', 'enable-ui-devtools'}
  internal_name_pattern = re.compile(r'"(?P<maybe_name>\w+(-\w+)*)"')
  for about_flags_path in [
    root_path / 'chrome' / 'browser' / 'about_flags.cc',
    root_path / 'ios' / 'chrome' / 'browser' / 'flags' / 'about_flags.mm',
    # Check other files that are known to define internal flag name constants.
    # Since most names are inlined in `about_flags.{cc,mm}`, this list is not
    # expected to grow.
    root_path / 'chrome' / 'browser' / 'site_isolation' / 'about_flags.h',
  ]:
    with about_flags_path.open(encoding='utf-8') as about_flags_file:
      # `about_flags.cc` is huge, so only make a single pass.
      for line in about_flags_file:
        for match in internal_name_pattern.finditer(line):
          unused_flags.discard(match['maybe_name'])
  return unused_flags


def main() -> int:
  if unused_flags := find_unused_flags_in_metadata():
    print(
      '`//chrome/browser/flag-metadata.json` appears to contain '
      'entries not used in `about_flags.cc` or `about_flags.mm`:'
    )
    for flag in sorted(unused_flags):
      print(f'  - {flag}')
    return 1
  return 0


if __name__ == '__main__':
  sys.exit(main())
