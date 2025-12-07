# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Visitor Object for traversing AST """

#
# IDLVisitor
#
# The IDLVisitor class will traverse an AST truncating portions of the tree
# when 'VisitFilter' returns false.  After the filter returns true, for each
# node, the visitor will call the 'Arrive' member passing in the node and
# and generic data object from the parent call.  The returned value is then
# passed to all children who's results are aggregated into a list.  The child
# results along with the original Arrive result are passed to the Depart
# function which returns the final result of the Visit.  By default this is
# the exact value that was return from the original arrive.
#

class IDLVisitor(object):
  def __init__(self):
    pass

  # Return TRUE if the node should be visited
  def VisitFilter(self, node, data):
    return True

  def Visit(self, node, data):
    if not self.VisitFilter(node, data): return None

    childdata = []
    newdata = self.Arrive(node, data)
    for child in node.GetChildren():
      ret = self.Visit(child, newdata)
      if ret is not None:
        childdata.append(ret)
    return self.Depart(node, newdata, childdata)

  def Arrive(self, node, data):
    __pychecker__ = 'unusednames=node'
    return data

  def Depart(self, node, data, childdata):
    __pychecker__ = 'unusednames=node,childdata'
    return data
