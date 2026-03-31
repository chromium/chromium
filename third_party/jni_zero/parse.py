# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import dataclasses
import os
import re
from typing import List
from typing import Optional

import common
import java_types

_MODIFIER_KEYWORDS = (r'(?:(?:' + '|'.join([
    'abstract',
    'default',
    'final',
    'native',
    'private',
    'protected',
    'public',
    'static',
    'synchronized',
]) + r')\s+)*')


class ParseError(Exception):
  suffix = ''

  def __str__(self):
    return super().__str__() + self.suffix


@dataclasses.dataclass(order=True)
class ParsedNative:
  name: str
  signature: java_types.JavaSignature
  native_class_name: str
  static: bool = False


@dataclasses.dataclass(order=True)
class ParsedCalledByNative:
  java_class: java_types.JavaClass
  name: str
  signature: java_types.JavaSignature
  static: bool
  unchecked: bool = False


@dataclasses.dataclass(order=True)
class ParsedField:
  java_class: java_types.JavaClass
  name: str
  java_type: java_types.JavaType
  static: bool
  final: bool
  const_value: Optional[str] = None


@dataclasses.dataclass
class _ParsedClass:
  java_class: java_types.JavaClass
  start_idx: int
  end_idx: int
  type_resolver: java_types.TypeResolver


@dataclasses.dataclass
class ParsedFile:
  filename: str
  type_resolver: java_types.TypeResolver
  proxy_methods: List[ParsedNative]
  non_proxy_methods: List[ParsedNative]
  called_by_natives: List[ParsedCalledByNative]
  fields: List[ParsedField]
  proxy_interface: Optional[java_types.JavaClass] = None
  proxy_visibility: Optional[str] = None
  module_name: Optional[str] = None  # E.g. @NativeMethods("module_name")
  jni_namespace: Optional[str] = None  # E.g. @JNINamespace("content")


@dataclasses.dataclass
class _ParsedProxyNatives:
  interface_name: str
  visibility: str
  module_name: str
  methods: List[ParsedNative]


# Match single line comments, multiline comments, character literals, and
# double-quoted strings.
_COMMENT_REMOVER_REGEX = re.compile(
    r'//.*?$|/\*.*?\*/[ \t]*|\'(?:\\.|[^\\\'])*\'|"(?:\\.|[^\\"])*"',
    re.DOTALL | re.MULTILINE)


def _remove_comments(contents):
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


# Remove <...> while maintaining "...".
_GENERICS_REGEX = re.compile(r'("(?:\\.|[^\\"\n])*")|(<[^<>\n]*>)')


def _remove_generics(value):
  """Strips Java generics from a string."""
  while True:
    # Replace "..." with itself, and <...> with " ".
    ret = _GENERICS_REGEX.sub(lambda m: m.group(1) or ' ', value)
    if len(ret) == len(value):
      return ret
    value = ret


_PACKAGE_REGEX = re.compile(r'^package\s+(\S+?);', flags=re.MULTILINE)


def _parse_package(contents):
  match = _PACKAGE_REGEX.search(contents)
  if not match:
    raise ParseError('Unable to find "package" line')
  return match.group(1)


_CLASSES_REGEX = re.compile(
    r'^((?:(?!\b(?:class|interface|enum)\b)'
    r'(?:[^{}"]|"[^"]*"))*?)\b'
    r'(?:class|interface|enum)\b\s+\b([\w.]+)'
    r'\s*[^{]*?\{', re.MULTILINE)
_INDENT_REGEX = re.compile(r'\s*')
_SAME_LINE_CLOSING_BRACE_REGEX = re.compile(r'\s*\}')


def _find_class_end(contents, decl_start_idx, class_name, open_brace_idx):
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


def _find_owning_class(parsed_classes, index):
  ret = None
  for c in parsed_classes:
    if c.start_idx <= index <= c.end_idx:
      ret = c
  if not ret:
    raise ParseError(f'Could not determine enclosing class for index {index}.')
  return ret


