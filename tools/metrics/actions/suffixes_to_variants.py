# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Migrates action_suffixes to patterned actions. See console output for
the elements that need manual refactoring."""

import argparse
import logging
import os

from xml.dom import minidom

import actions_model
import path_util

ACTIONS_PATH = path_util.GetInputFile('tools/metrics/actions/actions.xml')


def _GenerateTokenName(action_suffix: minidom.Element) -> str:
  """Generates a descriptive token name from a list of affected actions."""
  # Get a list of all the affected action names.
  affected_action_nodes = []
  for node in action_suffix.getElementsByTagName('affected-action'):
    affected_action_nodes.append(node.getAttribute('name'))
  common_prefix = affected_action_nodes[0]
  # If more than one affected action, find the longest common prefix.
  if len(affected_action_nodes) > 1:
    common_prefix = os.path.commonprefix(affected_action_nodes)
    # Sometimes there is no common prefix, but there is common suffix
    if not common_prefix:
      common_prefix = os.path.commonprefix(
          [s.split('.')[-1] for s in affected_action_nodes])
    # Trim any dots, if included
    common_prefix = common_prefix.strip('_.')

    if len(common_prefix) < 4:
      first_suffix_name = affected_action_nodes[0]
      token_name = f'unnamed_token_{first_suffix_name}'
      logging.warning(
          'token %s needs to be named manually. '
          'Affected actions: %s.', token_name, len(affected_action_nodes))
      action_suffix.setAttribute('name', token_name)
      return token_name

    # Replace dots with underscores and add a descriptive suffix.
    token_name = common_prefix.replace('.', '_') + '_Type'
  # For inline variants, the token name doesnt need to be descriptive.
  else:
    token_name = "ActionType"

  action_suffix.setAttribute('name', token_name)
  return token_name


def _RemoveSuffixesComment(node, action_suffixes_name):
  """Remove suffixes related comments from |node|."""
  for child in node.childNodes:
    if child.nodeType == minidom.Node.COMMENT_NODE:
      if ('Name completed by' in child.data
          and action_suffixes_name in child.data):
        node.removeChild(child)


def _UpdateDescription(action, action_suffixes_name):
  """Appends a placeholder string to the |action|'s description node."""
  description = action.getElementsByTagName('description')
  assert len(
      description) == 1, 'A action should have a single description node.'
  description = description[0]
  if description.firstChild.nodeType != description.TEXT_NODE:
    raise ValueError('description_node doesn\'t contain text.')
  description.firstChild.replaceWholeText(
      '%s {%s}' % (description.firstChild.data.strip(), action_suffixes_name))


def _AreAllAffectedactionsFound(affected_actions, actions):
  """Checks that are all affected actions found in |actions|."""
  action_names = [action.getAttribute('name') for action in actions]
  return all(
      affected_action.getAttribute('name') in action_names
      for affected_action in affected_actions)


def _GetSuffixesDict(nodes, all_actions):
  """Gets a dict of simple action-suffix to be used in the migration.

  Returns two dicts of action-suffix to be migrated to the new patterned
  actions syntax.

  The first dict: the keys are the action-suffix's affected action name
  and the values are the action_suffixes nodes that have only one
  affected-action. These actions-suffixes can be converted to inline
  patterned actions.

  The second dict: the keys are the action_suffixes name and the values
  are the action_suffixes nodes whose affected-actions are all present in
  the |all_actions|. These action suffixes can be converted to out-of-line
  variants.

  Args:
    nodes: A Nodelist of actions_suffixes nodes.
    all_actions: A Nodelist of all chosen actions.

  Returns:
    A dict of actions-suffixes nodes keyed by their names.
  """
  single_affected = {}
  all_affected_found = {}
  for action_suffixes in nodes:
    affected_actions = action_suffixes.getElementsByTagName('affected-action')
    if len(affected_actions) == 1:
      affected_action = affected_actions[0].getAttribute('name')
      single_affected[affected_action] = action_suffixes
    elif _AreAllAffectedactionsFound(affected_actions, all_actions):
      for affected_action in affected_actions:
        affected_action = affected_action.getAttribute('name')
        if affected_action in all_affected_found:
          logging.warning(
              'action %s is already associated with other suffixes. '
              'Please manually migrate it.', affected_action)
          continue
        all_affected_found[affected_action] = action_suffixes
  return single_affected, all_affected_found


def _GetBaseVariant(doc):
  """Returns a <variant> node whose name is an empty string as the base variant.

  We add the base variant to every single patterned action.

  Args:
    doc: A Document object which is used to create a new <variant> node.

  Returns:
     A <variant> node, or None if base="true" is set.
  """
  base_variant = doc.createElement('variant')
  base_variant.setAttribute('name', '')
  base_variant.setAttribute('summary', 'aggregated')
  return base_variant


def _PopulateVariantsWithSuffixes(doc, node, action_suffixes, suffixes_name):
  """Populates <variant> nodes to |node| from <suffix>.

  This function returns True if none of the suffixes contains 'base' attribute.
  If this function returns false, the caller's action node will not be
  updated. This is mainly because base suffix is a much more complicated case
  and thus it can not be automatically updated at least for now.

  Args:
    doc: A Document object which is used to create a new <variant> node.
    node: The node to be populated. it should be either <token> for inline
      variants or <variants> for out-of-line variants.
    action_suffixes: A <action_suffixes> node.

  Returns:
    True if the node can be updated automatically.
  """
  separator = action_suffixes.getAttribute('separator')
  for suffix in action_suffixes.getElementsByTagName('suffix'):
    suffix_name = suffix.getAttribute('name')
    if not suffix_name:
      logging.warning(
          'action suffixes: %s contains empty string suffix and thus we '
          'have to manually update the empty string variant in these base '
          'actions: %s.', suffixes_name, ','.join(
              h.getAttribute('name')
              for h in action_suffixes.getElementsByTagName('affected-action')))
      return False
    variant = doc.createElement('variant')
    variant.setAttribute('name', separator + suffix_name)
    if suffix.hasAttribute('label'):
      variant.setAttribute('summary', suffix.getAttribute('label'))
    node.appendChild(variant)
  return True


