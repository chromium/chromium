# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Parsing helpers that do not depend on the source language's syntax."""

import dataclasses
import re
from typing import List
from typing import Optional

import common
import java_types


class ParseError(Exception):
  suffix = ''

  def __str__(self):
    return super().__str__() + self.suffix


@dataclasses.dataclass(order=True)  # Field order matters.
class ParsedNative:
  static: bool
  name: str
  signature: java_types.JavaSignature
  native_class_name: str = None


@dataclasses.dataclass(order=True)  # Field order matters.
class ParsedCalledByNative:
  static: bool
  name: str
  signature: java_types.JavaSignature
  type_params: java_types.JavaTypeParamList
  unchecked: bool = False


@dataclasses.dataclass(order=True)  # Field order matters.
class ParsedField:
  static: bool
  final: bool
  name: str
  java_type: java_types.JavaType
  const_value: Optional[str] = None


@dataclasses.dataclass(order=True)  # Field order matters.
class ParsedClass:
  type_resolver: java_types.TypeResolver
  _start_idx: int
  _end_idx: int
  jni_type: Optional[str] = None
  called_by_natives: List[ParsedCalledByNative] = (dataclasses.field(
      default_factory=list))
  fields: List[ParsedField] = dataclasses.field(default_factory=list)
  non_proxy_methods: List[ParsedNative] = dataclasses.field(
      default_factory=list)


@dataclasses.dataclass
class ParsedFile:
  filename: str
  outer_class: ParsedClass
  classes_with_jni: List[ParsedClass]  # ParsedCalledByNative or CalledByNative
  proxy_methods: List[ParsedNative]
  type_tokens: dict = dataclasses.field(default_factory=dict)
  proxy_interface: Optional[java_types.JavaClass] = None
  proxy_visibility: Optional[str] = None
  jni_namespace: Optional[str] = None  # E.g. @JNINamespace("content")


@dataclasses.dataclass
class _ParsedProxyNatives:
  interface_name: str
  visibility: str
  methods: List[ParsedNative]


# Match single line comments, multiline comments, character literals,
# triple-quoted strings, and double-quoted strings.
_COMMENT_REMOVER_REGEX = re.compile(
    r'//.*?$|/\*.*?\*/[ \t]*|\'(?:\\.|[^\\\'])*\'|"""[\s\S]*?"""'
    r'|"(?:\\.|[^\\"])*"', re.DOTALL | re.MULTILINE)


def remove_comments(contents):
  # We need to support both inline and block comments, and we need to handle
  # strings that contain '//' or '/*'.
  def replacer(match):
    # Replace matches that are comments with nothing; return literals/strings
    # unchanged.
    s = match.group(0)
    if s.startswith('/'):
      return ''
    else:
      return s

  return _COMMENT_REMOVER_REGEX.sub(replacer, contents)


last_match = []


def find_iter_with_note(regex, data, **kwargs):
  for match in regex.finditer(data, **kwargs):
    last_match.append(match.group())
    yield match
    last_match.pop()


# Kotlin does not require a semicolon.
_PACKAGE_REGEX = re.compile(r'^package\s+([\w.]+)', flags=re.MULTILINE)


def parse_package(contents, require=True):
  match = _PACKAGE_REGEX.search(contents)
  if not match:
    if require:
      raise ParseError('Unable to find "package" line')
    return ''
  return match.group(1).replace('.', '/')


_INDENT_REGEX = re.compile(r'\s*')
_SAME_LINE_CLOSING_BRACE_REGEX = re.compile(r'\s*\}')


def find_class_end(contents, decl_start_idx, class_name, open_brace_idx):
  # Find the indent of the class line
  line_start_idx = contents.rfind('\n', 0, decl_start_idx) + 1
  indent = _INDENT_REGEX.match(contents, line_start_idx).group(0)

  # Check for empty class {}
  if m := _SAME_LINE_CLOSING_BRACE_REGEX.match(contents, open_brace_idx):
    return m.end()

  # Find closing brace.
  close_brace_str = f'\n{indent}}}'
  end_idx = contents.find(close_brace_str, open_brace_idx)
  if end_idx != -1:
    return end_idx + len(close_brace_str)

  raise ParseError(f'Could not find end of class {class_name}. '
                   'Ensure indentation of ending brace is correct.')