# Does not handle doubly-nested classes.
def _parse_java_classes(contents,
                        package_prefix,
                        package_prefix_filter,
                        is_javap=False):
  package = _parse_package(contents).replace('.', '/')
  parsed_classes = []
  for m in _CLASSES_REGEX.finditer(contents):
    preamble, class_name = m.groups()
    # Ignore annotations like @Foo("contains the words class Bar")
    if preamble.count('"') % 2 != 0:
      continue

    if not parsed_classes:
      java_class = java_types.JavaClass(f'{package}/{class_name}')
      null_marked = contents.find('@NullMarked', 0, m.start(2)) != -1
      if package_prefix and common.should_prefix_package(
          java_class.package_with_dots, package_prefix_filter):
        java_class = java_class.make_prefixed(package_prefix)
      end_idx = len(contents)
      type_resolver = java_types.TypeResolver(
          java_class,
          null_marked=null_marked,
          package_prefix=package_prefix,
          package_prefix_filter=package_prefix_filter)
      if not is_javap:
        for c in _parse_imports(contents, m.end()):
          type_resolver.add_import(c)
    else:
      outer_class = parsed_classes[0]
      java_class = outer_class.java_class.make_nested(class_name)
      end_idx = _find_class_end(contents, m.end(1), class_name, m.end(0))
      type_resolver = outer_class.type_resolver.add_child(java_class=java_class)

    class_keyword_start = m.end(1)
    parsed_classes.append(
        _ParsedClass(java_class=java_class,
                     start_idx=class_keyword_start,
                     end_idx=end_idx,
                     type_resolver=type_resolver))

  if not parsed_classes:
    raise ParseError('No classes found.')

  parsed_classes[0].type_resolver.nested_classes = sorted(
      set(parsed_classes[0].type_resolver.nested_classes))
  return parsed_classes

# Complicated example:
# @JniType("std::optional<void(*)(const std::vector<bool>&)>") Callback<Boolean> funcType,
# Eager search for quotes to skip over )s within quotes.
_ANNOTATION_REGEX = re.compile(
    r'@(?P<name>[\w.]+)(?P<args>\((?:\".*?\")*[^)]*\))?\s*')
# Only supports ("foo")
_ANNOTATION_ARGS_REGEX = re.compile(r'\(\s*"(?P<value>[^"]*?)"\s*\)\s*',
                                    flags=re.DOTALL)

def _parse_annotations(value):
  annotations = {}
  for m in _ANNOTATION_REGEX.finditer(value):
    string_value = ''
    if match_args := m.group('args'):
      if match_arg_value := _ANNOTATION_ARGS_REGEX.match(match_args):
        string_value = match_arg_value.group('value')
    annotations[m.group('name')] = string_value

  # Use replace rather than tracking end index to handle:
  # "OuterClass.@Nullable InnerClass"
  return annotations, _ANNOTATION_REGEX.sub('', value)


def _parse_type(type_resolver, value):
  """Parses a string into a JavaType."""
  annotations, parsed_value = _parse_annotations(value)
  array_dimensions = 0
  while parsed_value[-2:] == '[]':
    array_dimensions += 1
    # strip to remove possible spaces between type and [].
    parsed_value = parsed_value[:-2].strip()

  if parsed_value in java_types.PRIMITIVES:
    primitive_name = parsed_value
    java_class = None
  else:
    primitive_name = None
    java_class = type_resolver.resolve(parsed_value)

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
      raise Error('Found non-templatized @JniType("std::vector") on '
                  f'non-array, non-Collection type: {java_class} '
                  f'(when parsing {value}')

  if primitive_name and array_dimensions == 0:
    nullable = False
  elif type_resolver.null_marked:
    nullable = annotations.get('Nullable') is not None
  else:
    nullable = annotations.get('NonNull') is None

  return java_types.JavaType(array_dimensions=array_dimensions,
                             primitive_name=primitive_name,
                             java_class=java_class,
                             converted_type=converted_type,
                             nullable=nullable)


_FINAL_REGEX = re.compile(r'\bfinal\s')


