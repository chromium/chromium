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


@dataclasses.dataclass(order=True)  # Field order matters.
class ParsedNative:
  static: bool
  name: str
  signature: java_types.JavaSignature
  native_class_name: str


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
  called_by_natives: List[ParsedCalledByNative] = (dataclasses.field(
      default_factory=list))
  fields: List[ParsedField] = dataclasses.field(default_factory=list)


@dataclasses.dataclass
class ParsedFile:
  filename: str
  outer_class: ParsedClass
  classes_with_jni: List[ParsedClass]  # ParsedCalledByNative or CalledByNative
  proxy_methods: List[ParsedNative]
  non_proxy_methods: List[ParsedNative]
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


_last_match = []


def _find_iter_with_note(regex, data, **kwargs):
  for match in regex.finditer(data, **kwargs):
    _last_match.append(match.group())
    yield match
    _last_match.pop()


_PACKAGE_REGEX = re.compile(r'^package\s+(\S+?);', flags=re.MULTILINE)


def _parse_package(contents, require=True):
  match = _PACKAGE_REGEX.search(contents)
  if not match:
    if require:
      raise ParseError('Unable to find "package" line')
    return ''
  return match.group(1).replace('.', '/')


_CLASSES_REGEX = re.compile(
    r'^((?:(?!\b(?:class|interface|enum)\b)'
    r'(?:[^{}"]|"[^"]*"))*?)\b'
    r'(?:class|interface|enum)\b\s+\b([\w.]+)'
    r'(<[\s\S]*?>)?\s*[^{]*?\{', re.MULTILINE)
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


def _find_owning_class(parsed_classes, match):
  ret = None
  index = match.start()
  for c in parsed_classes:
    if c._start_idx <= index <= c._end_idx:
      ret = c
  if not ret:
    raise ParseError(f'Could not determine enclosing class for: {match}\n'
                     f' Classes: {parsed_classes}')
  return ret


# Does not handle doubly-nested classes.
def _parse_java_classes(contents,
                        expected_name,
                        package_prefix=None,
                        package_prefix_filter=None,
                        is_javap=False):
  package = _parse_package(contents, require=not is_javap)
  null_marked = False
  parsed_classes = []
  for m in _find_iter_with_note(_CLASSES_REGEX, contents):
    preamble, class_name, generics_str = m.groups()
    # Ignore annotations like @Foo("contains the words class Bar")
    if preamble.count('"') % 2 != 0:
      continue

    if generics_str and generics_str.count('<') != generics_str.count('>'):
      # Regex failed to capture full nested generics.
      # Find the balanced closing bracket.
      start_idx = m.start(3)
      pos = start_idx
      while True:
        pos = contents.find('>', pos + 1)
        if pos == -1:
          break
        if contents.count('<', start_idx,
                          pos + 1) == contents.count('>', start_idx, pos + 1):
          generics_str = contents[start_idx:pos + 1]
          break

    is_outer_class = not parsed_classes
    if is_outer_class:
      # javap uses fully-qualified names.
      if '.' in class_name:
        java_class = java_types.JavaClass(class_name.replace('.', '/'))
      else:
        java_class = java_types.JavaClass(f'{package}/{class_name}')

      if java_class.name != expected_name:
        raise ParseError(
            f'Found class "{class_name}" but expected "{expected_name}".')

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
      type_resolver = outer_class.type_resolver
      java_class = type_resolver.java_class.make_nested(class_name)
      if java_class in type_resolver.nested_classes:
        # Class is nested in a method, ignore it.
        continue
      type_resolver = type_resolver.add_child(java_class=java_class)
      end_idx = _find_class_end(contents, m.end(1), class_name, m.end(0))

    class_keyword_start = m.end()
    if generics_str:
      type_resolver.type_params = _parse_type_params(type_resolver,
                                                     generics_str[1:-1])
    parsed_classes.append(
        ParsedClass(_start_idx=class_keyword_start,
                    _end_idx=end_idx,
                    type_resolver=type_resolver))

  if parsed_classes:
    parsed_classes[0].type_resolver.nested_classes.sort()
  return parsed_classes


# Complicated example:
# @JniType("std::optional<void(*)(const std::vector<bool>&)>") Callback<Boolean> funcType
# Eager search for quotes to skip over )s within quotes.
_ANNOTATION_REGEX = re.compile(
    r'@(?P<name>[\w.]+)(?:\((?:"(?P<arg>.*?)")?[^)]*\))?\s*')


