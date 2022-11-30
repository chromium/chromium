# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import fnmatch
import os
import re
import sys

# List of CSS properties to be removed.
CSS_PROPERTIES_TO_REMOVE = [
  '-moz-appearance',
  '-moz-box-sizing',
  '-moz-flex-basis',
  '-moz-user-select',

  '-ms-align-content',
  '-ms-align-self',
  '-ms-flex',
  '-ms-flex-align',
  '-ms-flex-basis',
  '-ms-flex-line-pack',
  '-ms-flexbox',
  '-ms-flex-direction',
  '-ms-flex-pack',
  '-ms-flex-wrap',
  '-ms-inline-flexbox',
  '-ms-user-select',

  '-webkit-align-content',
  '-webkit-align-items',
  '-webkit-align-self',
  '-webkit-animation',
  '-webkit-animation-duration',
  '-webkit-animation-iteration-count',
  '-webkit-animation-name',
  '-webkit-animation-timing-function',
  '-webkit-flex',
  '-webkit-flex-basis',
  '-webkit-flex-direction',
  '-webkit-flex-wrap',
  '-webkit-inline-flex',
  '-webkit-justify-content',
  '-webkit-transform',
  '-webkit-transform-origin',
  '-webkit-transition',
  '-webkit-transition-delay',
  '-webkit-transition-property',
  '-webkit-user-select',
]


# Regex to detect a CSS line of interest (helps avoiding edge cases, like
# removing the 1st line of a multi-line CSS rule).
CSS_LINE_REGEX = '^\s*[^;\s]+:\s*[^;]+;\s*(/\*.+/*/)*\s*$';


def ProcessFile(filename):
  # Gather indices of lines to be removed.
  indices_to_remove = [];
  with open(filename) as f:
    lines = f.readlines()
    for i, line in enumerate(lines):
      if ShouldRemoveLine(line):
        indices_to_remove.append(i)

  if len(indices_to_remove):
    print('stripping CSS from: ' + filename)

  # Process line numbers in descinding order, such that the array can be
  # modified in-place.
  indices_to_remove.reverse()
  for i in indices_to_remove:
    del lines[i]

  # Reconstruct file.
  with open(filename, 'w') as f:
    for l in lines:
      f.write(l)
  return


def ShouldRemoveLine(line):
  pred = lambda p: re.search(CSS_LINE_REGEX, line) and re.search(p, line)
  return any(pred(p) for p in CSS_PROPERTIES_TO_REMOVE)


def main(argv):
  parser = argparse.ArgumentParser('Strips CSS rules not needed by Chrome')
  parser.add_argument(
      '--file_extension', choices=['js', 'html'], required=True)
  opts = parser.parse_args(sys.argv[1:])

  files_to_process = [os.path.join(dirpath, f)
    for dirpath, dirnames, files in os.walk('components-chromium')
    for f in fnmatch.filter(files, '*.' + opts.file_extension)]

  for f in files_to_process:
    ProcessFile(f)


if __name__ == '__main__':
  main(sys.argv[1:])
