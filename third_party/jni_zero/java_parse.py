# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import re

import common
import java_types
import parse_common

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

_CLASSES_REGEX = re.compile(
    r'^((?:(?!\b(?:class|interface|enum)\b)'
    r'(?:[^{}"]|"[^"]*"))*?)\b'
    r'(?:class|interface|enum)\b\s+\b([\w.$]+)'
    r'(<[\s\S]*?>)?\s*[^{]*?\{', re.MULTILINE)


# Does not handle doubly-nested classes.
def _parse_java_classes(contents,
                        expected_name,
                        package_prefix=None,
                        package_prefix_filter=None,
                        is_javap=False,
                        type_catalog=None,
                        enable_safe_pointers=False):
  package = parse_common.parse_package(contents, require=not is_javap)
  null_marked = False
  parsed_classes = []
  for m in parse_common.find_iter_with_note(_CLASSES_REGEX, contents):
    preamble, class_name, generics_str = m.groups()
    # Ignore annotations like @Foo("contains the words class Bar")
    if preamble.count('"') % 2 != 0:
      continue

    if generics_str and generics_str.count('<') != generics_str.count('>'):
      # Regex failed to capture full nested generics.
      generics_str = (parse_common.find_balanced_generics(contents, m.start(3))
                      or generics_str)

    is_outer_class = not parsed_classes
    if is_outer_class:
      # javap uses fully-qualified names.
      if '.' in class_name:
        java_class = java_types.JavaClass(class_name.replace('.', '/'))
      else:
        java_class = java_types.JavaClass(f'{package}/{class_name}')

      if java_class.name != expected_name:
        raise parse_common.ParseError(
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
          package_prefix_filter=package_prefix_filter,
          type_catalog=type_catalog,
          enable_safe_pointers=enable_safe_pointers)
      if not is_javap:
        for c in parse_common.parse_imports(contents, m.end()):
          type_resolver.add_import(c)
    else:
      outer_class = parsed_classes[0]
      type_resolver = outer_class.type_resolver
      java_class = type_resolver.java_class.make_nested(class_name)
      if java_class in type_resolver.nested_classes:
        # Class is nested in a method, ignore it.
        continue
      type_resolver = type_resolver.add_child(java_class=java_class)
      end_idx = parse_common.find_class_end(contents, m.end(1), class_name,
                                            m.end(0))

    class_keyword_start = m.end()
    if generics_str:
      type_resolver.type_params = _parse_type_params(type_resolver,
                                                     generics_str[1:-1])
    annotations, _ = parse_common.parse_annotations(preamble)
    jni_type = annotations.get('JniType')
    parsed_classes.append(
        parse_common.ParsedClass(_start_idx=class_keyword_start,
                                 _end_idx=end_idx,
                                 type_resolver=type_resolver,
                                 jni_type=jni_type))

  if parsed_classes:
    parsed_classes[0].type_resolver.nested_classes.sort()
  return parsed_classes


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
      raise parse_common.ParseError(f'Could not parse wildcard type: {value}')
    return _parse_type(type_resolver, parts[1])

  annotations, parsed_value = parse_common.parse_annotations(value)
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
          for g in parse_common.split_by_delimiter(generics_str, ','))

    java_class = type_resolver.resolve(parsed_value)
    primitive_name = None

  if type_resolver.null_marked:
    nullable = annotations.get('Nullable') is not None
  else:
    nullable = annotations.get('NonNull') is None

  return parse_common.make_java_type(type_resolver,
                                     value=value,
                                     parsed_value=parsed_value,
                                     java_class=java_class,
                                     primitive_name=primitive_name,
                                     array_dimensions=array_dimensions,
                                     generics=generics,
                                     annotations=annotations,
                                     nullable=nullable)


def _parse_type_params(type_resolver, value):
  if not value or type_resolver.java_class == java_types.CLASS_CLASS:
    return java_types.EMPTY_TYPE_PARAM_LIST
  params = []
  for param_str in parse_common.split_by_delimiter(value, ','):
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

  # Split parameter list by commas that are not nested inside generics (e.g. Map<K, V>).
  param_strs = [p for p in parse_common.split_by_delimiter(value, ',') if p]

  for i, param_str in enumerate(param_strs):
    # Split by spaces outside generics to separate annotations, type, and optional param name.
    # We must not split on spaces within generics (e.g. Comparator<? super E>).
    normalized_param = re.sub(r'\s+', ' ', param_str)
    parts = [
        x for x in parse_common.split_by_delimiter(normalized_param, ' ') if x
    ]
    if len(parts) == 1:
      # In javap output or interface declarations without param names, parts has only the type.
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


