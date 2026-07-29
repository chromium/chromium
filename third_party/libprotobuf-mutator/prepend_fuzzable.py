#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import re
import shlex
import sys

def process_file(infile, outfile):
  with open(infile, "r", encoding="utf-8") as f:
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

  if count == 0:
    syntax_regex = re.compile(
        r"""
        ^
        (?P<syntax_line>
            \s*                 # Leading whitespace
            (?:syntax|edition)  # 'syntax' or 'edition'
            \s*=\s*             # Equals sign with surrounding whitespace
            ["'][^"']+["']      # Value in double or single quotes e.g. "proto3"
            \s*;                # Semicolon with leading whitespace
        )                       # End capture group
        """,
        re.MULTILINE | re.VERBOSE
    )

    new_content, syntax_count = syntax_regex.subn(
        r"\g<syntax_line>\n\npackage fuzzable;", content, count=1
    )

    if syntax_count == 0:
      new_content = "package fuzzable;\n\n" + content


  import_regex = re.compile(
      r'^(?P<prefix>\s*import\s+(?:public\s+|weak\s+)?")'
      r'(?P<path>.*?)'
      r'\.proto(?P<suffix>";)',
      re.MULTILINE
  )

  def import_replacement(match_obj):
    path = match_obj.group("path")
    if path.startswith("google/protobuf/"):
      return match_obj.group(0)
    return f'{match_obj.group("prefix")}{path}_fuzzable.proto{match_obj.group("suffix")}'

  new_content = import_regex.sub(import_replacement, new_content)

  # Rewrite absolute type references (e.g., .reporting.Record -> .fuzzable.reporting.Record).
  abs_ref_regex = re.compile(
      r"""
      (?<![\w.)\]])     # Must NOT be preceded by a letter, number, dot, ), or ]
      \.                # The literal leading dot of the absolute reference
      (?P<type_path>    # Captured type_path.
          [a-zA-Z_]     # Must start with a letter or underscore (prevents matching floats like .5)
          [\w.]*        # Followed by any number of word characters or dots
      )                 # End named capture group
      """,
      re.VERBOSE,
  )
  # Do not modify well-known system packages.
  ignored_packages = ("google.protobuf.", "google.rpc.", "google.type.")

  def abs_ref_replacement(match_obj):
    type_path = match_obj.group("type_path")
    if type_path.startswith(ignored_packages):
      return match_obj.group(0)
    return f".fuzzable.{type_path}"

  new_content = abs_ref_regex.sub(abs_ref_replacement, new_content)

  with open(outfile, "w", encoding="utf-8") as f:
    f.write(new_content)


def main():
  parser = argparse.ArgumentParser(
      description="Prepend 'fuzzable.' to the package name in a proto file."
  )
  parser.add_argument(
      "--file-list",
      required=True,
      help="Response file containing list of input/output pairs on separate lines"
  )
  args = parser.parse_args()

  with open(args.file_list, "r", encoding="utf-8") as f:
    content = f.read()

  parts = shlex.split(content)
  if len(parts) % 2 != 0:
    raise RuntimeError("File list must have an even number of paths")

  for i in range(0, len(parts), 2):
    process_file(parts[i], parts[i + 1])

if __name__ == "__main__":
  sys.exit(main())
