# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Pure validation logic for histograms.

This module contains standalone, testable validation functions for Chromium
histograms. These checks are typically called by presubmit scripts (e.g.
PRESUBMIT.py) to enforce styling, correctness, and safety constraints
before changes are committed.
"""

import dataclasses
import re
from typing import List, Set, Tuple


@dataclasses.dataclass
class AffectedFileForLineCheck:
  path: str
  changed_lines: List[Tuple[int, str]]  # list of (line_number, line_content)


def check_booleans_are_enums(
    affected_files: List[AffectedFileForLineCheck],
) -> List[Tuple[str, int, str]]:
  """Checks that histograms that use Booleans do not use units."""
  results = []
  inclusion_pattern = re.compile(r'units="[Bb]oolean')

  for affected_file in affected_files:
    if 'histograms.xml' in affected_file.path:
      for line_number, line in affected_file.changed_lines:
        if inclusion_pattern.search(line):
          results.append((affected_file.path, line_number, line))
  return results


def check_removed_segmentation_histograms(
    removed_histograms: Set[str],
    segmentation_histograms: Set[str],
) -> Set[str]:
  """Checks if any histogram used by segmentation platform is removed."""
  return removed_histograms.intersection(segmentation_histograms)


def check_if_introduced_too_many_histograms(
    added_histograms: Set[str],
    threshold: int,
) -> bool:
  """Checks if the number of histograms introduced in the CL is too high."""
  return len(added_histograms) > threshold