def _parse_annotations(value):
  """Returns a dict of annotations and the value with them removed."""
  if '@' not in value:
    return {}, value
  annotations = {}
  # Must ignore: List<@JniType("std::vector<std::vector<int>>") String>
  # Must not ignore: "OuterClass.@Nullable InnerClass"
  # Must not ignore: @Contract("_, !null -> !null")
  sb = []
  cursor_idx = 0
  for m in _find_iter_with_note(_ANNOTATION_REGEX, value):
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


def _split_by_delimiter(value, delimiter):
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


def _parse_type(type_resolver, value):
  """Parses a string into a JavaType."""
  # E.g. List<?>, List<? extends Foo>
  if value[0] == '?':
    # Since we do not model inheritence in our C++ mirror classes, there is no
    # value in tracking wildcard types.
    if len(value) == 1 or ' super ' in value:
      return java_types.OBJECT
    parts = value.split(' extends ', 1)
    if len(parts) != 2:
      raise ParseError(f'Could not parse wildcard type: {value}')
    return _parse_type(type_resolver, parts[1])

  annotations, parsed_value = _parse_annotations(value)
  array_dimensions = 0
  while parsed_value[-2:] == '[]':
    array_dimensions += 1
    # strip to remove possible spaces between type and [].
    parsed_value = parsed_value[:-2].rstrip()

  generics = None
  if parsed_value in java_types.PRIMITIVES:
    java_class = None
    primitive_name = parsed_value
  else:
    if parsed_value[-1] == '>':
      parsed_value, generics_str = parsed_value.split('<', 1)
      parsed_value = parsed_value.rstrip()
      generics_str = generics_str[:-1]
      generics = tuple(
          _parse_type(type_resolver, g)
          for g in _split_by_delimiter(generics_str, ','))

    java_class = type_resolver.resolve(parsed_value)
    primitive_name = None
    if java_class == java_types.CLASS_CLASS:
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
      raise Error('Found non-templatized @JniType("std::vector") on '
                  f'non-array, non-Collection type: {java_class} '
                  f'(when parsing {value}')

  if primitive_name and array_dimensions == 0:
    nullable = False
  elif type_resolver.null_marked:
    nullable = annotations.get('Nullable') is not None
  else:
    nullable = annotations.get('NonNull') is None

  return java_types.JavaType(java_class=java_class,
                             primitive_name=primitive_name,
                             array_dimensions=array_dimensions,
                             converted_type=converted_type,
                             nullable=nullable,
                             generics=generics)


def _parse_type_params(type_resolver, value):
  if not value or type_resolver.java_class == java_types.CLASS_CLASS:
    return java_types.EMPTY_TYPE_PARAM_LIST
  params = []
  for param_str in _split_by_delimiter(value, ','):
    upper_bound_type = java_types.OBJECT
    if ' extends ' in param_str:
      name, bound_str = param_str.split(' extends ', 1)
      upper_bound_type = _parse_type(type_resolver, bound_str.strip())
    elif ' super ' in param_str:
      name, _ = param_str.split(' super ', 1)
    else:
      name = param_str
    params.append(java_types.JavaTypeParam.make(name, upper_bound_type))

  return java_types.JavaTypeParamList(params)


_FINAL_REGEX = re.compile(r'\bfinal\s')


