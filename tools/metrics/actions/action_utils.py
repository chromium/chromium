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


def _CreateActionFromVariant(action: Action, variant: Variant,
                             token: Token) -> Action:
  """Creates a new action by substituting token with variant in template action.

  The properties of returned action are derived from provided template action
  with token being substituted in its name and description.

  For example:
    `_CreateActionFromVariant("ClickOn.{Location}", "MainFrame", "Location")`
    will return action `ClickOn.MainFrame`.

  Args:
    action: an Action object to combine with suffix.
    variant: a Variant object to combine with action.
    token: a Token object to get the key from.

  Returns:
    An action with the template token replaced with specified variant.
  """
  new_name = action.name.replace('{' + token.key + '}', variant.name)

  if action.description:
    new_action_description = action.description + ' ' + variant.summary
  else:
    new_action_description = (
        'Please enter the description of this user action. ' + variant.summary)

  new_tokens = [new_token for new_token in action.tokens if new_token != token]

  return Action(new_name, new_action_description,
                list(action.owners) if action.owners else [],
                action.not_user_triggered, action.obsolete, new_tokens)


def _CreateActionVariantsFor(action: Action) -> Dict[str, Action]:
  """Returns dictionary of actions expanded from provided template action."""

  # If there are no tokens to fill, the action goes to the list as-is.
  if not action.tokens:
    return {action.name: action}

  ret_val: Dict[str, Action] = {}

  current_token = action.tokens[0]
  if not current_token.variants:
    raise ValueError(f"Action {action} does not have variants"
                     " for {current_token.key} token.")

  for variant in current_token.variants:
    ret_val |= _CreateActionVariantsFor(
        _CreateActionFromVariant(action, variant, current_token))

  return ret_val


def CreateActionsFromVariants(
    actions_dict: Dict[str, Action]) -> Dict[str, Action]:
  """Converts template actions dictionary into a dictionary of expanded actions

  We allow the actions.xml to contain tokens within the name that are linked
  with a specific type. Those template actions represent multiple actions that
  will be physically present in UMA.

  For example:

  ```
  <action name="ClickOn.{Location}">
    <token key="Location">
      <variant name="MainFrame" summary="Double click action."/>
      <variant name="IFrame" summary="Click and drag action."/>
    </token>
  </action>
  ```

  will actually generate actions:
   * ClickOn.MainFrame
   * ClickOn.IFrame

  If an input action doesn't have tokens it is returned without any changes.

  The input dictionary is keyed by template action name (including tokens),
  while the output dictionary keys are generated actions with no tokens
  in them anymore.

  Args:
    actions_dict: A dict of existing action name to Action object with names
        potentially containing tokens.

  Returns:
    A dictionary of actions with tokens expanded to all possible variants and
        no longer contain any unresolved tokens.

  Raises:
    ValueError if the information about tokens possible values is missing
    from Action objects in actions_dict.
  """
  expanded_actions: Dict[str, Action] = {}

  for action in actions_dict.values():
    expanded_actions |= _CreateActionVariantsFor(action)

  return expanded_actions
