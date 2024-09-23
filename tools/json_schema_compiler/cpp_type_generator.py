# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from code_util import Code
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
      self._default_namespace = list(model.namespaces.values())[0]
    self._namespace_resolver = namespace_resolver

  def GetOptionalReturnType(self, typename, support_errors=False):
    """ Composes a C++ return type to be used as a return value. Wraps the
    typename in an optional, for the regular case, or uses a base::expected for
    when it should support string errors.
    """
    return (('base::expected<{typename}, std::u16string>' if support_errors else
             'std::optional<{typename}>').format(typename=typename))

  def GetEnumNoneValue(self, type_, full_name=True):
    """Gets the enum value in the given model. Property indicating no value has
    been set.
    """
    prefix = ''
    if full_name:
      classname = cpp_util.Classname(type_.name)
      prefix = '{typename}::'.format(typename=classname)
    return '{enum_name}kNone'.format(enum_name=prefix)

  def GetEnumDefaultValue(self, type_, current_namespace):
    """Gets the representation for an enum default initialised, which is the
    typename with a default initialiser. e.g. MyEnum().
    """
    cpp_type = self.GetCppType(type_)
    return '{cpp_type}()'.format(cpp_type=cpp_type)

  def FormatStringForEnumValue(self, name):
    """Formats a string enum entry to the common constant format favoured by the
    style guide.

    Output examples:
      SHOUTY_CASE: kShoutyCase
      underscore_case: kUnderscoreCase
      dash-case: kDashCase
      camelCaseWithLowerFirst: kCamelCaseWithLowerFirst
      CamelCaseWithUpperFirst: kCamelCaseWithUpperFirst.
      x86_64: kX86_64
      x86_ARCH: kX86Arch
      '': EmptyString
      kConstantSupport: kConstantSupport.
    """

    if not name:
      return 'EmptyString'

    # For cases where the enum entry is something like kValue, an exception is
    # made to drop the initial `k` to avoid generating a key that looks like
    # kKvalue, which is less readable than kValue.
    if len(name) > 1 and name.startswith('k') and name[1].isupper():
      name = name[1:]

    change_to_upper = True
    last_was_lower = True
    result = ''
    for char in name:
      if char in {'_', '-'}:
        change_to_upper = True
      elif change_to_upper:
        # Numbers must be kept separate, for better readability (e.g. kX86_64).
        if char.isnumeric() and result and result[-1].isnumeric():
          result += '_'

        result += char.upper()
        change_to_upper = False
        last_was_lower = False
      elif last_was_lower and char.isupper():
        result += char
        last_was_lower = False
      elif char.isupper():
        result += char.lower()
      else:
        result += char
        last_was_lower = True

    return result

  def GetEnumValue(self, type_, enum_value, full_name=True):
    """Gets the enum value of the given model.Property of the given type.

    |full_name| is set to true, producing an enum value with a fully qualified
    name.

    e.g Enum::kValue
    """
    prefix = ''
    if full_name:
      classname = cpp_util.Classname(type_.name)
      prefix = '{classname}::'.format(classname=classname)
    # We kCamelCase the string, also removing any _ from the name, to allow
    # SHOUTY_CASE keys to be kCamelCase as well.
    return '{prefix}k{name}'.format(prefix=prefix,
                                    name=self.FormatStringForEnumValue(
                                        enum_value.name))

  def GetCppType(self, type_, is_optional=False):
    """Translates a model.Property or model.Type into its C++ type.

    If REF types from different namespaces are referenced, will resolve
    using self._namespace_resolver.

    Use |is_optional| if the type is optional. This will wrap the type either
    in an optional, or in a unique_ptr if possible (it is not possible to wrap
    an enum).
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
    elif type_.property_type in (PropertyType.ENUM, PropertyType.OBJECT,
                                 PropertyType.CHOICES):
      if self._default_namespace is type_.namespace:
        cpp_type = cpp_util.Classname(type_.name)
      else:
        cpp_namespace = cpp_util.GetCppNamespace(
            type_.namespace.environment.namespace_pattern,
            type_.namespace.unix_name)
        cpp_type = '%s::%s' % (cpp_namespace, cpp_util.Classname(type_.name))
    elif type_.property_type == PropertyType.ANY:
      cpp_type = 'base::Value'
    elif type_.property_type == PropertyType.FUNCTION:
      if type_.is_serializable_function:
        # Serializable functions get transformed into strings.
        cpp_type = 'std::string'
      else:
        # Non-serializable functions come into the json schema compiler as
        # empty objects. We can record these as empty Value::Dict so that
        # we know if the function was passed in or not.
        cpp_type = 'base::Value::Dict'
    elif type_.property_type == PropertyType.ARRAY:
      item_cpp_type = self.GetCppType(type_.item_type)
      if item_cpp_type == 'base::Value':
        cpp_type = 'base::Value::List'
      else:
        cpp_type = 'std::vector<%s>' % item_cpp_type
    elif type_.property_type == PropertyType.BINARY:
      cpp_type = 'std::vector<uint8_t>'
    else:
      raise NotImplementedError('Cannot get type of %s' % type_.property_type)

    # HACK: optional ENUM is represented elsewhere with a _NONE value, so it
    # never needs to be wrapped in pointer shenanigans.
    # TODO(kalman): change this - but it's an exceedingly far-reaching change.
    if not self.FollowRef(type_).property_type == PropertyType.ENUM:
      if is_optional:
        if cpp_util.ShouldUseStdOptional(self.FollowRef(type_)):
          cpp_type = 'std::optional<%s>' % cpp_type
        else:
          cpp_type = 'std::unique_ptr<%s>' % cpp_type

    return cpp_type

  def IsCopyable(self, type_):
    return not (self.FollowRef(type_).property_type
                in (PropertyType.ANY, PropertyType.ARRAY, PropertyType.OBJECT,
                    PropertyType.CHOICES))

  def GenerateForwardDeclarations(self):
    """Returns the forward declarations for self._default_namespace.
    """
    c = Code()
    for namespace, deps in self._NamespaceTypeDependencies().items():
      filtered_deps = [
          dep for dep in deps
          # Add more ways to forward declare things as necessary.
          if (not dep.hard and dep.type_.property_type in (PropertyType.CHOICES,
                                                           PropertyType.OBJECT))
      ]
      if not filtered_deps:
        continue

      cpp_namespace = cpp_util.GetCppNamespace(
          namespace.environment.namespace_pattern, namespace.unix_name)
      c.Concat(cpp_util.OpenNamespace(cpp_namespace))
      for dep in filtered_deps:
        c.Append('struct %s;' % dep.type_.name)
      c.Concat(cpp_util.CloseNamespace(cpp_namespace))
    return c

  def GenerateIncludes(self, include_soft=False, generate_error_messages=False):
    """Returns the #include lines for self._default_namespace.
    """
    c = Code()

    # The inclusion of the std::string_view header is dependent on either the
    # presence of enums, or manifest keys.
    include_string_view = (self._default_namespace.manifest_keys or any(
        type_.property_type is PropertyType.ENUM
        for type_ in self._default_namespace.types.values()))

    if include_string_view:
      c.Append('#include <string_view>')

    # The header for `base::expected` should be included whenever error messages
    # are supposed to be returned, which only occurs with object, choices, or
    # functions.
    if (generate_error_messages
        and (len(self._default_namespace.functions.values()) or any(
            type_.property_type in [PropertyType.OBJECT, PropertyType.CHOICES]
            for type_ in self._default_namespace.types.values()))):
      c.Append('#include "base/types/expected.h"')

    # Note: It's possible that there are multiple dependencies from the same
    # API. Make sure to only include them once.
    added_paths = set()
    for namespace, dependencies in self._NamespaceTypeDependencies().items():
      for dependency in dependencies:
        if dependency.hard or include_soft:
          path = '%s/%s.h' % (namespace.source_file_dir, namespace.unix_name)
          if path not in added_paths:
            added_paths.add(path)
            c.Append('#include "%s"' % cpp_util.ToPosixPath(path))
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
      if function.returns_async:
        for param in function.returns_async.params:
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
      underlying_type = self._FindType(type_.ref_type)
      # Enums from other namespaces are always hard dependencies, since
      # optional enums are represented via the _NONE value instead of a
      # pointer.
      dep_is_hard = (True if underlying_type.property_type == PropertyType.ENUM
                     else hard)
      deps.add(_TypeDependency(underlying_type, hard=dep_is_hard))
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
      c.Append(line % {"type": cpp_type, "name": cpp_name, "value": cpp_value})
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
