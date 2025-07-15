#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Hierarchical property system for IDL AST """
import re
import sys

from idl_log import ErrOut, InfoOut, WarnOut

#
# IDLPropertyNode
#
# A property node is a hierarchically aware system for mapping
# keys to values, such that a local dictionary is search first,
# followed by parent dictionaries in order.
#
class IDLPropertyNode(object):
  def __init__(self):
    self.parents = []
    self.property_map = {}

  def AddParent(self, parent):
    assert parent
    self.parents.append(parent)

  def SetProperty(self, name, val):
    self.property_map[name] = val

  def GetProperty(self, name):
    # Check locally for the property, and return it if found.
    prop = self.property_map.get(name, None)
    if prop is not None:
      return prop
    # If not, seach parents in order
    for parent in self.parents:
      prop = parent.GetProperty(name)
      if prop is not None:
        return prop
    # Otherwise, it can not be found.
    return None

  def GetPropertyLocal(self, name):
    # Search for the property, but only locally.
    return self.property_map.get(name, None)

  def GetPropertyList(self):
    return self.property_map.keys()

#
# Testing functions
#

# Build a property node, setting the properties including a name, and
# associate the children with this new node.
#
def BuildNode(name, props, children=None, parents=None):
  node = IDLPropertyNode()
  node.SetProperty('NAME', name)
  for prop in props:
    toks = prop.split('=')
    node.SetProperty(toks[0], toks[1])
  if children:
    for child in children:
      child.AddParent(node)
  if parents:
    for parent in parents:
      node.AddParent(parent)
  return node

def ExpectProp(node, name, val):
  found = node.GetProperty(name)
  if found != val:
    ErrOut.Log('Got property %s expecting %s' % (found, val))
    return 1
  return 0

#
# Verify property inheritance
#
def PropertyTest():
  errors = 0
  left = BuildNode('Left', ['Left=Left'])
  right = BuildNode('Right', ['Right=Right'])
  top = BuildNode('Top', ['Left=Top', 'Right=Top'], [left, right])

  errors += ExpectProp(top, 'Left', 'Top')
  errors += ExpectProp(top, 'Right', 'Top')

  errors += ExpectProp(left, 'Left', 'Left')
  errors += ExpectProp(left, 'Right', 'Top')

  errors += ExpectProp(right, 'Left', 'Top')
  errors += ExpectProp(right, 'Right', 'Right')

  if not errors:
    InfoOut.Log('Passed PropertyTest')
  return errors


def Main():
  errors = 0
  errors += PropertyTest()

  if errors:
    ErrOut.Log('IDLNode failed with %d errors.' % errors)
    return  -1
  return 0


if __name__ == '__main__':
  sys.exit(Main())
