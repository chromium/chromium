# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""".

"""
import collections

# HIDDEN is a marker used to suppress a value, making it as if it were not set
# in that object. This causes the search to continue through the tree.
# This is most useful as a return value of dynamic values that want to find
# the value they are shadowing.
HIDDEN = object()


class VisitComplete(Exception):
  """Indicates a vist traversal has finished early."""


class Visitor(object):
  """The base class for anything that wants to "visit" all variables.

  The two main uses of visitor are search and export. They differ in that export
  is trying to find all variables, whereas search is just looking for one.
  """

  def __init__(self):
    self.stack = []

  def VisitNode(self, node):
    """Called for every node in the tree."""
    if not node.enabled:
      return self
    try:
      try:
        self.stack.append(node)
        self.StartNode()
        # Visit all the values first
        for key in self.KeysOf(node.values):
          self.Visit(key, node.values[key])
        # And now recurse into all the children
        for child in  node.children:
          self.VisitNode(child)
      finally:
        self.EndNode()
        self.stack.pop()
    except VisitComplete:
      if self.stack:
        # propagate back up the stack
        raise
    return self

  def Visit(self, key, value):
    """Visit is called for every variable in each node."""

  def StartNode(self):
    """StartNode is called once for each node before traversal."""

  def EndNode(self):
    """Visit is called for every node after traversal."""

  @property
  def root_node(self):
    """Returns the variable at the root of the current traversal."""
    return self.stack[0]

  @property
  def current_node(self):
    """Returns the node currently being scanned."""
    return self.stack[-1]

  def Resolve(self, key, value):
    """Returns a fully substituted value.

    This asks the root node to do the actual work.
    Args:
      key: The key being visited.
      value: The unresolved value associated with the key.
    Returns:
      the fully resolved value.
    """
    return self.root_node.Resolve(self, key, value)

  def Where(self):
    """Returns the current traversal stack as a string."""
    return '/'.join([entry.name for entry in self.stack])


class SearchVisitor(Visitor):
  """A Visitor that finds a single matching key."""

  def __init__(self, key):
    super(SearchVisitor, self).__init__()
    self.key = key
    self.found = False
    self.error = None

  def KeysOf(self, store):
    if self.key in store:
      yield self.key

  def Visit(self, key, value):
    value, error = self.Resolve(key, value)
    if value is not HIDDEN:
      self.found = True
      self.value = value
      self.error = error
      raise VisitComplete()


class WhereVisitor(SearchVisitor):
  """A SearchVisitor that returns the path to the matching key."""

  def Visit(self, key, value):
    self.where = self.Where()
    super(WhereVisitor, self).Visit(key, value)


class ExportVisitor(Visitor):
  """A visitor that builds a fully resolved map of all variables."""

  def __init__(self, store):
    super(ExportVisitor, self).__init__()
    self.store = store

  def KeysOf(self, store):
    if self.current_node.export is False:
      # not exporting from this config
      return
    for key in store.keys():
      if key in self.store:
        # duplicate
        continue
      if (self.current_node.export is None) and key.startswith('_'):
        # non exported name
        continue
      yield key

  def Visit(self, key, value):
    value, _ = self.Resolve(key, value)
    if value is not HIDDEN:
      self.store[key] = value


class Node(object):
  """The base class for objects in a visitable node tree."""

  def __init__(self, name='--', enabled=True, export=True):
    self._name = name
    self._children = collections.deque()
    self._values = {}
    self._viewers = []
    self.trail = []
    self._enabled = enabled
    self._export = export
    self._export_cache = None

  @property
  def name(self):
    return self._name

  @name.setter
  def name(self, value):
    self._name = value

  @property
  def enabled(self):
    return self._enabled

  @enabled.setter
  def enabled(self, value):
    if self._enabled == value:
      return
    self._enabled = value
    self.NotifyChanged()

  @property
  def export(self):
    return self._export

  @property
  def exported(self):
    if self._export_cache is None:
      self._export_cache = ExportVisitor({}).VisitNode(self).store
    return self._export_cache

  @property
  def values(self):
    return self._values

  @property
  def children(self):
    return self._children

  def RegisterViewer(self, viewer):
    self._viewers.append(viewer)

  def UnregisterViewer(self, viewer):
    self._viewers.remove(viewer)

  def OnChanged(self, child):
    _ = child
    self.NotifyChanged()

  def NotifyChanged(self):
    self._export_cache = None
    for viewers in self._viewers:
      viewers.OnChanged(self)

  def _AddChild(self, child):
    if child and child != self and child not in self._children:
      self._children.appendleft(child)
      child.RegisterViewer(self)

  def AddChild(self, child):
    self._AddChild(child)
    self.NotifyChanged()
    return self

  def AddChildren(self, *children):
    for child in children:
      self._AddChild(child)
    self.NotifyChanged()
    return self

  def Find(self, key):
    search = SearchVisitor(key).VisitNode(self)
    if not search.found:
      return None
    return search.value

  def WhereIs(self, key):
    search = WhereVisitor(key).VisitNode(self)
    if not search.found:
      return None
    return search.where

  def Get(self, key, raise_errors=False):
    search = SearchVisitor(key).VisitNode(self)
    if not search.found:
      self.Missing(key)
    if search.error and raise_errors:
      raise search.error  # bad type inference pylint: disable=raising-bad-type
    return search.value

  def Missing(self, key):
    raise KeyError(key)

  def Resolve(self, visitor, key, value):
    _ = visitor, key
    return value

  def Wipe(self):
    for child in self._children:
      child.UnregisterViewer(self)
    self._children = collections.deque()
    self._values = {}
    self.NotifyChanged()
