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

from typing import Dict, List, Tuple
from xml.dom import minidom

import setup_modules

import chromium_src.tools.metrics.common.xml_utils as xml_utils

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


def ExtractVariants(variants_node: minidom.Element) -> List[Variant]:
  """Extracts a list of variants from a <variants> or <token> node."""
  variants = []
  for variant_node in xml_utils.IterElementsWithTag(variants_node, 'variant',
                                                    1):
    name = variant_node.getAttribute('name')
    summary = variant_node.getAttribute('summary')
    if not summary:
      summary = xml_utils.GetTextFromChildNodes(variant_node)
    variants.append(Variant(name, summary))
  return variants


def _ExtractTokens(action: minidom.Element,
                   variants_dict: Dict[str, List[Variant]]) -> List[Token]:
  """Extracts tokens and variants from the given action element.

  Args:
    action: A DOM Element corresponding to a action.
    variants_dict: A dictionary of variants extracted from the tree.

  Returns:
    A tuple where the first element is a list of extracted Tokens, and the
      second indicates if any errors were detected while extracting them.
  """
  tokens_seen = set()
  tokens = []
  action_name = action.getAttribute('name')

  for token_node in xml_utils.IterElementsWithTag(action, 'token', 1):
    token_key = token_node.getAttribute('key')
    if token_key in tokens_seen:
      raise ValueError(f'Action {action_name} contains duplicate token key '
                       f'{token_key}, please ensure token keys are unique.')
    tokens_seen.add(token_key)

    token_key_format = '{' + token_key + '}'
    if token_key_format not in action_name:
      raise ValueError(
          f'User Action {action_name} includes a token tag but the token key '
          f'is not present in action name. Please insert the token key into '
          f'the action name in order for the token to be added.')

    token = Token(key=token_key)

    # If 'variants' attribute is set for the <token>, get the list of Variant
    # objects from from the |variants_dict| (out-of-line variants).
    if token_node.hasAttribute('variants'):
      variants_name = token_node.getAttribute('variants')
      if variants_name not in variants_dict:
        raise ValueError(
            f'The variants attribute {variants_name} of token key {token_key} '
            f'of action {action_name} does not have a corresponding '
            f'<variants> tag.')
      token.variants_name = variants_name
      token.variants = variants_dict[variants_name]

    # Extract any inline variants.
    else:
      token.variants = ExtractVariants(token_node)
    tokens.append(token)

  return tokens


def _ExtractText(parent_dom: minidom.Element, tag_name: str) -> List[str]:
  """Extract the text enclosed by |tag_name| under |parent_dom|

  Args:
    parent_dom: The parent Element under which text node is searched for.
    tag_name: The name of the tag which contains a text node.

  Returns:
    A (list of) string enclosed by |tag_name| under |parent_dom|.
  """
  texts = []
  for node in parent_dom.getElementsByTagName(tag_name):
    text = xml_utils.GetTextFromChildNodes(node)
    if text:
      texts.append(text)
  return texts


def ParseActionFile(
    file_content: str
) -> Tuple[Dict[str, Action], List[minidom.Node], Dict[str, List[Variant]]]:
  """Parse the XML data currently stored in the file.

  Args:
    file_content: a string containing the action XML file content.

  Returns:
    (actions_dict, comment_nodes, variants_dict):
      - actions_dict is a dict from user action name to Action object.
      - comment_nodes is a list of top-level comment nodes.
      - variants_dict is a dict of Variant objects.

  Raises:
    ValueError when mandatory elements of the XML format are missing
    or XML is malformed.
  """
  dom = minidom.parseString(file_content)

  comment_nodes = []
  # Get top-level comments. It is assumed that all comments are placed before
  # <actions> tag. Therefore the loop will stop if it encounters a non-comment
  # node.
  for node in dom.childNodes:
    if node.nodeType == minidom.Node.COMMENT_NODE:
      comment_nodes.append(node)
    else:
      break

  actions_dict = {}
  variants_dict = {}
  for variants_dom in dom.getElementsByTagName('variants'):
    variants_name = variants_dom.getAttribute('name')
    variants_dict[variants_name] = ExtractVariants(variants_dom)

  # Get each user action data.
  for action_dom in dom.getElementsByTagName('action'):
    action_name = action_dom.getAttribute('name')
    not_user_triggered = bool(action_dom.getAttribute('not_user_triggered'))

    owners = _ExtractText(action_dom, 'owner')
    # There is only one description for each user action. Get the first element
    # of the returned list.
    description_list = _ExtractText(action_dom, 'description')
    if len(description_list) > 1:
      raise ValueError(
          f'User action "{action_name}" has more than one description. Exactly '
          f'one description is needed for each user action. Please '
          f'fix.')
    description = description_list[0] if description_list else None

    # There is at most one obsolete tag for each user action.
    obsolete_list = _ExtractText(action_dom, 'obsolete')
    if len(obsolete_list) > 1:
      raise ValueError(
          f'User action "{action_name}" has more than one obsolete tag. At'
          f' most one obsolete tag can be added for each user action. Please'
          f' fix.')
    obsolete = obsolete_list[0] if obsolete_list else None

    tokens = _ExtractTokens(action_dom, variants_dict)

    actions_dict[action_name] = Action(action_name, description, owners,
                                       not_user_triggered, obsolete, tokens)

  return actions_dict, comment_nodes, variants_dict


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
