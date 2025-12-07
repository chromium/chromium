# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A utility module for parsing and applying action suffixes in actions.xml.

Note: There is a copy of this file used internally by the UMA processing
infrastructure. Any changes to this file should also be done (manually) to the
internal copy. Please contact tools/metrics/OWNERS for more details.
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from typing import Dict, List


class Error(Exception):
  pass


class UndefinedActionItemError(Error):
  pass


class InvalidOrderingAttributeError(Error):
  pass


class VariantNameEmptyError(Error):
  pass


class InvalidAffecteddActionNameError(Error):
  pass


class Action(object):
  """Represents Chrome user action.

  Attributes:
    name: name of the action.
    description: description of the action.
    owners: list of action owners
    not_user_triggered: if action is not user triggered
    obsolete: explanation on why user action is not being used anymore
    from_suffix: If True, this action was computed via a suffix.
  """

  def __init__(self,
               name: str,
               description: str | None,
               owners: List[str] | None,
               not_user_triggered: bool = False,
               obsolete: str | None = None,
               tokens: List | None = [],
               from_suffix: bool | None = False):
    self.name = name
    self.description = description
    self.owners = owners
    self.not_user_triggered = not_user_triggered
    self.obsolete = obsolete
    self.from_suffix = from_suffix
    self.tokens = tokens or []


class Variants(object):
  """Variants object in actions.xml.

  Attributes:
    name: name used as token key.

  Raises:
    VariantNameEmptyError: if the variant does not have a name
  """

  def __init__(self, name: str):
    if not name:
      raise VariantNameEmptyError('Variants name cannot be empty.')

    self.name = name


class Variant(object):
  """Represents a single variant within variants object.

  Attributes:
    name: variant name.
    summary: description of the variant.
  """

  def __init__(self, name: str, summary: str):
    self.name = name
    self.summary = summary


class Token(object):
  """Token tag of an action.

  Attributes:
    key: token key.
  """

  def __init__(self, key: str):
    self.key = key
    self.variants_name = None
    self.variants = []
    self.implicit = False


def _CreateActionFromVariant(actions_dict: Dict[str, Action], action: Action,
                             variant: Variant, token: Token) -> None:
  """Creates a new action with action and variant and adds it to actions_dict.

  Args:
    actions_dict: dict of existing action name to Action object.
    action: an Action object to combine with suffix.
    variant: a Variant object to combine with action.
    token: a Token object to get the key from.
  """
  new_name = action.name.replace('{' + token.key + '}', variant.name)

  if action.description:
    new_action_description = action.description + ' ' + variant.summary
  else:
    new_action_description = (
        'Please enter the description of this user action. ' + variant.summary)

  actions_dict[new_name] = Action(new_name, new_action_description,
                                  list(action.owners) if action.owners else [],
                                  action.not_user_triggered, action.obsolete)


def CreateActionsFromVariants(actions_dict: Dict[str, Action],
                              variants_dict: Dict[str, List[Variant]]) -> bool:
  """Creates new actions from variants and adds them to actions_dict.

  If an action contains a token that refers to a variants block that is not
  defined, this is silently ignored.

  Args:
    actions_dict: A dict of existing action name to Action object.
    variants_dict: A dict of variants name to list of Variant objects.
  """
  # Create a dict of action name to Action object for actions with tokens.
  action_to_variants_dict = {
      name: action.tokens
      for name, action in actions_dict.items() if action.tokens
  }

  expanded_actions = set()

  for action_name, tokens in action_to_variants_dict.items():
    if not action_name in actions_dict:
      continue
    existing_action = actions_dict[action_name]
    for token in tokens:
      variants = token.variants
      if token.variants_name:
        variants = variants_dict.get(token.variants_name, [])
      for variant in variants:
        _CreateActionFromVariant(actions_dict, existing_action, variant, token)

      expanded_actions.add(action_name)
