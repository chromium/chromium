# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# [VPYTHON:BEGIN]
# python_version: "3.8"
# wheel: <
#   name: "infra/python/wheels/hypothesis-py3"
#   version: "version:6.9.1"
# >
# wheel: <
#   name: "infra/python/wheels/attrs-py2_py3"
#   version: "version:20.3.0"
# >
# wheel: <
#   name: "infra/python/wheels/sortedcontainers-py3"
#   version: "version:2.4.0"
# >
# [VPYTHON:END]
"""Tests for condition."""

import itertools
import unittest
from datetime import timedelta

from hypothesis import given, settings, strategies as st  # type: ignore

import conditions


# A Hypothesis strategy for generating Conditions.
def rec_condition_st(max_leaves):
  return st.recursive(
      st.sampled_from(conditions.TERMINALS),
      lambda children: st.tuples(st.just('not'), children) | st.tuples(
          st.sampled_from(['or', 'and']), st.lists(children)),
      max_leaves=max_leaves,
  )


# We separate out ALWAYS and NEVER from the recursive part, as they should only
# ever appear as a singular thing at the top-level.
def condition_st(max_leaves):
  return st.sampled_from([conditions.ALWAYS, conditions.NEVER
                          ]) | rec_condition_st(max_leaves)


# Taken from the example in the itertools docs.
def powerset(iterable):
  """returns an iterator over all subsets of iterable."""
  s = list(iterable)
  return itertools.chain.from_iterable(
      itertools.combinations(s, r) for r in range(len(s) + 1))


class ConditionsTest(unittest.TestCase):
  # We have to keep max_leaves quite low and deadline quite high, as simplify is
  # exponential in the number of unique terminals and blows up quite quickly.
  # Deadlines increased to 1,000 ms due to crbug.com/1331743
  @given(cond=condition_st(max_leaves=7))
  @settings(deadline=timedelta(milliseconds=1000))
  def test_simplified_condition_evaluates_the_same(self, cond):
    simplified = conditions.simplify(cond)

    for true_vars in powerset(conditions.find_terminals(cond)):
      true_vars = set(true_vars)
      # Exclude cases where mutually exclusive vars are set.
      if any(
          len(group & true_vars) > 1 for group in conditions.CONDITION_GROUPS):
        continue

      self.assertEqual(conditions.evaluate(cond, true_vars),
                       conditions.evaluate(simplified, true_vars))

  @given(cond=condition_st(max_leaves=7))
  @settings(deadline=timedelta(milliseconds=1000))
  def test_simplified_condition_is_at_least_as_small_as_original(self, cond):
    simplified = conditions.simplify(cond)

    self.assertLessEqual(len(conditions.find_terminals(simplified)),
                         len(conditions.find_terminals(cond)))


if __name__ == '__main__':
  unittest.main()
