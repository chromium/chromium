#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import urllib.request
import sys

_METADATA_URL = 'https://fonts.google.com/metadata/icons?lod=MINIMAL'


def main():
  """Updates tools/resources/icon_list.json from the Google Symbols API.
  """
  try:
    print(f'Fetching icon metadata from {_METADATA_URL}...')
    with urllib.request.urlopen(_METADATA_URL, timeout=10) as f:
      content = f.read().decode('utf-8')
      # Strip XSS protection prefix
      if content.startswith(")]}'"):
        content = content[4:]
      data = json.loads(content)

    assert 'icons' in data, 'Invalid JSON format (missing "icons" key).'

    script_dir = os.path.dirname(os.path.abspath(__file__))
    output_path = os.path.join(script_dir, 'icon_list.json')

    with open(output_path, 'w', encoding='utf-8') as out:
      json.dump(data, out, indent=2, sort_keys=True)

    print(
        f'Successfully updated {output_path} with {len(data["icons"])} icons.')
    return 0

  except Exception as e:
    print(f'Error: {e}')
    return 1


if __name__ == '__main__':
  sys.exit(main())