def find_owning_class(parsed_classes, match):
  ret = None
  index = match.start()
  for c in parsed_classes:
    if c._start_idx <= index <= c._end_idx:
      ret = c
  if not ret:
    raise ParseError(f'Could not determine enclosing class for: {match}\n'
                     f' Classes: {parsed_classes}')
  return ret


def find_balanced_generics(contents, start_idx):
  """Returns the balanced <...> that starts at |start_idx|, or None."""
  pos = start_idx
  while True:
    pos = contents.find('>', pos + 1)
    if pos == -1:
      return None
    if contents.count('<', start_idx,
                      pos + 1) == contents.count('>', start_idx, pos + 1):
      return contents[start_idx:pos + 1]


# Complicated example:
# @JniType("std::optional<void(*)(const std::vector<bool>&)>") Callback<Boolean> funcType
# Eager search for quotes to skip over )s within quotes.
_ANNOTATION_REGEX = re.compile(
    r'@(?P<name>[\w.]+)(?:\((?:"(?P<arg>.*?)")?[^)]*\))?\s*')


def parse_annotations(value):
  """Returns a dict of annotations and the value with them removed."""
  if '@' not in value:
    return {}, value
  annotations = {}
  # Must ignore: List<@JniType("std::vector<std::vector<int>>") String>
  # Must not ignore: "OuterClass.@Nullable InnerClass"
  # Must not ignore: @Contract("_, !null -> !null")
  sb = []
  cursor_idx = 0
  for m in find_iter_with_note(_ANNOTATION_REGEX, value):
    # Check for being within generics.
    start_idx = m.start()
    # Hack to account for -> in @Contract()
    num_open = value.count('<', cursor_idx, start_idx)
    num_closed = (value.count('>', cursor_idx, start_idx) -
                  value.count('->', cursor_idx, start_idx))
    if num_open != num_closed:
      continue
    sb.append(value[cursor_idx:start_idx])
    annotations[m.group('name')] = m.group('arg') or ''
    cursor_idx = m.end()
  sb.append(value[cursor_idx:])

  return annotations, ''.join(sb)


def split_by_delimiter(value, delimiter):
  """Splits by delimiter, but ignores delimiters inside < >."""
  if not value:
    return []
  if '<' not in value:
    return [x.strip() for x in value.split(delimiter)]

  ret = []
  cursor_idx = 0
  start_idx = 0
  while True:
    start_idx = value.find(delimiter, start_idx)
    if start_idx == -1:
      break
    num_open = value.count('<', cursor_idx, start_idx)
    num_closed = (value.count('>', cursor_idx, start_idx) -
                  value.count('->', cursor_idx, start_idx))
    if num_open == num_closed:
      ret.append(value[cursor_idx:start_idx].strip())
      cursor_idx = start_idx + len(delimiter)
    start_idx += len(delimiter)

  ret.append(value[cursor_idx:].strip())
  return ret


def _validate_safe_pointer(type_resolver, value, parsed_value, java_class,
                           generics, array_dimensions):
  if array_dimensions > 0:
    raise ParseError(
        f'Arrays of safe pointers ({parsed_value}) are not supported: '
        f'{value}')
  if not type_resolver.enable_safe_pointers:
    raise ParseError(f'Safe JNI pointers are not enabled. Did you forget '
                     f'--enable-safe-pointers for {value}?')
  if not generics or len(generics) != 1:
    raise ParseError(
        f'Safe pointer type "{parsed_value}" must have exactly one generic '
        f'type parameter: {value}')
  inner = generics[0]
  if inner.primitive_name or inner.array_dimensions:
    raise ParseError(
        f'Safe pointer inner type must be a JniTypeToken interface, not '
        f'"{inner.non_array_full_name_with_slashes}": {value}')


def _resolve_token(type_resolver, java_type):
  """Fills in a JniTypeToken's C++ type from the type catalog."""
  if java_type.converted_type or not java_type.java_class:
    return java_type
  fqn = java_type.java_class.class_without_prefix.full_name_with_slashes
  converted_type = type_resolver.type_catalog.get(fqn)
  if not converted_type:
    return java_type
  return dataclasses.replace(java_type, converted_type=converted_type)


