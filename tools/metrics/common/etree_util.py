# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions for parsing XML strings into ElementTree nodes."""

import xml.etree.ElementTree as ET
import xml.sax


class _FirstTagFoundError(Exception):
  """Raised when the first tag is found in an XML document.

  This isn't actually an error. Raising this exception is how we end parsing XML
  documents early.
  """
  pass


class _FirstTagFinder(xml.sax.ContentHandler):
  """An XML SAX parser that raises as soon as a tag is found.

  Call getFirstTagLine to determine which line the tag was found on.
  """

  def __init__(self):
    xml.sax.ContentHandler.__init__(self)
    self.first_tag_line = 0
    self.first_tag_column = 0

  def GetFirstTagLine(self):
    return self.first_tag_line

  def GetFirstTagColumn(self):
    return self.first_tag_column

  def setDocumentLocator(self, locator):
    self.location = locator

  def startElement(self, tag, attributes):
    del tag, attributes  # Unused.

    # Now that the first tag is found, remember the location of it.
    self.first_tag_line = self.location.getLineNumber()
    self.first_tag_column = self.location.getColumnNumber()

    # End parsing by throwing.
    raise _FirstTagFoundError()


class _CommentedXMLParser(ET.XMLParser):
  """An ElementTree builder that preserves comments."""

  def __init__(self, *args, **kwargs):
    super(_CommentedXMLParser, self).__init__(*args, **kwargs)
    self._parser.CommentHandler = self.comment

  def comment(self, data):  # pylint: disable=invalid-name
    self._target.start(ET.Comment, {})
    self._target.data(data)
    self._target.end(ET.Comment)


def GetTopLevelContent(file_content):
  """Returns a string of all the text in the xml file before the first tag."""
  handler = _FirstTagFinder()

  first_tag_line = 0
  first_tag_column = 0
  try:
    xml.sax.parseString(file_content, handler)
  except _FirstTagFoundError:
    # This is the expected case, it means a tag was found in the doc.
    first_tag_line = handler.GetFirstTagLine()
    first_tag_column = handler.GetFirstTagColumn()
  if first_tag_line == 0 and first_tag_column == 0:
    return ''

  char = 0
  for _ in range(first_tag_line - 1):
    char = file_content.index('\n', char) + 1
  char += first_tag_column - 1

  # |char| is now pointing at the final character before the opening tag '<'.
  top_content = file_content[:char + 1].strip()
  if not top_content:
    return ''

  return top_content + '\n\n'


def ParseXMLString(raw_xml):
  """Parses raw_xml and returns an ElementTree node that includes comments."""
  return ET.fromstring(raw_xml, _CommentedXMLParser())
