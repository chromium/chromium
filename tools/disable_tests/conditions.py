# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Conditions represent a build configuration under which to disable a test.
This file contains a canonical list of conditions and their properties, and code
for composing, parsing, and simplifying them. This is independent of their
representation within any particular test format.
"""

import collections
import functools
import itertools
import types
from typing import List, Optional, Union, Set, Tuple

import errors


class BaseCondition:
  """BaseCondition is a class for sentinel values ALWAYS and NEVER."""


# These represent a condition that's always true, and a condition that's never
# true.
ALWAYS = BaseCondition()
NEVER = BaseCondition()


@functools.total_ordering
class Terminal:
  """A boolean variable, the value of which depends on the configuration."""

  def __init__(self, name: str, group: Optional[str]):
    """
    Args:
      name: The generic name for this terminal. Used to specify conditions on
        the command line.
      group: The group to which this condition belongs. Every Terminal with the
        same group is mutually exclusive. For example, "os" - we can't be
        compiling for Linux and Mac at the same time.

    We also add fields for each test format, initialised to None. It's up to the
    file defining the relevant code to fill these out.
    """

    self.name: str = name
    self.group: Optional[str] = group
    self.gtest_info = None
    self.expectations_info = None

  def __str__(self):
    return (f"Terminal('{self.name}')")

  def __repr__(self):
    return str(self)

  def __lt__(self, other):
    """Define a consistent ordering for conditions written into test files"""

    if not isinstance(other, Terminal):
      return False

    # Ungrouped terminals should compare greater than grouped, and compare based
    # on name to each-other.
    if (self.group is not None) == (other.group is not None):
      return (self.group, self.name) < (other.group, other.name)
    return self.group is not None

  # TODO: We could probably just use object identity here (and for __hash__),
  # since we only use a fixed set of Terminal objects.
  def __eq__(self, other):
    # Names are expected to be unique keys
    return isinstance(other, Terminal) and self.name == other.name

  def __hash__(self):
    return hash(self.name)


# TODO: We should think about how to incorporate overlapping conditions. For
# instance, multiple versions of the same OS, where we might just specify "Mac"
# to refer to any version, or "Mac-10.15" for that specific version. Or "x86" to
# refer to the architecture as a whole, regardless of 32 vs 64-bit.
#
# We could handle this via expanding the higher-level one, for instance "Mac"
# being parsed directly into (or, ["Mac-10.15", "Mac-11", ...]). But we'd also
# need to handle this on the other end, to reduce it back down after
# simplifying.
TERMINALS = [
    Terminal('android', group='os'),
    Terminal('chromeos', group='os'),
    Terminal('fuchsia', group='os'),
    Terminal('ios', group='os'),
    Terminal('linux', group='os'),
    Terminal('mac', group='os'),
    Terminal('win', group='os'),
    Terminal('arm64', group='arch'),
    Terminal('x86', group='arch'),
    Terminal('x86-64', group='arch'),
    Terminal('lacros', group='lacros/ash'),
    Terminal('ash', group='lacros/ash'),
    Terminal('asan', group=None),
    Terminal('msan', group=None),
    Terminal('tsan', group=None),
]

# Terminals should have unique names.
assert len({t.name for t in TERMINALS}) == len(TERMINALS)

# A condition can be one of three things:
# 1. A BaseCondition (ALWAYS or NEVER).
# 2. An operator, represented as a tuple with the operator name followed by its
#    arguments.
# 3. A Terminal.
Condition = Union[BaseCondition, tuple, Terminal]


def get_term(name: str) -> Terminal:
  """Look up a Terminal by name."""

  t = next((t for t in TERMINALS if t.name == name), None)
  if t is not None:
    return t

  raise ValueError(f"Unknown condition '{name}'")


# TODO: We should check that the parsed condition makes sense with respect to
# condition groups. For instance, the condition 'linux & mac' can never be true.
def parse(condition_strs: List[str]) -> Condition:
  """Parse a list of condition strings, as passed on the command line.

  Each element of condition_strs is a set of Terminal names joined with '&'s.
  The list is implicitly 'or'ed together.
  """

  # When no conditions are given, this is taken to mean "always".
  if not condition_strs:
    return ALWAYS

  try:
    return op_of('or', [
        op_of('and', [get_term(x.strip()) for x in cond.split('&')])
        for cond in condition_strs
    ])
  except ValueError as e:
    # Catching the exception raised by get_term.
    valid_conds = '\n'.join(sorted(f'\t{term.name}' for term in TERMINALS))
    raise errors.UserError(f"{e}\nValid conditions are:\n{valid_conds}")


def op_of(op: str, args: List[Condition]) -> Condition:
  """Make an operator, simplifying the single-argument case."""

  if len(args) == 1:
    return args[0]
  return (op, args)


def merge(existing_cond: Condition, new_cond: Condition) -> Condition:
  """Merge two conditions together.

  Given an existing condition, parsed from a file, and a new condition to be
  added, combine the two to produce a merged condition.
  """

  # If currently ALWAYS, merging would only ever produce ALWAYS too. In this
  # case the user likely want to change the conditions or re-enable.
  if existing_cond == ALWAYS:
    return new_cond

  # If currently NEVER, ignore the current value - NEVER or X = X
  if existing_cond == NEVER:
    return new_cond

  # If new cond is ALWAYS, ignore the current value - X or ALWAYS = ALWAYS
  if new_cond == ALWAYS:
    return ALWAYS

  # Similar to the first branch, if the user has specified NEVER then ignore the
  # current value, as they're re-enabling it.
  if new_cond == NEVER:
    return NEVER

  # Otherwise, take the union of the two conditions
  cond = ('or', [existing_cond, new_cond])
  return simplify(cond)


def generate_condition_groups(terms: List[Terminal]) -> List[Set[Terminal]]:
  """Partition a list of Terminals by their 'group' attribute."""

  by_group = collections.defaultdict(set)
  ungrouped = []
  for term in terms:
    if term.group is not None:
      by_group[term.group].add(term)
    else:
      # Every Terminal without a 'group' attribute gets its own group, as
      # they're not mutually exclusive with anything.
      ungrouped.append({term})

  groups = list(by_group.values())
  groups += ungrouped

  return groups


# Pre-compute condition groups for use when simplifying.
CONDITION_GROUPS = generate_condition_groups(TERMINALS)


def simplify(cond: Condition) -> Condition:
  """Given a Condition, produce an equivalent but simpler Condition.

  This function uses the Quine-McCluskey algorithm. It's not implemented very
  efficiently, but it works and is fast enough for now.
  """

  if isinstance(cond, BaseCondition):
    return cond

  # Quine-McCluskey uses three values - true, false, and "don't care". The
  # latter represents two things:
  # * For values of input variables, the case where either true or false will
  #   suffice. This is used when combining conditions that differ only in that
  #   variable into one where that variable isn't specified.
  # * For resulting values of the function, the case where we don't care what
  #   value the function produces. We use this for mutually exclusive
  #   conditions.  For example, (linux & mac) is impossible, so we assign this a
  #   "don't care" value. This avoids producing a bunch of redundant stuff like
  #   (linux & ~mac).
  DONT_CARE = 2

  # First, compute the truth table of the function. We produce a set of
  # "minterms", which are the combinations of input values for which the output
  # is 1. We also produce a set of "don't care" values, which are the
  # combinations of input values for which we don't care what the output is.
  #
  # Both of these are represented via tuples of {0, 1} values, which the value
  # at index 'i' corresponds to variables[i].
  # TODO: This could use a more efficient representation. Some packed integer
  # using two bits per element or something.
  variables = list(sorted(find_terminals(cond)))
  dont_cares = []
  min_terms = []
  for possible_input in itertools.product([0, 1], repeat=len(variables)):
    # Generate every possible input, and evaluate the condition for that input.
    # This is exponential in the number of variables, but in practice the number
    # should be low (and is strictly bounded by len(TERMINALS)).
    true_vars = {variables[i] for i, val in enumerate(possible_input) if val}
    if any(len(group & true_vars) > 1 for group in CONDITION_GROUPS):
      # Any combination which sets more than one variable from the same group to
      # 1 is impossible, so we don't care about the output.
      dont_cares.append(possible_input)
    elif evaluate(cond, true_vars):
      min_terms.append(possible_input)

  # The meat of the algorithm. Try to combine minterms which differ by only a
  # single variable.
  # For example, (0, 1) and (0, 0) can be combined into (0, DONT_CARE), as the
  # value of the second variable doesn't affect the output.
  #
  # We work in rounds, combining together all minterms from the previous round
  # that can be. This may include combining the same minterm with multiple
  # different minterms. Keep going until no more minterms can be combined.
  #
  # Any minterm which can't be combined with another is a "prime implicant",
  # that is, it's a necessary part of the representation of the function. The
  # union of all prime implicants specifies the function.
  combined_some_minterms = True
  prev_round = set(min_terms + dont_cares)
  prime_implicants: List[Tuple] = []
  while combined_some_minterms:
    new_implicants = set()
    used = set()
    combined_some_minterms = False

    # TODO: Rather than taking combinations of the entire set of minterms, we
    # can instead group by the number of '1's. Then we only need to combine
    # elements from adjacent groups.
    for a, b in itertools.combinations(prev_round, 2):
      diff_index = None
      for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
          if diff_index is not None:
            # In this case there are at least two points of difference, so these
            # two can't be combined.
            break
          diff_index = i
      else:
        if diff_index is not None:
          # Replace the sole differing variable with DONT_CARE to produce the
          # combined minterm. Flag both inputs as having been used, and
          # therefore as not being prime implicants.
          new_implicants.add(a[:diff_index] + (DONT_CARE, ) +
                             a[diff_index + 1:])
          used |= {a, b}
          combined_some_minterms = True

    # Collect any minterms that weren't used in this round as prime implicants.
    prime_implicants.extend(prev_round - used)
    prev_round = new_implicants

  # TODO: This isn't yet minimal - the set of prime implicants may have some
  # redundancy which can be reduced further. For now we just accept that and
  # use this set as-is. If we encounter any case for which we don't produce the
  # minimal result, we'll need to implement something like Petrick's method.

  # Finally, create our simplified condition using the computed set of prime
  # implicants.
  # TODO: Ordering. We should define some stable ordering to use. We probably
  # want to group stuff based on CONDITION_GROUPS, so all the OS-related
  # conditions are together, for instance. And then alphabetically within that.
  or_args: List[Condition] = []
  for pi in sorted(prime_implicants):
    and_args: List[Condition] = []
    for i, x in enumerate(pi):
      if x == DONT_CARE:
        continue

      var = variables[i]
      if x == 0:
        and_args.append(('not', var))
      else:
        assert x == 1
        and_args.append(var)

    or_args.append(op_of('and', and_args))

  return op_of('or', or_args)


def find_terminals(cond: Condition) -> Set[Terminal]:
  """Find all leaf Terminal nodes of this Condition."""

  if isinstance(cond, BaseCondition):
    return set()

  if isinstance(cond, Terminal):
    return {cond}

  assert isinstance(cond, tuple)
  op, args = cond
  if op == 'not':
    return find_terminals(args)
  return {var for arg in args for var in find_terminals(arg)}


def evaluate(cond: Condition, true_vars: Set[Terminal]) -> bool:
  """Evaluate a condition with a given set of true variables."""

  if isinstance(cond, BaseCondition):
    return cond is ALWAYS

  if isinstance(cond, Terminal):
    return cond in true_vars

  # => must be a tuple

  op, args = cond
  if op == 'not':
    return not evaluate(args, true_vars)

  return {'or': any, 'and': all}[op](evaluate(arg, true_vars) for arg in args)