def _parse_param_list(type_resolver, value) -> java_types.JavaParamList:
  if not value or value.isspace():
    return java_types.EMPTY_PARAM_LIST
  params = []
  value = _FINAL_REGEX.sub('', value)
  pending = ''
  for param_str in value.split(','):
    # Combine multiple entries when , is in an annotation.
    # E.g.: @JniType("std::map<std::string, std::string>") Map arg0
    if pending:
      pending += ',' + param_str
      if '"' not in param_str:
        continue
      param_str = pending
      pending = ''
    elif param_str.count('"') == 1:
      pending = param_str
      continue
    param_str = param_str.strip()
    param_str, _, param_name = param_str.rpartition(' ')
    param_str = param_str.rstrip()

    # Handle varargs.
    if param_str.endswith('...'):
      param_str = param_str[:-3] + '[]'

    param_type = _parse_type(type_resolver, param_str)
    params.append(java_types.JavaParam(param_type, param_name))

  return java_types.JavaParamList(params)


_NATIVE_METHODS_INTERFACE_REGEX = re.compile(
    r'@NativeMethods(?:\(\s*"(?P<module_name>\w+)"\s*\))?[\S\s]+?'
    r'(?P<visibility>public)?\s*\binterface\s*'
    r'(?P<interface_name>\w*)\s*{(?P<interface_body>(\s*.*)+?\s*)}')

_PROXY_NATIVE_REGEX = re.compile(r'\s*(.*?)\s+(\w+)\((.*?)\);', flags=re.DOTALL)

_PUBLIC_REGEX = re.compile(r'\bpublic\s')


def _parse_proxy_natives(type_resolver, contents):
  matches = list(_NATIVE_METHODS_INTERFACE_REGEX.finditer(contents))
  if not matches:
    return None
  if len(matches) > 1:
    raise ParseError(
        'Multiple @NativeMethod interfaces in one class is not supported.')

  match = matches[0]
  ret = _ParsedProxyNatives(interface_name=match.group('interface_name'),
                            visibility=match.group('visibility'),
                            module_name=match.group('module_name'),
                            methods=[])
  interface_body = match.group('interface_body')

  for m in _PROXY_NATIVE_REGEX.finditer(interface_body):
    preamble, name, params_part = m.groups()
    preamble = _PUBLIC_REGEX.sub('', preamble)
    annotations, _ = _parse_annotations(preamble)
    params = _parse_param_list(type_resolver, params_part)
    return_type = _parse_type(type_resolver, preamble)
    signature = java_types.JavaSignature.from_params(return_type, params)
    ret.methods.append(
        ParsedNative(
            name=name,
            signature=signature,
            native_class_name=annotations.get('NativeClassQualifiedName')))
  if not ret.methods:
    raise ParseError('Found no methods within @NativeMethod interface.')
  ret.methods.sort()
  return ret


_NON_PROXY_NATIVES_REGEX = re.compile(
    r'(@NativeClassQualifiedName'
    r'\(\"(?P<native_class_name>\S*?)\"\)\s+)?'
    r'(?P<qualifiers>\w+\s\w+|\w+|\s+)\s*native\s+'
    r'(?P<return_type>\S*)\s+'
    r'native(?P<name>\w+)\((?P<params>.*?)\);', re.DOTALL)


def _parse_non_proxy_natives(type_resolver, contents):
  ret = []
  for match in _NON_PROXY_NATIVES_REGEX.finditer(contents):
    name = match.group('name')
    return_type = _parse_type(type_resolver, match.group('return_type'))
    params = _parse_param_list(type_resolver, match.group('params'))
    signature = java_types.JavaSignature.from_params(return_type, params)
    native_class_name = match.group('native_class_name')
    static = 'static' in match.group('qualifiers')
    ret.append(
        ParsedNative(name=name,
                     signature=signature,
                     native_class_name=native_class_name,
                     static=static))
  ret.sort()
  return ret


# javap shows inherited methods from interfaces / super classes, including when
# they are identical but with broader return types. In order to avoid
# collisions, remove these dupes by using the first one listed.
def _filter_duplicate_return_types(called_by_natives):
  cbn_by_key = {}
  for cbn in called_by_natives:
    cbn_by_key.setdefault((cbn.name, cbn.signature.param_types), cbn)
  return list(cbn_by_key.values())


