#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import parse
import pprint

import common
import java_types


def _parsed_file_to_string(parsed_file):
  sb = common.StringBuilder()
  classes = list(parsed_file.classes_with_jni)
  if parsed_file.outer_class not in classes:
    classes.append(parsed_file.outer_class)
    classes.sort()

  for parsed_class in classes:
    type_resolver = parsed_class.type_resolver
    to_java = lambda t: t.to_java(
        type_resolver=type_resolver, with_generics=True, with_annotations=True)
    to_generics = lambda t: t.type_params.to_java(type_resolver)
    param_to_str = lambda p: f'{to_java(p.java_type)} {p.name}'

    generics = to_generics(type_resolver)
    sb(f'public class {type_resolver.java_class.name_with_dots}{generics} {{\n')
    for cbn in parsed_class.called_by_natives:
      sb('  public')
      if cbn.static:
        sb(' static')
      generics = to_generics(cbn)
      if generics:
        sb(f' {generics}')
      sb(f' {to_java(cbn.signature.return_type)} {cbn.name}(')

      sb(', '.join(param_to_str(p) for p in cbn.signature.param_list))
      sb(');\n')

    for field in parsed_class.fields:
      sb('  public ')
      if field.static:
        sb('static ')
      if field.final:
        sb('final ')
      sb(f'{to_java(field.java_type)} {field.name}')
      if field.const_value:
        sb(f' = {field.const_value}')
      sb(';\n')
    sb('}\n')

  for n in parsed_file.proxy_methods:
    sb(f'native {to_java(n.signature.return_type)} {n.name}(')
    sb(', '.join(param_to_str(p) for p in n.signature.param_list))
    sb(');\n')
  return sb.to_string()


def _parse_java_file_data(filename, contents):
  return parse.parse_java_file_data(filename,
                                    contents,
                                    package_prefix=None,
                                    package_prefix_filter=None,
                                    enable_legacy_natives=False,
                                    allow_private_called_by_natives=False)


