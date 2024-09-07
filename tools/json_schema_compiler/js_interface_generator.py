# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Generator that produces an interface file for the Closure Compiler.
Note: This is a work in progress, and generated interfaces may require tweaking.
"""

from code_util import Code
from js_util import JsUtil
from model import *
from schema_util import *

import os
import sys
import re


class JsInterfaceGenerator(object):

  def Generate(self, namespace):
    return _Generator(namespace).Generate()


class _Generator(object):

  def __init__(self, namespace):
    self._namespace = namespace
    first = namespace.name[0].upper()
    rest = namespace.name[1:]
    self._interface = first + rest
    self._js_util = JsUtil()

  def Generate(self):
    """Generates a Code object with the schema for the entire namespace.
    """
    c = Code()
    (c.Append(self._GetHeader(sys.argv[0], self._namespace.name)) \
      .Append())

    self._AppendInterfaceObject(c)
    c.Append()

    c.Sblock('%s.prototype = {' % self._interface)

    for function in self._namespace.functions.values():
      self._AppendFunction(c, function)

    c.TrimTrailingNewlines()
    c.Eblock('};')
    c.Append()

    for event in self._namespace.events.values():
      self._AppendEvent(c, event)

    c.TrimTrailingNewlines()

    return c

  def _GetHeader(self, tool, namespace):
    """Returns the file header text.
    """
    return (self._js_util.GetLicense() + '\n' + self._js_util.GetInfo(tool) +
            '\n' +
            ('/** @fileoverview Interface for %s that can be overriden. */' %
             namespace))

  def _AppendInterfaceObject(self, c):
    """Appends the code creating the interface object.
       For example:
       /** @interface */
       function SettingsPrivate() {}
    """
    (c.Append('/** @interface */') \
      .Append('function %s() {}' % self._interface))

  def _AppendFunction(self, c, function):
    """Appends the inteface for a function, including a JSDoc comment.
    """
    if function.deprecated:
      return

    def getParamNames(f):
      names = []
      for param in f.params:
        names.append(param.name)
      # TODO(crbug.com/40728031) Update this to represent promises
      # better, rather than just appended as a callback param.
      if f.returns_async:
        names.append(f.returns_async.name)
      return names

    self._js_util.AppendFunctionJsDoc(c, self._namespace.name, function)

    c.Append('%s: function(%s) {},' %
             (function.name, ', '.join(getParamNames(function))))
    c.Append()

  def _AppendEvent(self, c, event):
    """Appends the interface for an event.
    """
    c.Sblock(line='/**', line_prefix=' * ')
    if (event.description):
      c.Comment(event.description, comment_prefix='')
    c.Append('@type {!ChromeEvent}')
    self._js_util.AppendSeeLink(c, self._namespace.name, 'event', event.name)
    c.Eblock(' */')

    c.Append('%s.prototype.%s;' % (self._interface, event.name))

    c.Append()
