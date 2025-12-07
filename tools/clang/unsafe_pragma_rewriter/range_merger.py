#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def start_point(ranges):
  line1, col1, _, _ = ranges
  return (line1, col1)


def end_point(ranges):
  _, _, line2, col2 = ranges
  return (line2, col2)


def combine(start, end):
  line1, col1, _, _ = start
  _, _, line2, col2 = end
  return (line1, col1, line2, col2)


def merge_ranges(ranges):
  if not ranges:
    return []

  ranges.sort()
  result = ranges[:1]
  for range in ranges[1:]:
    current = result[-1]
    if start_point(range) <= end_point(current):
      if end_point(range) > end_point(current):
        result[-1] = combine(current, range)
    else:
      result.append(range)

  return result


if __name__ == "__main__":
  # Run unit tests.
  ranges = [(1, 1, 2, 2), (1, 3, 1, 17)]
  expected = [(1, 1, 2, 2)]
  actual = merge_ranges(ranges)
  assert expected == actual, \
    f"Expected {expected}, actual {actual}"

  ranges = [(1, 1, 2, 2), (1, 3, 2, 17)]
  expected = [(1, 1, 2, 17)]
  actual = merge_ranges(ranges)
  assert expected == actual, \
    f"Expected {expected}, actual {actual}"

  ranges = [(1, 1, 2, 2), (2, 3, 2, 17)]
  expected = [(1, 1, 2, 2), (2, 3, 2, 17)]
  actual = merge_ranges(ranges)
  assert expected == actual, \
    f"Expected {expected}, actual {actual}"
