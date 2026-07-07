# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import difflib
import math
import models


def _SelectEvenlySpaced(lst, n):
  length = len(lst)
  if n >= length:
    return lst

  step = length / n
  # Pick the element at the end of each calculated interval
  return [lst[math.ceil((i + 1) * step) - 1] for i in range(n)]


def SampleSymbols(candidates, changed_files=None):
  """Selects a representative subset of symbols to disassemble.

  Args:
    candidates: List of symbols to choose from. Should not contain any
        unchanged symbols.

  Returns:
    List of symbols.
  """
  if not candidates:
    return []

  selected = set()

  # Select at most 5 of each diff type.
  groups_by_status = candidates.GroupedByDiffStatus()
  for group in groups_by_status:
    sorted_symbols = sorted(group, key=lambda s: abs(s.size))
    selected.update(_SelectEvenlySpaced(sorted_symbols, 4))
    # Always take the 2 largest symbols, since they are generally more
    # interesting. The most largest is already added by _SelectEvenlySpaced().
    if len(sorted_symbols) > 4:
      selected.add(sorted_symbols[-2])

  # Make sure we have at least 8 symbols from changed files.
  if changed_files:
    changed_files = set(changed_files)
    contributed_files = {
        s.source_path
        for s in selected if s.source_path in changed_files
    }
    pool = [
        c for c in candidates
        if c.source_path in changed_files and c not in selected
    ]
    needed = 8 - sum(1 for s in selected if s.source_path in changed_files)

    for _ in range(needed):
      if not pool:
        break
      # Score: size + 100 bonus if file has not contributed yet
      pool.sort(key=lambda s: abs(s.size) +
                (100 if s.source_path not in contributed_files else 0),
                reverse=True)
      best = pool.pop(0)
      selected.add(best)
      contributed_files.add(best.source_path)

  return sorted(selected, key=lambda s: abs(s.size), reverse=True)


def CreateUnifiedDiff(name, before, after):
  """Helper to create a unified diff between before and after disassembly."""
  unified_diff = difflib.unified_diff(before,
                                      after,
                                      fromfile='before/' + name,
                                      tofile='after/' + name,
                                      n=10)
  return ''.join(unified_diff)
