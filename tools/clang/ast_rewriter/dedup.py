# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

###
# The ASTRewriter plugin emits substitution directives independently for each
# TU. This means there will be several duplicates (e.g. in headers that are
# included in multiple source files). The format used for paths is also
# inconsistent.
#
# This is a general-purpose post-processing script that can deduplicate,
# filter edits based on path, add user headers if not already present, etc.
# It also adds the begin/end tags when writing out the edits.
#
# usage: `python3 dedup.py directives.txt`
###

### Configurable options
# List of headers to add to each modified file
headers_to_add = ["base/strings/to_string.h"]
# List of paths we do/don't want to replace in
paths_to_exclude = ["third_party"]
paths_to_include = ["/components/", "/content/", "/chrome/"]


# Paths we don't want to process
def filter_path(path):
  """
  Examine a path and return true if we want to filter it out,
  e.g. because it's in third_party. Feel free to customize the logic.
  """
  if (any(exclude in path for exclude in paths_to_exclude)):
    return True

  if (not any(include in path for include in paths_to_include)):
    return True

  return False


### Actual work
def ProcessFile(filename, deduped_contents, unique_paths):
  """ Read every replacement in a file, normalizing paths and removing
      duplicates, as well as any paths we choose to filter out. Keep track
      of all unique paths we see so we know which files to add headers to.

      filename: the name of the file to be processed
      deduped_contents: the set of replacements we've already processed
      unique_paths: the set of unique replacement paths we've seen.
  """
  with open(filename) as f:
    for line in f.readlines():
      parts = line.split(":::")
      if len(parts) < 2:
        print("Skipping unexpected line: ", line)
        continue
      path = os.path.normpath(parts[1])
      if filter_path(path):
        continue

      if path not in unique_paths:
        unique_paths.add(path)

      parts[1] = path
      new_line = ":::".join(parts)
      if new_line not in deduped_contents:
        deduped_contents.add(new_line)


def DedupFiles(filenames):
  deduped_contents = set()
  unique_paths = set()

  for file in filenames:
    ProcessFile(file, deduped_contents, unique_paths)

  # This may not be necessary if the tool already emits these directives,
  # but sometimes that may be inconvenient.
  for path in unique_paths:
    for header in headers_to_add:
      deduped_contents.add(
          f"include-user-header:::{path}:::-1:::-1:::{header}\n")

  output_file = "deduped.txt"
  WriteFile(output_file, sorted(deduped_contents))


def WriteFile(outfile, lines):
  with open(outfile, "w") as f:
    f.write("==== BEGIN EDITS ====\n")
    f.write("".join(lines))
    f.write("==== END EDITS ====\n")


if __name__ == "__main__":
  DedupFiles(sys.argv[1:])
