# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations
import dataclasses
from typing import Dict
from typing import Optional
from typing import Tuple

import common
import java_lang_classes

CPP_TYPE_BY_JAVA_TYPE = {
    'boolean': 'jboolean',
    'byte': 'jbyte',
    'char': 'jchar',
    'double': 'jdouble',
    'float': 'jfloat',
    'int': 'jint',
    'long': 'jlong',
    'short': 'jshort',
    'void': 'void',
    'java/lang/Class': 'jclass',
    'java/lang/Object': 'jobject',
    'java/lang/String': 'jstring',
    'java/lang/Throwable': 'jthrowable',
}

# Replaced with CPP_TYPE_BY_JAVA_TYPE based on --use-std-primitive-types.
CPP_UNDERLYING_TYPE_BY_JAVA_TYPE = {
    'boolean': 'bool',  # underlying type of jboolean
    'byte': 'int8_t',  # underlying type of jbyte
    'char': 'uint16_t',  # underlying type of jchar
    'double': 'double',  # underlying type of jdouble
    'float': 'float',  # underlying type of jfloat
    'int': 'int32_t',  # underlying type of jint
    'long': 'int64_t',  # underlying type of jlong
    'short': 'int16_t',  # underlying type of jshort
    'void': 'void',
    'java/lang/Class': 'jclass',
    'java/lang/Object': 'jobject',
    'java/lang/String': 'jstring',
    'java/lang/Throwable': 'jthrowable',
}

_DESCRIPTOR_CHAR_BY_PRIMITIVE_TYPE = {
    'boolean': 'Z',
    'byte': 'B',
    'char': 'C',
    'double': 'D',
    'float': 'F',
    'int': 'I',
    'long': 'J',
    'short': 'S',
    'void': 'V',
}

_PRIMITIVE_TYPE_BY_DESCRIPTOR_CHAR = {
    v: k
    for k, v in _DESCRIPTOR_CHAR_BY_PRIMITIVE_TYPE.items()
}

_DEFAULT_VALUE_BY_PRIMITIVE_TYPE = {
    'boolean': 'false',
    'byte': '0',
    'char': '0',
    'double': '0',
    'float': '0',
    'int': '0',
    'long': '0',
    'short': '0',
    'void': '',
}

PRIMITIVES = frozenset(_DEFAULT_VALUE_BY_PRIMITIVE_TYPE)


