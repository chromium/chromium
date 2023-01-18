# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''The 'grit rc2grd' tool.'''


import os.path
import getopt
import re
import sys

from io import StringIO

import grit.node.empty
from grit.node import include
from grit.node import structure
from grit.node import message

from grit.gather import rc
from grit.gather import tr_html

from grit.tool import interface
from grit.tool import postprocess_interface
from grit.tool import preprocess_interface

from grit import grd_reader
from grit import lazy_re
from grit import tclib
from grit import util


# Matches files referenced from an .rc file
_FILE_REF = lazy_re.compile(r'''
  ^(?P<id>[A-Z_0-9.]+)[ \t]+
  (?P<type>[A-Z_0-9]+)[ \t]+
  "(?P<file>.*?([^"]|""))"[ \t]*$''', re.VERBOSE | re.MULTILINE)


# Matches a dialog section
_DIALOG = lazy_re.compile(
    r'^(?P<id>[A-Z0-9_]+)\s+DIALOG(EX)?\s.+?^BEGIN\s*$.+?^END\s*$',
    re.MULTILINE | re.DOTALL)


# Matches a menu section
_MENU = lazy_re.compile(r'^(?P<id>[A-Z0-9_]+)\s+MENU.+?^BEGIN\s*$.+?^END\s*$',
                        re.MULTILINE | re.DOTALL)


# Matches a versioninfo section
_VERSIONINFO = lazy_re.compile(
    r'^(?P<id>[A-Z0-9_]+)\s+VERSIONINFO\s.+?^BEGIN\s*$.+?^END\s*$',
    re.MULTILINE | re.DOTALL)


# Matches a stringtable
_STRING_TABLE = lazy_re.compile(
    (r'^STRINGTABLE(\s+(PRELOAD|DISCARDABLE|CHARACTERISTICS.+|LANGUAGE.+|'
     r'VERSION.+))*\s*\nBEGIN\s*$(?P<body>.+?)^END\s*$'),
    re.MULTILINE | re.DOTALL)


# Matches each message inside a stringtable, breaking it up into comments,
# the ID of the message, and the (RC-escaped) message text.
_MESSAGE = lazy_re.compile(r'''
  (?P<comment>(^\s+//.+?)*)  # 0 or more lines of comments preceding the message
  ^\s*
  (?P<id>[A-Za-z0-9_]+)  # id
  \s+
  "(?P<text>.*?([^"]|""))"([^"]|$)  # The message itself
  ''', re.MULTILINE | re.DOTALL | re.VERBOSE)


# Matches each line of comment text in a multi-line comment.
_COMMENT_TEXT = lazy_re.compile(r'^\s*//\s*(?P<text>.+?)$', re.MULTILINE)


# Matches a string that is empty or all whitespace
_WHITESPACE_ONLY = lazy_re.compile(r'\A\s*\Z', re.MULTILINE)


# Finds printf and FormatMessage style format specifiers
# Uses non-capturing groups except for the outermost group, so the output of
# re.split() should include both the normal text and what we intend to
# replace with placeholders.
# TODO(joi) Check documentation for printf (and Windows variants) and FormatMessage
_FORMAT_SPECIFIER = lazy_re.compile(
  r'(%[-# +]?(?:[0-9]*|\*)(?:\.(?:[0-9]+|\*))?(?:h|l|L)?' # printf up to last char
  r'(?:d|i|o|u|x|X|e|E|f|F|g|G|c|r|s|ls|ws)'              # printf last char
  r'|\$[1-9][0-9]*)')                                     # FormatMessage


