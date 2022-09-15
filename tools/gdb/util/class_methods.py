# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper library for defining XMethod implementations on C++ classes.

Include this library and then define python implementations of C++ methods
using the Class and member_function decorator functions.
"""

import gdb
import gdb.xmethod
import operator
import re


class MemberFunction(object):
  def __init__(self, return_type, name, arguments, wrapped_function):
    self.return_type = return_type
    self.name = name
    self.arguments = arguments
    self.function_ = wrapped_function

  def __call__(self, *args):
    self.function_(*args)


def member_function(return_type, name, arguments):
  """Decorate a member function.

  See Class decorator for example usage within a class.

  Args:
    return_type: The return type of the function (e.g. 'int')
    name: The function name (e.g. 'sum')
    arguments: The argument types for this function (e.g. ['int', 'int'])

    Each type can be a string (e.g. 'int', 'std::string', 'T*') or a
    function which constructs the return type. See CreateTypeResolver
    for details about type resolution.
  """
  def DefineMember(fn):
    return MemberFunction(return_type, name, arguments, fn)
  return DefineMember


def Class(class_name, template_types):
  """Decorate a python class with its corresponding C++ type.
  Args:
    class_name: The canonical string identifier for the class (e.g. base::Foo)
    template_types: An array of names for each templated type (e.g. ['K',
    'V'])

  Example:
    As an example, the following is an implementation of size() and operator[]
    on std::__1::vector, functions which are normally inlined and not
    normally callable from gdb.

    @class_methods.Class('std::__1::vector', template_types=['T'])
    class LibcppVector(object):
      @class_methods.member_function('T&', 'operator[]', ['int'])
      def element(obj, i):
        return obj['__begin_'][i]

      @class_methods.member_function('size_t', 'size', [])
      def size(obj):
        return obj['__end_'] - obj['__begin_']

  Note:
    Note that functions are looked up by the function name, which means that
    functions cannot currently have overloaded implementations for different
    arguments.
  """

  class MethodWorkerWrapper(gdb.xmethod.XMethod):
    """Wrapper of an XMethodWorker class as an XMethod."""
    def __init__(self, name, worker_class):
      super(MethodWorkerWrapper, self).__init__(name)
      self.name = name
      self.worker_class = worker_class


  class ClassMatcher(gdb.xmethod.XMethodMatcher):
    """Matches member functions of one class template."""
    def __init__(self, obj):
      super(ClassMatcher, self).__init__(class_name)

      # Constructs a regular expression to match this type.
      self._class_regex = re.compile(
          '^' + re.escape(class_name) +
          ('<.*>' if len(template_types) > 0 else '') + '$')

      # Construct a dictionary and array of methods
      self.dict = {}
      self.methods = []
      for name in dir(obj):
        attr = getattr(obj, name)
        if not isinstance(attr, MemberFunction):
          continue

        name = attr.name
        return_type = CreateTypeResolver(attr.return_type)
        arguments = [CreateTypeResolver(arg) for arg in
                     attr.arguments]
        method = MethodWorkerWrapper(
            attr.name,
            CreateTemplatedMethodWorker(return_type,
                                        arguments, attr.function_))
        self.methods.append(method)

    def match(self, class_type, method_name):
      if not re.match(self._class_regex, class_type.tag):
        return None
      templates = [class_type.template_argument(i) for i in
                   range(len(template_types))]
      return [method.worker_class(templates) for method in self.methods
              if method.name == method_name and method.enabled]


  def CreateTypeResolver(type_desc):
    """Creates a callback which resolves to the appropriate type when
    invoked.

    This is a helper to allow specifying simple types as strings when
    writing function descriptions. For complex cases, a callback can be
    passed which will be invoked when template instantiation is known.

    Args:
      type_desc: A callback generating the type or a string description of
          the type to lookup. Supported types are classes in the
          template_classes array (e.g. T) which will be looked up when those
          templated classes are known, or globally visible type names (e.g.
          int, base::Foo).

          Types can be modified by appending a '*' or '&' to denote a
          pointer or reference.

          If a callback is used, the callback will be passed an array of the
          instantiated template types.

    Note:
      This does not parse complex types such as std::vector<T>::iterator,
      to refer to types like these you must currently write a callback
      which constructs the appropriate type.
    """
    if callable(type_desc):
      return type_desc
    if type_desc == 'void':
      return lambda T: None
    if type_desc[-1] == '&':
      inner_resolver = CreateTypeResolver(type_desc[:-1])
      return lambda template_types: inner_resolver(template_types).reference()
    if type_desc[-1] == '*':
      inner_resolver = CreateTypeResolver(type_desc[:-1])
      return lambda template_types: inner_resolver(template_types).pointer()
    try:
      template_index = template_types.index(type_desc)
      return operator.itemgetter(template_index)
    except ValueError:
      return lambda template_types: gdb.lookup_type(type_desc)


  def CreateTemplatedMethodWorker(return_callback, args_callbacks,
                                  method_callback):
    class TemplatedMethodWorker(gdb.xmethod.XMethodWorker):
      def __init__(self, templates):
        super(TemplatedMethodWorker, self).__init__()
        self._templates = templates

      def get_arg_types(self):
        return [cb(self._templates) for cb in args_callbacks]

      def get_result_type(self, obj):
        return return_callback(self._templates)

      def __call__(self, *args):
        return method_callback(*args)
    return TemplatedMethodWorker

  def DefineClass(obj):
    matcher = ClassMatcher(obj)
    gdb.xmethod.register_xmethod_matcher(None, matcher)
    return matcher

  return DefineClass