def make_java_type(type_resolver, *, value, parsed_value, java_class,
                   primitive_name, array_dimensions, generics, annotations,
                   nullable):
  """Validates parsed type components and creates a JavaType."""
  if java_class is not None:
    if generics and not java_class.is_safe_pointer():
      if any(t.converted_type for t in generics):
        raise ParseError('@JniType not allowed within generics: ' + value)

    if java_class.is_safe_pointer():
      if generics and len(generics) == 1:
        generics = (_resolve_token(type_resolver, generics[0]), )
      _validate_safe_pointer(type_resolver, value, parsed_value, java_class,
                             generics, array_dimensions)
    elif java_class == java_types.CLASS_CLASS:
      generics = None

  converted_type = annotations.get('JniType', None)
  if converted_type == 'std::vector':
    # Allow "std::vector" as shorthand for types that can be inferred:
    if array_dimensions == 1 and primitive_name:
      # e.g.: std::vector<int32_t>
      inner = java_types.CPP_UNDERLYING_TYPE_BY_JAVA_TYPE.get(primitive_name)
      converted_type += f'<{inner}>'
    elif array_dimensions > 0 or java_class in java_types.COLLECTION_CLASSES:
      # std::vector<jni_zero::ScopedJavaLocalRef<jobject>>
      converted_type += '<jni_zero::ScopedJavaLocalRef<jobject>>'
    else:
      raise ParseError('Found non-templatized @JniType("std::vector") on '
                       f'non-array, non-Collection type: {java_class} '
                       f'(when parsing {value})')

  if primitive_name and array_dimensions == 0:
    nullable = False

  return java_types.JavaType(java_class=java_class,
                             primitive_name=primitive_name,
                             array_dimensions=array_dimensions,
                             converted_type=converted_type,
                             nullable=nullable,
                             generics=generics)


_NATIVE_METHODS_INTERFACE_REGEX = re.compile(
    r'@NativeMethods[\S\s]+?'
    r'(?P<visibility>public)?\s*\binterface\s*'
    r'(?P<interface_name>\w*)\s*{(?P<interface_body>(\s*.*)+?\s*)}')


def parse_proxy_natives(type_resolver, contents, *, iter_methods):
  """Parses the @NativeMethods interface in |contents|.

  |iter_methods| is a language-specific callable that takes
  (type_resolver, interface_body) and yields a tuple of
  (name, annotations, params_part, params, return_type) for each method.
  """
  matches = list(_NATIVE_METHODS_INTERFACE_REGEX.finditer(contents))
  if not matches:
    return None
  if len(matches) > 1:
    raise ParseError(
        'Multiple @NativeMethod interfaces in one class is not supported.')

  match = matches[0]
  ret = _ParsedProxyNatives(interface_name=match.group('interface_name'),
                            visibility=match.group('visibility'),
                            methods=[])
  interface_body = match.group('interface_body')

  for name, annotations, params_part, params, return_type in iter_methods(
      type_resolver, interface_body):
    if return_type.java_class == java_types.JNI_PTR_CLASS:
      raise ParseError(
          f'Method "{name}" returns JniPtr, but a short-borrow pointer is '
          f'auto-invalidated after the call and cannot be returned. Use '
          f'JniUniquePtr or JniRawPtr to hand ownership to Java.')
    for p in params:
      if (p.java_type.is_safe_pointer()
          and p.java_type.java_class != java_types.JNI_PTR_CLASS):
        raise ParseError(
            f'Method "{name}" parameter "{p.name}" has type '
            f'{p.java_type.java_class.name}, but @NativeMethods parameters '
            f'must use JniPtr<T>. (JniUniquePtr and JniRawPtr implement '
            f'JniPtr and can be passed as arguments).')
    native_class_name = annotations.get('NativeClassQualifiedName')
    if params:
      first_param = params[0]
      is_long_member = (first_param.java_type.is_primitive()
                        and first_param.java_type.primitive_name == 'long'
                        and first_param.name.startswith('native'))
      is_safe_ptr_member = (first_param.java_type.is_safe_pointer()
                            and first_param.name == 'self')
      if (first_param.java_type.is_safe_pointer()
          and first_param.name.startswith('native')):
        raise ParseError(
            f'Method "{name}" safe pointer first parameter '
            f'"{first_param.name}" starts with "native". Use "self" to '
            f'dispatch to a C++ member function, or another name for a free '
            f'function.')
      if is_long_member or is_safe_ptr_member:
        first_param_annotations, _ = parse_annotations(
            split_by_delimiter(params_part, ',')[0])
        # Kotlin marks nullable types with "?" rather than with @Nullable.
        if 'Nullable' in first_param_annotations or (
            type_resolver.null_marked and first_param.java_type.nullable):
          raise ParseError(
              f'Method "{name}" first parameter "{first_param.name}" dispatches '
              f'to a C++ member function and cannot be @Nullable.')
        if is_safe_ptr_member and native_class_name:
          raise ParseError(
              f'Method "{name}" specifies both @NativeClassQualifiedName and a '
              f'safe pointer first parameter "self". The C++ '
              f'class is already defined by @JniType on '
              f'{first_param.java_type.generics[0].java_class.name}.')
    signature = java_types.JavaSignature.from_params(return_type, params)
    ret.methods.append(
        ParsedNative(static=False,
                     name=name,
                     signature=signature,
                     native_class_name=native_class_name))
  if not ret.methods:
    raise ParseError('Found no methods within @NativeMethod interface.')
  ret.methods.sort()
  return ret


