# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The 'grit android2grd' tool."""


import getopt
import os.path
import sys
from xml.dom import Node
import xml.dom.minidom

from io import StringIO

import grit.node.empty
from grit.node import node_io
from grit.node import message

from grit.tool import interface

from grit import grd_reader
from grit import lazy_re
from grit import tclib


# The name of a string in strings.xml
_STRING_NAME = lazy_re.compile(r'[a-z0-9_]+\Z')

# A string's character limit in strings.xml
_CHAR_LIMIT = lazy_re.compile(r'\[CHAR_LIMIT=(\d+)\]')

# Finds String.Format() style format specifiers such as "%-5.2f".
_FORMAT_SPECIFIER = lazy_re.compile(
  r'%'
  r'([1-9][0-9]*\$|<)?'            # argument_index
  r'([-#+ 0,(]*)'                  # flags
  r'([0-9]+)?'                     # width
  r'(\.[0-9]+)?'                   # precision
  r'([bBhHsScCdoxXeEfgGaAtT%n])')  # conversion


class Android2Grd(interface.Tool):
  """Tool for converting Android string.xml files into chrome Grd files.

Usage: grit [global options] android2grd [OPTIONS] STRINGS_XML

The Android2Grd tool will convert an Android strings.xml file (whose path is
specified by STRINGS_XML) and create a chrome style grd file containing the
relevant information.

Because grd documents are much richer than strings.xml documents we supplement
the information required by grds using OPTIONS with sensible defaults.

OPTIONS may be any of the following:

    --name       FILENAME    Specify the base FILENAME. This should be without
                             any file type suffix. By default
                             "chrome_android_strings" will be used.

    --languages  LANGUAGES   Comma separated list of ISO language codes (e.g.
                             en-US, en-GB, ru, zh-CN). These codes will be used
                             to determine the names of resource and translations
                             files that will be declared by the output grd file.

    --grd-dir    GRD_DIR     Specify where the resultant grd file
                             (FILENAME.grd) should be output. By default this
                             will be the present working directory.

    --header-dir HEADER_DIR  Specify the location of the directory where grit
                             generated C++ headers (whose name will be
                             FILENAME.h) will be placed. Use an empty string to
                             disable rc generation. Default: empty.

    --rc-dir     RC_DIR      Specify the directory where resource files will
                             be located relative to grit build's output
                             directory. Use an empty string to disable rc
                             generation. Default: empty.

    --xml-dir    XML_DIR     Specify where to place localized strings.xml files
                             relative to grit build's output directory. For each
                             language xx a values-xx/strings.xml file will be
                             generated. Use an empty string to disable
                             strings.xml generation. Default: '.'.

    --xtb-dir    XTB_DIR     Specify where the xtb files containing translations
                             will be located relative to the grd file. Default:
                             '.'.
"""

  _NAME_FLAG = 'name'
  _LANGUAGES_FLAG = 'languages'
  _GRD_DIR_FLAG = 'grd-dir'
  _RC_DIR_FLAG = 'rc-dir'
  _HEADER_DIR_FLAG = 'header-dir'
  _XTB_DIR_FLAG = 'xtb-dir'
  _XML_DIR_FLAG = 'xml-dir'

  def __init__(self):
    self.name = 'chrome_android_strings'
    self.languages = []
    self.grd_dir = '.'
    self.rc_dir = None
    self.xtb_dir = '.'
    self.xml_res_dir = '.'
    self.header_dir = None

  def ShortDescription(self):
    """Returns a short description of the Android2Grd tool.

    Overridden from grit.interface.Tool

    Returns:
      A string containing a short description of the android2grd tool.
    """
    return 'Converts Android string.xml files into Chrome grd files.'

  def ParseOptions(self, args):
    """Set this objects and return all non-option arguments."""
    flags = [
        Android2Grd._NAME_FLAG,
        Android2Grd._LANGUAGES_FLAG,
        Android2Grd._GRD_DIR_FLAG,
        Android2Grd._RC_DIR_FLAG,
        Android2Grd._HEADER_DIR_FLAG,
        Android2Grd._XTB_DIR_FLAG,
        Android2Grd._XML_DIR_FLAG, ]
    (opts, args) = getopt.getopt(
        args, None, ['%s=' % o for o in flags] + ['help'])

    for key, val in opts:
      # Get rid of the preceding hypens.
      k = key[2:]
      if k == Android2Grd._NAME_FLAG:
        self.name = val
      elif k == Android2Grd._LANGUAGES_FLAG:
        self.languages = val.split(',')
      elif k == Android2Grd._GRD_DIR_FLAG:
        self.grd_dir = val
      elif k == Android2Grd._RC_DIR_FLAG:
        self.rc_dir = val
      elif k == Android2Grd._HEADER_DIR_FLAG:
        self.header_dir = val
      elif k == Android2Grd._XTB_DIR_FLAG:
        self.xtb_dir = val
      elif k == Android2Grd._XML_DIR_FLAG:
        self.xml_res_dir = val
      elif k == 'help':
        self.ShowUsage()
        sys.exit(0)
    return args

  def Run(self, opts, args):
    """Runs the Android2Grd tool.

    Inherited from grit.interface.Tool.

    Args:
      opts: List of string arguments that should be parsed.
      args: String containing the path of the strings.xml file to be converted.
    """
    args = self.ParseOptions(args)
    if len(args) != 1:
      print('Tool requires one argument, the path to the Android '
            'strings.xml resource file to be converted.')
      return 2
    self.SetOptions(opts)

    android_path = args[0]

    # Read and parse the Android strings.xml file.
    with open(android_path) as android_file:
      android_dom = xml.dom.minidom.parse(android_file)

    # Do the hard work -- convert the Android dom to grd file contents.
    grd_dom = self.AndroidDomToGrdDom(android_dom)
    grd_string = str(grd_dom)

    # Write the grd string to a file in grd_dir.
    grd_filename = self.name + '.grd'
    grd_path = os.path.join(self.grd_dir, grd_filename)
    with open(grd_path, 'w') as grd_file:
      grd_file.write(grd_string)

  def AndroidDomToGrdDom(self, android_dom):
    """Converts a strings.xml DOM into a DOM representing the contents of
    a grd file.

    Args:
      android_dom: A xml.dom.Document containing the contents of the Android
          string.xml document.
    Returns:
      The DOM for the grd xml document produced by converting the Android DOM.
    """

    # Start with a basic skeleton for the .grd file.
    root = grd_reader.Parse(StringIO(
      '''<?xml version="1.0" encoding="UTF-8"?>
         <grit base_dir="." latest_public_release="0"
             current_release="1" source_lang_id="en">
           <outputs />
           <translations />
           <release seq="1">
             <messages fallback_to_english="true" />
           </release>
         </grit>'''), dir='.')
    outputs = root.children[0]
    translations = root.children[1]
    messages = root.children[2].children[0]
    assert (isinstance(messages, grit.node.empty.MessagesNode) and
            isinstance(translations, grit.node.empty.TranslationsNode) and
            isinstance(outputs, grit.node.empty.OutputsNode))

    if self.header_dir:
      cpp_header = self.__CreateCppHeaderOutputNode(outputs, self.header_dir)
    for lang in self.languages:
      # Create an output element for each language.
      if self.rc_dir:
        self.__CreateRcOutputNode(outputs, lang, self.rc_dir)
      if self.xml_res_dir:
        self.__CreateAndroidXmlOutputNode(outputs, lang, self.xml_res_dir)
      if lang != 'en':
        self.__CreateFileNode(translations, lang)
    # Convert all the strings.xml strings into grd messages.
    self.__CreateMessageNodes(messages, android_dom.documentElement)

    return root

  def __CreateMessageNodes(self, messages, resources):
    """Creates the <message> elements and adds them as children of <messages>.

    Args:
      messages: the <messages> element in the strings.xml dom.
      resources: the <resources> element in the grd dom.
    """
    # <string> elements contain the definition of the resource.
    # The description of a <string> element is contained within the comment
    # node element immediately preceeding the string element in question.
    description = ''
    for child in resources.childNodes:
      if child.nodeType == Node.COMMENT_NODE:
        # Remove leading/trailing whitespace; collapse consecutive whitespaces.
        description = ' '.join(child.data.split())
      elif child.nodeType == Node.ELEMENT_NODE:
        if child.tagName != 'string':
          print('Warning: ignoring unknown tag <%s>' % child.tagName)
        else:
          translatable = self.IsTranslatable(child)
          raw_name = child.getAttribute('name')
          if not _STRING_NAME.match(raw_name):
            print('Error: illegal string name: %s' % raw_name)
          grd_name = 'IDS_' + raw_name.upper()
          # Transform the <string> node contents into a tclib.Message, taking
          # care to handle whitespace transformations and escaped characters,
          # and coverting <xliff:g> placeholders into <ph> placeholders.
          msg = self.CreateTclibMessage(child)
          msg_node = self.__CreateMessageNode(messages, grd_name, description,
              msg, translatable)
          messages.AddChild(msg_node)
          # Reset the description once a message has been parsed.
          description = ''

  def CreateTclibMessage(self, android_string):
    """Transforms a <string/> element from strings.xml into a tclib.Message.

    Interprets whitespace, quotes, and escaped characters in the android_string
    according to Android's formatting and styling rules for strings.  Also
    converts <xliff:g> placeholders into <ph> placeholders, e.g.:

      <xliff:g id="website" example="google.com">%s</xliff:g>
        becomes
      <ph name="website"><ex>google.com</ex>%s</ph>

    Returns:
      The tclib.Message.
    """
    msg = tclib.Message()
    current_text = ''  # Accumulated text that hasn't yet been added to msg.
    nodes = android_string.childNodes

    for i, node in enumerate(nodes):
      # Handle text nodes.
      if node.nodeType in (Node.TEXT_NODE, Node.CDATA_SECTION_NODE):
        current_text += node.data

      # Handle <xliff:g> and other tags.
      elif node.nodeType == Node.ELEMENT_NODE:
        if node.tagName == 'xliff:g':
          assert node.hasAttribute('id'), 'missing id: ' + node.data()
          placeholder_id = node.getAttribute('id')
          placeholder_text = self.__FormatPlaceholderText(node)
          placeholder_example = node.getAttribute('example')
          if not placeholder_example:
            print('Info: placeholder does not contain an example: %s' %
                  node.toxml())
            placeholder_example = placeholder_id.upper()
          msg.AppendPlaceholder(tclib.Placeholder(placeholder_id,
              placeholder_text, placeholder_example))
        else:
          print('Warning: removing tag <%s> which must be inside a '
                'placeholder: %s' % (node.tagName, node.toxml()))
          msg.AppendText(self.__FormatPlaceholderText(node))

      # Handle other nodes.
      elif node.nodeType != Node.COMMENT_NODE:
        assert False, 'Unknown node type: %s' % node.nodeType

      is_last_node = (i == len(nodes) - 1)
      if (current_text and
          (is_last_node or nodes[i + 1].nodeType == Node.ELEMENT_NODE)):
        # For messages containing just text and comments (no xml tags) Android
        # strips leading and trailing whitespace.  We mimic that behavior.
        if not msg.GetContent() and is_last_node:
          current_text = current_text.strip()
        msg.AppendText(self.__FormatAndroidString(current_text))
        current_text = ''

    return msg

  def __FormatAndroidString(self, android_string, inside_placeholder=False):
    r"""Returns android_string formatted for a .grd file.

      * Collapses consecutive whitespaces, except when inside double-quotes.
      * Replaces \\, \n, \t, \", \' with \, newline, tab, ", '.
    """
    backslash_map = {'\\' : '\\', 'n' : '\n', 't' : '\t', '"' : '"', "'" : "'"}
    is_quoted_section = False  # True when we're inside double quotes.
    is_backslash_sequence = False  # True after seeing an unescaped backslash.
    prev_char = ''
    output = []
    for c in android_string:
      if is_backslash_sequence:
        # Unescape \\, \n, \t, \", and \'.
        assert c in backslash_map, 'Illegal escape sequence: \\%s' % c
        output.append(backslash_map[c])
        is_backslash_sequence = False
      elif c == '\\':
        is_backslash_sequence = True
      elif c.isspace() and not is_quoted_section:
        # Turn whitespace into ' ' and collapse consecutive whitespaces.
        if not prev_char.isspace():
          output.append(' ')
      elif c == '"':
        is_quoted_section = not is_quoted_section
      else:
        output.append(c)
      prev_char = c
    output = ''.join(output)

    if is_quoted_section:
      print('Warning: unbalanced quotes in string: %s' % android_string)

    if is_backslash_sequence:
      print('Warning: trailing backslash in string: %s' % android_string)

    # Check for format specifiers outside of placeholder tags.
    if not inside_placeholder:
      format_specifier = _FORMAT_SPECIFIER.search(output)
      if format_specifier:
        print('Warning: format specifiers are not inside a placeholder '
              '<xliff:g/> tag: %s' % output)

    return output

  def __FormatPlaceholderText(self, placeholder_node):
    """Returns the text inside of an <xliff:g> placeholder node."""
    text = []
    for childNode in placeholder_node.childNodes:
      if childNode.nodeType in (Node.TEXT_NODE, Node.CDATA_SECTION_NODE):
        text.append(childNode.data)
      elif childNode.nodeType != Node.COMMENT_NODE:
        assert False, 'Unknown node type in ' + placeholder_node.toxml()
    return self.__FormatAndroidString(''.join(text), inside_placeholder=True)

  def __CreateMessageNode(self, messages_node, grd_name, description, msg,
                          translatable):
    """Creates and initializes a <message> element.

    Message elements correspond to Android <string> elements in that they
    declare a string resource along with a programmatic id.
    """
    if not description:
      print('Warning: no description for %s' % grd_name)
    # Check that we actually fit within the character limit we've specified.
    match = _CHAR_LIMIT.search(description)
    if match:
      char_limit = int(match.group(1))
      msg_content = msg.GetRealContent()
      if len(msg_content) > char_limit:
        print('Warning: CHAR_LIMIT for %s is %d, but length is %d: %s' %
              (grd_name, char_limit, len(msg_content), msg_content))
    return message.MessageNode.Construct(parent=messages_node,
                                         name=grd_name,
                                         message=msg,
                                         desc=description,
                                         translateable=translatable)

  def __CreateFileNode(self, translations_node, lang):
    """Creates and initializes the <file> elements.

    File elements provide information on the location of translation files
    (xtbs)
    """
    xtb_file = os.path.normpath(os.path.join(
        self.xtb_dir, '%s_%s.xtb' % (self.name, lang)))
    fnode = node_io.FileNode()
    fnode.StartParsing('file', translations_node)
    fnode.HandleAttribute('path', xtb_file)
    fnode.HandleAttribute('lang', lang)
    fnode.EndParsing()
    translations_node.AddChild(fnode)
    return fnode

  def __CreateCppHeaderOutputNode(self, outputs_node, header_dir):
    """Creates the <output> element corresponding to the generated c header."""
    header_file_name = os.path.join(header_dir, self.name + '.h')
    header_node = node_io.OutputNode()
    header_node.StartParsing('output', outputs_node)
    header_node.HandleAttribute('filename', header_file_name)
    header_node.HandleAttribute('type', 'rc_header')
    emit_node = node_io.EmitNode()
    emit_node.StartParsing('emit', header_node)
    emit_node.HandleAttribute('emit_type', 'prepend')
    emit_node.EndParsing()
    header_node.AddChild(emit_node)
    header_node.EndParsing()
    outputs_node.AddChild(header_node)
    return header_node

  def __CreateRcOutputNode(self, outputs_node, lang, rc_dir):
    """Creates the <output> element corresponding to various rc file output."""
    rc_file_name = self.name + '_' + lang + ".rc"
    rc_path = os.path.join(rc_dir, rc_file_name)
    node = node_io.OutputNode()
    node.StartParsing('output', outputs_node)
    node.HandleAttribute('filename', rc_path)
    node.HandleAttribute('lang', lang)
    node.HandleAttribute('type', 'rc_all')
    node.EndParsing()
    outputs_node.AddChild(node)
    return node

  def __CreateAndroidXmlOutputNode(self, outputs_node, locale, xml_res_dir):
    """Creates the <output> element corresponding to various rc file output."""
    # Need to check to see if the locale has a region, e.g. the GB in en-GB.
    # When a locale has a region Android expects the region to be prefixed
    # with an 'r'. For example for en-GB Android expects a values-en-rGB
    # directory.  Also, Android expects nb, tl, in, iw, ji as the language
    # codes for Norwegian, Tagalog/Filipino, Indonesian, Hebrew, and Yiddish:
    # http://developer.android.com/reference/java/util/Locale.html
    if locale == 'es-419':
      android_locale = 'es-rUS'
    else:
      android_lang, dash, region = locale.partition('-')
      lang_map = {'no': 'nb', 'fil': 'tl', 'id': 'in', 'he': 'iw', 'yi': 'ji'}
      android_lang = lang_map.get(android_lang, android_lang)
      android_locale = android_lang + ('-r' + region if region else '')
    values = 'values-' + android_locale if android_locale != 'en' else 'values'
    xml_path = os.path.normpath(os.path.join(
        xml_res_dir, values, 'strings.xml'))

    node = node_io.OutputNode()
    node.StartParsing('output', outputs_node)
    node.HandleAttribute('filename', xml_path)
    node.HandleAttribute('lang', locale)
    node.HandleAttribute('type', 'android')
    node.EndParsing()
    outputs_node.AddChild(node)
    return node

  def IsTranslatable(self, android_string):
    """Determines if a <string> element is a candidate for translation.

    A <string> element is by default translatable unless otherwise marked.
    """
    if android_string.hasAttribute('translatable'):
      value = android_string.getAttribute('translatable').lower()
      if value not in ('true', 'false'):
        print('Warning: translatable attribute has invalid value: %s' % value)
      return value == 'true'
    else:
      return True
