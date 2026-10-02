# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Parser for Kotlin source files.

Mirrors the structure of java_parse.py. Helpers that do not depend on the
language's syntax live in parse_common.py and are shared.
"""

import os
import re

import common
import java_types
import parse_common

_PRIMITIVE_MAP = {
    'Boolean': 'boolean',
    'Byte': 'byte',
    'Char': 'char',
    'Short': 'short',
    'Int': 'int',
    'Long': 'long',
    'Float': 'float',
    'Double': 'double',
}

# Kotlin primitives become their boxed type when nullable (Int? -> Integer), or
# when used as a type argument (List<Int> -> List<Integer>).
_BOXED_CLASS_MAP = {
    'Boolean': java_types.JavaClass('java/lang/Boolean'),
    'Byte': java_types.JavaClass('java/lang/Byte'),
    'Char': java_types.JavaClass('java/lang/Character'),
    'Short': java_types.JavaClass('java/lang/Short'),
    'Int': java_types.JavaClass('java/lang/Integer'),
    'Long': java_types.JavaClass('java/lang/Long'),
    'Float': java_types.JavaClass('java/lang/Float'),
    'Double': java_types.JavaClass('java/lang/Double'),
}

# E.g. IntArray -> int[]
_PRIMITIVE_ARRAY_MAP = {f'{k}Array': v for k, v in _PRIMITIVE_MAP.items()}

# Types that are implicitly imported by Kotlin, and that TypeResolver would
# otherwise resolve to the current package (it handles java.lang types itself).
# Unlike in Kotlin, these take precedence over explicit imports and nested
# classes.
_KOTLIN_CLASS_MAP = {
    'Any': java_types.OBJECT_CLASS,
    'Collection': java_types.COLLECTION_CLASS,
    'List': java_types.LIST_CLASS,
    'Map': java_types.MAP_CLASS,
    'Set': java_types.SET_CLASS,
    'MutableCollection': java_types.COLLECTION_CLASS,
    'MutableList': java_types.LIST_CLASS,
    'MutableMap': java_types.MAP_CLASS,
    'MutableSet': java_types.SET_CLASS,
    'ArrayList': java_types.JavaClass('java/util/ArrayList'),
    'HashMap': java_types.JavaClass('java/util/HashMap'),
    'HashSet': java_types.JavaClass('java/util/HashSet'),
}

# Mirrors java_parse._MODIFIER_KEYWORDS. Modifiers that change a method's JVM
# signature or name (e.g. "suspend", or "internal", which mangles names) are
# deliberately absent so that declarations using them fail to parse rather than
# being silently mistranslated.
_MODIFIER_KEYWORDS = (r'(?:(?:' + '|'.join([
    'abstract',
    'final',
    'open',
    'override',
    'private',
    'protected',
    'public',
]) + r')\s+)*')

# Like java_parse._CLASSES_REGEX, but with "object" (Kotlin's singleton),
# without "enum" (Kotlin spells it "enum class", matched by the "class" case),
# and with an optional body. Bodyless classes (e.g. "class Foo(val x: Int)")
# are common in Kotlin, and must be matched so that references to nested ones
# resolve correctly, and so that the search for "{" does not run on into the
# next declaration.
_CLASSES_REGEX = re.compile(
    r'^((?:(?!\b(?:class|interface|object)\b)'
    r'(?:[^{}"]|"[^"]*"))*?)\b'
    r'(?:class|interface|object)\b\s+\b([\w.$]+)'
    r'(<[\s\S]*?>)?'
    # The header spans multiple lines only within parentheses (e.g. a primary
    # constructor), or after ":" or ",".
    r'(?:\((?:[^()]|\([^()]*\))*\)|[^{}()\n]|(?<=[:,])\n)*(\{)?',
    re.MULTILINE)


# Does not handle doubly-nested classes.
def _parse_kotlin_classes(contents,
                          expected_name,
                          package_prefix=None,
                          package_prefix_filter=None,
                          type_catalog=None,
                          enable_safe_pointers=False):
  package = parse_common.parse_package(contents)
  parsed_classes = []
  for m in parse_common.find_iter_with_note(_CLASSES_REGEX, contents):
    preamble, class_name, generics_str, open_brace = m.groups()
    # Ignore annotations like @Foo("contains the words class Bar")
    if preamble.count('"') % 2 != 0:
      continue
    # Companion objects are not parsed as classes (unnamed ones do not even
    # match), since @JvmStatic puts their members on the enclosing class.
    if preamble.rstrip().endswith('companion'):
      continue

    if generics_str and generics_str.count('<') != generics_str.count('>'):
      # Regex failed to capture full nested generics.
      generics_str = (parse_common.find_balanced_generics(contents, m.start(3))
                      or generics_str)

    # A bodyless class ends with its header.
    end_idx = m.end()
    if open_brace:
      end_idx = parse_common.find_class_end(contents, m.end(1), class_name,
                                            m.end(0))

    is_outer_class = not parsed_classes
    if is_outer_class:
      java_class = java_types.JavaClass(f'{package}/{class_name}')

      if java_class.name != expected_name:
        raise parse_common.ParseError(
            f'Found class "{class_name}" but expected "{expected_name}".')

      if package_prefix and common.should_prefix_package(
          java_class.package_with_dots, package_prefix_filter):
        java_class = java_class.make_prefixed(package_prefix)
      # Kotlin types are non-null unless marked with "?".
      type_resolver = java_types.TypeResolver(
          java_class,
          null_marked=True,
          package_prefix=package_prefix,
          package_prefix_filter=package_prefix_filter,
          type_catalog=type_catalog,
          enable_safe_pointers=enable_safe_pointers)
      for c in parse_common.parse_imports(contents, m.end()):
        type_resolver.add_import(c)
    elif m.start() >= parsed_classes[0]._end_idx:
      # Another top-level class. These resolve like any other class from the
      # same package, and so need not be registered.
      continue
    else:
      outer_class = parsed_classes[0]
      type_resolver = outer_class.type_resolver
      java_class = type_resolver.java_class.make_nested(class_name)
      if java_class in type_resolver.nested_classes:
        # Class is nested in a method, ignore it.
        continue
      type_resolver = type_resolver.add_child(java_class=java_class)

    # Unlike Java, the class range includes the header, since that is where
    # primary constructors are declared.
    class_keyword_start = m.end(1)
    if generics_str:
      type_resolver.type_params = _parse_type_params(type_resolver,
                                                     generics_str[1:-1])
    parsed_classes.append(
        parse_common.ParsedClass(_start_idx=class_keyword_start,
                                 _end_idx=end_idx,
                                 type_resolver=type_resolver))

  if parsed_classes:
    parsed_classes[0].type_resolver.nested_classes.sort()
  return parsed_classes


def _parse_type(type_resolver, value, boxed=False):
  """Parses a string into a JavaType.

  |boxed| is set when primitives cannot be used, which is the case for type
  arguments (e.g. List<Int> is List<Integer> on the JVM).
  """
  annotations, parsed_value = parse_common.parse_annotations(value)
  parsed_value = parsed_value.strip()
  # E.g. String?, Array<String>?
  nullable = parsed_value.endswith('?')
  if nullable:
    parsed_value = parsed_value[:-1].rstrip()
  # E.g. List<*>, List<in Foo> (like Java's "?" and "? super Foo").
  if parsed_value == '*' or parsed_value.startswith('in '):
    return java_types.OBJECT
  # E.g. List<out Foo> (like Java's "? extends Foo").
  parsed_value = parsed_value.removeprefix('out ')

  java_class = None
  primitive_name = None
  array_dimensions = 0
  generics = None

  if parsed_value in _PRIMITIVE_MAP:
    if nullable or boxed:
      java_class = _BOXED_CLASS_MAP[parsed_value]
    else:
      primitive_name = _PRIMITIVE_MAP[parsed_value]
  elif parsed_value in _PRIMITIVE_ARRAY_MAP:
    primitive_name = _PRIMITIVE_ARRAY_MAP[parsed_value]
    array_dimensions = 1
  elif parsed_value.startswith('Array<') and parsed_value.endswith('>'):
    # Array<Int> is Integer[], whereas IntArray is int[].
    inner = _parse_type(type_resolver,
                        parsed_value[len('Array<'):-1],
                        boxed=True)
    java_class = inner.java_class
    primitive_name = inner.primitive_name
    array_dimensions = inner.array_dimensions + 1
    generics = inner.generics
  else:
    if parsed_value.endswith('>'):
      parsed_value, generics_str = parsed_value[:-1].split('<', 1)
      parsed_value = parsed_value.rstrip()
      generics = tuple(
          _parse_type(type_resolver, g, boxed=True)
          for g in parse_common.split_by_delimiter(generics_str, ','))

    java_class = (_KOTLIN_CLASS_MAP.get(parsed_value)
                  or type_resolver.resolve(parsed_value))

  return parse_common.make_java_type(type_resolver,
                                     value=value,
                                     parsed_value=parsed_value,
                                     java_class=java_class,
                                     primitive_name=primitive_name,
                                     array_dimensions=array_dimensions,
                                     generics=generics,
                                     annotations=annotations,
                                     nullable=nullable)


# E.g. "out P" or "in T : Any". Variance does not affect JVM signatures.
_VARIANCE_REGEX = re.compile(r'^\s*(?:in|out)\s+')


def _parse_type_params(type_resolver, value):
  if not value:
    return java_types.EMPTY_TYPE_PARAM_LIST
  params = []
  # E.g. <T : List<String>, out P>
  for param_str in parse_common.split_by_delimiter(value, ','):
    param_str = _VARIANCE_REGEX.sub('', param_str)
    upper_bound_type = java_types.OBJECT
    if ':' in param_str:
      name, bound_str = param_str.split(':', 1)
      upper_bound_type = _parse_type(type_resolver, bound_str)
    else:
      name = param_str
    params.append(java_types.JavaTypeParam.make(name.strip(), upper_bound_type))

  return java_types.JavaTypeParamList(params)


# E.g. names: @JniType("std::string") String
# "val" and "var" appear in the parameter lists of primary constructors.
_PARAM_REGEX = re.compile(
    r'(?:(?:val|var)\s+)?(?P<name>\w+)\s*:\s*(?P<type>[^=]+)')


def _parse_param_list(type_resolver, value) -> java_types.JavaParamList:
  if not value or value.isspace():
    return java_types.EMPTY_PARAM_LIST
  params = []

  # Split parameter list by commas that are not nested inside generics (e.g.
  # Map<K, V>). Ignores the trailing comma of multi-line parameter lists.
  param_strs = [p for p in parse_common.split_by_delimiter(value, ',') if p]

  for param_str in param_strs:
    m = _PARAM_REGEX.fullmatch(param_str)
    if not m:
      raise parse_common.ParseError(
          f'Could not parse parameter declaration: {param_str}')

    param_type = _parse_type(type_resolver, m.group('type'))
    params.append(java_types.JavaParam(param_type, m.group('name')))

  return java_types.JavaParamList(params)


# Kotlin declarations have no trailing ";", so the parameter list ends at the
# ")" that is followed by an optional return type and then end-of-line.
_PROXY_NATIVE_REGEX = re.compile(
    r'(?P<annotations>(?:@[\w.]+(?:\([^)]*\))?\s*)*)'
    r'\bfun\s+(?P<name>\w+)\s*\((?P<params>[\s\S]*?)\)'
    r'(?:\s*:\s*(?P<return_type>[^\n]+))?'
    r'[ \t]*(?=\n|$)')


def _iter_proxy_methods(type_resolver, interface_body):
  num_methods = 0
  for m in parse_common.find_iter_with_note(_PROXY_NATIVE_REGEX,
                                            interface_body):
    num_methods += 1
    annotations_str, name, params_str, return_type_str = m.groups()
    annotations, _ = parse_common.parse_annotations(annotations_str)
    params = _parse_param_list(type_resolver, params_str)
    return_type = (_parse_type(type_resolver, return_type_str)
                   if return_type_str else java_types.VOID)
    yield name, annotations, params_str, params, return_type
  # The regex does not match all of Kotlin (e.g. generic methods), so ensure
  # that no methods were silently skipped.
  if interface_body.count('fun ') != num_methods:
    raise parse_common.ParseError(
        'Could not parse all methods within @NativeMethod '
        f'interface:\n{interface_body}')


# Like java_parse._CALLED_BY_NATIVE_REGEX, but for functions, constructors
# (which have no name), and property getters (which have no parameter list).
_CALLED_BY_NATIVE_REGEX = re.compile(
    # Annotations before and after @CalledByNative (e.g. @JvmStatic).
    r'(?P<annotations>(?:@[\w.]+(?:\([^)]*\))?\s*)*?'
    r'@CalledByNative(?P<Unchecked>(?:Unchecked)?|ForTesting)\s+'
    r'(?:@[\w.]+(?:\([^)]*\))?\s+)*)'
    r'(?P<modifiers>' + _MODIFIER_KEYWORDS + r')'
    r'(?:(?:fun\s+(?P<name>\w+)|constructor)\s*\((?P<params>[\s\S]*?)\)'
    r'|(?:val|var)\s+(?P<property>\w+)(?=\s*:))'
    # Kotlin declarations have no trailing ";", so the type ends at the start of
    # the body, or at the end of the line. Types are inferred when omitted, so
    # they are required for properties and for expression bodies ("= ...").
    r'(?:\s*:\s*(?P<return_type>[^={\n]+)|[ \t]*(?=[{\n]|$))')


def _property_getter_name(name):
  """Returns the name of the getter that Kotlin generates for a property."""
  # "val isFoo" generates isFoo(), everything else generates getFoo().
  if name.startswith('is') and name[2:3].isupper():
    return name
  return 'get' + common.capitalize(name)


def _parse_called_by_natives(contents,
                             parsed_classes,
                             *,
                             allow_private_called_by_natives=False):
  for match in parse_common.find_iter_with_note(_CALLED_BY_NATIVE_REGEX,
                                                contents):
    is_private = 'private' in match.group('modifiers')
    if is_private and not allow_private_called_by_natives:
      raise parse_common.ParseError(
          f'@CalledByNative methods must not be private. '
          f'Found:\n{match.group(0)}\n')

    # @JvmStatic is what makes a member static on the JVM. Without it, members
    # of an object live on the singleton instance rather than on the class.
    is_static = '@JvmStatic' in match.group('annotations')

    # Companion objects are not parsed as classes, so members of one are
    # attributed to the enclosing class - which is where @JvmStatic puts them.
    parsed_class = parse_common.find_owning_class(parsed_classes, match)
    type_resolver = parsed_class.type_resolver

    return_type_str = match.group('return_type')
    name = match.group('name')
    if match.group('property'):
      name = _property_getter_name(match.group('property'))
    elif not name:
      # What follows the ":" of a constructor is a delegation call or super
      # class rather than a return type.
      return_type_str = None
      name = '<init>'
    return_type = java_types.VOID
    if return_type_str:
      return_type = _parse_type(type_resolver, return_type_str)

    params = _parse_param_list(type_resolver, match.group('params'))
    signature = java_types.JavaSignature.from_params(return_type, params)
    parse_common.check_called_by_native_return_type(name, return_type)
    unchecked = 'Unchecked' in match.group('Unchecked')
    parsed_class.called_by_natives.append(
        parse_common.ParsedCalledByNative(
            name=name,
            signature=signature,
            static=is_static,
            type_params=java_types.EMPTY_TYPE_PARAM_LIST,
            unchecked=unchecked))

  # Check for any @CalledByNative occurrences that were not matched, including
  # ones with unsupported use-site targets (e.g. "@set:CalledByNative").
  unmatched_lines = _CALLED_BY_NATIVE_REGEX.sub('', contents).splitlines()
  for i, line in enumerate(unmatched_lines):
    if re.search(r'@(?:\w+:)?CalledByNative', line):
      context = '\n'.join(unmatched_lines[i:i + 5])
      raise parse_common.ParseError(
          'Could not parse @CalledByNative method signature:\n' + context)


def parse_kotlin_file(filename,
                      contents,
                      *,
                      package_prefix,
                      package_prefix_filter,
                      allow_private_called_by_natives,
                      type_catalog=None,
                      enable_safe_pointers=False):
  # Of the annotation use-site targets, only "@get:" (for property getters) is
  # supported. It is normalized away rather than taught to every regex.
  contents = parse_common.remove_comments(contents).replace('@get:', '@')

  expected_name = os.path.splitext(os.path.basename(filename))[0]
  parsed_classes = _parse_kotlin_classes(
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
      type_resolver, contents, iter_methods=_iter_proxy_methods)
  jni_namespace = parse_common.parse_jni_namespace(contents)

  _parse_called_by_natives(
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
    # Unlike Java, Kotlin declarations are public by default.
    ret.proxy_visibility = 'public'
    ret.proxy_methods = parsed_proxy_natives.methods

  return ret
