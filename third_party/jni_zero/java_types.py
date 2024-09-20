# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dataclasses
from typing import Dict
from typing import Optional
from typing import Tuple

import common
import java_lang_classes

_CPP_TYPE_BY_JAVA_TYPE = {
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
  # This is only meaningful if make_prefix have been called on the original class.
  _prefix: str = None

  def __post_init__(self):
    assert '.' not in self._fqn, f'{self._fqn} should have / and $, but not .'

  def __str__(self):
    return self.full_name_with_slashes

  @property
  def name(self):
    return self._fqn.rsplit('/', 1)[-1]

  @property
  def name_with_dots(self):
    return self.name.replace('$', '.')

  @property
  def nested_name(self):
    return self.name.rsplit('$', 1)[-1]

  @property
  def package_with_slashes(self):
    return self._fqn.rsplit('/', 1)[0]

  @property
  def package_with_dots(self):
    return self.package_with_slashes.replace('/', '.')

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
    if not self._prefix:
      return self
    return JavaClass(self._fqn[len(self._prefix) + 1:])

  @property
  def outer_class_name(self):
    return self.name.split('$', 1)[0]

  def is_nested(self):
    return '$' in self.name

  def get_outer_class(self):
    return JavaClass(f'{self.package_with_slashes}/{self.outer_class_name}')

  def is_system_class(self):
    return self._fqn.startswith(('android/', 'java/'))

  def to_java(self, type_resolver=None):
    # Empty resolver used to shorted java.lang classes.
    type_resolver = type_resolver or _EMPTY_TYPE_RESOLVER
    return type_resolver.contextualize(self)

  def to_cpp(self):
    return common.escape_class_name(self.full_name_with_slashes)

  def as_type(self):
    return JavaType(java_class=self)

  def make_prefixed(self, prefix):
    if not prefix:
      return self
    prefix = prefix.replace('.', '/')
    return JavaClass(f'{prefix}/{self._fqn}', prefix)

  def make_nested(self, name):
    return JavaClass(f'{self._fqn}${name}')


@dataclasses.dataclass(frozen=True)
class JavaType:
  """Represents a parameter or return type."""
  array_dimensions: int = 0
  primitive_name: Optional[str] = None
  java_class: Optional[JavaClass] = None
  converted_type: Optional[str] = dataclasses.field(default=None, compare=False)
  nullable: bool = True

  @staticmethod
  def from_descriptor(descriptor):
    # E.g.: [Ljava/lang/Class;
    without_arrays = descriptor.lstrip('[')
    array_dimensions = len(descriptor) - len(without_arrays)
    descriptor = without_arrays

    if descriptor[0] == 'L':
      assert descriptor[-1] == ';', 'invalid descriptor: ' + descriptor
      return JavaType(array_dimensions=array_dimensions,
                      java_class=JavaClass(descriptor[1:-1]))
    primitive_name = _PRIMITIVE_TYPE_BY_DESCRIPTOR_CHAR[descriptor[0]]
    return JavaType(array_dimensions=array_dimensions,
                    primitive_name=primitive_name)

  @property
  def non_array_full_name_with_slashes(self):
    return self.primitive_name or self.java_class.full_name_with_slashes

  # Cannot use dataclass(order=True) because some fields are None.
  def __lt__(self, other):
    if self.primitive_name and not other.primitive_name:
      return True
    if other.primitive_name and not self.primitive_name:
      return False
    lhs = (self.array_dimensions, self.primitive_name or self.java_class)
    rhs = (other.array_dimensions, other.primitive_name or other.java_class)
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

  def is_collection(self):
    return not self.is_array() and self.java_class in COLLECTION_CLASSES

  def is_void(self):
    return self.primitive_name == 'void'

  def to_array_element_type(self):
    assert self.is_array()
    return JavaType(array_dimensions=self.array_dimensions - 1,
                    primitive_name=self.primitive_name,
                    java_class=self.java_class)

  def to_descriptor(self):
    """Converts a Java type into a JNI signature type."""
    if self.primitive_name:
      name = _DESCRIPTOR_CHAR_BY_PRIMITIVE_TYPE[self.primitive_name]
    else:
      name = f'L{self.java_class.full_name_with_slashes};'
    return ('[' * self.array_dimensions) + name

  def to_java(self, type_resolver=None):
    if self.primitive_name:
      ret = self.primitive_name
    else:
      ret = self.java_class.to_java(type_resolver)
    return ret + '[]' * self.array_dimensions

  def to_cpp(self):
    """Returns a C datatype for the given java type."""
    if self.array_dimensions > 1:
      return 'jobjectArray'
    if self.array_dimensions > 0 and self.primitive_name is None:
      # There is no jstringArray.
      return 'jobjectArray'

    cpp_type = _CPP_TYPE_BY_JAVA_TYPE.get(self.non_array_full_name_with_slashes,
                                          'jobject')
    if self.array_dimensions:
      cpp_type = f'{cpp_type}Array'
    return cpp_type

  def to_cpp_default_value(self):
    """Returns a valid C return value for the given java type."""
    if self.is_primitive():
      return _DEFAULT_VALUE_BY_PRIMITIVE_TYPE[self.primitive_name]
    return 'nullptr'

  def to_proxy(self):
    """Converts to types used over JNI boundary."""
    return self if self.is_primitive() else OBJECT


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
  def to_proxy(self):
    """Converts to types used over JNI boundary."""
    return JavaParamList(p.to_proxy() for p in self)

  def to_java_declaration(self, type_resolver=None):
    return ', '.join(
        p.to_java_declaration(type_resolver=type_resolver) for p in self)

  def to_call_str(self):
    return ', '.join(p.name for p in self)


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
                         param_types=tuple(p.java_type for p in param_list),
                         param_list=param_list)

  @staticmethod
  def from_descriptor(descriptor):
    # E.g.: (Ljava/lang/Object;Ljava/lang/Runnable;)Ljava/lang/Class;
    assert descriptor[0] == '('
    i = 1
    start_idx = i
    params = []
    while True:
      char = descriptor[i]
      if char == ')':
        break
      elif char == '[':
        i += 1
        continue
      elif char == 'L':
        end_idx = descriptor.index(';', i) + 1
      else:
        end_idx = i + 1
      param_type = JavaType.from_descriptor(descriptor[start_idx:end_idx])
      params.append(JavaParam(param_type, f'p{len(params)}'))
      i = end_idx
      start_idx = end_idx

    return_type = JavaType.from_descriptor(descriptor[i + 1:])
    return JavaSignature.from_params(return_type, JavaParamList(params))

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

  def with_params_reordered(self):
    return JavaSignature.from_params(
        self.return_type,
        JavaParamList(
            tuple(sorted(self.param_list,
                         key=lambda x: x.java_type.to_proxy()))))