@dataclasses.dataclass(frozen=True, order=True)
class JavaClass:
  """Represents a reference type."""
  _fqn: str
  # Package prefix (via make_prefix()).
  _prefix: str = None
  upper_bound_type: Optional[JavaClass] = None

  def __post_init__(self):
    assert '.' not in self._fqn, f'{self._fqn} should have / and $, but not .'

  def __str__(self):
    return self.full_name_with_slashes

  @staticmethod
  def for_generics(placeholder_name, upper_bound_type):
    return JavaClass(placeholder_name, upper_bound_type=upper_bound_type)

  @property
  def name(self):
    return self._fqn.rsplit('/', 1)[-1]

  @property
  def name_with_dots(self):
    return self.name.replace('$', '.')

  @property
  def name_with_underscores(self):
    return self.name.replace('$', '_')

  @property
  def nested_name(self):
    return self.name.rsplit('$', 1)[-1]

  @property
  def jobject_name(self):
    if self.upper_bound_type is not None:
      return self._fqn

    name_with_underscores = self.name.replace('$', '_')
    return CPP_TYPE_BY_JAVA_TYPE.get(self.full_name_with_slashes,
                                     f'J{name_with_underscores}')

  @property
  def mirror_namespace(self):
    return self.class_without_prefix.package_with_colons

  @property
  def package_with_slashes(self):
    return self._fqn.rsplit('/', 1)[0]

  @property
  def package_with_dots(self):
    return self.package_with_slashes.replace('/', '.')

  @property
  def package_with_colons(self):
    return self.package_with_slashes.replace('/', '::')

  @property
  def package_with_underscores(self):
    return self.package_with_slashes.replace('/', '_')

  @property
  def full_name_with_slashes(self):
    return self._fqn

  @property
  def full_name_with_dots(self):
    return self._fqn.replace('/', '.').replace('$', '.')

  @property
  def prefix_with_dots(self):
    return self._prefix.replace('/', '.') if self._prefix else self._prefix

  @property
  def class_without_prefix(self):
    if not self._prefix or self.upper_bound_type is not None:
      return self
    return JavaClass(self._fqn[len(self._prefix) + 1:])

  @property
  def outer_class_name(self):
    return self.name.split('$', 1)[0]

  def is_generic_type(self):
    return self.upper_bound_type is not None

  def is_nested(self):
    return '$' in self.name

  def get_outer_class(self):
    return JavaClass(f'{self.package_with_slashes}/{self.outer_class_name}',
                     self._prefix)

  def is_prefixed(self):
    assert self.upper_bound_type is None
    return bool(self._prefix)

  def is_system_class(self):
    return self._fqn.startswith(('android/', 'java/'))

  def to_java(self, type_resolver=None):
    if self.upper_bound_type is not None:
      return self._fqn
    # Empty resolver used to shorten java.lang classes.
    type_resolver = type_resolver or _EMPTY_TYPE_RESOLVER
    return type_resolver.contextualize(self)

  def to_cpp(self):
    return common.jni_mangle(self.full_name_with_slashes)

  def enable_mirror(self):
    return self.full_name_with_slashes not in CPP_TYPE_BY_JAVA_TYPE

  def to_mirror_cpp(self):
    if self.upper_bound_type is not None:
      return self._fqn
    if not self.enable_mirror():
      return self.jobject_name
    return f'::{self.mirror_namespace}::{self.jobject_name}'

  def as_type(self):
    assert self.upper_bound_type is None
    return JavaType(java_class=self)

  def as_generic_type(self):
    assert self.upper_bound_type is not None
    return JavaType(java_class=self)

  def make_prefixed(self, prefix):
    assert self.upper_bound_type is None
    if not prefix:
      return self
    prefix = prefix.replace('.', '/')
    return JavaClass(f'{prefix}/{self._fqn}', prefix)

  def make_nested(self, name):
    assert self.upper_bound_type is None
    return JavaClass(f'{self._fqn}${name}', self._prefix)