class TestParse(unittest.TestCase):

  def _assert_golden(self, expected, actual):
    actual = _parsed_file_to_string(actual)
    if expected != actual:
      raise AssertionError('Actual was:\n' + actual)

  def testSplitByDelimiter(self):
    # Test basic split by comma
    self.assertEqual(parse._split_by_delimiter('a, b, c', ','), ['a', 'b', 'c'])
    # Test split with generics
    self.assertEqual(parse._split_by_delimiter('a<b, c>, d', ','),
                     ['a<b, c>', 'd'])
    # Test split with nested generics
    self.assertEqual(
        parse._split_by_delimiter('Map<String, Map<String, String>>, int', ','),
        ['Map<String, Map<String, String>>', 'int'])
    # Test empty cases
    self.assertEqual(parse._split_by_delimiter('', ','), [])
    self.assertEqual(parse._split_by_delimiter('   ', ','), [''])
    self.assertEqual(parse._split_by_delimiter('a, b,', ','), ['a', 'b', ''])
    # Test split by space
    self.assertEqual(parse._split_by_delimiter('List<String> arg0', ' '),
                     ['List<String>', 'arg0'])
    self.assertEqual(
        parse._split_by_delimiter('Map<? super String, ? extends String> arg0',
                                  ' '),
        ['Map<? super String, ? extends String>', 'arg0'])
    # Test complex cases
    self.assertEqual(
        parse._split_by_delimiter('Callback<List<String>> c, int x', ','),
        ['Callback<List<String>> c', 'int x'])
    # Test edge case: no delimiters
    self.assertEqual(parse._split_by_delimiter('a<b>', ','), ['a<b>'])
    # Test fast path (no generics)
    self.assertEqual(parse._split_by_delimiter('x,y,z', ','), ['x', 'y', 'z'])

  def testParseJavaClassesNestedGenericsWithNewlines(self):
    contents = """
package org.jni_zero;
import java.util.List;
public class MyClass<
  T extends List<String>,
  P extends List<String>> {
}
"""
    expected = """\
public class MyClass<T extends List<String>, P extends List<String>> {
}
"""
    parsed_file = _parse_java_file_data('MyClass.java', contents)
    self._assert_golden(expected, parsed_file)

  def testParseSampleProxyEdgeCases(self):
    contents = """
package org.jni_zero;
@SomeAnnotation("that contains class Foo ")
class SampleProxyEdgeCases<E extends Enum<E>> {
   void method1() {
     class Foo {}
   }
   void method2() {
     class Foo {}
   }
}
"""
    expected = """\
public class SampleProxyEdgeCases<E extends Enum<E>> {
}
"""
    parsed_file = _parse_java_file_data('SampleProxyEdgeCases.java', contents)
    self._assert_golden(expected, parsed_file)

  def testParseCallback2(self):
    contents = """
package org.chromium.base;
@NullMarked
public interface Callback2<T1 extends @Nullable Object, T2 extends @Nullable Object> {
    void onResult(T1 result1, T2 result2);
    abstract class JniHelper {
        @CalledByNative
        static void onResultFromNative(Callback2<T1, T2> callback, T1 r1, T2 r2) {
            callback.onResult(r1, r2);
        }
    }
}
"""
    expected = """\
public class Callback2<T1, T2> {
}
public class Callback2.JniHelper {
  public static void onResultFromNative(Callback2<T1, T2> callback, T1 r1, T2 r2);
}
"""
    parsed_file = _parse_java_file_data('Callback2.java', contents)
    self._assert_golden(expected, parsed_file)

  def testParseWithAnnotationsAndModifiers(self):
    contents = """
package org.jni_zero;
@Foo
@NullMarked
@Bar("baz")
public final class AnnotatedClass {
  @CalledByNative
  @Contract("_, !null -> !null")
  public @Nullable @JniType("std::string") String cbn() {
  }

  @NativeMethods
  interface Natives {
    List<@JniType("vec<vec<int>>") String> foo(@JniType("vec<vec<int>>") String bar);
    Outer.@Nullable Inner bar(@Nullable @JniType("string") String arg);
  }
}
"""
    expected = """\
public class AnnotatedClass {
  public @JniType("std::string") @Nullable String cbn();
}
native @Nullable Outer.Inner bar(@JniType("string") @Nullable String arg);
native List<String> foo(@JniType("vec<vec<int>>") String bar);
"""
    parsed_file = _parse_java_file_data('AnnotatedClass.java', contents)
    self._assert_golden(expected, parsed_file)

  def testParseMethodWithGenerics(self):
    contents = """
package org.jni_zero;
import java.util.Map;
public class Test {
    @CalledByNative
    public static Map<String, int[][]> foo() {
        return null;
    }
}
"""
    expected = """\
public class Test {
  public static @Nullable Map<String, int[][]> foo();
}
"""
    parsed_file = _parse_java_file_data('Test.java', contents)
    self._assert_golden(expected, parsed_file)

  def testParseNestedClassWithGenerics(self):
    contents = """
package org.jni_zero;
public class Outer<T1> {
    public static class Inner<T2> {
        @CalledByNative("Inner")
        public T2 foo(T1 arg) {}
    }
}
"""
    expected = """\
public class Outer<T1> {
}
public class Outer.Inner<T2> {
  public @Nullable T2 foo(@Nullable T1 arg);
}
"""
    parsed_file = _parse_java_file_data('Outer.java', contents)
    self._assert_golden(expected, parsed_file)

  def testParseJavapWithComplexGenerics(self):
    contents = """
@NullMarked
public class org.jni_zero.Complex<T extends java.util.List<java.lang.String>> {
  public <U extends java.lang.Runnable> T complexMethod(java.util.Map<java.lang.String, java.util.List<U>> arg);
  public <T2 extends java.lang.Object> org.jni_zero.Complex(T2);
}
"""
    expected = """\
public class Complex<T extends java.util.List<String>> {
  public <T2> void <init>(T2 p0);
  public <U extends Runnable> T complexMethod(java.util.Map<String, java.util.List<U>> arg);
}
"""
    parsed_file = parse.parse_javap_data('Complex.class', contents)
    self._assert_golden(expected, parsed_file)

  def testParseJavapWithNestedGenericsAndDuplicateReturnTypes(self):
    contents = """
Compiled from "List.java"
public interface java.util.List<E> extends java.util.SequencedCollection<E> {
  public abstract <T> T[] toArray(T[]);
  public default void sort(java.util.Comparator<? super E>);
  public static <E> java.util.List<E> of(E...);
  public static <E> java.util.List<E> copyOf(java.util.Collection<? extends E>);
  public default List<E> reversed();
  public default java.util.SequencedCollection reversed();
  public <T extends java.lang.Comparable<? super T>> void method2(T[]);
}
"""
    expected = """\
public class List<E> {
  public static <E> @Nullable List<E> copyOf(@Nullable Collection<E> p0);
  public <T extends Comparable<Object>> void method2(@Nullable T[] p0);
  public static <E> @Nullable List<E> of(@Nullable E[] p0);
  public @Nullable List<E> reversed();
  public void sort(@Nullable Comparator<Object> p0);
  public <T> @Nullable T[] toArray(@Nullable T[] p0);
}
"""
    parsed_file = parse.parse_javap_data('List.class', contents)
    self._assert_golden(expected, parsed_file)

  def testParseJavapWithMap(self):
    contents = """
public interface java.util.Map<K, V> {
  public <K, V> java.util.Map<K, V> entrySet();
}
"""
    expected = """\
public class Map<K, V> {
  public <K, V> @Nullable Map<K, V> entrySet();
}
"""
    parsed_file = parse.parse_javap_data('Map.class', contents)
    self._assert_golden(expected, parsed_file)


if __name__ == '__main__':
  unittest.main()
