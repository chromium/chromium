# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Handling of the <message> element.
'''

import re

from grit.node import base

from grit import clique
from grit import exception
from grit import lazy_re
from grit import tclib
from grit import util


# Matches exactly three dots ending a line or followed by whitespace.
_ELLIPSIS_PATTERN = lazy_re.compile(r'(?<!\.)\.\.\.(?=$|\s)')
_ELLIPSIS_SYMBOL = '\u2026'  # Ellipsis

# Finds whitespace at the start and end of a string which can be multiline.
_WHITESPACE = lazy_re.compile(r'(?P<start>\s*)(?P<body>.+?)(?P<end>\s*)\Z',
                              re.DOTALL | re.MULTILINE)

# <ph> placeholder elements should contain the special character formatters
# used to format <ph> element content.
# Android format.
_ANDROID_FORMAT = (r'%[1-9]+\$'
                   r'([-#+ 0,(]*)([0-9]+)?(\.[0-9]+)?'
                   r'([bBhHsScCdoxXeEfgGaAtT%n])')
# Chrome l10n format.
_CHROME_FORMAT = r'\$+\d'
# Windows EWT numeric and GRIT %s %d formats.
_OTHER_FORMAT = r'%[0-9sd]'

# Finds formatters that must be in a placeholder (<ph>) element.
_FORMATTERS = lazy_re.compile(
    '(%s)|(%s)|(%s)' % (_ANDROID_FORMAT, _CHROME_FORMAT, _OTHER_FORMAT))
_BAD_PLACEHOLDER_MSG = ('ERROR: Placeholder formatter found outside of <ph> '
                        'tag in message "%s" in %s.')
_INVALID_PH_CHAR_MSG = ('ERROR: Invalid format characters found in message '
                        '"%s" <ph> tag in %s.')

# Finds HTML tag tokens.
_HTMLTOKEN = lazy_re.compile(r'<[/]?[a-z][a-z0-9]*[^>]*>', re.I)

# Finds HTML entities.
_HTMLENTITY = lazy_re.compile(r'&[^\s]*;')


class MessageNode(base.ContentNode):
  '''A <message> element.'''

  # For splitting a list of things that can be separated by commas or
  # whitespace
  _SPLIT_RE = lazy_re.compile(r'\s*,\s*|\s+')

  def __init__(self):
    super().__init__()
    # Valid after EndParsing, this is the MessageClique that contains the
    # source message and any translations of it that have been loaded.
    self.clique = None

    # We don't send leading and trailing whitespace into the translation
    # console, but rather tack it onto the source message and any
    # translations when formatting them into RC files or what have you.
    self.ws_at_start = ''  # Any whitespace characters at the start of the text
    self.ws_at_end = ''  # --"-- at the end of the text

    # A list of "shortcut groups" this message is in.  We check to make sure
    # that shortcut keys (e.g. &J) within each shortcut group are unique.
    self.shortcut_groups_ = []

    # Formatter-specific data used to control the output of individual strings.
    # formatter_data is a space separated list of C preprocessor-style
    # definitions. Names without values are given the empty string value.
    # Example: "foo=5 bar baz=100"
    self.formatter_data = {}

    # Whether or not to convert ... -> U+2026 within Translate().
    self._replace_ellipsis = False

  def _IsValidChild(self, child):
    return isinstance(child, (PhNode))

  def _IsValidAttribute(self, name, value):
    if name not in [
        'name', 'offset', 'translateable', 'desc', 'meaning',
        'internal_comment', 'shortcut_groups', 'custom_type', 'validation_expr',
        'use_name_for_id', 'sub_variable', 'formatter_data',
        'is_accessibility_with_no_ui'
    ]:
      return False
    if (name in ('translateable', 'sub_variable') and
        value not in ['true', 'false']):
      return False
    return True

  def SetReplaceEllipsis(self, value):
    r'''Sets whether to replace ... with \u2026.
    '''
    self._replace_ellipsis = value

  def MandatoryAttributes(self):
    return ['name|offset']

  def DefaultAttributes(self):
    return {
        'custom_type': '',
        'desc': '',
        'formatter_data': '',
        'internal_comment': '',
        'is_accessibility_with_no_ui': 'false',
        'meaning': '',
        'shortcut_groups': '',
        'sub_variable': 'false',
        'translateable': 'true',
        'use_name_for_id': 'false',
        'validation_expr': '',
    }

  def HandleAttribute(self, attrib, value):
    base.ContentNode.HandleAttribute(self, attrib, value)
    if attrib != 'formatter_data':
      return

    # Parse value, a space-separated list of defines, into a dict.
    # Example: "foo=5 bar" -> {'foo':'5', 'bar':''}
    for item in value.split():
      name, _, val = item.partition('=')
      self.formatter_data[name] = val

  def GetTextualIds(self):
    '''
    Returns the concatenation of the parent's node first_id and
    this node's offset if it has one, otherwise just call the
    superclass' implementation
    '''
    if 'offset' not in self.attrs:
      return super().GetTextualIds()

    # we search for the first grouping node in the parents' list
    # to take care of the case where the first parent is an <if> node
    grouping_parent = self.parent
    import grit.node.empty
    while grouping_parent and not isinstance(grouping_parent,
                                             grit.node.empty.GroupingNode):
      grouping_parent = grouping_parent.parent

    assert 'first_id' in grouping_parent.attrs
    return [grouping_parent.attrs['first_id'] + '_' + self.attrs['offset']]

  def IsTranslateable(self):
    return self.attrs['translateable'] == 'true'

  def EndParsing(self):
    super().EndParsing()

    # Make the text (including placeholder references) and list of placeholders,
    # verify placeholder formats, then strip and store leading and trailing
    # whitespace and create the tclib.Message() and a clique to contain it.

    text = ''
    placeholders = []

    for item in self.mixed_content:
      if isinstance(item, str):
        # Not a <ph> element: fail if any <ph> formatters are detected.
        if _FORMATTERS.search(item):
          print(_BAD_PLACEHOLDER_MSG % (item, self.source))
          raise exception.PlaceholderNotInsidePhNode
        text += item
      else:
        # Extract the <ph> element components.
        presentation = item.attrs['name'].upper()
        text += presentation
        ex = ' '  # <ex> example element cdata if present.
        if len(item.children):
          ex = item.children[0].GetCdata()
        original = item.GetCdata()

        # Sanity check the <ph> element content.
        cdata = original
        # Replace all HTML tag tokens in cdata.
        match = _HTMLTOKEN.search(cdata)
        while match:
          cdata = cdata.replace(match.group(0), '_')
          match = _HTMLTOKEN.search(cdata)
        # Replace all HTML entities in cdata.
        match = _HTMLENTITY.search(cdata)
        while match:
          cdata = cdata.replace(match.group(0), '_')
          match = _HTMLENTITY.search(cdata)
        # Remove first matching formatter from cdata.
        match = _FORMATTERS.search(cdata)
        if match:
          cdata = cdata.replace(match.group(0), '')
        # Fail if <ph> special chars remain in cdata.
        if re.search(r'[%\$]', cdata):
          message_id = self.attrs['name'] + ' ' + original;
          print(_INVALID_PH_CHAR_MSG % (message_id, self.source))
          raise exception.InvalidCharactersInsidePhNode

        # Otherwise, accept this <ph> placeholder.
        placeholders.append(tclib.Placeholder(presentation, original, ex))

    m = _WHITESPACE.match(text)
    if m:
      self.ws_at_start = m.group('start')
      self.ws_at_end = m.group('end')
      text = m.group('body')

    self.shortcut_groups_ = self._SPLIT_RE.split(self.attrs['shortcut_groups'])
    self.shortcut_groups_ = [i for i in self.shortcut_groups_ if i != '']

    description_or_id = self.attrs['desc']
    if description_or_id == '' and 'name' in self.attrs:
      description_or_id = 'ID: %s' % self.attrs['name']

    assigned_id = None
    if self.attrs['use_name_for_id'] == 'true':
      assigned_id = self.attrs['name']
    message = tclib.Message(text=text, placeholders=placeholders,
                            description=description_or_id,
                            meaning=self.attrs['meaning'],
                            assigned_id=assigned_id)
    self.InstallMessage(message)

  def InstallMessage(self, message):
    '''Sets this node's clique from a tclib.Message instance.

    Args:
      message: A tclib.Message.
    '''
    self.clique = self.UberClique().MakeClique(message, self.IsTranslateable())
    for group in self.shortcut_groups_:
      self.clique.AddToShortcutGroup(group)
    if self.attrs['custom_type'] != '':
      self.clique.SetCustomType(util.NewClassInstance(self.attrs['custom_type'],
                                                      clique.CustomType))
    elif self.attrs['validation_expr'] != '':
      self.clique.SetCustomType(
        clique.OneOffCustomType(self.attrs['validation_expr']))

  def SubstituteMessages(self, substituter):
    '''Applies substitution to this message.

    Args:
      substituter: a grit.util.Substituter object.
    '''
    message = substituter.SubstituteMessage(self.clique.GetMessage())
    if message is not self.clique.GetMessage():
      self.InstallMessage(message)

  def GetCliques(self):
    return [self.clique] if self.clique else []

  def Translate(self, lang):
    '''Returns a translated version of this message.
    '''
    assert self.clique
    msg = self.clique.MessageForLanguage(lang,
                                         self.PseudoIsAllowed(),
                                         self.ShouldFallbackToEnglish()
                                         ).GetRealContent()
    if self._replace_ellipsis:
      msg = _ELLIPSIS_PATTERN.sub(_ELLIPSIS_SYMBOL, msg)
    # Always remove all byte order marks (\uFEFF) https://crbug.com/1033305
    msg = msg.replace('\uFEFF','')
    return msg.replace('[GRITLANGCODE]', lang)

  def NameOrOffset(self):
    key = 'name' if 'name' in self.attrs else 'offset'
    return self.attrs[key]

  def ExpandVariables(self):
    '''We always expand variables on Messages.'''
    return True

  def GetDataPackValue(self, lang, encoding):
    '''Returns a str represenation for a data_pack entry.'''
    message = self.ws_at_start + self.Translate(lang) + self.ws_at_end
    return util.Encode(message, encoding)

  def IsResourceMapSource(self):
    return True

  @staticmethod
  def Construct(parent, message, name, desc='', meaning='', translateable=True):
    '''Constructs a new message node that is a child of 'parent', with the
    name, desc, meaning and translateable attributes set using the same-named
    parameters and the text of the message and any placeholders taken from
    'message', which must be a tclib.Message() object.'''
    # Convert type to appropriate string
    translateable = 'true' if translateable else 'false'

    node = MessageNode()
    node.StartParsing('message', parent)
    node.HandleAttribute('name', name)
    node.HandleAttribute('desc', desc)
    node.HandleAttribute('meaning', meaning)
    node.HandleAttribute('translateable', translateable)

    items = message.GetContent()
    for ix, item in enumerate(items):
      if isinstance(item, str):
        # Ensure whitespace at front and back of message is correctly handled.
        if ix == 0:
          item = "'''" + item
        if ix == len(items) - 1:
          item = item + "'''"

        node.AppendContent(item)
      else:
        phnode = PhNode()
        phnode.StartParsing('ph', node)
        phnode.HandleAttribute('name', item.GetPresentation())
        phnode.AppendContent(item.GetOriginal())

        if len(item.GetExample()) and item.GetExample() != ' ':
          exnode = ExNode()
          exnode.StartParsing('ex', phnode)
          exnode.AppendContent(item.GetExample())
          exnode.EndParsing()
          phnode.AddChild(exnode)

        phnode.EndParsing()
        node.AddChild(phnode)

    node.EndParsing()
    return node


class PhNode(base.ContentNode):
  '''A <ph> element.'''

  def _IsValidChild(self, child):
    return isinstance(child, ExNode)

  def MandatoryAttributes(self):
    return ['name']

  def EndParsing(self):
    super().EndParsing()
    # We only allow a single example for each placeholder
    if len(self.children) > 1:
      raise exception.TooManyExamples()

  def GetTextualIds(self):
    # The 'name' attribute is not an ID.
    return []


class ExNode(base.ContentNode):
  '''An <ex> element.'''
  pass