def check_called_by_native_return_type(name, return_type):
  if (return_type.is_safe_pointer()
      and return_type.java_class != java_types.JNI_PTR_CLASS):
    raise ParseError(
        f'Method "{name}" has return type {return_type.java_class.name}, '
        f'but @CalledByNative return types must use JniPtr<T>. '
        f'(JniUniquePtr and JniRawPtr implement JniPtr and can be returned).')


# Kotlin does not require a semicolon.
_IMPORT_REGEX = re.compile(r'^import\s+([\w.$]+)[ \t]*(?:;|$)',
                           flags=re.MULTILINE)
_IMPORT_CLASS_NAME_REGEX = re.compile(r'^(.*?)\.([A-Z].*)')


def parse_imports(contents, endpos):
  # Regex skips static imports as well as wildcard imports.
  names = _IMPORT_REGEX.findall(contents, endpos=endpos)
  for name in names:
    if m := _IMPORT_CLASS_NAME_REGEX.match(name):
      package, class_name = m.groups()
      yield java_types.JavaClass(
          package.replace('.', '/') + '/' + class_name.replace('.', '$'))


_JNI_NAMESPACE_REGEX = re.compile(r'@JNINamespace\("(.*?)"\)')


def parse_jni_namespace(contents):
  m = _JNI_NAMESPACE_REGEX.findall(contents)
  if not m:
    return ''
  if len(m) > 1:
    raise ParseError('Found multiple @JNINamespace annotations.')
  return m[0]


def sort_jni(parsed_classes):
  for c in parsed_classes:
    c.called_by_natives.sort()
    c.fields.sort()
    c.non_proxy_methods.sort()


def extract_type_catalog(parsed_classes, type_catalog=None):
  merged_catalog = {}
  if type_catalog:
    merged_catalog.update(type_catalog)
  type_tokens = {}
  for parsed_class in parsed_classes:
    if parsed_class.jni_type:
      fqn = parsed_class.type_resolver.java_class.class_without_prefix.full_name_with_slashes
      merged_catalog[fqn] = parsed_class.jni_type
      type_tokens[fqn] = parsed_class.jni_type
  for parsed_class in parsed_classes:
    parsed_class.type_resolver.type_catalog = merged_catalog
  return type_tokens


def parse_file(filename, parse_func, **kwargs):
  """Reads |filename| and parses it with |parse_func|.

  |parse_func| is java_parse.parse_java_file or kotlin_parse.parse_kotlin_file.
  """
  try:
    with open(filename) as f:
      contents = f.read()
    return parse_func(filename, contents, **kwargs)
  except Exception as e:
    if last_match:
      common.add_note(e, f'in match {last_match}')
    common.add_note(e, f'when parsing {filename}')
    raise
