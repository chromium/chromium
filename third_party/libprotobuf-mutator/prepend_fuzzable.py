#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import re
import sys

def main():
  parser = argparse.ArgumentParser(
      description="Prepend 'fuzzable.' to the package name in a proto file."
  )
  parser.add_argument("--infile", required=True, help="Input proto file")
  parser.add_argument("--outfile", required=True, help="Output proto file")
  args = parser.parse_args()

  with open(args.infile, "r", encoding="utf-8") as f:
    content = f.read()

  # Find package line, e.g., package mc_fuzzer; or package some.nested.package;
  package_regex = re.compile(
      r"^(?P<prefix>\s*package\s+)"
      r"(?P<name>[a-zA-Z0-9_.]+)"
      r"(?P<suffix>.*?;)",
      re.MULTILINE
  )

  new_content, count = package_regex.subn(
      r"\g<prefix>fuzzable.\g<name>\g<suffix>", content
  )

  # TODO(crbug.com/505034799): Add support for protos without a package name.
  assert count > 0, "Protos without package names are not supported yet."

  with open(args.outfile, "w", encoding="utf-8") as f:
    f.write(new_content)

if __name__ == "__main__":
  sys.exit(main())
