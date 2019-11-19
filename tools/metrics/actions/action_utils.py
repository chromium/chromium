# Copyright 2015 The Chromium Authors. All rights reserved.
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


class Error(Exception):
  pass


class UndefinedActionItemError(Error):
  pass


class InvalidOrderingAttributeError(Error):
  pass


class SuffixNameEmptyError(Error):
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
               name,
               description,
               owners,
               not_user_triggered=False,
               obsolete=None,
               from_suffix=False):
    self.name = name
    self.description = description
    self.owners = owners
    self.not_user_triggered = not_user_triggered
    self.obsolete = obsolete
    self.from_suffix = from_suffix


class Suffix(object):
  """Action suffix in actions.xml.

  Attributes:
    name: name of the suffix.
    description: description of the suffix.
    separator: the separator between affected action name and suffix name.
    ordering: 'suffix' or 'prefix'. if set to prefix, suffix name will be
              inserted after the first dot separator of affected action name.
  """

  def __init__(self, name, description, separator, ordering):
    if not name:
      raise SuffixNameEmptyError('Suffix name cannot be empty.')

    if ordering != 'suffix' and ordering != 'prefix':
      raise InvalidOrderingAttributeError("Ordering has to be either 'prefix' "
                                          "or 'suffix'.")

    self.name = name
    self.description = description
    self.separator = separator
    self.ordering = ordering

  def __repr__(self):
    return '<%s, %s, %s, %s>' % (self.name, self.description, self.separator,
                                 self.ordering)


def CreateActionsFromSuffixes(actions_dict, action_suffix_nodes):
  """Creates new actions from suffixes and adds them to actions_dict.

  Args:
    actions_dict: dict of existing action name to Action object.
    action_suffix_nodes: a list of action-suffix nodes

  Returns:
    A dictionary of action name to list of Suffix objects for that action.

  Raises:
    UndefinedActionItemError: if an affected action name can't be found
  """
  action_to_suffixes_dict = _CreateActionToSuffixesDict(action_suffix_nodes)

  # Some actions in action_to_suffixes_dict keys may yet to be created.
  # Therefore, while new actions can be created and added to the existing
  # actions keep calling _CreateActionsFromSuffixes.
  while _CreateActionsFromSuffixes(actions_dict, action_to_suffixes_dict):
    pass

  # If action_to_suffixes_dict is not empty by the end, we have missing actions.
  if action_to_suffixes_dict:
    raise UndefinedActionItemError('Following actions are missing: %s.' %
                                   (list(action_to_suffixes_dict.keys())))


def _CreateActionToSuffixesDict(action_suffix_nodes):
  """Creates a dict of action name to list of Suffix objects for that action.

  Args:
    action_suffix_nodes: a list of action-suffix nodes

  Returns:
    A dictionary of action name to list of Suffix objects for that action.
  """
  action_to_suffixes_dict = {}
  for action_suffix_node in action_suffix_nodes:
    separator = _GetAttribute(action_suffix_node, 'separator', '_')
    ordering = _GetAttribute(action_suffix_node, 'ordering', 'suffix')
    suffixes = [Suffix(suffix_node.getAttribute('name'),
                       suffix_node.getAttribute('label'),
                       separator, ordering) for suffix_node in
                action_suffix_node.getElementsByTagName('suffix')]

    action_nodes = action_suffix_node.getElementsByTagName('affected-action')
    for action_node in action_nodes:
      action_name = action_node.getAttribute('name')
      # If <affected-action> has <with-suffix> child nodes, only those suffixes
      # should be used with that action. filter the list of suffix names if so.
      action_suffix_names = [suffix_node.getAttribute('name') for suffix_node in
                             action_node.getElementsByTagName('with-suffix')]
      if action_suffix_names:
        action_suffixes = [suffix for suffix in suffixes if suffix.name in
                           action_suffix_names]
      else:
        action_suffixes = list(suffixes)

      if action_name in action_to_suffixes_dict:
        action_to_suffixes_dict[action_name] += action_suffixes
      else:
        action_to_suffixes_dict[action_name] = action_suffixes

  return action_to_suffixes_dict


def _GetAttribute(node, attribute_name, default_value):
  """Returns the attribute's value or default_value if attribute doesn't exist.

  Args:
    node: an XML dom element.
    attribute_name: name of the attribute.
    default_value: default value to return if attribute doesn't exist.

  Returns:
    The value of the attribute or default_value if attribute doesn't exist.
  """
  if node.hasAttribute(attribute_name):
    return node.getAttribute(attribute_name)
  else:
    return default_value


def _CreateActionsFromSuffixes(actions_dict, action_to_suffixes_dict):
  """Creates new actions with action-suffix pairs and adds them to actions_dict.

  For every key (action name) in action_to_suffixes_dict, This function looks
  to see whether it exists in actions_dict. If so it combines the Action object
  from actions_dict with all the Suffix objects from action_to_suffixes_dict to
  create new Action objects. New Action objects are added to actions_dict and
  the action name is removed from action_to_suffixes_dict.

  Args:
    actions_dict: dict of existing action name to Action object.
    action_to_suffixes_dict: dict of action name to list of Suffix objects it
                             will combine with.

  Returns:
    True if any new action was added, False otherwise.
  """
  expanded_actions = set()
  for action_name, suffixes in action_to_suffixes_dict.items():
    if action_name in actions_dict:
      existing_action = actions_dict[action_name]
      for suffix in suffixes:
        _CreateActionFromSuffix(actions_dict, existing_action, suffix)

      expanded_actions.add(action_name)

  for action_name in expanded_actions:
    del action_to_suffixes_dict[action_name]

  return bool(expanded_actions)


def _CreateActionFromSuffix(actions_dict, action, suffix):
  """Creates a new action with action and suffix and adds it to actions_dict.

  Args:
    actions_dict: dict of existing action name to Action object.
    action: an Action object to combine with suffix.
    suffix: a suffix object to combine with action.

  Returns:
    None.

  Raises:
    InvalidAffecteddActionNameError: if the action name does not contain a dot
  """
  if suffix.ordering == 'suffix':
    new_action_name = action.name + suffix.separator + suffix.name
  else:
    (before, dot, after) = action.name.partition('.')
    if not after:
      raise InvalidAffecteddActionNameError(
          "Action name '%s' must contain a '.'." % action.name)
    new_action_name = before + dot + suffix.name + suffix.separator + after

  new_action_description = action.description + ' ' + suffix.description

  actions_dict[new_action_name] = Action(
      new_action_name,
      new_action_description,
      list(action.owners),
      action.not_user_triggered,
      action.obsolete,
      from_suffix=True)