# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implements Gitiles' notification, aside and promotion blocks.

This extention makes the Markdown parser recognize the Gitiles' extended
blocks notation. The syntax is explained at:

https://gerrit.googlesource.com/gitiles/+/master/Documentation/markdown.md#Notification_aside_promotion-blocks
"""

from markdown.blockprocessors import BlockProcessor
from markdown.extensions import Extension
import re
import xml.etree.ElementTree as etree


class _GitilesExtBlockProcessor(BlockProcessor):
  """Process Gitiles' notification, aside and promotion blocks."""

  RE_START = re.compile(r'^\*\*\* (note|aside|promo) *\n')
  RE_END = re.compile(r'\n\*\*\* *\n?$')

  def __init__(self, *args, **kwargs):
    self._last_parent = None
    BlockProcessor.__init__(self, *args, **kwargs)

  def test(self, parent, block):
    return self.RE_START.search(block) or self.RE_END.search(block)

  def run(self, parent, blocks):
    raw_block = blocks.pop(0)
    match_start = self.RE_START.search(raw_block)
    if match_start:
      # Opening a new block.
      rest = raw_block[match_start.end():]

      if self._last_parent:
        # Inconsistent state (nested starting markers). Ignore the marker
        # and keep going.
        blocks.insert(0, rest)
        return

      div = etree.SubElement(parent, 'div')
      # Setting the class name is sufficient, because doc.css already has
      # styles for these classes.
      div.set('class', match_start.group(1))
      self._last_parent = parent
      blocks.insert(0, rest)
      self.parser.parseBlocks(div, blocks)
      return

    match_end = self.RE_END.search(raw_block)
    if match_end:
      # Ending an existing block.

      # Process the text preceding the ending marker in the current context
      # (i.e. within the div block).
      rest = raw_block[:match_end.start()]
      self.parser.parseBlocks(parent, [rest])

      if not self._last_parent:
        # Inconsistent state (the ending marker is found but there is no
        # matching starting marker).
        # Let's continue as if we did not see the ending marker.
        return

      last_parent = self._last_parent
      self._last_parent = None
      self.parser.parseBlocks(last_parent, blocks)
      return


class _GitilesExtBlockExtension(Extension):
  """Add Gitiles' extended blocks to Markdown, with a priority higher than the
  highest builtin."""

  def extendMarkdown(self, md):
    md.parser.blockprocessors.register(_GitilesExtBlockProcessor(md.parser),
                                       'gitilesextblocks', 101)


def makeExtension(*args, **kwargs):
  return _GitilesExtBlockExtension(*args, **kwargs)
