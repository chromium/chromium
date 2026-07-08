#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Extracts host, project, change, and patchset from a Gerrit CL URL."""

import argparse
import json
import re
import sys
import urllib.parse


def parse_gerrit_url(url: str) -> dict[str, str | int]:
  """Parses a Gerrit CL URL into host, project, change, and patchset.

    Args:
        url: Absolute Gerrit CL URL string.

    Returns:
        Dictionary containing host, project, change (int), and patchset (int).
    """
  parsed = urllib.parse.urlparse(url)
  host = parsed.netloc
  path = parsed.path
  if not host or not path:
    raise ValueError(f'Invalid Gerrit URL: {url}')

  match = re.match(r'^/c/(.+?)/\+/(\d+)(?:/(\d+))?', path)
  if not match:
    raise ValueError(f'Could not extract CL details from path: {path}')

  project = match.group(1)
  change = int(match.group(2))
  patchset = int(match.group(3)) if match.group(3) else 1

  return {
      'host': host,
      'project': project,
      'change': change,
      'patchset': patchset,
  }


def main() -> None:
  """Parses CLI arguments and prints Gerrit CL details as JSON."""
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('url', help='Gerrit CL URL')
  args = parser.parse_args()

  try:
    details = parse_gerrit_url(args.url)
    print(json.dumps(details, indent=2))
  except ValueError as e:
    print(str(e), file=sys.stderr)
    sys.exit(1)


if __name__ == '__main__':
  main()