_PROXY_NATIVE_REGEX = re.compile(r'\s*(.*?)\s+(\w+)\((.*?)\);', flags=re.DOTALL)

_PUBLIC_REGEX = re.compile(r'\bpublic\s')


def _iter_java_proxy_methods(type_resolver, interface_body):
  for m in parse_common.find_iter_with_note(_PROXY_NATIVE_REGEX,
                                            interface_body):
    preamble, name, params_part = m.groups()
    preamble = _PUBLIC_REGEX.sub('', preamble)
    annotations, _ = parse_common.parse_annotations(preamble)
    params = _parse_param_list(type_resolver, params_part)
    return_type = _parse_type(type_resolver, preamble)
    yield name, annotations, params_part, params, return_type


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
              r'(?:\(".*"\))?'
              r'(?P<method_annotations>(?:\s*@[\w.]+(?:\(.*?\))?)+)?')
  # Enfore a space after the type params to account for:
  # <T extends java.lang.Comparable<? super T>>
  sb.append(r'\s+(?P<modifiers>' + _MODIFIER_KEYWORDS + r')'
            r'(?:(?P<type_params><.*?>) )?')
  if not is_javap:
    sb.append(r'\s*(?P<return_type_annotations>(?:\s*@[\w.]+(?:\(.*?\))?)+)?')
  sb.append(r'\s*(?P<return_type>[\S ]*?)'
            r'\s*(?P<name>[\w.$]+)'
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


def _parse_called_by_natives_or_javap(contents,
                                      parsed_classes,
                                      *,
                                      is_javap=False,
                                      natives_only=False,
                                      allow_private_called_by_natives=False):
  regex = _JAVAP_METHOD_REGEX if is_javap else _CALLED_BY_NATIVE_REGEX
  pos = parsed_classes[0]._start_idx
  for match in parse_common.find_iter_with_note(regex, contents, pos=pos):
    modifiers = match.group('modifiers')
    is_native = 'native' in modifiers
    if natives_only and not is_native:
      continue

    is_private = 'private' in modifiers
    if is_private and not is_native and not allow_private_called_by_natives:
      raise parse_common.ParseError(
          f'@CalledByNative methods must not be private. '
          f'Found:\n{match.group(0)}\n')

    parsed_class = parse_common.find_owning_class(parsed_classes, match)
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
    if natives_only:
      if parsed_class is not parsed_classes[0]:
        raise parse_common.ParseError(
            f'native methods on nested classes not currently '
            f'supported: {parsed_class}')
      parsed_class.non_proxy_methods.append(
          parse_common.ParsedNative(static='static' in modifiers,
                                    name=name,
                                    signature=signature))
    else:
      parse_common.check_called_by_native_return_type(name, return_type)
      unchecked = not is_javap and 'Unchecked' in match.group('Unchecked')
      parsed_class.called_by_natives.append(
          parse_common.ParsedCalledByNative(name=name,
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
        raise parse_common.ParseError(
            'Could not parse @CalledByNative method signature:\n' + context)

  for c in parsed_classes:
    c.called_by_natives = _filter_duplicate_return_types(c.called_by_natives)


_FIELD_REGEX = re.compile(r'^(?:@\w+\s+)*'
                          r'\s*(?P<modifiers>' + _MODIFIER_KEYWORDS + r')'
                          r'(?P<type>[\w.<>\[\]]+)\s+'
                          r'(?P<name>\w+)'
                          r'(?:\s*=\s*(?P<value>[^;]+))?;',
                          flags=re.MULTILINE)


def _parse_fields(contents, parsed_classes):
  for match in parse_common.find_iter_with_note(_FIELD_REGEX, contents):
    modifiers = match.group('modifiers')
    parsed_class = parse_common.find_owning_class(parsed_classes, match)
    type_resolver = parsed_class.type_resolver

    const_value = match.group('value')
    if const_value:
      # Strip long / double / float suffix letters.
      const_value = const_value.rstrip('dflDFL')

    parsed_class.fields.append(
        parse_common.ParsedField(name=match.group('name'),
                                 java_type=_parse_type(type_resolver,
                                                       match.group('type')),
                                 static='static' in modifiers,
                                 final='final' in modifiers,
                                 const_value=const_value))


def parse_java_file_data(filename,
                         contents,
                         *,
                         package_prefix,
                         package_prefix_filter,
                         allow_private_called_by_natives,
                         type_catalog=None,
                         enable_safe_pointers=False):
  contents = parse_common.remove_comments(contents)

  expected_name = os.path.splitext(os.path.basename(filename))[0]
  parsed_classes = _parse_java_classes(
      contents,
      expected_name,
      package_prefix,
      package_prefix_filter,
      type_catalog=type_catalog,
      enable_safe_pointers=enable_safe_pointers)

  if not parsed_classes:
    raise parse_common.ParseError('No classes found.')

  type_tokens = parse_common.extract_type_catalog(parsed_classes, type_catalog)

  outer_class = parsed_classes[0]
  type_resolver = outer_class.type_resolver

  parsed_proxy_natives = parse_common.parse_proxy_natives(
      type_resolver, contents, iter_methods=_iter_java_proxy_methods)
  jni_namespace = parse_common.parse_jni_namespace(contents)

  _parse_called_by_natives_or_javap(
      contents,
      parsed_classes,
      allow_private_called_by_natives=allow_private_called_by_natives)

  classes_with_jni = sorted(
      c for c in parsed_classes
      if c.called_by_natives or c.fields or c.non_proxy_methods)
  parse_common.sort_jni(classes_with_jni)
  ret = parse_common.ParsedFile(filename=filename,
                                outer_class=outer_class,
                                classes_with_jni=classes_with_jni,
                                type_tokens=type_tokens,
                                jni_namespace=jni_namespace,
                                proxy_methods=[])

  if parsed_proxy_natives:
    outer_java_class = outer_class.type_resolver.java_class
    ret.proxy_interface = outer_java_class.make_nested(
        parsed_proxy_natives.interface_name)
    ret.proxy_visibility = parsed_proxy_natives.visibility
    ret.proxy_methods = parsed_proxy_natives.methods

  return ret


def _resolve_type(java_type, type_catalog):
  if java_type.is_safe_pointer():
    inner = java_type.generics[0]
    if not inner.converted_type:
      fqn = inner.java_class.class_without_prefix.full_name_with_slashes
      converted_type = type_catalog.get(fqn)
      if not converted_type:
        raise parse_common.ParseError(
            f'Safe pointer inner type "{inner.non_array_full_name_with_slashes}" '
            f'does not resolve to a C++ type. Annotate its JniTypeToken '
            f'interface with @JniType("::your::CppType") (or provide it via '
            f'the type catalog): {java_type.to_java(with_generics=True)}')
      # Mutate frozen dataclass in place since JavaType is not used in
      # sets or dicts at this phase.
      object.__setattr__(inner, 'converted_type', converted_type)


def _resolve_signature(signature, type_catalog):
  for java_type in signature.iter_types():
    _resolve_type(java_type, type_catalog)


def resolve_safe_pointers(parsed_files, type_catalog):
  """Resolves and validates safe pointer types across parsed files."""
  for pf in parsed_files:
    try:
      for m in pf.proxy_methods:
        _resolve_signature(m.signature, type_catalog)
      for c in pf.classes_with_jni:
        for cbn in c.called_by_natives:
          _resolve_signature(cbn.signature, type_catalog)
        for f in c.fields:
          _resolve_type(f.java_type, type_catalog)
        for m in c.non_proxy_methods:
          _resolve_signature(m.signature, type_catalog)
    except Exception as e:
      # This runs after parse_java_file() has returned, so it must attach the
      # filename itself.
      common.add_note(e, f'when parsing {pf.filename}')
      raise


def parse_java_file(filename,
                    *,
                    package_prefix=None,
                    package_prefix_filter=None,
                    allow_private_called_by_natives=False,
                    type_catalog=None,
                    enable_safe_pointers=False):
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
        allow_private_called_by_natives=allow_private_called_by_natives,
        type_catalog=type_catalog,
        enable_safe_pointers=enable_safe_pointers)
  except Exception as e:
    if parse_common.last_match:
      common.add_note(e, f'in match {parse_common.last_match}')
    common.add_note(e, f'when parsing {filename}')
    raise


def parse_javap_data(filename, contents, natives_only=False):
  try:
    if contents.startswith('Compiled from'):
      contents = contents.split('\n', 1)[1]

    expected_name = os.path.splitext(os.path.basename(filename))[0]
    parsed_classes = _parse_java_classes(contents, expected_name, is_javap=True)

    # For javap there is only ever one class.
    assert len(parsed_classes) == 1

    if natives_only:
      _parse_called_by_natives_or_javap(contents,
                                        parsed_classes,
                                        is_javap=True,
                                        natives_only=True)
    else:
      _parse_fields(contents, parsed_classes)
      _parse_called_by_natives_or_javap(contents, parsed_classes, is_javap=True)

    parse_common.sort_jni(parsed_classes)
    return parse_common.ParsedFile(filename=filename,
                                   outer_class=parsed_classes[0],
                                   classes_with_jni=parsed_classes,
                                   proxy_methods=[])
  except Exception as e:
    if parse_common.last_match:
      common.add_note(e, f'in match {parse_common.last_match}')
    common.add_note(e, f'when parsing javap output for {filename}')
    raise
