# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Generator that produces an externs file for the Closure Compiler.
Note: This is a work in progress, and generated externs may require tweaking.

See https://developers.google.com/closure/compiler/docs/api-tutorial3#externs
"""

from code_util import Code
from js_util import JsUtil
from model import *
from schema_util import *

import os
import sys
import re

NOTE = """// NOTE: The format of types has changed. 'FooType' is now
//   'chrome.%s.FooType'.
// Please run the closure compiler before committing changes.
// See https://chromium.googlesource.com/chromium/src/+/main/docs/closure_compilation.md
"""


class JsExternsGenerator(object):

  def Generate(self, namespace):
    return _Generator(namespace).Generate()


class _Generator(object):

  def __init__(self, namespace):
    self._namespace = namespace
    self._class_name = None
    self._js_util = JsUtil()

  def Generate(self):
    """Generates a Code object with the schema for the entire namespace.
    """
    c = Code()
    # /abs/path/src/tools/json_schema_compiler/
    script_dir = os.path.dirname(os.path.abspath(__file__))
    # /abs/path/src/
    src_root = os.path.normpath(os.path.join(script_dir, '..', '..'))
    # tools/json_schema_compiler/
    src_to_script = os.path.relpath(script_dir, src_root)
    # tools/json_schema_compiler/compiler.py
    compiler_path = os.path.join(src_to_script, 'compiler.py')
    (c.Append(self._GetHeader(compiler_path, self._namespace.name)) \
      .Append())

    self._AppendNamespaceObject(c)

    for js_type in self._namespace.types.values():
      self._AppendType(c, js_type)

    for prop in self._namespace.properties.values():
      self._AppendProperty(c, prop)

    for function in self._namespace.functions.values():
      self._AppendFunction(c, function)

    for event in self._namespace.events.values():
      self._AppendEvent(c, event)

    c.TrimTrailingNewlines()

    return c

  def _GetHeader(self, tool, namespace):
    """Returns the file header text.
    """
    return (self._js_util.GetLicense() + '\n' + self._js_util.GetInfo(tool) +
            (NOTE % namespace) + '\n' + '/**\n' +
            (' * @fileoverview Externs generated from namespace: %s\n' %
             namespace) + ' * @externs\n' + ' */')

  def _AppendType(self, c, js_type):
    """Given a Type object, generates the Code for this type's definition.
    """
    if js_type.property_type is PropertyType.ENUM:
      self._AppendEnumJsDoc(c, js_type)
    else:
      self._AppendTypeJsDoc(c, js_type)
    c.Append()

  def _AppendEnumJsDoc(self, c, js_type):
    """ Given an Enum Type object, generates the Code for the enum's definition.
    """
    c.Sblock(line='/**', line_prefix=' * ')
    c.Append('@enum {string}')
    self._js_util.AppendSeeLink(c, self._namespace.name, 'type',
                                js_type.simple_name)
    c.Eblock(' */')
    c.Append('%s.%s = {' % (self._GetNamespace(), js_type.name))
    c.Append('\n'.join([
        "  %s: '%s'," % (self._js_util.GetPropertyName(v.name), v.name)
        for v in js_type.enum_values
    ]))
    c.Append('};')

  def _IsTypeConstructor(self, js_type):
    """Returns true if the given type should be a @constructor. If this returns
       false, the type is a typedef.
    """
    return any(prop.type_.property_type is PropertyType.FUNCTION
               for prop in js_type.properties.values())

  def _AppendTypeJsDoc(self, c, js_type, optional=False):
    """Appends the documentation for a type as a Code.
    """
    c.Sblock(line='/**', line_prefix=' * ')

    if js_type.description:
      for line in js_type.description.splitlines():
        c.Comment(line, comment_prefix='')

    is_constructor = self._IsTypeConstructor(js_type)
    if js_type.property_type is not PropertyType.OBJECT:
      self._js_util.AppendTypeJsDoc(c, self._namespace.name, js_type, optional)
    elif is_constructor:
      c.Comment('@constructor', comment_prefix='', wrap_indent=4)
      c.Comment('@private', comment_prefix='', wrap_indent=4)
    else:
      self._AppendTypedef(c, js_type.properties)

    self._js_util.AppendSeeLink(c, self._namespace.name, 'type',
                                js_type.simple_name)
    c.Eblock(' */')

    var = '%s.%s' % (self._GetNamespace(), js_type.simple_name)
    if is_constructor: var += ' = function() {}'
    var += ';'
    c.Append(var)

    if is_constructor:
      c.Append()
      self._class_name = js_type.name
      for prop in js_type.properties.values():
        if prop.type_.property_type is PropertyType.FUNCTION:
          self._AppendFunction(c, prop.type_.function)
        else:
          self._AppendTypeJsDoc(c, prop.type_, prop.optional)
          c.Append()
      self._class_name = None

  def _AppendTypedef(self, c, properties):
    """Given an OrderedDict of properties, Appends code containing a @typedef.
    """

    c.Append('@typedef {')
    if properties:
      self._js_util.AppendObjectDefinition(c,
                                           self._namespace.name,
                                           properties,
                                           new_line=False)
    else:
      c.Append('Object', new_line=False)
    c.Append('}', new_line=False)

  def _AppendProperty(self, c, prop):
    """Appends the code representing a top-level property, including its
       documentation. For example:

       /** @type {string} */
       chrome.runtime.id;
    """
    self._AppendTypeJsDoc(c, prop.type_, prop.optional)
    c.Append()

  def _AppendFunction(self, c, function):
    """Appends the code representing a function, including its documentation.
       For example:

       /**
        * @param {string} title The new title.
        */
       chrome.window.setTitle = function(title) {};
    """
    self._js_util.AppendFunctionJsDoc(c, self._namespace.name, function)
    params = self._GetFunctionParams(function)
    c.Append('%s.%s = function(%s) {};' %
             (self._GetNamespace(), function.name, params))
    c.Append()

  def _AppendEvent(self, c, event):
    """Appends the code representing an event.
       For example:

       /** @type {!ChromeEvent} */
       chrome.bookmarks.onChildrenReordered;
    """
    c.Sblock(line='/**', line_prefix=' * ')
    if (event.description):
      c.Comment(event.description, comment_prefix='')
    c.Append('@type {!ChromeEvent}')
    self._js_util.AppendSeeLink(c, self._namespace.name, 'event', event.name)
    c.Eblock(' */')
    c.Append('%s.%s;' % (self._GetNamespace(), event.name))
    c.Append()

  def _AppendNamespaceObject(self, c):
    """Appends the code creating namespace object.
       For example:

       /** @const */
       chrome.bookmarks = {};
    """
    c.Append('/** @const */')
    c.Append('chrome.%s = {};' % self._namespace.name)
    c.Append()

  def _GetFunctionParams(self, function):
    """Returns the function params string for function.
    """
    params = function.params[:]
    param_names = [param.name for param in params]
    # TODO(crbug.com/40728031): Update this to represent promises better,
    # rather than just appended as a callback.
    if function.returns_async:
      param_names.append(function.returns_async.name)
    return ', '.join(param_names)

  def _GetNamespace(self):
    """Returns the namespace to be prepended to a top-level typedef.

       For example, it might return "chrome.namespace".

       Also optionally includes the class name if this is in the context
       of outputting the members of a class.

       For example, "chrome.namespace.ClassName.prototype"
    """
    if self._class_name:
      return 'chrome.%s.%s.prototype' % (self._namespace.name, self._class_name)
    return 'chrome.%s' % self._namespace.name