class TypeResolver:
  """Converts type names to fully qualified names."""
  def __init__(self, java_class):
    self.java_class = java_class
    self.imports = []
    self.nested_classes = []

  def add_import(self, java_class):
    self.imports.append(java_class)

  def add_nested_class(self, java_class):
    self.nested_classes.append(java_class)

  def contextualize(self, java_class):
    """Return the shortest string that resolves to the given class."""
    type_package = java_class.package_with_slashes
    if type_package in ('java/lang', self.java_class.package_with_slashes):
      return java_class.name_with_dots
    if java_class in self.imports:
      return java_class.name_with_dots

    return java_class.full_name_with_dots

  def resolve(self, name):
    """Return a JavaClass for the given type name."""
    assert name not in PRIMITIVES, 'Name: ' + name
    assert ' ' not in name, 'Name: ' + name
    assert name != '', 'Cannot resolve empty string'

    if '/' in name:
      # Coming from javap, use the fully qualified name directly.
      return JavaClass(name)

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
_LIST_CLASS = JavaClass('java/util/List')

# Collection and types that extend it (for use with toArray()).
# More can be added here if the need arises.
COLLECTION_CLASSES = (
    _LIST_CLASS,
    JavaClass('java/util/Collection'),
    JavaClass('java/util/Set'),
)

OBJECT = JavaType(java_class=OBJECT_CLASS)
CLASS = JavaType(java_class=CLASS_CLASS)
LIST = JavaType(java_class=_LIST_CLASS)
INT = JavaType(primitive_name='int')
VOID = JavaType(primitive_name='void')

_EMPTY_TYPE_RESOLVER = TypeResolver(OBJECT_CLASS)
EMPTY_PARAM_LIST = JavaParamList()