def MigrateToInlinePatterenedAction(doc, action, action_suffix):
  """Migrates a single action suffix to an inline patterned action."""
  # Keep a deep copy in case when the |action| fails to be migrated.
  old_action = action.cloneNode(deep=True)
  # There are no names for action-suffix, so we generate one.
  action_suffix_name = _GenerateTokenName(action_suffix)
  # Update action name.
  action.setAttribute(
      'name', '%s{%s}' % (action.getAttribute('name'), action_suffix_name))

  # Append |action_suffixes_name| placeholder string to the summary text.
  _UpdateDescription(action, action_suffix_name)

  # Create an inline <token> node.
  token = doc.createElement('token')
  token.setAttribute('key', action_suffix_name)
  base_variant = _GetBaseVariant(doc)
  if base_variant:
    token.appendChild(base_variant)

  # Populate <variant>s to the inline <token> node.
  if not _PopulateVariantsWithSuffixes(doc, token, action_suffix,
                                       action_suffix_name):
    logging.warning('action-suffix: %s needs manual effort', action_suffix_name)
    actions = action.parentNode
    actions.removeChild(action)
    # Restore old action when we the script fails to migrate it.
    actions.appendChild(old_action)
  else:
    # Add tokens to action.
    action.appendChild(token)
    # Remove action-suffix element
    action_suffix.parentNode.removeChild(action_suffix)
    # Remove obsolete comments from the action node.
    _RemoveSuffixesComment(action, action_suffix_name)


def MigrateToOutOflinePatterenedAction(doc, action, action_suffix):
  """Migrates a action suffixes to out-of-line patterned action."""
  # Update action's name with the action_suffixes' name.
  # There are no names for action-suffix, so we generate one.
  action_suffix_name = _GenerateTokenName(action_suffix)
  # Update action name.
  action.setAttribute(
      'name', '%s{%s}' % (action.getAttribute('name'), action_suffix_name))

  # Append |action_suffix_name| placeholder string to the summary text.
  _UpdateDescription(action, action_suffix_name)

  # Create a <token> node that links to an out-of-line <variants>.
  token = doc.createElement('token')
  token.setAttribute('key', action_suffix_name)
  token.setAttribute('variants', action_suffix_name)
  action.appendChild(token)
  # Remove obsolete comments from the action node.
  _RemoveSuffixesComment(action, action_suffix_name)


def _MigrateOutOfLineVariants(doc, actions, suffixes_to_convert):
  """Converts a action-suffixes node to an out-of-line variants."""
  actions_node = actions.getElementsByTagName('actions')
  assert len(actions_node) == 1, (
      'Actions.xml should have only one <actions> node.')
  for suffixes in suffixes_to_convert:
    action_suffixes_name = suffixes.getAttribute('name')
    variants = doc.createElement('variants')
    variants.setAttribute('name', action_suffixes_name)
    base_variant = _GetBaseVariant(doc)
    variants.appendChild(base_variant)
    if not _PopulateVariantsWithSuffixes(doc, variants, suffixes,
                                         action_suffixes_name):
      logging.warning('action_suffixes: %s needs manual effort',
                      action_suffixes_name)
    else:
      actions_node[0].appendChild(variants)
      suffixes.parentNode.removeChild(suffixes)


def SuffixesToVariantsMigration(args):
  """Migrates all action suffixes to patterned actions."""
  actions_file = minidom.parse(open(ACTIONS_PATH))
  action_suffixes_nodes = actions_file.getElementsByTagName('action-suffix')
  doc = minidom.Document()
  single_affected, all_affected_found = _GetSuffixesDict(
      action_suffixes_nodes, actions_file.getElementsByTagName('action'))
  suffixes_to_convert = set()
  for action in actions_file.getElementsByTagName('action'):
    name = action.getAttribute('name')
    # Migrate inline patterned actions.
    if name in single_affected.keys():
      MigrateToInlinePatterenedAction(doc, action, single_affected[name])
    elif name in all_affected_found.keys():
      suffixes_to_convert.add(all_affected_found[name])
      MigrateToOutOflinePatterenedAction(doc, action, all_affected_found[name])

  _MigrateOutOfLineVariants(doc, actions_file, suffixes_to_convert)

  # Update actions.xml with patterned actions.
  with open(ACTIONS_PATH, 'w') as f:
    pretty_xml_string = actions_model.PrettifyTree(actions_file)
    f.write(pretty_xml_string)

  # Remove action_suffixes that have already been migrated.
  with open(ACTIONS_PATH, 'w') as f:
    pretty_xml_string = actions_model.PrettifyTree(actions_file)
    f.write(pretty_xml_string)


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--start',
      help='Start migration from a certain character (inclusive).',
      default='a')
  parser.add_argument('--end',
                      help='End migration at a certain character (inclusive).',
                      default='z')
  args = parser.parse_args()
  assert len(args.start) == 1 and len(args.end) == 1, (
      'start and end flag should only contain a single letter.')
  SuffixesToVariantsMigration(args)