@dataclasses.dataclass(frozen=True)
class JavaType:
  """Represents a parameter or return type."""
  java_class: Optional[JavaClass] = None
  primitive_name: Optional[str] = None
  array_dimensions: int = 0
  converted_type: Optional[str] = dataclasses.field(default=None, compare=False)
  nullable: bool = dataclasses.field(default=True, compare=False)
  generics: Optional[Tuple['JavaType', ...]] = None

  def __post_init__(self):
    assert (self.java_class is None) != (self.primitive_name is None), self
    assert not (self.is_primitive() and self.nullable), self

  def with_generics(self, generics):
    return dataclasses.replace(self, generics=generics)

  @property
  def class_without_prefix(self):
    return self.java_class.class_without_prefix if self.java_class else None

  @property
  def non_array_full_name_with_slashes(self):
    return self.primitive_name or self.java_class.full_name_with_slashes

  @property
  def num_generics(self):
    return 0 if not self.generics else len(self.generics)

  # Cannot use dataclass(order=True) because some fields are None.
  def __lt__(self, other):
    if self.primitive_name and not other.primitive_name:
      return True
    if other.primitive_name and not self.primitive_name:
      return False
    lhs = (self.array_dimensions, self.non_array_full_name_with_slashes,
           self.generics)
    rhs = (other.array_dimensions, other.non_array_full_name_with_slashes,
           other.generics)
    return lhs < rhs

  def is_primitive(self):
    return self.primitive_name is not None and self.array_dimensions == 0

  def is_array(self):
    return self.array_dimensions > 0

  def is_primitive_array(self):
    return self.primitive_name is not None and self.array_dimensions > 0

  def is_object_array(self):
    return self.array_dimensions > 1 or (self.primitive_name is None
                                         and self.array_dimensions > 0)

  def is_system_class(self):
    return self.java_class is not None and self.java_class.is_system_class()

  def is_generic_type(self):
    return self.java_class and self.java_class.is_generic_type()

  def to_cpp(self):
    """Returns a C datatype for the given java type."""
    if self.is_object_array():
      return 'jobjectArray'
    if self.array_dimensions:
      cpp_type = CPP_TYPE_BY_JAVA_TYPE.get(
          self.non_array_full_name_with_slashes, 'jobject')
      return f'{cpp_type}Array'
    return CPP_UNDERLYING_TYPE_BY_JAVA_TYPE.get(
        self.non_array_full_name_with_slashes, 'jobject')

  def is_collection(self):
    return not self.is_array() and self.java_class in COLLECTION_CLASSES

  def is_void(self):
    return self.primitive_name == 'void'

  def is_string(self):
    return self == STRING

  def to_array_element_type(self):
    assert self.is_array()
    return JavaType(java_class=self.java_class,
                    primitive_name=self.primitive_name,
                    array_dimensions=self.array_dimensions - 1,
                    nullable=bool(self.java_class or self.array_dimensions > 1))

  def to_descriptor(self):
    """Converts a Java type into a JNI signature type."""
    if self.primitive_name:
      name = _DESCRIPTOR_CHAR_BY_PRIMITIVE_TYPE[self.primitive_name]
    else:
      if upper_bound_type := self.java_class.upper_bound_type:
        java_class = upper_bound_type.java_class
      else:
        java_class = self.java_class
      name = f'L{java_class.full_name_with_slashes};'
    return ('[' * self.array_dimensions) + name

  def to_java(self,
              type_resolver=None,
              with_prefix=True,
              with_generics=False,
              with_annotations=False):
    ret = ''
    if with_annotations:
      if self.converted_type:
        ret = f'@JniType("{self.converted_type}") '
      if self.nullable:
        ret += f'@Nullable '
    if self.primitive_name:
      ret += self.primitive_name
    elif self.java_class:
      java_class = self.java_class
      if not with_prefix:
        java_class = java_class.class_without_prefix
      ret += java_class.to_java(type_resolver)

    if with_generics and self.generics:
      ret += '<' + ', '.join(
          g.to_java(type_resolver=type_resolver,
                    with_prefix=with_prefix,
                    with_generics=True) for g in self.generics) + '>'

    if self.array_dimensions > 0:
      ret += '[]' * self.array_dimensions
    return ret

  def to_mirror_cpp(self):
    if not self.enable_mirror():
      return self.to_cpp()
    dim = self.array_dimensions
    if self.java_class:
      ret = self.java_class.to_mirror_cpp()
      if self.generics:
        ret += '<%s>' % ', '.join(g.to_mirror_cpp() for g in self.generics)
    else:
      ret = CPP_UNDERLYING_TYPE_BY_JAVA_TYPE.get(
          self.non_array_full_name_with_slashes, 'jobject')
    return ('JArray<' * dim) + ret + ('>' * dim)

  def to_cpp_default_value(self):
    """Returns a valid C return value for the given java type."""
    if self.is_primitive():
      return _DEFAULT_VALUE_BY_PRIMITIVE_TYPE[self.primitive_name]
    return 'nullptr'

  def to_proxy(self):
    """Converts to types used over JNI boundary."""
    return self if self.is_primitive() else OBJECT

  def enable_mirror(self):
    """Whether to use a jobject subclass e.g. JMyClass."""
    return (((self.java_class and self.java_class.enable_mirror())
             or self.array_dimensions > 0) and not self.converted_type)


@dataclasses.dataclass(frozen=True)
class JavaTypeParam:
  java_class: JavaClass

  @staticmethod
  def make(name, upper_bound_type):
    return JavaTypeParam(JavaClass.for_generics(name, upper_bound_type))

  @property
  def name(self):
    return self.java_class.full_name_with_slashes

  @property
  def upper_bound_type(self):
    return self.java_class.upper_bound_type

  def to_java(self, type_resolver=None):
    ret = self.java_class.to_java(type_resolver=type_resolver)
    if self.upper_bound_type and self.upper_bound_type != OBJECT:
      ret += ' extends ' + self.upper_bound_type.to_java(
          type_resolver=type_resolver, with_generics=True)
    return ret