# Regex to match a string like "@CalledByNative public void foo(int bar)".
_CALLED_BY_NATIVE_REGEX = re.compile(
    r'@CalledByNative((?P<Unchecked>(?:Unchecked)?|ForTesting))'
    r'(?:\("(?P<annotation_value>.*)"\))?'
    r'(?P<method_annotations>(?:\s*@\w+(?:\(.*?\))?)+)?'
    r'\s+(?P<modifiers>' + _MODIFIER_KEYWORDS + r')' +
    r'(?P<return_type_annotations>(?:\s*@\w+(?:\(.*?\))?)+)?'
    r'\s*(?P<return_type>[\S ]*?)'
    r'\s*(?P<name>\w+)'
    r'\s*\(\s*(?P<params>[^{;]*)\)'
    r'\s*(?:throws\s+[^{;]+)?'
    r'[{;]')


def _parse_called_by_natives(parsed_classes, contents, *,
                             allow_private_called_by_natives):
  ret = []
  for match in _CALLED_BY_NATIVE_REGEX.finditer(contents):
    parsed_class = _find_owning_class(parsed_classes, match.start())
    type_resolver = parsed_class.type_resolver

    return_type_grp = match.group('return_type')
    name = match.group('name')
    if return_type_grp:
      pre_annotations = match.group('method_annotations') or ''
      post_annotations = match.group('return_type_annotations') or ''
      # Combine all the annotations before parsing the return type.
      return_type_str = str.strip(f'{pre_annotations} {post_annotations}'
                                  f' {return_type_grp}')
      return_type = _parse_type(type_resolver, return_type_str)
    else:
      return_type = java_types.VOID
      name = '<init>'

    params = _parse_param_list(type_resolver, match.group('params'))
    signature = java_types.JavaSignature.from_params(return_type, params)

    modifiers = match.group('modifiers')
    if not allow_private_called_by_natives and 'private' in modifiers:
      raise ParseError(f'@CalledByNative methods must not be private. '
                       f'Found:\n{match.group(0)}\n')
    ret.append(
        ParsedCalledByNative(java_class=type_resolver.java_class,
                             name=name,
                             signature=signature,
                             static='static' in modifiers,
                             unchecked='Unchecked' in match.group('Unchecked')))

  # Check for any @CalledByNative occurrences that were not matched.
  unmatched_lines = _CALLED_BY_NATIVE_REGEX.sub('', contents).splitlines()
  for i, line in enumerate(unmatched_lines):
    if '@CalledByNative' in line:
      context = '\n'.join(unmatched_lines[i:i + 5])
      raise ParseError('Could not parse @CalledByNative method signature:\n' +
                       context)

  ret.sort()
  return ret


_FIELD_REGEX = re.compile(r'^(?:@\w+\s+)*'
                          r'\s*(?P<modifiers>' + _MODIFIER_KEYWORDS + r')'
                          r'(?P<type>[\w.<>\[\]]+)\s+'
                          r'(?P<name>\w+)'
                          r'(?:\s*=\s*(?P<value>[^;]+))?;',
                          flags=re.MULTILINE)


def _parse_fields(contents, type_resolver):
  ret = []
  for match in _FIELD_REGEX.finditer(contents):
    modifiers = match.group('modifiers')
    const_value = match.group('value')
    if const_value:
      # Strip long / double / float suffix letters.
      const_value = const_value.rstrip('dflDFL')

    ret.append(
        ParsedField(java_class=type_resolver.java_class,
                    name=match.group('name'),
                    java_type=_parse_type(type_resolver, match.group('type')),
                    static='static' in modifiers,
                    final='final' in modifiers,
                    const_value=const_value))
  ret.sort()
  return ret


_IMPORT_REGEX = re.compile(r'^import\s+([^\s*]+);', flags=re.MULTILINE)
_IMPORT_CLASS_NAME_REGEX = re.compile(r'^(.*?)\.([A-Z].*)')


def _parse_imports(contents, endpos):
  # Regex skips static imports as well as wildcard imports.
  names = _IMPORT_REGEX.findall(contents, endpos=endpos)
  for name in names:
    m = _IMPORT_CLASS_NAME_REGEX.match(name)
    if m:
      package, class_name = m.groups()
      yield java_types.JavaClass(
          package.replace('.', '/') + '/' + class_name.replace('.', '$'))


_JNI_NAMESPACE_REGEX = re.compile(r'@JNINamespace\("(.*?)"\)')


def _parse_jni_namespace(contents):
  m = _JNI_NAMESPACE_REGEX.findall(contents)
  if not m:
    return ''
  if len(m) > 1:
    raise ParseError('Found multiple @JNINamespace annotations.')
  return m[0]