def _parse_param_list(type_resolver, value) -> java_types.JavaParamList:
  if not value or value.isspace():
    return java_types.EMPTY_PARAM_LIST
  params = []
  value = _FINAL_REGEX.sub('', value)

  # Split by commas that are not inside generics or quotes.
  param_strs = _split_by_delimiter(value, ',')

  for i, param_str in enumerate(param_strs):
    if not param_str:
      continue

    parts = _split_by_delimiter(param_str, ' ')
    if len(parts) == 1:
      param_name = f'p{i}'
      param_str = parts[0]
    else:
      param_name = parts[-1]
      param_str = ' '.join(parts[:-1])

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

  for m in _find_iter_with_note(_PROXY_NATIVE_REGEX, interface_body):
    preamble, name, params_part = m.groups()
    preamble = _PUBLIC_REGEX.sub('', preamble)
    annotations, _ = _parse_annotations(preamble)
    params = _parse_param_list(type_resolver, params_part)
    return_type = _parse_type(type_resolver, preamble)
    signature = java_types.JavaSignature.from_params(return_type, params)
    ret.methods.append(
        ParsedNative(
            static=False,
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
  for match in _find_iter_with_note(_NON_PROXY_NATIVES_REGEX, contents):
    name = match.group('name')
    return_type = _parse_type(type_resolver, match.group('return_type'))
    params = _parse_param_list(type_resolver, match.group('params'))
    signature = java_types.JavaSignature.from_params(return_type, params)
    native_class_name = match.group('native_class_name')
    static = 'static' in match.group('qualifiers')
    ret.append(
        ParsedNative(static=static,
                     name=name,
                     signature=signature,
                     native_class_name=native_class_name))
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


def _make_called_by_native_regex(is_javap):
  sb = []
  if not is_javap:
    sb.append(r'@CalledByNative((?P<Unchecked>(?:Unchecked)?|ForTesting))'
              r'(?:\("(?P<annotation_value>.*)"\))?'
              r'(?P<method_annotations>(?:\s*@\w+(?:\(.*?\))?)+)?')
  # Enfore a space after the type params to account for:
  # <T extends java.lang.Comparable<? super T>>
  sb.append(r'\s+(?P<modifiers>' + _MODIFIER_KEYWORDS + r')'
            r'(?:(?P<type_params><.*?>) )?')
  if not is_javap:
    sb.append(r'\s*(?P<return_type_annotations>(?:\s*@\w+(?:\(.*?\))?)+)?')
  sb.append(r'\s*(?P<return_type>[\S ]*?)'
            r'\s*(?P<name>[\w.]+)'
            r'\s*\(\s*(?P<params>[^{;]*)\)'
            r'\s*(?:throws\s+[^{;]+)?'
            r'[{;]')

  # When parsing .java, the @CalledByNative makes the search not match
  # unrelated things.
  # When parsing javap, we restrict to single lines in order to avoid false
  # positives.
  pattern = ''.join(sb)
  if is_javap:
    pattern = pattern.replace(r'\s', ' ')
  return re.compile(pattern, flags=re.MULTILINE)


# Matches methds that have @CalledByNative.
_CALLED_BY_NATIVE_REGEX = _make_called_by_native_regex(is_javap=False)

# Matches all methods & assumes no annotations.
_JAVAP_METHOD_REGEX = _make_called_by_native_regex(is_javap=True)


def _parse_called_by_natives(contents,
                             parsed_classes,
                             *,
                             is_javap=False,
                             allow_private_called_by_natives=False):
  regex = _JAVAP_METHOD_REGEX if is_javap else _CALLED_BY_NATIVE_REGEX
  pos = parsed_classes[0]._start_idx
  for match in _find_iter_with_note(regex, contents, pos=pos):
    modifiers = match.group('modifiers')
    if 'private' in modifiers and not allow_private_called_by_natives:
      raise ParseError(f'@CalledByNative methods must not be private. '
                       f'Found:\n{match.group(0)}\n')

    parsed_class = _find_owning_class(parsed_classes, match)
    type_resolver = parsed_class.type_resolver

    type_params_str = match.group('type_params')
    type_params = java_types.EMPTY_TYPE_PARAM_LIST
    if type_params_str:
      type_params = _parse_type_params(type_resolver, type_params_str[1:-1])
      type_resolver = type_resolver.make_method_resolver(
          type_params=type_params)

    return_type_str = match.group('return_type')
    name = match.group('name')
    if return_type_str:
      if not is_javap:
        pre_annotations = match.group('method_annotations') or ''
        post_annotations = match.group('return_type_annotations') or ''
        # Combine all the annotations before parsing the return type.
        return_type_str = str.strip(f'{pre_annotations} {post_annotations}'
                                    f' {return_type_str}')
      return_type = _parse_type(type_resolver, return_type_str)
    else:
      return_type = java_types.VOID
      name = '<init>'

    params = _parse_param_list(type_resolver, match.group('params'))
    signature = java_types.JavaSignature.from_params(return_type, params)

    unchecked = not is_javap and 'Unchecked' in match.group('Unchecked')
    parsed_class.called_by_natives.append(
        ParsedCalledByNative(name=name,
                             signature=signature,
                             static='static' in modifiers,
                             type_params=type_params,
                             unchecked=unchecked))

  if not is_javap:
    # Check for any @CalledByNative occurrences that were not matched.
    unmatched_lines = _CALLED_BY_NATIVE_REGEX.sub('', contents).splitlines()
    for i, line in enumerate(unmatched_lines):
      if '@CalledByNative' in line:
        context = '\n'.join(unmatched_lines[i:i + 5])
        raise ParseError('Could not parse @CalledByNative method signature:\n' +
                         context)

  for c in parsed_classes:
    c.called_by_natives = _filter_duplicate_return_types(c.called_by_natives)


_FIELD_REGEX = re.compile(r'^(?:@\w+\s+)*'
                          r'\s*(?P<modifiers>' + _MODIFIER_KEYWORDS + r')'
                          r'(?P<type>[\w.<>\[\]]+)\s+'
                          r'(?P<name>\w+)'
                          r'(?:\s*=\s*(?P<value>[^;]+))?;',
                          flags=re.MULTILINE)


def _parse_fields(contents, parsed_classes):
  for match in _find_iter_with_note(_FIELD_REGEX, contents):
    modifiers = match.group('modifiers')
    parsed_class = _find_owning_class(parsed_classes, match)
    type_resolver = parsed_class.type_resolver

    const_value = match.group('value')
    if const_value:
      # Strip long / double / float suffix letters.
      const_value = const_value.rstrip('dflDFL')

    parsed_class.fields.append(
        ParsedField(name=match.group('name'),
                    java_type=_parse_type(type_resolver, match.group('type')),
                    static='static' in modifiers,
                    final='final' in modifiers,
                    const_value=const_value))


_IMPORT_REGEX = re.compile(r'^import\s+([^\s*]+);', flags=re.MULTILINE)
_IMPORT_CLASS_NAME_REGEX = re.compile(r'^(.*?)\.([A-Z].*)')


def _parse_imports(contents, endpos):
  # Regex skips static imports as well as wildcard imports.
  names = _IMPORT_REGEX.findall(contents, endpos=endpos)
  for name in names:
    if m := _IMPORT_CLASS_NAME_REGEX.match(name):
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


def _sort_jni(parsed_classes):
  for c in parsed_classes:
    c.called_by_natives.sort()
    c.fields.sort()


def parse_java_file_data(filename, contents, *, package_prefix,
                         package_prefix_filter, enable_legacy_natives,
                         allow_private_called_by_natives):
  contents = _remove_comments(contents)

  expected_name = os.path.splitext(os.path.basename(filename))[0]
  parsed_classes = _parse_java_classes(contents, expected_name, package_prefix,
                                       package_prefix_filter)

  if not parsed_classes:
    raise ParseError('No classes found.')

  outer_class = parsed_classes[0]
  type_resolver = outer_class.type_resolver

  parsed_proxy_natives = _parse_proxy_natives(type_resolver, contents)
  jni_namespace = _parse_jni_namespace(contents)

  if enable_legacy_natives:
    non_proxy_methods = _parse_non_proxy_natives(type_resolver, contents)
  else:
    non_proxy_methods = []
  _parse_called_by_natives(
      contents,
      parsed_classes,
      allow_private_called_by_natives=allow_private_called_by_natives)

  classes_with_jni = sorted(c for c in parsed_classes
                            if c.called_by_natives or c.fields)
  _sort_jni(classes_with_jni)
  ret = ParsedFile(filename=filename,
                   outer_class=outer_class,
                   classes_with_jni=classes_with_jni,
                   jni_namespace=jni_namespace,
                   proxy_methods=[],
                   non_proxy_methods=non_proxy_methods)

  if parsed_proxy_natives:
    outer_java_class = outer_class.type_resolver.java_class
    ret.module_name = parsed_proxy_natives.module_name
    ret.proxy_interface = outer_java_class.make_nested(
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
    assert not filename.endswith('.kt'), (
        f'Found {filename}, but Kotlin is not supported by JNI generator.')
    with open(filename) as f:
      contents = f.read()
    return parse_java_file_data(
        filename,
        contents,
        package_prefix=package_prefix,
        package_prefix_filter=package_prefix_filter,
        enable_legacy_natives=enable_legacy_natives,
        allow_private_called_by_natives=allow_private_called_by_natives)
  except Exception as e:
    if _last_match:
      common.add_note(e, f'in match {_last_match}')
    common.add_note(e, f'when parsing {filename}')
    raise


def parse_javap_data(filename, contents):
  try:
    if contents.startswith('Compiled from'):
      contents = contents.split('\n', 1)[1]

    expected_name = os.path.splitext(os.path.basename(filename))[0]
    parsed_classes = _parse_java_classes(contents, expected_name, is_javap=True)

    # For javap there is only ever one class.
    assert len(parsed_classes) == 1

    _parse_fields(contents, parsed_classes)
    _parse_called_by_natives(contents, parsed_classes, is_javap=True)
    _sort_jni(parsed_classes)
    return ParsedFile(filename=filename,
                      outer_class=parsed_classes[0],
                      classes_with_jni=parsed_classes,
                      proxy_methods=[],
                      non_proxy_methods=[])
  except Exception as e:
    if _last_match:
      common.add_note(e, f'in match {_last_match}')
    common.add_note(e, f'when parsing javap output for {filename}')
    raise