class JavaTypeParamList(tuple):

  def to_java(self, type_resolver=None):
    if not self:
      return ''
    inner = ', '.join(t.to_java(type_resolver=type_resolver) for t in self)
    return f'<{inner}>'

  def get_types(self):
    return tuple(JavaType(java_class=p.java_class) for p in self)


@dataclasses.dataclass(frozen=True)
class JavaParam:
  """Represents a parameter."""
  java_type: JavaType
  name: str

  def to_proxy(self):
    """Converts to types used over JNI boundary."""
    return JavaParam(self.java_type.to_proxy(), self.name)

  def cpp_name(self):
    if self.name in ('env', 'jcaller'):
      return f'_{self.name}'
    return self.name

  def to_java_declaration(self, type_resolver=None):
    return '%s %s' % (self.java_type.to_java(type_resolver), self.name)


class JavaParamList(tuple):
  """Represents a parameter list."""

  def get_types(self):
    return tuple(p.java_type for p in self)

  def to_proxy(self):
    """Converts to types used over JNI boundary."""
    return JavaParamList(p.to_proxy() for p in self)

  def to_java_declaration(self, type_resolver=None):
    return ', '.join(
        p.to_java_declaration(type_resolver=type_resolver) for p in self)


@dataclasses.dataclass(frozen=True, order=True)
class JavaSignature:
  """Represents a method signature (return type + parameter types)."""
  return_type: JavaType
  param_types: Tuple[JavaType]
  # Signatures should be considered equal if parameter names differ, so exclude
  # param_list from comparisons.
  param_list: JavaParamList = dataclasses.field(compare=False)

  @staticmethod
  def from_params(return_type, param_list):
    return JavaSignature(return_type=return_type,
                         param_types=param_list.get_types(),
                         param_list=param_list)

  def iter_types(self):
    if not self.return_type.is_void():
      yield self.return_type
    yield from (p.java_type for p in self.param_list)

  def to_descriptor(self):
    """Returns the JNI signature."""
    sb = ['(']
    sb += [t.to_descriptor() for t in self.param_types]
    sb += [')']
    sb += [self.return_type.to_descriptor()]
    return ''.join(sb)

  def to_proxy(self):
    """Converts to types used over JNI boundary."""
    return_type = self.return_type.to_proxy()
    param_list = self.param_list.to_proxy()
    return JavaSignature.from_params(return_type, param_list)


