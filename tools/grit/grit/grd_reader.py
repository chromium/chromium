#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Class for reading GRD files into memory, without processing them.
'''


import os.path
import sys
import xml.sax
import xml.sax.handler

from grit import exception
from grit import util
from grit.node import mapping
from grit.node import misc


class StopParsingException(Exception):
  '''An exception used to stop parsing.'''
  pass


class GrdContentHandler(xml.sax.handler.ContentHandler):
  def __init__(self, stop_after, debug, dir, defines, tags_to_ignore,
               target_platform, source, skip_validation_checks):
    # Invariant of data:
    # 'root' is the root of the parse tree being created, or None if we haven't
    # parsed out any elements.
    # 'stack' is the a stack of elements that we push new nodes onto and
    # pop from when they finish parsing, or [] if we are not currently parsing.
    # 'stack[-1]' is the top of the stack.
    self.root = None
    self.stack = []
    self.stop_after = stop_after
    self.debug = debug
    self.dir = dir
    self.defines = defines
    self.tags_to_ignore = tags_to_ignore or set()
    self.ignore_depth = 0
    self.target_platform = target_platform
    self.source = source
    self.skip_validation_checks = skip_validation_checks

  def startElement(self, name, attrs):
    if self.ignore_depth or name in self.tags_to_ignore:
      if self.debug and self.ignore_depth == 0:
        print("Ignoring element %s and its children" % name)
      self.ignore_depth += 1
      return

    if self.debug:
      attr_list = ' '.join('%s="%s"' % kv for kv in attrs.items())
      print("Starting parsing of element %s with attributes %r" %
            (name, attr_list or '(none)'))

    typeattr = attrs.get('type')
    node = mapping.ElementToClass(name, typeattr)()
    node.source = self.source

    if self.stack:
      self.stack[-1].AddChild(node)
      node.StartParsing(name, self.stack[-1])
    else:
      assert self.root is None
      self.root = node
      if isinstance(self.root, misc.GritNode):
        if self.target_platform:
          self.root.SetTargetPlatform(self.target_platform)
      node.StartParsing(name, None)
      if self.defines:
        node.SetDefines(self.defines)
      node.skip_validation_checks = self.skip_validation_checks
    self.stack.append(node)

    for attr, attrval in attrs.items():
      node.HandleAttribute(attr, attrval)

  def endElement(self, name):
    if self.ignore_depth:
      self.ignore_depth -= 1
      return

    if name == 'part':
      partnode = self.stack[-1]
      partnode.started_inclusion = True
      # Add the contents of the sub-grd file as children of the <part> node.
      partname = os.path.join(self.dir, partnode.GetInputPath())
      # Check the GRDP file exists.
      if not os.path.exists(partname):
        raise exception.FileNotFound(partname)
      # Exceptions propagate to the handler in grd_reader.Parse().
      oldsource = self.source
      try:
        self.source = partname
        xml.sax.parse(partname, GrdPartContentHandler(self))
      finally:
        self.source = oldsource

    if self.debug:
      print("End parsing of element %s" % name)
    self.stack.pop().EndParsing()

    if name == self.stop_after:
      raise StopParsingException()

  def characters(self, content):
    if self.ignore_depth == 0:
      if self.stack[-1]:
        self.stack[-1].AppendContent(content)

  def ignorableWhitespace(self, whitespace):
    # TODO(joi): This is not supported by expat. Should use a different XML
    # parser?
    pass


class GrdPartContentHandler(xml.sax.handler.ContentHandler):
  def __init__(self, parent):
    self.parent = parent
    self.depth = 0

  def startElement(self, name, attrs):
    if self.depth:
      self.parent.startElement(name, attrs)
    else:
      if name != 'grit-part':
        raise exception.MissingElement("root tag must be <grit-part>")
      if attrs:
        raise exception.UnexpectedAttribute(
            "<grit-part> tag must not have attributes")
    self.depth += 1

  def endElement(self, name):
    self.depth -= 1
    if self.depth:
      self.parent.endElement(name)

  def characters(self, content):
    self.parent.characters(content)

  def ignorableWhitespace(self, whitespace):
    self.parent.ignorableWhitespace(whitespace)


def Parse(filename_or_stream,
          dir=None,
          stop_after=None,
          first_ids_file=None,
          debug=False,
          defines=None,
          tags_to_ignore=None,
          target_platform=None,
          predetermined_ids_file=None,
          skip_validation_checks=False):
  '''Parses a GRD file into a tree of nodes (from grit.node).

  If filename_or_stream is a stream, 'dir' should point to the directory
  notionally containing the stream (this feature is only used in unit tests).

  If 'stop_after' is provided, the parsing will stop once the first node
  with this name has been fully parsed (including all its contents).

  If 'debug' is true, lots of information about the parsing events will be
  printed out during parsing of the file.

  If 'first_ids_file' is non-empty, it is used to override the setting for the
  first_ids_file attribute of the <grit> root node. Note that the first_ids_file
  parameter should be relative to the cwd, even though the first_ids_file
  attribute of the <grit> node is relative to the grd file.

  If 'target_platform' is set, this is used to determine the target
  platform of builds, instead of using |sys.platform|.

  Args:
    filename_or_stream: './bla.xml'
    dir: None (if filename_or_stream is a filename) or '.'
    stop_after: 'inputs'
    first_ids_file: 'GRIT_DIR/../gritsettings/resource_ids'
    debug: False
    defines: dictionary of defines, like {'chromeos': '1'}
    target_platform: None or the value that would be returned by sys.platform
        on your target platform.
    predetermined_ids_file: File path to a file containing a pre-determined
        mapping from resource names to resource ids which will be used to assign
        resource ids to those resources.
    skip_validation_checks: Whether to skip any validation after successfull
        parsing, for example uniqueness of IDs, or that all variables encountered
        in <if expr> statements are defined.

  Return:
    Subclass of grit.node.base.Node

  Throws:
    grit.exception.Parsing
  '''

  if isinstance(filename_or_stream, str):
    source = filename_or_stream
    if dir is None:
      dir = util.dirname(filename_or_stream)
  else:
    source = None

  handler = GrdContentHandler(stop_after=stop_after,
                              debug=debug,
                              dir=dir,
                              defines=defines,
                              tags_to_ignore=tags_to_ignore,
                              target_platform=target_platform,
                              source=source,
                              skip_validation_checks=skip_validation_checks)
  try:
    xml.sax.parse(filename_or_stream, handler)
  except StopParsingException:
    assert stop_after
    pass
  except:
    if not debug:
      print("parse exception: run GRIT with the -x flag to debug .grd problems")
    raise

  if handler.root.name != 'grit':
    raise exception.MissingElement("root tag must be <grit>")

  if hasattr(handler.root, 'SetOwnDir'):
    # Fix up the base_dir so it is relative to the input file.
    assert dir is not None
    handler.root.SetOwnDir(dir)

  if isinstance(handler.root, misc.GritNode):
    handler.root.SetPredeterminedIdsFile(predetermined_ids_file)
    if first_ids_file:
      # Make the path to the first_ids_file relative to the grd file,
      # unless it begins with GRIT_DIR.
      GRIT_DIR_PREFIX = 'GRIT_DIR'
      if not (first_ids_file.startswith(GRIT_DIR_PREFIX)
          and first_ids_file[len(GRIT_DIR_PREFIX)] in ['/', '\\']):
        rel_dir = os.path.relpath(os.getcwd(), dir)
        first_ids_file = util.normpath(os.path.join(rel_dir, first_ids_file))
      handler.root.attrs['first_ids_file'] = first_ids_file
    # Assign first ids to the nodes that don't have them.
    handler.root.AssignFirstIds(filename_or_stream, defines)

  return handler.root


if __name__ == '__main__':
  util.ChangeStdoutEncoding()
  print(str(Parse(sys.argv[1])))
