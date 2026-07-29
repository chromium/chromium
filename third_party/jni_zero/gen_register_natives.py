# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Entry point for "gen-register-natives" command."""

import fnmatch
import os
import re
import shutil
import subprocess
import zipfile

from codegen import header_common
from codegen import natives_header
from codegen import register_natives
import common
import java_types
import jni_generator
import parse

_CLASS_NAME_RE = re.compile(r'\b(?:class|interface|enum)\s+([\w.$]+)',
                            re.MULTILINE)


def _GenerateLinkerScript(linker_script_path, jni_objs):
  # Use full names rather than globs so that these take precedence over the
  # standard Java_* rule.
  jni_mode = common.JniMode()
  sb = []
  sb.append('{\n')
  sb.append('  local:\n')
  for jni_obj in jni_objs:
    for native in jni_obj.natives:
      sb.append(f'    {native.boundary_name_cpp(jni_mode)};\n')
  sb.append('};\n')

  with common.atomic_output(linker_script_path, mode='w') as f:
    f.write(''.join(sb))


def _CreateHeader(jni_mode, jni_objs, args):
  """Returns the content of the header file."""
  header_guard = os.path.splitext(args.header_path)[0].upper() + '_'
  header_guard = re.sub(r'[/.-]', '_', header_guard)

  user_includes = [f'{args.include_path_prefix}jni_zero_internal.h']
  if args.extra_includes:
    user_includes += args.extra_includes

  preamble, epilogue = header_common.header_preamble(
      jni_generator.GetScriptName(),
      system_includes=['iterator'],  # For std::size().
      user_includes=user_includes,
      header_guard=header_guard)

  sb = common.StringBuilder()
  sb.line(preamble)

  java_classes = [o.java_class for o in jni_objs]

  with sb.section('Class Accessors.'):
    for java_class in java_classes:
      escaped_name = java_class.to_cpp()
      sb(f"""\
static jclass {escaped_name}_clazz(JNIEnv* env) {{
  static const char kClassName[] = "{java_class.full_name}";
  return jni_zero::internal::GetClassInternal(env, kClassName);
}}
""")

  with sb.section('Forward Declarations.'):
    for jni_obj in jni_objs:
      for native in jni_obj.natives:
        with sb.statement():
          natives_header.entry_point_declaration(sb, jni_mode, jni_obj, native)

  with sb.section('Helper Methods.'):
    with sb.namespace(''):
      for jni_obj in jni_objs:
        if jni_obj.non_proxy_natives:
          register_natives.non_proxy_register_function(sb, jni_obj)

  with sb.section('Main Register Function.'):
    register_natives.main_register_function(
        sb,
        jni_objs,
        args.namespace,
        register_natives_name=args.register_natives_name)
  sb(epilogue)
  return sb.to_string()


def _ParseJavap(stdout, default_namespace, jni_objs):
  parts = stdout.split('Compiled from ')
  for part in parts:
    match = _CLASS_NAME_RE.search(part)
    if not match:
      continue
    class_name = match.group(1)

    filename = class_name.replace('.', '/') + '.class'

    try:
      parsed_file = parse.parse_javap_data(filename, part, natives_only=True)
      if parsed_file.outer_class.non_proxy_methods:
        jni_obj = jni_generator.JniObject(parsed_file,
                                          from_javap=True,
                                          default_namespace=default_namespace)
        jni_objs.append(jni_obj)
    except Exception as e:
      print(f"Failed to parse class {class_name}: {e}")
      raise


def GenerateRegisterNativesFromJars(parser, args, jni_mode):
  if not args.javap:
    args.javap = shutil.which('javap')
    if not args.javap:
      parser.error('Could not find "javap" on your PATH. Use --javap to '
                   'specify its location.')

  class_blocklist = None
  if args.class_blocklist:
    class_blocklist = args.class_blocklist.split(',')

  jni_objs = []

  for jar_file in args.jar_files:
    with zipfile.ZipFile(jar_file, 'r') as jar:
      class_files = [
          x for x in jar.namelist()
          if x.endswith('.class') and 'module-info' not in x
      ]

    batch_size = 100
    for i in range(0, len(class_files), batch_size):
      batch = class_files[i:i + batch_size]
      class_names = [x[:-6].replace('/', '.') for x in batch]
      if class_blocklist:
        class_names = [
            x for x in class_names
            if not any(fnmatch.fnmatch(x, f) for f in class_blocklist)
        ]
      if not class_names:
        continue

      cmd = [args.javap, '-private', '-cp', jar_file] + class_names
      res = subprocess.run(cmd, capture_output=True, text=True, check=True)

      _ParseJavap(res.stdout, args.namespace, jni_objs)

  jni_objs.sort(key=lambda o: o.java_class.full_name)

  header_content = _CreateHeader(jni_mode, jni_objs, args)

  with common.atomic_output(args.header_path, mode='w') as f:
    f.write(header_content)

  if args.linker_script_path:
    _GenerateLinkerScript(args.linker_script_path, jni_objs)
