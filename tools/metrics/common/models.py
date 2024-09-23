# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Types for building models of metric description xml files.

UMA uses several XML files to allow clients to describe the metrics that they
collect, e.g.
https://chromium.googlesource.com/chromium/src/+/main/tools/metrics/rappor/rappor.xml

These types can be used to build models that describe the canonical formatted
structure of these files, and the models can be used to extract the contents of
those files, or convert content back into a canonicalized version of the file.
"""

import abc
import re
import xml.etree.ElementTree as ET
from xml.dom import minidom
import pretty_print_xml


# Non-basic type keys for storing comments and text attributes, so they don't
# conflict with regular keys, and can be skipped in JSON serialization.
PRECEDING_COMMENT_KEY = ('preceding_comment')
TRAILING_COMMENT_KEY = ('trailing_comment')
TEXT_KEY = ('text')


def IsTrailingComment(comment: str) -> bool:
  """Returns whether this comment is a trailing comment.

  In this context a trailing comment is one which should be anchored to the
  preceding node, rather than the following node. All comments that are not
  trailing comments are assumed to be anchored to the following node.
  """
  return comment.strip().startswith('LINT.ThenChange')


def GetPrecedingCommentsForNode(node):
  """Extracts comments in the current node.

  Args:
    node: The DOM node to extract comments from.

  Returns:
    A list of comment DOM nodes.
  """
  comments = []
  node = node.previousSibling
  while node:
    if node.nodeType == minidom.Node.COMMENT_NODE:
      if not IsTrailingComment(node.data):
        comments.append(node.data)
    elif node.nodeType != minidom.Node.TEXT_NODE:
      break
    node = node.previousSibling
  return comments[::-1]


def GetTrailingCommentsForNode(node):
  """Extracts comments in the current node.

  Args:
    node: The DOM node to extract comments from.

  Returns:
    A list of comment DOM nodes.
  """
  comments = []
  node = node.nextSibling
  while node:
    if node.nodeType == minidom.Node.COMMENT_NODE:
      if IsTrailingComment(node.data):
        comments.append(node.data)
    elif node.nodeType != minidom.Node.TEXT_NODE:
      break
    node = node.nextSibling
  return comments[::-1]


def PutCommentsInNode(doc, node, comments):
  """Appends comments to the DOM node.

  Args:
    doc: The document to create a comment in.
    node: The DOM node to write comments to.
    comments: A list of comments.
  """
  for comment in comments:
    node.appendChild(doc.createComment(comment))


def GetChildrenByTag(node, tag):
  """Gets all children of a particular tag type.

  Args:
    node: The DOM node to write comments to.
    tag: The tag of the nodes to collect.
  Returns:
    A list of DOM nodes.
  """
  return [child for child in node.childNodes if child.nodeName == tag]


def GetUnexpectedChildren(node, tags):
  """Gets a set of unexpected children from |node|."""
  # Ingore text and comment nodes.
  return (set(child.nodeName for child in node.childNodes) - set(tags) - set(
      ('#comment', '#text')))


class NodeType(object):
  """Base type for a type of XML node.

  Args:
    indent: True iff this node should have its children indented when pretty
        printing.
    extra_newlines: None or a triple of integers describing the number of
        newlines that should be printed (after_open, before_close, after_close)
    single_line: True iff this node may be squashed into a single line.
    alphabetization: A list of [(tag, keyfn)] pairs, which specify the tags of
       the children that should be sorted, and the functions to get sort keys
       from xml nodes.
  """
  __metaclass__ = abc.ABCMeta

  def __init__(self, tag,
               indent=True,
               extra_newlines=None,
               single_line=False,
               alphabetization=None):
    self.tag = tag
    self.indent = indent
    self.extra_newlines = extra_newlines
    self.single_line = single_line
    self.alphabetization = alphabetization

  @abc.abstractmethod
  def Unmarshall(self, node):
    """Extracts the content of the node to an object.

    Args:
      node: The XML node to extract data from.

    Returns:
      An object extracted from the node.
    """

  @abc.abstractmethod
  def Marshall(self, doc, obj):
    """Converts an object into an XML node of this type.

    Args:
      doc: A document create an XML node in.
      obj: The object to be encoded into the XML.

    Returns:
      An XML node encoding the object.
    """

  def GetPrecedingComments(self, obj):
    """Gets comments for the object being encoded.

    Args:
      obj: The object to be encoded into the XML.

    Returns:
      A list of comment nodes for the object.
    """
    del obj  # Used in ObjectNodeType implementation
    # The base NodeType does not store comments
    return []

  def GetTrailingComments(self, obj):
    """Gets comments for the object being encoded.

    Args:
      obj: The object to be encoded into the XML.

    Returns:
      A list of comment nodes for the object.
    """
    del obj  # Used in ObjectNodeType implementation
    # The base NodeType does not store comments
    return []

  def MarshallIntoNode(self, doc, node, obj):
    """Marshalls the object and appends it to a node, with comments.

    Args:
      doc: A document create an XML node in.
      node: An XML node to marshall the object into.
      obj: The object to be encoded into the XML.
    """
    PutCommentsInNode(doc, node, self.GetPrecedingComments(obj))
    node.appendChild(self.Marshall(doc, obj))
    PutCommentsInNode(doc, node, self.GetTrailingComments(obj))

  def GetAttributes(self):
    """Gets a sorted list of attributes that this node can have.

    Returns:
      A list of names of XML attributes, sorted by the order they should appear.
    """
    return []

  def GetRequiredAttributes(self):
    """Gets a list of required attributes that this node has.

    Returns:
      A list of names of required attributes of the node.
    """
    return []

  def GetNodeTypes(self):
    """Gets a map of tags to node types for all dependent types.

    Returns:
      A map of tags to node-types for this node and all of the nodes that it
      can contain.
    """
    return {self.tag: self}


class TextNodeType(NodeType):
  """A type for simple nodes that just have a tag and some text content.

  Unmarshalls nodes to strings.

  Args:
    tag: The name of XML tag for this type of node.
  """

  def __str__(self):
    return 'TextNodeType("%s")' % self.tag

  def Unmarshall(self, node):
    """Extracts the content of the node to an object.

    Args:
      node: The XML node to extract data from.

    Returns:
      The object representation of the node.
    """

    obj = {}
    obj[PRECEDING_COMMENT_KEY] = GetPrecedingCommentsForNode(node)
    obj[TRAILING_COMMENT_KEY] = GetTrailingCommentsForNode(node)

    if not node.firstChild:
      return obj
    text = node.firstChild.nodeValue
    obj[TEXT_KEY] = '\n\n'.join(pretty_print_xml.SplitParagraphs(text))

    # TextNode shouldn't have any child.
    unexpected = GetUnexpectedChildren(node, set())
    if unexpected:
      raise ValueError("Unexpected children: %s in <%s> node" %
                       (','.join(unexpected), self.tag))

    return obj

  def Marshall(self, doc, obj):
    """Converts an object into an XML node of this type.

    Args:
      doc: A document to create an XML node in.
      obj: An object to be encoded into the XML.

    Returns:
      An XML node encoding the object.
    """
    node = doc.createElement(self.tag)
    text = obj.get(TEXT_KEY)
    if text:
      node.appendChild(doc.createTextNode(text))
    return node

  def GetPrecedingComments(self, obj):
    """Gets comments for the object being encoded.

    Args:
      obj: The object to be encoded into the XML.

    Returns:
      A list of comment nodes for the object.
    """
    return obj[PRECEDING_COMMENT_KEY]

  def GetTrailingComments(self, obj):
    """Gets comments for the object being encoded.

    Args:
      obj: The object to be encoded into the XML.

    Returns:
      A list of comment nodes for the object.
    """
    return obj[TRAILING_COMMENT_KEY]

class ChildType(object):
  """Metadata about a node type's children.

  Args:
    attr: The field name of the parents model object storing the child's model.
    node_type: The NodeType of the child.
    multiple: True if the child can be repeated.
  """

  def __init__(self, attr, node_type, multiple):
    self.attr = attr
    self.node_type = node_type
    self.multiple = multiple


class ObjectNodeType(NodeType):
  r"""A complex node type that has attributes or other nodes as children.

  Unmarshalls nodes to objects.

  Args:
    tag: The name of XML tag for this type of node.
    attributes: A list of (name, type, regex) tubles, e.g. [('foo', unicode,
        r'^\w+$')].  The order of the attributes determines the ordering of
        attributes, when serializing objects to XML. The "regex" can be None
        to do no validation, otherwise the attribute must match that pattern.
    text_attribute: An attribute stored in the text content of the node.
    children: A list of ChildTypes describing the objects' children.

  Raises:
    ValueError: Attributes contains duplicate definitions.
  """

  def __init__(self,
               tag,
               attributes=None,
               required_attributes=None,
               children=None,
               text_attribute=None,
               **kwargs):
    NodeType.__init__(self, tag, **kwargs)
    self.attributes = attributes or []
    self.required_attributes = required_attributes or []
    self.children = children or []
    self.text_attribute = text_attribute
    if len(self.attributes) != len(set(a for a, _, _ in self.attributes)):
      raise ValueError('Duplicate attribute definition.')

  def __str__(self):
    return 'ObjectNodeType("%s")' % self.tag

  def Unmarshall(self, node):
    """Extracts the content of the node to an object.

    Args:
      node: The XML node to extract data from.

    Returns:
      An object extracted from the node.

    Raises:
      ValueError: The node is missing required children.
    """
    obj = {}
    obj[PRECEDING_COMMENT_KEY] = GetPrecedingCommentsForNode(node)
    obj[TRAILING_COMMENT_KEY] = GetTrailingCommentsForNode(node)

    for attr, attr_type, attr_re in self.attributes:
      if node.hasAttribute(attr):
        obj[attr] = attr_type(node.getAttribute(attr))
      if attr_re is not None:
        attr_val = obj.get(attr, '')
        if not re.match(attr_re, attr_val):
          raise ValueError('%s "%s" does not match regex "%s"' %
                           (attr, attr_val, attr_re))

    # We need to iterate through all the children and get their nodeValue,
    # to account for the cases where other children node precedes the text
    # attribute.
    obj[self.text_attribute] = ''
    child = node.firstChild
    while child:
      obj[self.text_attribute] += (child.nodeValue.strip()
                                   if child.nodeValue else '')
      child = child.nextSibling

    # This prevents setting a None key with empty string value
    if obj[self.text_attribute] == '':
      del obj[self.text_attribute]

    for child in self.children:
      nodes = GetChildrenByTag(node, child.node_type.tag)
      if child.multiple:
        obj[child.attr] = [
            child.node_type.Unmarshall(n) for n in nodes]
      elif nodes:
        obj[child.attr] = child.node_type.Unmarshall(nodes[0])

    unexpected = GetUnexpectedChildren(
        node, set([child.node_type.tag for child in self.children]))
    if unexpected:
      raise ValueError("Unexpected children: %s in <%s> node" %
                       (','.join(unexpected), self.tag))

    return obj

  def Marshall(self, doc, obj):
    """Converts an object into an XML node of this type.

    Args:
      doc: A document create an XML node in.
      obj: The object to be encoded into the XML.

    Returns:
      An XML node encoding the object.
    """
    node = doc.createElement(self.tag)
    for attr, _, _ in self.attributes:
      if attr in obj:
        node.setAttribute(attr, str(obj[attr]))

    if self.text_attribute and self.text_attribute in obj:
      node.appendChild(doc.createTextNode(obj[self.text_attribute]))

    for child in self.children:
      if child.multiple:
        for child_obj in obj[child.attr]:
          child.node_type.MarshallIntoNode(doc, node, child_obj)
      elif child.attr in obj:
        child.node_type.MarshallIntoNode(doc, node, obj[child.attr])
    return node

  def GetPrecedingComments(self, obj):
    """Gets comments for the object being encoded.

    Args:
      obj: The object to be encoded into the XML.

    Returns:
      A list of comment nodes for the object.
    """
    return obj[PRECEDING_COMMENT_KEY]

  def GetTrailingComments(self, obj):
    """Gets comments for the object being encoded.

    Args:
      obj: The object to be encoded into the XML.

    Returns:
      A list of comment nodes for the object.
    """
    return obj[TRAILING_COMMENT_KEY]

  def GetAttributes(self):
    """Gets a sorted list of attributes that this node can have.

    Returns:
      A list of names of XML attributes, sorted by the order they should appear.
    """
    return [attr for attr, _, _ in self.attributes]

  def GetRequiredAttributes(self):
    """Gets a list of required attributes that this node has.

    Returns:
      A list of names of required attributes, or an empty list if there is no
      required attribute.
    """
    return self.required_attributes or []

  def GetNodeTypes(self):
    """Get a map of tags to node types for all dependent types.

    Returns:
      A map of tags to node-types for this node and all of the nodes that it
      can contain.
    """
    types = {self.tag: self}
    for child in self.children:
      types.update(child.node_type.GetNodeTypes())
    return types


class DocumentType(object):
  """Model for the root of an XML description file.

  Args:
    root_type: A NodeType describing the root tag of the document.
  """

  def __init__(self, root_type):
    self.root_type = root_type

  def _ParseMinidom(self, minidom_doc):
    """Parses the input minidom document

    Args:
      minidom_doc: The input minidom document

    Returns:
      An object representing the unmarshalled content of the document's root
      node.
    """
    root = minidom_doc.getElementsByTagName(self.root_type.tag)[0]
    return self.root_type.Unmarshall(root)

  def Parse(self, input_file):
    """Parses the input file, which can be minidom, ET or xml string.

    The flexibility of input is to accommodate the currently different
    representations of ukm, enums, histograms and actions in their
    respective pretty_print.py.

    Args:
      input_file: The input file can be given in the form of minidom, ET
      or xml string.

    Returns:
      An object representing the unmarshalled content of the document's root
      node.
    """
    if not isinstance(input_file, minidom.Document):
      if isinstance(input_file, ET.Element):
        input_file = ET.tostring(input_file, encoding='utf-8', method='xml')
      input_file = minidom.parseString(input_file)
    return self._ParseMinidom(input_file)

  def GetPrintStyle(self):
    """Gets an XmlStyle object for pretty printing a document of this type.

    Returns:
      An XML style object.
    """
    types = self.root_type.GetNodeTypes()
    return pretty_print_xml.XmlStyle(
        attribute_order={t: types[t].GetAttributes()
                         for t in types},
        required_attributes={
            t: types[t].GetRequiredAttributes()
            for t in types
        },
        tags_that_have_extra_newline={
            t: types[t].extra_newlines
            for t in types if types[t].extra_newlines
        },
        tags_that_dont_indent=[t for t in types if not types[t].indent],
        tags_that_allow_single_line=[t for t in types if types[t].single_line],
        tags_alphabetization_rules={
            t: types[t].alphabetization
            for t in types if types[t].alphabetization
        })

  def _ToXML(self, obj):
    """Converts an object into an XML document.

    Args:
      obj: An object to serialize to XML.

    Returns:
      An XML minidom Document object.
    """
    doc = minidom.Document()
    self.root_type.MarshallIntoNode(doc, doc, obj)
    return doc

  def PrettyPrint(self, obj):
    """Converts an object into pretty-printed XML as a string.

    Args:
      obj: An object to serialize to XML.

    Returns:
      A string containing pretty printed XML.
    """
    return self.GetPrintStyle().PrettyPrintXml(self._ToXML(obj))
