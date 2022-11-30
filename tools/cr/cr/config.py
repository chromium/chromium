# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Configuration variable management for the cr tool.

This holds the classes that support the hierarchical variable management used
in the cr tool to provide all the command configuration controls.
"""

import string

import cr.visitor

_PARSE_CONSTANT_VALUES = [None, True, False]
_PARSE_CONSTANTS = dict((str(value), value) for value in _PARSE_CONSTANT_VALUES)

# GLOBALS is the singleton used to tie static global configuration objects
# together.
GLOBALS = []


class _MissingToErrorFormatter(string.Formatter):
  """A string formatter used in value resolve.

  The main extra it adds is a new conversion specifier 'e' that throws a
  KeyError if it could not find the value.
  This allows a string value to use {A_KEY!e} to indicate that it is a
  formatting error if A_KEY is not present.
  """

  def convert_field(self, value, conversion):
    if conversion == 'e':
      result = str(value)
      if not result:
        raise KeyError('unknown')
      return result
    return super(_MissingToErrorFormatter, self).convert_field(
        value, conversion)


class _Tracer(object):
  """Traces variable lookups.

  This adds a hook to a config object, and uses it to track all variable
  lookups that happen and add them to a trail. When done, it removes the hook
  again. This is used to provide debugging information about what variables are
  used in an operation.
  """

  def __init__(self, config):
    self.config = config
    self.trail = []

  def __enter__(self):
    self.config.fixup_hooks.append(self._Trace)
    return self

  def __exit__(self, *_):
    self.config.fixup_hooks.remove(self._Trace)
    self.config.trail = self.trail
    return False

  def _Trace(self, _, key, value):
    self.trail.append((key, value))
    return value


class Config(cr.visitor.Node, cr.loader.AutoExport):
  """The main variable holding class.

  This holds a set of unresolved key value pairs, and the set of child Config
  objects that should be referenced when looking up a key.
  Key search is one in a pre-order traversal, and new children are prepended.
  This means parents override children, and the most recently added child
  overrides the rest.

  Values can be simple python types, callable dynamic values, or strings.
  If the value is a string, it is assumed to be a standard python format string
  where the root config object is used to resolve the keys. This allows values
  to refer to variables that are overriden in another part of the hierarchy.
  """

  @classmethod
  def From(cls, *args, **kwargs):
    """Builds an unnamed config object from a set of key,value args."""
    return Config('??').Apply(args, kwargs)

  @classmethod
  def If(cls, condition, true_value, false_value=''):
    """Returns a config value that selects a value based on the condition.

    Args:
        condition: The variable name to select a value on.
        true_value: The value to use if the variable is True.
        false_value: The value to use if the resolved variable is False.
    Returns:
        A dynamic value.
    """
    def Resolve(base):
      test = base.Get(condition)
      if test:
        value = true_value
      else:
        value = false_value
      return base.Substitute(value)
    return Resolve

  @classmethod
  def Optional(cls, value, alternate=''):
    """Returns a dynamic value that defaults to an alternate.

    Args:
        value: The main value to resolve.
        alternate: The value to use if the main value does not resolve.
    Returns:
        value if it resolves, alternate otherwise.
    """
    def Resolve(base):
      try:
        return base.Substitute(value)
      except KeyError:
        return base.Substitute(alternate)
    return Resolve

  def __init__(self, name='--', literal=False, export=None, enabled=True):
    super(Config, self).__init__(name=name, enabled=enabled, export=export)
    self._literal = literal
    self._formatter = _MissingToErrorFormatter()
    self.fixup_hooks = []
    self.trail = []

  @property
  def literal(self):
    return self._literal

  def Substitute(self, value):
    return self._formatter.vformat(str(value), (), self)

  def Resolve(self, visitor, key, value):
    """Resolves a value to it's final form.

    Raw values can be callable, simple values, or contain format strings.
    Args:
      visitor: The visitor asking to resolve a value.
      key: The key being visited.
      value: The unresolved value associated with the key.
    Returns:
      the fully resolved value.
    """
    error = None
    if callable(value):
      value = value(self)
    # Using existence of value.swapcase as a proxy for is a string
    elif hasattr(value, 'swapcase'):
      if not visitor.current_node.literal:
        try:
          value = self.Substitute(value)
        except KeyError as e:
          error = e
    return self.Fixup(key, value), error

  def Fixup(self, key, value):
    for hook in self.fixup_hooks:
      value = hook(self, key, value)
    return value

  def Missing(self, key):
    for hook in self.fixup_hooks:
      hook(self, key, None)
    raise KeyError(key)

  @staticmethod
  def ParseValue(value):
    """Converts a string to a value.

    Takes a string from something like an environment variable, and tries to
    build an internal typed value. Recognizes Null, booleans, and numbers as
    special.
    Args:
        value: The the string value to interpret.
    Returns:
        the parsed form of the value.
    """
    if value in _PARSE_CONSTANTS:
      return _PARSE_CONSTANTS[value]
    try:
      return int(value)
    except ValueError:
      pass
    try:
      return float(value)
    except ValueError:
      pass
    return value

  def _Set(self, key, value):
    # early out if the value did not change, so we don't call change callbacks
    if value == self._values.get(key, None):
      return
    self._values[key] = value
    self.NotifyChanged()
    return self

  def ApplyMap(self, arg):
    for key, value in arg.items():
      self._Set(key, value)
    return self

  def Apply(self, args, kwargs):
    """Bulk set variables from arguments.

    Intended for internal use by the Set and From methods.
    Args:
        args: must be either a dict or something that can build a dict.
        kwargs: must be a dict.
    Returns:
        self for easy chaining.
    """
    if len(args) == 1:
      arg = args[0]
      if isinstance(arg, dict):
        self.ApplyMap(arg)
      else:
        self.ApplyMap(dict(arg))
    elif len(args) > 1:
      self.ApplyMap(dict(args))
    self.ApplyMap(kwargs)
    return self

  def Set(self, *args, **kwargs):
    return self.Apply(args, kwargs)

  def Trace(self):
    return _Tracer(self)

  def __getitem__(self, key):
    return self.Get(key)

  def __setitem__(self, key, value):
    self._Set(key, value)

  def __contains__(self, key):
    return self.Find(key) is not None