class TypeResolver:
  """Converts type names to fully qualified names."""

  def __init__(self,
               java_class,
               type_params=None,
               parent_resolver=None,
               null_marked=False,
               package_prefix=None,
               package_prefix_filter=None):
    self.java_class = java_class
    self.type_params = type_params or EMPTY_TYPE_PARAM_LIST
    self.parent_resolver = parent_resolver
    self.null_marked = null_marked
    self.imports = []
    self.nested_classes = []
    self.package_prefix = package_prefix
    self.package_prefix_filter = package_prefix_filter
    self._cache = {}

    assert self.java_class == self._maybe_prefix(
        self.java_class.class_without_prefix)

  def __lt__(self, other):
    return self.java_class < other.java_class

  def __hash__(self):
    return hash(self.java_class)

  @property
  def num_generics(self):
    return len(self.type_params)

  def make_method_resolver(self, *, type_params):
    return TypeResolver(self.java_class,
                        type_params=type_params,
                        null_marked=self.null_marked,
                        package_prefix=self.package_prefix,
                        package_prefix_filter=self.package_prefix_filter,
                        parent_resolver=self)

  def _maybe_prefix(self, java_class):
    if (not java_class.is_prefixed()
        and self.package_prefix and common.should_prefix_package(
            java_class.package_with_dots, self.package_prefix_filter)):
      java_class = java_class.make_prefixed(self.package_prefix)
    return java_class

  def add_import(self, java_class):
    self.imports.append(self._maybe_prefix(java_class))

  def add_child(self, *, java_class):
    java_class = self._maybe_prefix(java_class)
    assert java_class not in self.nested_classes
    self.nested_classes.append(java_class)
    return TypeResolver(java_class,
                        parent_resolver=self,
                        null_marked=self.null_marked,
                        package_prefix=self.package_prefix,
                        package_prefix_filter=self.package_prefix_filter)

  def contextualize(self, java_class):
    """Return the shortest string that resolves to the given class."""
    type_package = java_class.package_with_slashes
    if type_package in ('java/lang', self.java_class.package_with_slashes):
      return java_class.name_with_dots
    if java_class in self.imports:
      return java_class.name_with_dots

    return java_class.full_name_with_dots

  def resolve(self, name):
    """Resolves the given string to a JavaClass.

    Does not support |name| having generics, but returns a JavaType since
    generic type args might imply generics.
    E.g.: class Foo<T extends List<String>>
    """
    if ret := self._cache.get(name):
      return ret
    ret = self._resolve_internal(name)
    self._cache[name] = ret
    return ret

  def _resolve_internal(self, name):
    assert name not in PRIMITIVES, 'Name: ' + name
    assert name != '', 'Cannot resolve empty string'
    assert ' ' not in name, 'Name: ' + name
    assert '<' not in name, 'Name: ' + name

    # Check if it's already fully qualified.
    if '.' in name and name[0].islower():
      return JavaClass(name.replace('.', '/'))

    for p in self.type_params:
      if name == p.name:
        return p.java_class

    if self.java_class.name == name:
      return self.java_class

    for clazz in self.nested_classes:
      if name in (clazz.name, clazz.nested_name):
        return clazz

    # Is it from an import? (e.g. referencing Class from import pkg.Class).
    for clazz in self.imports:
      if name in (clazz.name, clazz.nested_name):
        return clazz

    # Is it an inner class from an outer class import? (e.g. referencing
    # Class.Inner from import pkg.Class).
    if '.' in name:
      # Assume lowercase means it's a fully qualifited name.
      if name[0].islower():
        return JavaClass(name.replace('.', '/'))
      # Otherwise, try and find the outer class in imports.
      components = name.split('.')
      outer = '/'.join(components[:-1])
      inner = components[-1]
      for clazz in self.imports:
        if clazz.name == outer:
          return clazz.make_nested(inner)
      name = name.replace('.', '$')

    if self.parent_resolver:
      return self.parent_resolver.resolve(name)

    # java.lang classes always take priority over types from the same package.
    # To use a type from the same package that has the same name as a java.lang
    # type, it must be explicitly imported.
    if java_lang_classes.contains(name):
      return JavaClass(f'java/lang/{name}')

    # Type not found, falling back to same package as this class.
    # Set the same prefix with this class.
    ret = JavaClass(
        f'{self.java_class.class_without_prefix.package_with_slashes}/{name}')
    return ret.make_prefixed(self.java_class.prefix_with_dots)


CLASS_CLASS = JavaClass('java/lang/Class')
OBJECT_CLASS = JavaClass('java/lang/Object')
STRING_CLASS = JavaClass('java/lang/String')
LIST_CLASS = JavaClass('java/util/List')

# Collection and types that extend it (for use with toArray()).
# More can be added here if the need arises.
COLLECTION_CLASSES = (
    LIST_CLASS,
    JavaClass('java/util/Collection'),
    JavaClass('java/util/Set'),
)

OBJECT = OBJECT_CLASS.as_type()
STRING = STRING_CLASS.as_type()
INT = JavaType(primitive_name='int', nullable=False)
DOUBLE = JavaType(primitive_name='double', nullable=False)
FLOAT = JavaType(primitive_name='float', nullable=False)
LONG = JavaType(primitive_name='long', nullable=False)
VOID = JavaType(primitive_name='void', nullable=False)

EMPTY_PARAM_LIST = JavaParamList()
EMPTY_TYPE_PARAM_LIST = JavaTypeParamList()
_EMPTY_TYPE_RESOLVER = TypeResolver(OBJECT_CLASS)
