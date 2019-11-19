# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The 'grit xmb' tool.
"""

from __future__ import print_function

import getopt
import os
import sys

from xml.sax import saxutils

import six

from grit import grd_reader
from grit import lazy_re
from grit import tclib
from grit import util
from grit.tool import interface


# Used to collapse presentable content to determine if
# xml:space="preserve" is needed.
_WHITESPACES_REGEX = lazy_re.compile(r'\s\s*')


# See XmlEscape below.
_XML_QUOTE_ESCAPES = {
    u"'":  u'&apos;',
    u'"':  u'&quot;',
}

def _XmlEscape(s):
  """Returns text escaped for XML in a way compatible with Google's
  internal Translation Console tool.  May be used for attributes as
  well as for contents.
  """
  return saxutils.escape(six.text_type(s), _XML_QUOTE_ESCAPES).encode('utf-8')


def _WriteAttribute(file, name, value):
  """Writes an XML attribute to the specified file.

    Args:
      file: file to write to
      name: name of the attribute
      value: (unescaped) value of the attribute
    """
  name = name.encode('utf-8')
  if value:
    file.write(b' %s="%s"' % (name, _XmlEscape(value)))


def _WriteMessage(file, message):
  presentable_content = message.GetPresentableContent()
  assert (isinstance(presentable_content, six.string_types) or
          (len(message.parts) == 1 and
           type(message.parts[0] == tclib.Placeholder)))
  preserve_space = presentable_content != _WHITESPACES_REGEX.sub(
      u' ', presentable_content.strip())

  file.write(b'<msg')
  _WriteAttribute(file, 'desc', message.GetDescription())
  _WriteAttribute(file, 'id', message.GetId())
  _WriteAttribute(file, 'meaning', message.GetMeaning())
  if preserve_space:
    _WriteAttribute(file, 'xml:space', 'preserve')
  file.write(b'>')
  if not preserve_space:
    file.write(b'\n  ')

  parts = message.GetContent()
  for part in parts:
    if isinstance(part, tclib.Placeholder):
      file.write(b'<ph')
      _WriteAttribute(file, 'name', part.GetPresentation())
      file.write(b'><ex>')
      file.write(_XmlEscape(part.GetExample()))
      file.write(b'</ex>')
      file.write(_XmlEscape(part.GetOriginal()))
      file.write(b'</ph>')
    else:
      file.write(_XmlEscape(part))
  if not preserve_space:
    file.write(b'\n')
  file.write(b'</msg>\n')


def WriteXmbFile(file, messages):
  """Writes the given grit.tclib.Message items to the specified open
  file-like object in the XMB format.
  """
  file.write(b"""<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE messagebundle [
<!ELEMENT messagebundle (msg)*>
<!ATTLIST messagebundle class CDATA #IMPLIED>

<!ELEMENT msg (#PCDATA|ph|source)*>
<!ATTLIST msg id CDATA #IMPLIED>
<!ATTLIST msg seq CDATA #IMPLIED>
<!ATTLIST msg name CDATA #IMPLIED>
<!ATTLIST msg desc CDATA #IMPLIED>
<!ATTLIST msg meaning CDATA #IMPLIED>
<!ATTLIST msg obsolete (obsolete) #IMPLIED>
<!ATTLIST msg xml:space (default|preserve) "default">
<!ATTLIST msg is_hidden CDATA #IMPLIED>

<!ELEMENT source (#PCDATA)>

<!ELEMENT ph (#PCDATA|ex)*>
<!ATTLIST ph name CDATA #REQUIRED>

<!ELEMENT ex (#PCDATA)>
]>
<messagebundle>
""")
  for message in messages:
    _WriteMessage(file, message)
  file.write(b'</messagebundle>')


class OutputXmb(interface.Tool):
  """Outputs all translateable messages in the .grd input file to an
.xmb file, which is the format used to give source messages to
Google's internal Translation Console tool.  The format could easily
be used for other systems.

Usage: grit xmb [-i|-h] [-l LIMITFILE] OUTPUTPATH

OUTPUTPATH is the path you want to output the .xmb file to.

The -l option can be used to output only some of the resources to the .xmb file.
LIMITFILE is the path to a file that is used to limit the items output to the
xmb file.  If the filename extension is .grd, the file must be a .grd file
and the tool only output the contents of nodes from the input file that also
exist in the limit file (as compared on the 'name' attribute). Otherwise it must
contain a list of the IDs that output should be limited to, one ID per line, and
the tool will only output nodes with 'name' attributes that match one of the
IDs.

The -i option causes 'grit xmb' to output an "IDs only" file instead of an XMB
file.  The "IDs only" file contains the message ID of each message that would
normally be output to the XMB file, one message ID per line.  It is designed for
use with the 'grit transl2tc' tool's -l option.

Other options:

  -D NAME[=VAL]     Specify a C-preprocessor-like define NAME with optional
                    value VAL (defaults to 1) which will be used to control
                    conditional inclusion of resources.

  -E NAME=VALUE     Set environment variable NAME to VALUE (within grit).

"""
  # The different output formats supported by this tool
  FORMAT_XMB = 0
  FORMAT_IDS_ONLY = 1

  def __init__(self, defines=None):
    super(OutputXmb, self).__init__()
    self.format = self.FORMAT_XMB
    self.defines = defines or {}

  def ShortDescription(self):
    return 'Exports all translateable messages into an XMB file.'

  def Run(self, opts, args):
    os.environ['cwd'] = os.getcwd()

    self.SetOptions(opts)

    limit_file = None
    limit_is_grd = False
    limit_file_dir = None
    own_opts, args = getopt.getopt(args, 'l:D:ih', ('help',))
    for key, val in own_opts:
      if key == '-l':
        limit_file = open(val, 'r')
        limit_file_dir = util.dirname(val)
        if not len(limit_file_dir):
          limit_file_dir = '.'
        limit_is_grd = os.path.splitext(val)[1] == '.grd'
      elif key == '-i':
        self.format = self.FORMAT_IDS_ONLY
      elif key == '-D':
        name, val = util.ParseDefine(val)
        self.defines[name] = val
      elif key == '-E':
        (env_name, env_value) = val.split('=', 1)
        os.environ[env_name] = env_value
      elif key == '--help':
        self.ShowUsage()
        sys.exit(0)
    if not len(args) == 1:
      print('grit xmb takes exactly one argument, the path to the XMB file '
            'to output.')
      return 2

    xmb_path = args[0]
    res_tree = grd_reader.Parse(opts.input, debug=opts.extra_verbose, defines=self.defines)
    res_tree.SetOutputLanguage('en')
    res_tree.SetDefines(self.defines)
    res_tree.OnlyTheseTranslations([])
    res_tree.RunGatherers()

    with open(xmb_path, 'wb') as output_file:
      self.Process(
        res_tree, output_file, limit_file, limit_is_grd, limit_file_dir)
    if limit_file:
      limit_file.close()
    print("Wrote %s" % xmb_path)

  def Process(self, res_tree, output_file, limit_file=None, limit_is_grd=False,
              dir=None):
    """Writes a document with the contents of res_tree into output_file,
    limiting output to the IDs specified in limit_file, which is a GRD file if
    limit_is_grd is true, otherwise a file with one ID per line.

    The format of the output document depends on this object's format attribute.
    It can be FORMAT_XMB or FORMAT_IDS_ONLY.

    The FORMAT_IDS_ONLY format causes this function to write just a list
    of the IDs of all messages that would have been added to the XMB file, one
    ID per line.

    The FORMAT_XMB format causes this function to output the (default) XMB
    format.

    Args:
      res_tree: base.Node()
      output_file: file open for writing
      limit_file: None or file open for reading
      limit_is_grd: True | False
      dir: Directory of the limit file
    """
    if limit_file:
      if limit_is_grd:
        limit_list = []
        limit_tree = grd_reader.Parse(limit_file,
                                      dir=dir,
                                      debug=self.o.extra_verbose)
        for node in limit_tree:
          if 'name' in node.attrs:
            limit_list.append(node.attrs['name'])
      else:
        # Not a GRD file, so it's just a file with one ID per line
        limit_list = [item.strip() for item in limit_file.read().split('\n')]

    ids_already_done = {}
    messages = []
    for node in res_tree:
      if (limit_file and
          not ('name' in node.attrs and node.attrs['name'] in limit_list)):
        continue
      if not node.IsTranslateable():
        continue

      for clique in node.GetCliques():
        if not clique.IsTranslateable():
          continue
        if not clique.GetMessage().GetRealContent():
          continue

        # Some explanation is in order here.  Note that we can have
        # many messages with the same ID.
        #
        # The way we work around this is to maintain a list of cliques
        # per message ID (in the UberClique) and select the "best" one
        # (the first one that has a description, or an arbitrary one
        # if there is no description) for inclusion in the XMB file.
        # The translations are all going to be the same for messages
        # with the same ID, although the way we replace placeholders
        # might be slightly different.
        id = clique.GetMessage().GetId()
        if id in ids_already_done:
          continue
        ids_already_done[id] = 1

        message = node.UberClique().BestClique(id).GetMessage()
        messages += [message]

    # Ensure a stable order of messages, to help regression testing.
    messages.sort(key=lambda x:x.GetId())

    if self.format == self.FORMAT_IDS_ONLY:
      # We just print the list of IDs to the output file.
      for msg in messages:
        output_file.write(msg.GetId())
        output_file.write('\n')
    else:
      assert self.format == self.FORMAT_XMB
      WriteXmbFile(output_file, messages)
