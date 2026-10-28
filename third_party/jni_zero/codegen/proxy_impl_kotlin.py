# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Codegen for FooJni.kt files.

Mirrors proxy_impl_java.py. A Kotlin (rather than Java) proxy is generated for
Kotlin sources because kotlinc cannot resolve the supertypes of a *Java* class
file against the Kotlin sources it is compiling. The owner's "Foo.Natives" is
not yet compiled when FooJni is on the classpath, so a Java FooJni would fail
to load. Kotlin class files are resolved lazily and do not have this problem.
"""

import common
import java_types


class _Context:

  def __init__(self, jni_obj, gen_jni_class, script_name, is_per_file):
    self.jni_obj = jni_obj
    self.gen_jni_class = gen_jni_class
    self.script_name = script_name
    self.is_per_file = is_per_file

    self.interface_name = jni_obj.proxy_interface.name_with_dots
    self.proxy_class = java_types.JavaClass(
        f'{self.jni_obj.java_class.full_name_with_slashes}Jni')
    self.type_resolver = java_types.TypeResolver(self.proxy_class)
    imports = jni_obj.CollectClassesToBeImported() + [
        java_types.JavaClass('javax/annotation/processing/Generated'),
        java_types.JavaClass('org/jni_zero/CheckDiscard'),
        java_types.JavaClass('org/jni_zero/JniTestInstanceHolder'),
    ]
    if any(n.return_type.java_class == java_types.JNI_RAW_PTR_CLASS or any(
        p.java_type.is_safe_pointer() for p in n.params)
           for n in jni_obj.proxy_natives):
      imports.append(java_types.JNI_ZERO_INTERNAL_CLASS)
    if not is_per_file:
      imports.append(gen_jni_class)
    self.type_resolver.imports = imports


def _implicit_array_class_param(native, type_resolver):
  return_type = native.return_type
  class_name = return_type.to_array_element_type().to_kotlin(type_resolver)
  return class_name.rstrip('?') + '::class.java'


def _proxy_method(sb, ctx, native, method_fqn):
  return_type_str = native.return_type.to_kotlin(ctx.type_resolver)
  sig_params = native.params.to_kotlin_declaration(ctx.type_resolver)

  sb(f"""
override fun {native.name}({sig_params})""")
  if not native.return_type.is_void():
    sb(f': {return_type_str}')
  with sb.block():
    # Non-null reference parameters are enforced by the Kotlin type system.
    if native.first_param_cpp_type:
      if not native.params[0].java_type.is_safe_pointer():
        sb(f'assert({native.params[0].name} != 0L)\n')
    is_raw_ptr_ret = (
        native.return_type.java_class == java_types.JNI_RAW_PTR_CLASS)
    if not native.return_type.is_void():
      sb('return ')
    if is_raw_ptr_ret:
      sb('JniZeroInternal.createRawPtr(')
    sb(method_fqn)
    with sb.param_list() as plist:
      for p in native.params:
        if p.java_type.is_safe_pointer():
          plist.append(f'JniZeroInternal.getNativePtr({p.name})')
        else:
          plist.append(p.name)
      if native.needs_implicit_array_element_class_param:
        plist.append(_implicit_array_class_param(native, ctx.type_resolver))
    if is_raw_ptr_ret:
      sb(')')
    # "as Any?" (or casting createRawPtr's JniRawPtr<*>! to JniRawPtr<*>)
    # would be a "redundant cast" warning.
    if (not native.return_type.is_primitive() and not is_raw_ptr_ret
        and return_type_str != 'Any?'):
      sb(f' as {return_type_str}')
    sb('\n')


def _native_method(sb, ctx, native, name):
  sig_params = native.proxy_params.to_kotlin_declaration(ctx.type_resolver)
  return_type = native.proxy_return_type.to_kotlin()
  sb(f'@JvmStatic private external fun {name}({sig_params}): {return_type}\n')


def _class_body(sb, ctx):
  # Like FooJni.java: a class that implements Natives, plus static get() and
  # setInstanceForTesting(). An "object" would also allow FooJni.foo(), which
  # would bypass get() and thus test overrides.
  sb('companion object')
  with sb.block():
    sb(f"""\
private var sOverride: JniTestInstanceHolder? = null

@JvmStatic
fun get(): {ctx.interface_name} {{
  val holder = sOverride
  if (holder != null && holder.value != null) {{
    return holder.value as {ctx.interface_name}
  }}
  return {ctx.proxy_class.name}()
}}

@JvmStatic
fun setInstanceForTesting(impl: {ctx.interface_name}?) {{
  val holder = sOverride ?: JniTestInstanceHolder.create()
  sOverride = holder
  holder.value = impl
}}
""")
    if ctx.is_per_file:
      sb('\n')
      for native in ctx.jni_obj.proxy_natives:
        _native_method(sb, ctx, native, native.per_file_name)

  for native in ctx.jni_obj.proxy_natives:
    if ctx.is_per_file:
      method_fqn = native.per_file_name
    else:
      method_fqn = f'{ctx.gen_jni_class.name}.{native.proxy_name}'
    _proxy_method(sb, ctx, native, method_fqn)


def _imports(sb, ctx):
  classes = set()
  for c in ctx.type_resolver.imports:
    # Kotlin spells these differently and imports them implicitly.
    if c.full_name_with_slashes in java_types.KOTLIN_NAME_BY_CLASS:
      continue
    # The class generated here will go through jarjar and thus we want to avoid
    # prefixes (with the exception of GEN_JNI).
    c = c if c is ctx.gen_jni_class else c.class_without_prefix
    if c.is_nested:
      # We will refer to all nested classes by OuterClass.InnerClass. We do this
      # to reduce risk of naming collisions.
      c = c.get_outer_class()
    classes.add(c.full_name_with_dots)

  for c in sorted(classes):
    sb(f'import {c}\n')


def Generate(jni_mode, jni_obj, *, gen_jni_class, script_name):
  ctx = _Context(jni_obj, gen_jni_class, script_name, jni_mode.is_per_file)

  sb = common.StringBuilder()
  sb(f"""\
//
// This file was generated by {script_name}
//
package {jni_obj.java_class.class_without_prefix.package_with_dots}

""")
  _imports(sb, ctx)
  sb('\n')

  class_name = ctx.proxy_class.name
  if not ctx.is_per_file:
    sb('@CheckDiscard("crbug.com/993421")\n')
  sb('@Generated("JNI Zero")\n')
  sb('@Suppress("UNCHECKED_CAST")\n')
  sb(f'class {class_name} : {ctx.interface_name}')
  with sb.block():
    _class_body(sb, ctx)
  return sb.to_string()