class Rc2Grd(interface.Tool):
  '''A tool for converting .rc files to .grd files.  This tool is only for
converting the source (nontranslated) .rc file to a .grd file.  For importing
existing translations, use the rc2xtb tool.

Usage:  grit [global options] rc2grd [OPTIONS] RCFILE

The tool takes a single argument, which is the path to the .rc file to convert.
It outputs a .grd file with the same name in the same directory as the .rc file.
The .grd file may have one or more TODO comments for things that have to be
cleaned up manually.

OPTIONS may be any of the following:

  -e ENCODING    Specify the ENCODING of the .rc file. Default is 'cp1252'.

  -h TYPE        Specify the TYPE attribute for HTML structures.
                 Default is 'tr_html'.

  -u ENCODING    Specify the ENCODING of HTML files. Default is 'utf-8'.

  -n MATCH       Specify the regular expression to match in comments that will
                 indicate that the resource the comment belongs to is not
                 translateable. Default is 'Not locali(s|z)able'.

  -r GRDFILE     Specify that GRDFILE should be used as a "role model" for
                 any placeholders that otherwise would have had TODO names.
                 This attempts to find an identical message in the GRDFILE
                 and uses that instead of the automatically placeholderized
                 message.

  --pre CLASS    Specify an optional, fully qualified classname, which
                 has to be a subclass of grit.tool.PreProcessor, to
                 run on the text of the RC file before conversion occurs.
                 This can be used to support constructs in the RC files
                 that GRIT cannot handle on its own.

  --post CLASS   Specify an optional, fully qualified classname, which
                 has to be a subclass of grit.tool.PostProcessor, to
                 run on the text of the converted RC file.
                 This can be used to alter the content of the RC file
                 based on the conversion that occured.

For menus, dialogs and version info, the .grd file will refer to the original
.rc file.  Once conversion is complete, you can strip the original .rc file
of its string table and all comments as these will be available in the .grd
file.

Note that this tool WILL NOT obey C preprocessor rules, so even if something
is #if 0-ed out it will still be included in the output of this tool
Therefore, if your .rc file contains sections like this, you should run the
C preprocessor on the .rc file or manually edit it before using this tool.
'''

  def ShortDescription(self):
    return 'A tool for converting .rc source files to .grd files.'

  def __init__(self):
    self.input_encoding = 'cp1252'
    self.html_type = 'tr_html'
    self.html_encoding = 'utf-8'
    self.not_localizable_re = re.compile('Not locali(s|z)able')
    self.role_model = None
    self.pre_process = None
    self.post_process = None

  def ParseOptions(self, args, help_func=None):
    '''Given a list of arguments, set this object's options and return
    all non-option arguments.
    '''
    (own_opts, args) = getopt.getopt(args, 'e:h:u:n:r',
                                     ('help', 'pre=', 'post='))
    for (key, val) in own_opts:
      if key == '-e':
        self.input_encoding = val
      elif key == '-h':
        self.html_type = val
      elif key == '-u':
        self.html_encoding = val
      elif key == '-n':
        self.not_localizable_re = re.compile(val)
      elif key == '-r':
        self.role_model = grd_reader.Parse(val)
      elif key == '--pre':
        self.pre_process = val
      elif key == '--post':
        self.post_process = val
      elif key == '--help':
        if help_func is None:
          self.ShowUsage()
        else:
          help_func()
        sys.exit(0)
    return args

  def Run(self, opts, args):
    args = self.ParseOptions(args)
    if len(args) != 1:
      print('This tool takes a single tool-specific argument, the path to the\n'
            '.rc file to process.')
      return 2
    self.SetOptions(opts)

    path = args[0]
    out_path = os.path.join(util.dirname(path),
                os.path.splitext(os.path.basename(path))[0] + '.grd')

    rctext = util.ReadFile(path, self.input_encoding)
    grd_text = str(self.Process(rctext, path))
    with util.WrapOutputStream(open(out_path, 'wb'), 'utf-8') as outfile:
      outfile.write(grd_text)

    print('Wrote output file %s.\nPlease check for TODO items in the file.' %
          (out_path,))


  def Process(self, rctext, rc_path):
    '''Processes 'rctext' and returns a resource tree corresponding to it.

    Args:
      rctext: complete text of the rc file
      rc_path: 'resource\resource.rc'

    Return:
      grit.node.base.Node subclass
    '''

    if self.pre_process:
      preprocess_class = util.NewClassInstance(self.pre_process,
                                               preprocess_interface.PreProcessor)
      if preprocess_class:
        rctext = preprocess_class.Process(rctext, rc_path)
      else:
        self.Out(
          'PreProcessing class could not be found. Skipping preprocessing.\n')

    # Start with a basic skeleton for the .grd file
    root = grd_reader.Parse(StringIO(
      '''<?xml version="1.0" encoding="UTF-8"?>
      <grit base_dir="." latest_public_release="0"
          current_release="1" source_lang_id="en">
        <outputs />
        <translations />
        <release seq="1">
          <includes />
          <structures />
          <messages />
        </release>
      </grit>'''), util.dirname(rc_path))
    includes = root.children[2].children[0]
    structures = root.children[2].children[1]
    messages = root.children[2].children[2]
    assert (isinstance(includes, grit.node.empty.IncludesNode) and
            isinstance(structures, grit.node.empty.StructuresNode) and
            isinstance(messages, grit.node.empty.MessagesNode))

    self.AddIncludes(rctext, includes)
    self.AddStructures(rctext, structures, os.path.basename(rc_path))
    self.AddMessages(rctext, messages)

    self.VerboseOut('Validating that all IDs are unique...\n')
    root.ValidateUniqueIds()
    self.ExtraVerboseOut('Done validating that all IDs are unique.\n')

    if self.post_process:
      postprocess_class = util.NewClassInstance(self.post_process,
                                                postprocess_interface.PostProcessor)
      if postprocess_class:
        root = postprocess_class.Process(rctext, rc_path, root)
      else:
        self.Out(
          'PostProcessing class could not be found. Skipping postprocessing.\n')

    return root


  def IsHtml(self, res_type, fname):
    '''Check whether both the type and file extension indicate HTML'''
    fext = fname.split('.')[-1].lower()
    return res_type == 'HTML' and fext in ('htm', 'html')


  def AddIncludes(self, rctext, node):
    '''Scans 'rctext' for included resources (e.g. BITMAP, ICON) and
    adds each included resource as an <include> child node of 'node'.'''
    for m in _FILE_REF.finditer(rctext):
      id = m.group('id')
      res_type = m.group('type').upper()
      fname = rc.Section.UnEscape(m.group('file'))
      assert fname.find('\n') == -1
      if not self.IsHtml(res_type, fname):
        self.VerboseOut('Processing %s with ID %s (filename: %s)\n' %
                        (res_type, id, fname))
        node.AddChild(include.IncludeNode.Construct(node, id, res_type, fname))


  def AddStructures(self, rctext, node, rc_filename):
    '''Scans 'rctext' for structured resources (e.g. menus, dialogs, version
    information resources and HTML templates) and adds each as a <structure>
    child of 'node'.'''
    # First add HTML includes
    for m in _FILE_REF.finditer(rctext):
      id = m.group('id')
      res_type = m.group('type').upper()
      fname = rc.Section.UnEscape(m.group('file'))
      if self.IsHtml(type, fname):
        node.AddChild(structure.StructureNode.Construct(
          node, id, self.html_type, fname, self.html_encoding))

    # Then add all RC includes
    def AddStructure(res_type, id):
      self.VerboseOut('Processing %s with ID %s\n' % (res_type, id))
      node.AddChild(structure.StructureNode.Construct(node, id, res_type,
                                                      rc_filename,
                                                      encoding=self.input_encoding))
    for m in _MENU.finditer(rctext):
      AddStructure('menu', m.group('id'))
    for m in _DIALOG.finditer(rctext):
      AddStructure('dialog', m.group('id'))
    for m in _VERSIONINFO.finditer(rctext):
      AddStructure('version', m.group('id'))


  def AddMessages(self, rctext, node):
    '''Scans 'rctext' for all messages in string tables, preprocesses them as
    much as possible for placeholders (e.g. messages containing $1, $2 or %s, %d
    type format specifiers get those specifiers replaced with placeholders, and
    HTML-formatted messages get run through the HTML-placeholderizer).  Adds
    each message as a <message> node child of 'node'.'''
    for tm in _STRING_TABLE.finditer(rctext):
      table = tm.group('body')
      for mm in _MESSAGE.finditer(table):
        comment_block = mm.group('comment')
        comment_text = []
        for cm in _COMMENT_TEXT.finditer(comment_block):
          comment_text.append(cm.group('text'))
        comment_text = ' '.join(comment_text)

        id = mm.group('id')
        text = rc.Section.UnEscape(mm.group('text'))

        self.VerboseOut('Processing message %s (text: "%s")\n' % (id, text))

        msg_obj = self.Placeholderize(text)

        # Messages that contain only placeholders do not need translation.
        is_translateable = False
        for item in msg_obj.GetContent():
          if isinstance(item, str):
            if not _WHITESPACE_ONLY.match(item):
              is_translateable = True

        if self.not_localizable_re.search(comment_text):
          is_translateable = False

        message_meaning = ''
        internal_comment = ''

        # If we have a "role model" (existing GRD file) and this node exists
        # in the role model, use the description, meaning and translateable
        # attributes from the role model.
        if self.role_model:
          role_node = self.role_model.GetNodeById(id)
          if role_node:
            is_translateable = role_node.IsTranslateable()
            message_meaning = role_node.attrs['meaning']
            comment_text = role_node.attrs['desc']
            internal_comment = role_node.attrs['internal_comment']

        # For nontranslateable messages, we don't want the complexity of
        # placeholderizing everything.
        if not is_translateable:
          msg_obj = tclib.Message(text=text)

        msg_node = message.MessageNode.Construct(node, msg_obj, id,
                                                 desc=comment_text,
                                                 translateable=is_translateable,
                                                 meaning=message_meaning)
        msg_node.attrs['internal_comment'] = internal_comment

        node.AddChild(msg_node)
        self.ExtraVerboseOut('Done processing message %s\n' % id)


  def Placeholderize(self, text):
    '''Creates a tclib.Message object from 'text', attempting to recognize
    a few different formats of text that can be automatically placeholderized
    (HTML code, printf-style format strings, and FormatMessage-style format
    strings).
    '''

    try:
      # First try HTML placeholderizing.
      # TODO(joi) Allow use of non-TotalRecall flavors of HTML placeholderizing
      msg = tr_html.HtmlToMessage(text, True)
      for item in msg.GetContent():
        if not isinstance(item, str):
          return msg  # Contained at least one placeholder, so we're done

      # HTML placeholderization didn't do anything, so try to find printf or
      # FormatMessage format specifiers and change them into placeholders.
      msg = tclib.Message()
      parts = _FORMAT_SPECIFIER.split(text)
      todo_counter = 1  # We make placeholder IDs 'TODO_0001' etc.
      for part in parts:
        if _FORMAT_SPECIFIER.match(part):
          msg.AppendPlaceholder(tclib.Placeholder(
            'TODO_%04d' % todo_counter, part, 'TODO'))
          todo_counter += 1
        elif part != '':
          msg.AppendText(part)

      if self.role_model and len(parts) > 1:  # there are TODO placeholders
        role_model_msg = self.role_model.UberClique().BestCliqueByOriginalText(
          msg.GetRealContent(), '')
        if role_model_msg:
          # replace wholesale to get placeholder names and examples
          msg = role_model_msg

      return msg
    except:
      print('Exception processing message with text "%s"' % text)
      raise
