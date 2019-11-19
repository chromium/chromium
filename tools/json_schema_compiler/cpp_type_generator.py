# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from code import Code
from model import PropertyType
import cpp_util
from json_parse import OrderedDict
import schema_util

class _TypeDependency(object):
  """Contains information about a dependency a namespace has on a type: the
  type's model, and whether that dependency is "hard" meaning that it cannot be
  forward declared.
  """
  def __init__(self, type_, hard=False):
    self.type_ = type_
    self.hard = hard

  def GetSortKey(self):
    return '%s.%s' % (self.type_.namespace.name, self.type_.name)


class CppTypeGenerator(object):
  """Manages the types of properties and provides utilities for getting the
  C++ type out of a model.Property
  """
  def __init__(self, model, namespace_resolver, default_namespace=None):
    """Creates a cpp_type_generator. The given root_namespace should be of the
    format extensions::api::sub. The generator will generate code suitable for
    use in the given model's namespace.
    """
    self._default_namespace = default_namespace
    if self._default_namespace is None:
      self._default_namespace = model.namespaces.values()[0]
    self._namespace_resolver = namespace_resolver

  def GetEnumNoneValue(self, type_):
    """Gets the enum value in the given model.Property indicating no value has
    been set.
    """
    return '%s_NONE' % self.FollowRef(type_).unix_name.upper()

  def GetEnumLastValue(self, type_):
    """Gets the enum value in the given model.Property indicating the last value
    for the type.
    """
    return '%s_LAST' % self.FollowRef(type_).unix_name.upper()

  def GetEnumValue(self, type_, enum_value):
    """Gets the enum value of the given model.Property of the given type.

    e.g VAR_STRING
    """
    value = cpp_util.Classname(enum_value.name.upper())
    prefix = (type_.cpp_enum_prefix_override or
              self.FollowRef(type_).unix_name)
    value = '%s_%s' % (prefix.upper(), value)
    # To avoid collisions with built-in OS_* preprocessor definitions, we add a
    # trailing slash to enum names that start with OS_.
    if value.startswith("OS_"):
      value += "_"
    return value

  def GetCppType(self, type_, is_ptr=False, is_in_container=False):
    """Translates a model.Property or model.Type into its C++ type.

    If REF types from different namespaces are referenced, will resolve
    using self._namespace_resolver.

    Use |is_ptr| if the type is optional. This will wrap the type in a
    scoped_ptr if possible (it is not possible to wrap an enum).

    Use |is_in_container| if the type is appearing in a collection, e.g. a
    std::vector or std::map. This will wrap it in the correct type with spacing.
    """
    cpp_type = None
    if type_.property_type == PropertyType.REF:
      ref_type = self._FindType(type_.ref_type)
      if ref_type is None:
        raise KeyError('Cannot find referenced type: %s' % type_.ref_type)
      cpp_type = self.GetCppType(ref_type)
    elif type_.property_type == PropertyType.BOOLEAN:
      cpp_type = 'bool'
    elif type_.property_type == PropertyType.INTEGER:
      cpp_type = 'int'
    elif type_.property_type == PropertyType.INT64:
      cpp_type = 'int64_t'
    elif type_.property_type == PropertyType.DOUBLE:
      cpp_type = 'double'
    elif type_.property_type == PropertyType.STRING:
      cpp_type = 'std::string'
    elif type_.property_type in (PropertyType.ENUM,
                                 PropertyType.OBJECT,
                                 PropertyType.CHOICES):
      if self._default_namespace is type_.namespace:
        cpp_type = cpp_util.Classname(type_.name)
      else:
        cpp_namespace = cpp_util.GetCppNamespace(
            type_.namespace.environment.namespace_pattern,
            type_.namespace.unix_name)
        cpp_type = '%s::%s' % (cpp_namespace,
                               cpp_util.Classname(type_.name))
    elif type_.property_type == PropertyType.ANY:
      cpp_type = 'base::Value'
    elif type_.property_type == PropertyType.FUNCTION:
      # Functions come into the json schema compiler as empty objects. We can
      # record these as empty DictionaryValues so that we know if the function
      # was passed in or not.
      cpp_type = 'base::DictionaryValue'
    elif type_.property_type == PropertyType.ARRAY:
      item_cpp_type = self.GetCppType(type_.item_type, is_in_container=True)
      cpp_type = 'std::vector<%s>' % item_cpp_type
    elif type_.property_type == PropertyType.BINARY:
      cpp_type = 'std::vector<uint8_t>'
    else:
      raise NotImplementedError('Cannot get type of %s' % type_.property_type)

    # HACK: optional ENUM is represented elsewhere with a _NONE value, so it
    # never needs to be wrapped in pointer shenanigans.
    # TODO(kalman): change this - but it's an exceedingly far-reaching change.
    if not self.FollowRef(type_).property_type == PropertyType.ENUM:
      is_base_value = (cpp_type == 'base::Value' or
                       cpp_type == 'base::DictionaryValue')
      # Wrap ptrs and base::Values in containers (which aren't movable) in
      # scoped_ptrs.
      if is_ptr or (is_in_container and is_base_value):
        cpp_type = 'std::unique_ptr<%s>' % cpp_type

    return cpp_type

  def IsCopyable(self, type_):
    return not (self.FollowRef(type_).property_type in (PropertyType.ANY,
                                                        PropertyType.ARRAY,
                                                        PropertyType.OBJECT,
                                                        PropertyType.CHOICES))

  def GenerateForwardDeclarations(self):
    """Returns the forward declarations for self._default_namespace.
    """
    c = Code()
    for namespace, deps in self._NamespaceTypeDependencies().items():
      filtered_deps = [
        dep for dep in deps
        # Add more ways to forward declare things as necessary.
        if (not dep.hard and
            dep.type_.property_type in (PropertyType.CHOICES,
                                        PropertyType.OBJECT))]
      if not filtered_deps:
        continue

      cpp_namespace = cpp_util.GetCppNamespace(
          namespace.environment.namespace_pattern,
          namespace.unix_name)
      c.Concat(cpp_util.OpenNamespace(cpp_namespace))
      for dep in filtered_deps:
        c.Append('struct %s;' % dep.type_.name)
      c.Concat(cpp_util.CloseNamespace(cpp_namespace))
    return c

  def GenerateIncludes(self, include_soft=False):
    """Returns the #include lines for self._default_namespace.
    """
    c = Code()
    for namespace, dependencies in self._NamespaceTypeDependencies().items():
      for dependency in dependencies:
        if dependency.hard or include_soft:
          c.Append('#include "%s/%s.h"' % (namespace.source_file_dir,
                                           namespace.unix_name))
    return c

  def _FindType(self, full_name):
    """Finds the model.Type with name |qualified_name|. If it's not from
    |self._default_namespace| then it needs to be qualified.
    """
    namespace = self._namespace_resolver.ResolveType(full_name,
                                                     self._default_namespace)
    if namespace is None:
      raise KeyError('Cannot resolve type %s. Maybe it needs a prefix '
                     'if it comes from another namespace?' % full_name)
    return namespace.types[schema_util.StripNamespace(full_name)]

  def FollowRef(self, type_):
    """Follows $ref link of types to resolve the concrete type a ref refers to.

    If the property passed in is not of type PropertyType.REF, it will be
    returned unchanged.
    """
    if type_.property_type != PropertyType.REF:
      return type_
    return self.FollowRef(self._FindType(type_.ref_type))

  def _NamespaceTypeDependencies(self):
    """Returns a dict ordered by namespace name containing a mapping of
    model.Namespace to every _TypeDependency for |self._default_namespace|,
    sorted by the type's name.
    """
    dependencies = set()
    for function in self._default_namespace.functions.values():
      for param in function.params:
        dependencies |= self._TypeDependencies(param.type_,
                                               hard=not param.optional)
      if function.callback:
        for param in function.callback.params:
          dependencies |= self._TypeDependencies(param.type_,
                                                 hard=not param.optional)
    for type_ in self._default_namespace.types.values():
      for prop in type_.properties.values():
        dependencies |= self._TypeDependencies(prop.type_,
                                               hard=not prop.optional)
    for event in self._default_namespace.events.values():
      for param in event.params:
        dependencies |= self._TypeDependencies(param.type_,
                                               hard=not param.optional)

    # Make sure that the dependencies are returned in alphabetical order.
    dependency_namespaces = OrderedDict()
    for dependency in sorted(dependencies, key=_TypeDependency.GetSortKey):
      namespace = dependency.type_.namespace
      if namespace is self._default_namespace:
        continue
      if namespace not in dependency_namespaces:
        dependency_namespaces[namespace] = []
      dependency_namespaces[namespace].append(dependency)

    return dependency_namespaces

  def _TypeDependencies(self, type_, hard=False):
    """Gets all the type dependencies of a property.
    """
    deps = set()
    if type_.property_type == PropertyType.REF:
      deps.add(_TypeDependency(self._FindType(type_.ref_type), hard=hard))
    elif type_.property_type == PropertyType.ARRAY:
      # Types in containers are hard dependencies because they are stored
      # directly and use move semantics.
      deps = self._TypeDependencies(type_.item_type, hard=hard)
    elif type_.property_type == PropertyType.CHOICES:
      for type_ in type_.choices:
        deps |= self._TypeDependencies(type_, hard=self.IsCopyable(type_))
    elif type_.property_type == PropertyType.OBJECT:
      for p in type_.properties.values():
        deps |= self._TypeDependencies(p.type_, hard=not p.optional)
    return deps

  def GeneratePropertyValues(self, prop, line, nodoc=False):
    """Generates the Code to display all value-containing properties.
    """
    c = Code()
    if not nodoc:
      c.Comment(prop.description)

    if prop.value is not None:
      cpp_type = self.GetCppType(prop.type_)
      cpp_value = prop.value
      cpp_name = prop.name

      if cpp_type == 'std::string':
        cpp_value = '"%s"' % cpp_value
        cpp_type = 'char'
        cpp_name = '%s[]' % cpp_name
      c.Append(line % {
        "type": cpp_type,
        "name": cpp_name,
        "value": cpp_value
      })
    else:
      has_child_code = False
      c.Sblock('namespace %s {' % prop.name)
      for child_property in prop.type_.properties.values():
        child_code = self.GeneratePropertyValues(child_property,
                                                 line,
                                                 nodoc=nodoc)
        if child_code:
          has_child_code = True
          c.Concat(child_code)
      c.Eblock('}  // namespace %s' % prop.name)
      if not has_child_code:
        c = None
    return c
