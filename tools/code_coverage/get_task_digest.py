#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Extracts CAS isolate digest hash from Buildbucket properties."""

import argparse
import json
import re
import subprocess
import sys


def _query_properties(build_id: str, target: str) -> str | None:
  """Queries bb CLI for CAS digest in a single build.

    Args:
        build_id: Buildbucket build ID string.
        target: Target test suite or compilation output name.

    Returns:
        Hexadecimal CAS digest hash string or None.
    """
  cmd_in = ['bb', 'get', '-json', '-fields', 'input.properties', build_id]
  res = subprocess.run(cmd_in, capture_output=True, text=True, check=False)
  if res.returncode == 0:
    try:
      data = json.loads(res.stdout)
      props = data.get('input', {}).get('properties', {})
      hashes = props.get('swarm_hashes', {})
      if target in hashes and isinstance(hashes[target], str):
        return hashes[target].split('/')[0]
    except json.JSONDecodeError:
      pass

  cmd_out = ['bb', 'get', '-json', '-fields', 'output.properties', build_id]
  res = subprocess.run(cmd_out, capture_output=True, text=True, check=False)
  if res.returncode == 0:
    try:
      data = json.loads(res.stdout)
      props = data.get('output', {}).get('properties', {})
      if target in props and isinstance(props[target], str):
        return props[target].split('/')[0]
      hashes = props.get('swarm_hashes', {})
      if target in hashes and isinstance(hashes[target], str):
        return hashes[target].split('/')[0]
      trig = props.get('swarming_trigger_properties', {})
      trig_hashes = trig.get('swarm_hashes', {})
      if target in trig_hashes and isinstance(trig_hashes[target], str):
        return trig_hashes[target].split('/')[0]
    except json.JSONDecodeError:
      pass

  return None


def get_digest_from_properties(build_id: str, target: str) -> str | None:
  """Queries bb CLI to retrieve CAS digest from build or child compilator.

    Args:
        build_id: Buildbucket build ID string.
        target: Target test suite or compilation output name.

    Returns:
        Hexadecimal CAS digest hash string or None.
    """
  digest = _query_properties(build_id, target)
  if digest:
    return digest

  cmd = ['bb', 'get', '-json', '-fields', 'steps', build_id]
  res = subprocess.run(cmd, capture_output=True, text=True, check=False)
  if res.returncode == 0:
    try:
      data = json.loads(res.stdout)
      for step in data.get('steps', []):
        summary = step.get('summaryMarkdown', '')
        match = re.search(r'\b8\d{18}\b', summary)
        if match:
          return _query_properties(match.group(0), target)
    except json.JSONDecodeError:
      pass

  return None


def main() -> None:
  """Parses CLI args and outputs CAS digest hash to stdout."""
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--build', required=True, help='Buildbucket build ID')
  parser.add_argument('--step', required=True, help='Target suite name')
  args = parser.parse_args()

  digest = get_digest_from_properties(args.build, args.step)
  if not digest:
    print(f'CAS digest for "{args.step}" not found', file=sys.stderr)
    sys.exit(1)

  print(digest)


if __name__ == '__main__':
  main()
