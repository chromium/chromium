# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Base types for nodes in a GRIT resource tree.
'''

from __future__ import print_function

import ast
import os
import struct
import sys
from xml.sax import saxutils

import six

from grit import constants
from grit import clique
from grit import exception
from grit import util
from grit.node import brotli_util
import grit.format.gzip_string


class Node(object):
  '''An item in the tree that has children.'''

  # Valid content types that can be returned by _ContentType()
  _CONTENT_TYPE_NONE = 0   # No CDATA content but may have children
  _CONTENT_TYPE_CDATA = 1  # Only CDATA, no children.
  _CONTENT_TYPE_MIXED = 2  # CDATA and children, possibly intermingled

  # Default nodes to not whitelist skipped
  _whitelist_marked_as_skip = False

  # A class-static cache to speed up EvaluateExpression().
  # Keys are expressions (e.g. 'is_ios and lang == "fr"'). Values are tuples
  # (code, variables_in_expr) where code is the compiled expression and can be
  # directly eval'd, and variables_in_expr is the list of variable and method
  # names used in the expression (e.g. ['is_ios', 'lang']).
  eval_expr_cache = {}

  def __init__(self):
    self.children = []        # A list of child elements
    self.mixed_content = []   # A list of u'' and/or child elements (this
    # duplicates 'children' but
    # is needed to preserve markup-type content).
    self.name = u''           # The name of this element
    self.attrs = {}           # The set of attributes (keys to values)
    self.parent = None        # Our parent unless we are the root element.
    self.uberclique = None    # Allows overriding uberclique for parts of tree
    self.source = None        # File that this node was parsed from

  # This context handler allows you to write "with node:" and get a
  # line identifying the offending node if an exception escapes from the body
  # of the with statement.
  def __enter__(self):
    return self

  def __exit__(self, exc_type, exc_value, traceback):
    if exc_type is not None:
      print(u'Error processing node %s: %s' % (six.text_type(self), exc_value))

  def __iter__(self):
    '''A preorder iteration through the tree that this node is the root of.'''
    return self.Preorder()

  def Preorder(self):
    '''Generator that generates first this node, then the same generator for
    any child nodes.'''
    yield self
    for child in self.children:
      for iterchild in child.Preorder():
        yield iterchild

  def ActiveChildren(self):
    '''Returns the children of this node that should be included in the current
    configuration. Overridden by <if>.'''
    return [node for node in self.children if not node.WhitelistMarkedAsSkip()]

  def ActiveDescendants(self):
    '''Yields the current node and all descendants that should be included in
    the current configuration, in preorder.'''
    yield self
    for child in self.ActiveChildren():
      for descendant in child.ActiveDescendants():
        yield descendant

  def GetRoot(self):
    '''Returns the root Node in the tree this Node belongs to.'''
    curr = self
    while curr.parent:
      curr = curr.parent
    return curr

    # TODO(joi) Use this (currently untested) optimization?:
    #if hasattr(self, '_root'):
    #  return self._root
    #curr = self
    #while curr.parent and not hasattr(curr, '_root'):
    #  curr = curr.parent
    #if curr.parent:
    #  self._root = curr._root
    #else:
    #  self._root = curr
    #return self._root

  def StartParsing(self, name, parent):
    '''Called at the start of parsing.

    Args:
      name: u'elementname'
      parent: grit.node.base.Node or subclass or None
    '''
    assert isinstance(name, six.string_types)
    assert not parent or isinstance(parent, Node)
    self.name = name
    self.parent = parent

  def AddChild(self, child):
    '''Adds a child to the list of children of this node, if it is a valid
    child for the node.'''
    assert isinstance(child, Node)
    if (not self._IsValidChild(child) or
        self._ContentType() == self._CONTENT_TYPE_CDATA):
      explanation = 'invalid child %s for parent %s' % (str(child), self.name)
      raise exception.UnexpectedChild(explanation)
    self.children.append(child)
    self.mixed_content.append(child)

  def RemoveChild(self, child_id):
    '''Removes the first node that has a "name" attribute which
    matches "child_id" in the list of immediate children of
    this node.

    Args:
      child_id: String identifying the child to be removed
    '''
    index = 0
    # Safe not to copy since we only remove the first element found
    for child in self.children:
      name_attr = child.attrs['name']
      if name_attr == child_id:
        self.children.pop(index)
        self.mixed_content.pop(index)
        break
      index += 1

  def AppendContent(self, content):
    '''Appends a chunk of text as content of this node.

    Args:
      content: u'hello'

    Return:
      None
    '''
    assert isinstance(content, six.string_types)
    if self._ContentType() != self._CONTENT_TYPE_NONE:
      self.mixed_content.append(content)
    elif content.strip() != '':
      raise exception.UnexpectedContent()

  def HandleAttribute(self, attrib, value):
    '''Informs the node of an attribute that was parsed out of the GRD file
    for it.

    Args:
      attrib: 'name'
      value: 'fooblat'

    Return:
      None
    '''
    assert isinstance(attrib, six.string_types)
    assert isinstance(value, six.string_types)
    if self._IsValidAttribute(attrib, value):
      self.attrs[attrib] = value
    else:
      raise exception.UnexpectedAttribute(attrib)

  def EndParsing(self):
    '''Called at the end of parsing.'''

    # TODO(joi) Rewrite this, it's extremely ugly!
    if len(self.mixed_content):
      if isinstance(self.mixed_content[0], six.string_types):
        # Remove leading and trailing chunks of pure whitespace.
        while (len(self.mixed_content) and
               isinstance(self.mixed_content[0], six.string_types) and
               self.mixed_content[0].strip() == ''):
          self.mixed_content = self.mixed_content[1:]
        # Strip leading and trailing whitespace from mixed content chunks
        # at front and back.
        if (len(self.mixed_content) and
            isinstance(self.mixed_content[0], six.string_types)):
          self.mixed_content[0] = self.mixed_content[0].lstrip()
        # Remove leading and trailing ''' (used to demarcate whitespace)
        if (len(self.mixed_content) and
            isinstance(self.mixed_content[0], six.string_types)):
          if self.mixed_content[0].startswith("'''"):
            self.mixed_content[0] = self.mixed_content[0][3:]
    if len(self.mixed_content):
      if isinstance(self.mixed_content[-1], six.string_types):
        # Same stuff all over again for the tail end.
        while (len(self.mixed_content) and
               isinstance(self.mixed_content[-1], six.string_types) and
               self.mixed_content[-1].strip() == ''):
          self.mixed_content = self.mixed_content[:-1]
        if (len(self.mixed_content) and
            isinstance(self.mixed_content[-1], six.string_types)):
          self.mixed_content[-1] = self.mixed_content[-1].rstrip()
        if (len(self.mixed_content) and
            isinstance(self.mixed_content[-1], six.string_types)):
          if self.mixed_content[-1].endswith("'''"):
            self.mixed_content[-1] = self.mixed_content[-1][:-3]

    # Check that all mandatory attributes are there.
    for node_mandatt in self.MandatoryAttributes():
      mandatt_list = []
      if node_mandatt.find('|') >= 0:
        mandatt_list = node_mandatt.split('|')
      else:
        mandatt_list.append(node_mandatt)

      mandatt_option_found = False
      for mandatt in mandatt_list:
        assert mandatt not in self.DefaultAttributes()
        if mandatt in self.attrs:
          if not mandatt_option_found:
            mandatt_option_found = True
          else:
            raise exception.MutuallyExclusiveMandatoryAttribute(mandatt)

      if not mandatt_option_found:
        raise exception.MissingMandatoryAttribute(mandatt)

    # Add default attributes if not specified in input file.
    for defattr in self.DefaultAttributes():
      if not defattr in self.attrs:
        self.attrs[defattr] = self.DefaultAttributes()[defattr]

  def GetCdata(self):
    '''Returns all CDATA of this element, concatenated into a single
    string.  Note that this ignores any elements embedded in CDATA.'''
    return ''.join([c for c in self.mixed_content
                    if isinstance(c, six.string_types)])

  def __str__(self):
    '''Returns this node and all nodes below it as an XML document in a Unicode
    string.'''
    header = u'<?xml version="1.0" encoding="UTF-8"?>\n'
    return header + self.FormatXml()

  # Some Python 2 glue.
  __unicode__ = __str__

  def FormatXml(self, indent = u'', one_line = False):
    '''Returns this node and all nodes below it as an XML
    element in a Unicode string.  This differs from __unicode__ in that it does
    not include the <?xml> stuff at the top of the string.  If one_line is true,
    children and CDATA are layed out in a way that preserves internal
    whitespace.
    '''
    assert isinstance(indent, six.string_types)

    content_one_line = (one_line or
                        self._ContentType() == self._CONTENT_TYPE_MIXED)
    inside_content = self.ContentsAsXml(indent, content_one_line)

    # Then the attributes for this node.
    attribs = u''
    default_attribs = self.DefaultAttributes()
    for attrib, value in sorted(self.attrs.items()):
      # Only print an attribute if it is other than the default value.
      if attrib not in default_attribs or value != default_attribs[attrib]:
        attribs += u' %s=%s' % (attrib, saxutils.quoteattr(value))

    # Finally build the XML for our node and return it
    if len(inside_content) > 0:
      if one_line:
        return u'<%s%s>%s</%s>' % (self.name, attribs, inside_content, self.name)
      elif content_one_line:
        return u'%s<%s%s>\n%s  %s\n%s</%s>' % (
          indent, self.name, attribs,
          indent, inside_content,
          indent, self.name)
      else:
        return u'%s<%s%s>\n%s\n%s</%s>' % (
          indent, self.name, attribs,
          inside_content,
          indent, self.name)
    else:
      return u'%s<%s%s />' % (indent, self.name, attribs)

  def ContentsAsXml(self, indent, one_line):
    '''Returns the contents of this node (CDATA and child elements) in XML
    format.  If 'one_line' is true, the content will be laid out on one line.'''
    assert isinstance(indent, six.string_types)

    # Build the contents of the element.
    inside_parts = []
    last_item = None
    for mixed_item in self.mixed_content:
      if isinstance(mixed_item, Node):
        inside_parts.append(mixed_item.FormatXml(indent + u'  ', one_line))
        if not one_line:
          inside_parts.append(u'\n')
      else:
        message = mixed_item
        # If this is the first item and it starts with whitespace, we add
        # the ''' delimiter.
        if not last_item and message.lstrip() != message:
          message = u"'''" + message
        inside_parts.append(util.EncodeCdata(message))
      last_item = mixed_item

    # If there are only child nodes and no cdata, there will be a spurious
    # trailing \n
    if len(inside_parts) and inside_parts[-1] == '\n':
      inside_parts = inside_parts[:-1]

    # If the last item is a string (not a node) and ends with whitespace,
    # we need to add the ''' delimiter.
    if (isinstance(last_item, six.string_types) and
        last_item.rstrip() != last_item):
      inside_parts[-1] = inside_parts[-1] + u"'''"

    return u''.join(inside_parts)

  def SubstituteMessages(self, substituter):
    '''Applies substitutions to all messages in the tree.

    Called as a final step of RunGatherers.

    Args:
      substituter: a grit.util.Substituter object.
    '''
    for child in self.children:
      child.SubstituteMessages(substituter)

  def _IsValidChild(self, child):
    '''Returns true if 'child' is a valid child of this node.
    Overridden by subclasses.'''
    return False

  def _IsValidAttribute(self, name, value):
    '''Returns true if 'name' is the name of a valid attribute of this element
    and 'value' is a valid value for that attribute.  Overriden by
    subclasses unless they have only mandatory attributes.'''
    return (name in self.MandatoryAttributes() or
            name in self.DefaultAttributes())

  def _ContentType(self):
    '''Returns the type of content this element can have.  Overridden by
    subclasses.  The content type can be one of the _CONTENT_TYPE_XXX constants
    above.'''
    return self._CONTENT_TYPE_NONE

  def MandatoryAttributes(self):
    '''Returns a list of attribute names that are mandatory (non-optional)
    on the current element. One can specify a list of
    "mutually exclusive mandatory" attributes by specifying them as one
    element in the list, separated by a "|" character.
    '''
    return []

  def DefaultAttributes(self):
    '''Returns a dictionary of attribute names that have defaults, mapped to
    the default value.  Overridden by subclasses.'''
    return {}

  def GetCliques(self):
    '''Returns all MessageClique objects belonging to this node.  Overridden
    by subclasses.

    Return:
      [clique1, clique2] or []
    '''
    return []

  def ToRealPath(self, path_from_basedir):
    '''Returns a real path (which can be absolute or relative to the current
    working directory), given a path that is relative to the base directory
    set for the GRIT input file.

    Args:
      path_from_basedir: '..'

    Return:
      'resource'
    '''
    return util.normpath(os.path.join(self.GetRoot().GetBaseDir(),
                                      os.path.expandvars(path_from_basedir)))

  def GetInputPath(self):
    '''Returns a path, relative to the base directory set for the grd file,
    that points to the file the node refers to.
    '''
    # This implementation works for most nodes that have an input file.
    return self.attrs['file']

  def UberClique(self):
    '''Returns the uberclique that should be used for messages originating in
    a given node.  If the node itself has its uberclique set, that is what we
    use, otherwise we search upwards until we find one.  If we do not find one
    even at the root node, we set the root node's uberclique to a new
    uberclique instance.
    '''
    node = self
    while not node.uberclique and node.parent:
      node = node.parent
    if not node.uberclique:
      node.uberclique = clique.UberClique()
    return node.uberclique

  def IsTranslateable(self):
    '''Returns false if the node has contents that should not be translated,
    otherwise returns false (even if the node has no contents).
    '''
    if not 'translateable' in self.attrs:
      return True
    else:
      return self.attrs['translateable'] == 'true'

  def GetNodeById(self, id):
    '''Returns the node in the subtree parented by this node that has a 'name'
    attribute matching 'id'.  Returns None if no such node is found.
    '''
    for node in self:
      if 'name' in node.attrs and node.attrs['name'] == id:
        return node
    return None

  def GetChildrenOfType(self, type):
    '''Returns a list of all subnodes (recursing to all leaves) of this node
    that are of the indicated type (or tuple of types).

    Args:
      type: A type you could use with isinstance().

    Return:
      A list, possibly empty.
    '''
    return [child for child in self if isinstance(child, type)]

  def GetTextualIds(self):
    '''Returns a list of the textual ids of this node.
    '''
    if 'name' in self.attrs:
      return [self.attrs['name']]
    return []

  @classmethod
  def EvaluateExpression(cls, expr, defs, target_platform, extra_variables={}):
    '''Worker for EvaluateCondition (below) and conditions in XTB files.'''
    if expr in cls.eval_expr_cache:
      code, variables_in_expr = cls.eval_expr_cache[expr]
    else:
      # Get a list of all variable and method names used in the expression.
      syntax_tree = ast.parse(expr, mode='eval')
      variables_in_expr = [node.id for node in ast.walk(syntax_tree) if
          isinstance(node, ast.Name) and node.id not in ('True', 'False')]
      code = compile(syntax_tree, filename='<string>', mode='eval')
      cls.eval_expr_cache[expr] = code, variables_in_expr

    # Set values only for variables that are needed to eval the expression.
    variable_map = {}
    for name in variables_in_expr:
      if name == 'os':
        value = target_platform
      elif name == 'defs':
        value = defs

      elif name == 'is_linux':
        value = target_platform.startswith('linux')
      elif name == 'is_macosx':
        value = target_platform == 'darwin'
      elif name == 'is_win':
        value = target_platform in ('cygwin', 'win32')
      elif name == 'is_android':
        value = target_platform == 'android'
      elif name == 'is_ios':
        value = target_platform == 'ios'
      elif name == 'is_bsd':
        value = 'bsd' in target_platform
      elif name == 'is_posix':
        value = (target_platform in ('darwin', 'linux2', 'linux3', 'sunos5',
                                     'android', 'ios')
                 or 'bsd' in target_platform)

      elif name == 'pp_ifdef':
        def pp_ifdef(symbol):
          return symbol in defs
        value = pp_ifdef
      elif name == 'pp_if':
        def pp_if(symbol):
          return defs.get(symbol, False)
        value = pp_if

      elif name in defs:
        value = defs[name]
      elif name in extra_variables:
        value = extra_variables[name]
      else:
        # Undefined variables default to False.
        value = False

      variable_map[name] = value

    eval_result = eval(code, {}, variable_map)
    assert isinstance(eval_result, bool)
    return eval_result

  def EvaluateCondition(self, expr):
    '''Returns true if and only if the Python expression 'expr' evaluates
    to true.

    The expression is given a few local variables:
      - 'lang' is the language currently being output
           (the 'lang' attribute of the <output> element).
      - 'context' is the current output context
           (the 'context' attribute of the <output> element).
      - 'defs' is a map of C preprocessor-style symbol names to their values.
      - 'os' is the current platform (likely 'linux2', 'win32' or 'darwin').
      - 'pp_ifdef(symbol)' is a shorthand for "symbol in defs".
      - 'pp_if(symbol)' is a shorthand for "symbol in defs and defs[symbol]".
      - 'is_linux', 'is_macosx', 'is_win', 'is_posix' are true if 'os'
           matches the given platform.
    '''
    root = self.GetRoot()
    lang = getattr(root, 'output_language', '')
    context = getattr(root, 'output_context', '')
    defs = getattr(root, 'defines', {})
    target_platform = getattr(root, 'target_platform', '')
    extra_variables = {
        'lang': lang,
        'context': context,
    }
    return Node.EvaluateExpression(
        expr, defs, target_platform, extra_variables)

  def OnlyTheseTranslations(self, languages):
    '''Turns off loading of translations for languages not in the provided list.

    Attrs:
      languages: ['fr', 'zh_cn']
    '''
    for node in self:
      if (hasattr(node, 'IsTranslation') and
          node.IsTranslation() and
          node.GetLang() not in languages):
        node.DisableLoading()

  def FindBooleanAttribute(self, attr, default, skip_self):
    '''Searches all ancestors of the current node for the nearest enclosing
    definition of the given boolean attribute.

    Args:
      attr: 'fallback_to_english'
      default: What to return if no node defines the attribute.
      skip_self: Don't check the current node, only its parents.
    '''
    p = self.parent if skip_self else self
    while p:
      value = p.attrs.get(attr, 'default').lower()
      if value != 'default':
        return (value == 'true')
      p = p.parent
    return default

  def PseudoIsAllowed(self):
    '''Returns true if this node is allowed to use pseudo-translations.  This
    is true by default, unless this node is within a <release> node that has
    the allow_pseudo attribute set to false.
    '''
    return self.FindBooleanAttribute('allow_pseudo',
                                     default=True, skip_self=True)

  def ShouldFallbackToEnglish(self):
    '''Returns true iff this node should fall back to English when
    pseudotranslations are disabled and no translation is available for a
    given message.
    '''
    return self.FindBooleanAttribute('fallback_to_english',
                                     default=False, skip_self=True)

  def WhitelistMarkedAsSkip(self):
    '''Returns true if the node is marked to be skipped in the output by a
    whitelist.
    '''
    return self._whitelist_marked_as_skip

  def SetWhitelistMarkedAsSkip(self, mark_skipped):
    '''Sets WhitelistMarkedAsSkip.
    '''
    self._whitelist_marked_as_skip = mark_skipped

  def ExpandVariables(self):
    '''Whether we need to expand variables on a given node.'''
    return False

  def IsResourceMapSource(self):
    '''Whether this node is a resource map source.'''
    return False

  def CompressDataIfNeeded(self, data):
    '''Compress data using the format specified in the compress attribute.

    Args:
      data: The data to compressed.
    Returns:
      The data in gzipped or brotli compressed format. If the format is
      unspecified then this returns the data uncompressed.
    '''
    if self.attrs.get('compress') == 'gzip':
      # We only use rsyncable compression on Linux.
      # We exclude ChromeOS since ChromeOS bots are Linux based but do not have
      # the --rsyncable option built in for gzip. See crbug.com/617950.
      if sys.platform == 'linux2' and 'chromeos' not in self.GetRoot().defines:
        return grit.format.gzip_string.GzipStringRsyncable(data)
      return grit.format.gzip_string.GzipString(data)

    elif self.attrs.get('compress') == 'brotli':
      # The length of the uncompressed data as 8 bytes little-endian.
      size_bytes = struct.pack("<q", len(data))
      data = brotli_util.BrotliCompress(data)
      # BROTLI_CONST is prepended to brotli decompressed data in order to
      # easily check if a resource has been brotli compressed.
      # The length of the uncompressed data is also appended to the start,
      # truncated to 6 bytes, little-endian. size_bytes is 8 bytes,
      # need to truncate further to 6.
      formatter = b'%ds %dx %ds' % (6, 2, len(size_bytes) - 8)
      return (constants.BROTLI_CONST +
             b''.join(struct.unpack(formatter, size_bytes)) +
             data)

    elif self.attrs.get('compress') == 'false':
      return data

    else:
      raise Exception('Invalid value for compression')


class ContentNode(Node):
  '''Convenience baseclass for nodes that can have content.'''
  def _ContentType(self):
    return self._CONTENT_TYPE_MIXED
