# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Container nodes that don't have any logic.
'''

from __future__ import print_function

from grit.node import base
from grit.node import include
from grit.node import message
from grit.node import misc
from grit.node import node_io
from grit.node import structure


class GroupingNode(base.Node):
  '''Base class for all the grouping elements (<structures>, <includes>,
  <messages> and <identifiers>).'''
  def DefaultAttributes(self):
    return {
      'first_id' : '',
      'comment' : '',
      'fallback_to_english' : 'false',
      'fallback_to_low_resolution' : 'false',
    }


class IncludesNode(GroupingNode):
  '''The <includes> element.'''
  def _IsValidChild(self, child):
    return isinstance(child, (include.IncludeNode, misc.IfNode, misc.PartNode))


class MessagesNode(GroupingNode):
  '''The <messages> element.'''
  def _IsValidChild(self, child):
    return isinstance(child, (message.MessageNode, misc.IfNode, misc.PartNode))


class StructuresNode(GroupingNode):
  '''The <structures> element.'''
  def _IsValidChild(self, child):
    return isinstance(child, (structure.StructureNode,
                              misc.IfNode, misc.PartNode))


class TranslationsNode(base.Node):
  '''The <translations> element.'''
  def _IsValidChild(self, child):
    return isinstance(child, (node_io.FileNode, misc.IfNode, misc.PartNode))


class OutputsNode(base.Node):
  '''The <outputs> element.'''
  def _IsValidChild(self, child):
    return isinstance(child, (node_io.OutputNode, misc.IfNode, misc.PartNode))


class IdentifiersNode(GroupingNode):
  '''The <identifiers> element.'''
  def _IsValidChild(self, child):
    return isinstance(child, misc.IdentifierNode)