def _do_parse(filename, *, package_prefix, package_prefix_filter,
              enable_legacy_natives, allow_private_called_by_natives):
  assert not filename.endswith('.kt'), (
      f'Found {filename}, but Kotlin is not supported by JNI generator.')
  with open(filename) as f:
    contents = f.read()
  contents = _remove_comments(contents)
  contents = _remove_generics(contents)

  parsed_classes = _parse_java_classes(contents, package_prefix,
                                       package_prefix_filter)

  outer_class = parsed_classes[0].java_class
  expected_name = os.path.splitext(os.path.basename(filename))[0]
  if outer_class.name != expected_name:
    raise ParseError(
        f'Found class "{outer_class.name}" but expected "{expected_name}".')

  type_resolver = parsed_classes[0].type_resolver
  parsed_proxy_natives = _parse_proxy_natives(type_resolver, contents)
  jni_namespace = _parse_jni_namespace(contents)

  if enable_legacy_natives:
    non_proxy_methods = _parse_non_proxy_natives(type_resolver, contents)
  else:
    non_proxy_methods = []
  called_by_natives = _parse_called_by_natives(
      parsed_classes,
      contents,
      allow_private_called_by_natives=allow_private_called_by_natives)

  ret = ParsedFile(filename=filename,
                   jni_namespace=jni_namespace,
                   type_resolver=type_resolver,
                   proxy_methods=[],
                   non_proxy_methods=non_proxy_methods,
                   called_by_natives=called_by_natives,
                   fields=[])

  if parsed_proxy_natives:
    ret.module_name = parsed_proxy_natives.module_name
    ret.proxy_interface = outer_class.make_nested(
        parsed_proxy_natives.interface_name)
    ret.proxy_visibility = parsed_proxy_natives.visibility
    ret.proxy_methods = parsed_proxy_natives.methods

  return ret


def parse_java_file(filename,
                    *,
                    package_prefix=None,
                    package_prefix_filter=None,
                    enable_legacy_natives=False,
                    allow_private_called_by_natives=False):
  try:
    return _do_parse(
        filename,
        package_prefix=package_prefix,
        package_prefix_filter=package_prefix_filter,
        enable_legacy_natives=enable_legacy_natives,
        allow_private_called_by_natives=allow_private_called_by_natives)
  except Exception as e:
    note = f' (when parsing {filename})'
    if e.args and isinstance(e.args[0], str):
      e.args = (e.args[0] + note, *e.args[1:])
    else:
      e.args = e.args + (note, )
    raise


_JAVAP_CLASS_REGEX = re.compile(r'\b(?:class|interface) (\S+)')
_JAVAP_FIELD_REGEX = re.compile(
    rf'^\s*({_MODIFIER_KEYWORDS}).*? (\S+?)(?: = (.*?))?;\n\s+descriptor: ([^(\s]+)',
    flags=re.MULTILINE)
_JAVAP_METHOD_REGEX = re.compile(
    rf'^\s*({_MODIFIER_KEYWORDS}).*?(\S+?)\(.*\n\s+descriptor: (.*)',
    flags=re.MULTILINE)


def parse_javap(filename, contents):
  contents = _remove_generics(contents)
  match = _JAVAP_CLASS_REGEX.search(contents)
  if not match:
    raise ParseError('Could not find java class in javap output')
  java_class = java_types.JavaClass(match.group(1).replace('.', '/'))
  type_resolver = java_types.TypeResolver(java_class)

  fields = _parse_fields(contents, type_resolver)

  called_by_natives = []
  for match in _JAVAP_METHOD_REGEX.finditer(contents):
    modifiers, name, descriptor = match.groups()
    if name == java_class.full_name_with_dots:
      name = '<init>'
    signature = java_types.JavaSignature.from_descriptor(descriptor)

    called_by_natives.append(
        ParsedCalledByNative(java_class=java_class,
                             name=name,
                             signature=signature,
                             static='static' in modifiers))
  called_by_natives = _filter_duplicate_return_types(called_by_natives)
  called_by_natives.sort()
  return ParsedFile(filename=filename,
                    type_resolver=type_resolver,
                    proxy_methods=[],
                    non_proxy_methods=[],
                    called_by_natives=called_by_natives,
                    fields=fields)
