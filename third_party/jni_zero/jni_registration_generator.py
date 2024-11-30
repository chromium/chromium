# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Entry point for "link" command."""

import collections
import functools
import hashlib
import itertools
import multiprocessing
import os
import pathlib
import pickle
import posixpath
import re
import sys
import zipfile

from codegen import gen_jni_java
from codegen import header_common
from codegen import natives_header
from codegen import register_natives
import common
import java_types
import jni_generator
import parse
import proxy


_THIS_DIR = os.path.dirname(__file__)


def _ParseHelper(package_prefix, package_prefix_filter, path):
  return parse.parse_java_file(path,
                               package_prefix=package_prefix,
                               package_prefix_filter=package_prefix_filter)


def _LoadJniObjs(paths, namespace, package_prefix, package_prefix_filter):
  ret = {}
  if all(p.endswith('.jni.pickle') for p in paths):
    for pickle_path in paths:
      with open(pickle_path, 'rb') as f:
        parsed_files = pickle.load(f)
      ret[pickle_path] = [
          jni_generator.JniObject(pf,
                                  from_javap=False,
                                  default_namespace=namespace)
          for pf in parsed_files
      ]
  else:
    func = functools.partial(_ParseHelper, package_prefix,
                             package_prefix_filter)
    with multiprocessing.Pool() as pool:
      for pf in pool.imap_unordered(func, paths):
        ret[pf.filename] = [
            jni_generator.JniObject(pf,
                                    from_javap=False,
                                    default_namespace=namespace)
        ]

  return ret


def _FilterJniObjs(jni_objs_by_path, include_test_only, module_name):
  for jni_objs in jni_objs_by_path.values():
    # Remove test-only methods.
    if not include_test_only:
      for jni_obj in jni_objs:
        jni_obj.RemoveTestOnlyNatives()
    # Ignoring non-active modules and empty natives lists.
    jni_objs[:] = [
        o for o in jni_objs if o.natives and o.module_name == module_name
    ]


def _Flatten(jni_objs_by_path, paths):
  return itertools.chain(*(jni_objs_by_path[p] for p in paths))


def _CollectNatives(jni_objs, jni_mode):
  ret = []
  for jni_obj in jni_objs:
    ret += jni_obj.proxy_natives
  if jni_mode.is_muxing:
    # Order by simplest to most complex signatures.
    ret.sort(key=lambda n:
             (len(n.proxy_params), not n.return_type.is_void(), n.muxed_name))
  else:
    ret.sort(key=lambda n: (n.java_class, n.name))
  return ret


def _Generate(args,
              jni_mode,
              native_sources,
              java_sources,
              priority_java_sources,
              never_omit_switch_num=False):
  """Generates files required to perform JNI registration.

  Generates a srcjar containing a single class, GEN_JNI, that contains all
  native method declarations.

  Optionally generates a header file that provides RegisterNatives to perform
  JNI registration.

  Args:
    args: argparse.Namespace object.
    jni_mode: JniMode object.
    native_sources: A list of .jni.pickle or .java file paths taken from native
        dependency tree. The source of truth.
    java_sources: A list of .jni.pickle or .java file paths. Used to assert
        against native_sources.
    priority_java_sources: A list of .jni.pickle or .java file paths. Used to
        put these listed java files first in multiplexing.
    never_omit_switch_num: Relevent for multiplexing. Necessary when the
        native library must be interchangeable with another that uses
        priority_java_sources (aka, webview).
  """
  native_sources_set = set(native_sources)
  java_sources_set = set(java_sources)

  jni_objs_by_path = _LoadJniObjs(native_sources_set | java_sources_set,
                                  args.namespace, args.package_prefix,
                                  args.package_prefix_filter)
  _FilterJniObjs(jni_objs_by_path, args.include_test_only, args.module_name)

  present_jni_objs = list(
      _Flatten(jni_objs_by_path, native_sources_set & java_sources_set))

  # Can contain path not in present_jni_objs.
  priority_set = set(priority_java_sources or [])
  # Sort for determinism and to put priority_java_sources first.
  present_jni_objs.sort(
      key=lambda o: (o.filename not in priority_set, o.java_class))

  whole_hash = None
  priority_hash = None
  muxed_aliases_by_sig = None
  if jni_mode.is_muxing:
    whole_hash, priority_hash = _GenerateHashes(
        present_jni_objs,
        priority_set,
        never_omit_switch_num=never_omit_switch_num)
    muxed_aliases_by_sig = proxy.populate_muxed_switch_num(
        present_jni_objs, never_omit_switch_num=never_omit_switch_num)

  java_only_jni_objs = _CheckForJavaNativeMismatch(args, jni_objs_by_path,
                                                   native_sources_set,
                                                   java_sources_set)

  present_proxy_natives = _CollectNatives(present_jni_objs, jni_mode)
  absent_proxy_natives = _CollectNatives(java_only_jni_objs, jni_mode)
  boundary_proxy_natives = present_proxy_natives
  if jni_mode.is_muxing:
    boundary_proxy_natives = [
        n for n in present_proxy_natives if n.muxed_switch_num <= 0
    ]

  short_gen_jni_class = proxy.get_gen_jni_class(
      short=True,
      name_prefix=args.module_name,
      package_prefix=args.package_prefix,
      package_prefix_filter=args.package_prefix_filter)
  full_gen_jni_class = proxy.get_gen_jni_class(
      short=False,
      name_prefix=args.module_name,
      package_prefix=args.package_prefix,
      package_prefix_filter=args.package_prefix_filter)

  if args.header_path:
    if jni_mode.is_hashing or jni_mode.is_muxing:
      gen_jni_class = short_gen_jni_class
    else:
      gen_jni_class = full_gen_jni_class
    header_content = _CreateHeader(jni_mode, present_jni_objs,
                                   boundary_proxy_natives, gen_jni_class, args,
                                   muxed_aliases_by_sig, whole_hash,
                                   priority_hash)
    with common.atomic_output(args.header_path, mode='w') as f:
      f.write(header_content)

  with common.atomic_output(args.srcjar_path) as f:
    with zipfile.ZipFile(f, 'w') as srcjar:
      script_name = jni_generator.GetScriptName()
      if jni_mode.is_hashing or jni_mode.is_muxing:
        # org/jni_zero/GEN_JNI.java
        common.add_to_zip_hermetic(
            srcjar,
            f'{full_gen_jni_class.full_name_with_slashes}.java',
            data=gen_jni_java.generate_forwarding(jni_mode, script_name,
                                                  full_gen_jni_class,
                                                  short_gen_jni_class,
                                                  present_proxy_natives,
                                                  absent_proxy_natives))
        # J/N.java
        common.add_to_zip_hermetic(
            srcjar,
            f'{short_gen_jni_class.full_name_with_slashes}.java',
            data=gen_jni_java.generate_impl(jni_mode,
                                            script_name,
                                            short_gen_jni_class,
                                            boundary_proxy_natives,
                                            absent_proxy_natives,
                                            whole_hash=whole_hash,
                                            priority_hash=priority_hash))
      else:
        # org/jni_zero/GEN_JNI.java
        common.add_to_zip_hermetic(
            srcjar,
            f'{full_gen_jni_class.full_name_with_slashes}.java',
            data=gen_jni_java.generate_impl(jni_mode, script_name,
                                            full_gen_jni_class,
                                            boundary_proxy_natives,
                                            absent_proxy_natives))


def _CheckForJavaNativeMismatch(args, jni_objs_by_path, native_sources_set,
                                java_sources_set):
  native_only = native_sources_set - java_sources_set
  java_only = java_sources_set - native_sources_set

  java_only_jni_objs = sorted(_Flatten(jni_objs_by_path, java_only),
                              key=lambda jni_obj: jni_obj.filename)
  native_only_jni_objs = sorted(_Flatten(jni_objs_by_path, native_only),
                                key=lambda jni_obj: jni_obj.filename)
  failed = False
  if not args.add_stubs_for_missing_native and java_only_jni_objs:
    failed = True
    warning_message = '''Failed JNI assertion!
We reference Java files which use JNI, but our native library does not depend on
the corresponding generate_jni().
To bypass this check, add stubs to Java with --add-stubs-for-missing-jni.
Excess Java files:
'''
    sys.stderr.write(warning_message)
    sys.stderr.write('\n'.join(o.filename for o in java_only_jni_objs))
    sys.stderr.write('\n')
  if not args.remove_uncalled_methods and native_only_jni_objs:
    failed = True
    warning_message = '''Failed JNI assertion!
Our native library depends on generate_jnis which reference Java files that we
do not include in our final dex.
To bypass this check, delete these extra methods with --remove-uncalled-jni.
Unneeded Java files:
'''
    sys.stderr.write(warning_message)
    sys.stderr.write('\n'.join(o.filename for o in native_only_jni_objs))
    sys.stderr.write('\n')
  if failed:
    sys.exit(1)

  return java_only_jni_objs


def _GenerateHashes(jni_objs, priority_set, *, never_omit_switch_num):
  # We assume that if we have identical files and they are in the same order, we
  # will have switch number alignment. We do this, instead of directly
  # inspecting the switch numbers, because differentiating the priority sources
  # is a big pain.
  whole_hash = hashlib.md5()
  priority_hash = hashlib.md5()
  had_priority = False

  for i, jni_obj in enumerate(jni_objs):
    path = os.path.relpath(jni_obj.filename, start=_THIS_DIR)
    encoded = path.encode('utf-8')
    whole_hash.update(encoded)
    if jni_obj.filename in priority_set:
      had_priority = True
      priority_hash.update(encoded)

  uint64_t_max = (1 << 64) - 1
  int64_t_min = -(1 << 63)
  to_int64 = lambda h: (int(h.hexdigest(), 16) % uint64_t_max) + int64_t_min
  whole_ret = to_int64(whole_hash)

  # Make it clear when there are no priority items.
  priority_ret = to_int64(priority_hash) if had_priority else 0
  if never_omit_switch_num:
    priority_ret ^= 1
    whole_ret ^= 1
  return whole_ret, priority_ret


def _CreateHeader(jni_mode, jni_objs, boundary_proxy_natives, gen_jni_class,
                  args, muxed_aliases_by_sig, whole_hash, priority_hash):
  """Returns the content of the header file."""
  header_guard = os.path.splitext(args.header_path)[0].upper() + '_'
  header_guard = re.sub(r'[/.-]', '_', header_guard)

  preamble, epilogue = header_common.header_preamble(
      jni_generator.GetScriptName(),
      gen_jni_class,
      system_includes=['iterator'],  # For std::size().
      user_includes=['third_party/jni_zero/jni_zero_internal.h'],
      header_guard=header_guard)

  module_name = args.module_name or ''

  sb = common.StringBuilder()
  sb.line(preamble)
  if jni_mode.is_muxing:
    sb(f"""\
extern const int64_t kJniZeroHash{module_name}Whole = {whole_hash}LL;
extern const int64_t kJniZeroHash{module_name}Priority = {priority_hash}LL;
""")

  non_proxy_natives_java_classes = [
      o.java_class for o in jni_objs if o.non_proxy_natives
  ]
  non_proxy_natives_java_classes.sort()

  if non_proxy_natives_java_classes:
    with sb.section('Class Accessors.'):
      header_common.class_accessors(sb, non_proxy_natives_java_classes,
                                    module_name)

  with sb.section('Forward Declarations.'):
    for jni_obj in jni_objs:
      for native in jni_obj.natives:
        with sb.statement():
          natives_header.entry_point_declaration(sb, jni_mode, jni_obj, native,
                                                 gen_jni_class)

  if jni_mode.is_muxing and boundary_proxy_natives:
    with sb.section('Multiplexing Methods.'):
      for native in boundary_proxy_natives:
        muxed_aliases = muxed_aliases_by_sig[native.muxed_signature]
        natives_header.multiplexing_boundary_method(sb, muxed_aliases,
                                                    gen_jni_class)

  if args.manual_jni_registration:
    # Helper methods use presence of gen_jni_class to denote presense of proxy
    # methods.
    if not boundary_proxy_natives:
      gen_jni_class = None

    with sb.section('Helper Methods.'):
      with sb.namespace(''):
        if boundary_proxy_natives:
          register_natives.gen_jni_register_function(sb, jni_mode,
                                                     gen_jni_class,
                                                     boundary_proxy_natives)

        for jni_obj in jni_objs:
          if jni_obj.non_proxy_natives:
            register_natives.non_proxy_register_function(sb, jni_obj)

    with sb.section('Main Register Function.'):
      register_natives.main_register_function(sb, jni_objs, args.namespace,
                                              gen_jni_class)
  sb(epilogue)
  return sb.to_string()


def _ParseSourceList(path):
  # Path can have duplicates.
  with open(path) as f:
    return sorted(set(f.read().splitlines()))


def _write_depfile(depfile_path, first_gn_output, inputs):
  def _process_path(path):
    assert not os.path.isabs(path), f'Found abs path in depfile: {path}'
    if os.path.sep != posixpath.sep:
      path = str(pathlib.Path(path).as_posix())
    assert '\\' not in path, f'Found \\ in depfile: {path}'
    return path.replace(' ', '\\ ')

  sb = []
  sb.append(_process_path(first_gn_output))
  if inputs:
    # Sort and uniquify to ensure file is hermetic.
    # One path per line to keep it human readable.
    sb.append(': \\\n ')
    sb.append(' \\\n '.join(sorted(_process_path(p) for p in set(inputs))))
  else:
    sb.append(': ')
  sb.append('\n')

  pathlib.Path(depfile_path).write_text(''.join(sb))


def main(parser, args, jni_mode):
  if not args.header_path and args.manual_jni_registration:
    parser.error('--manual-jni-registration requires --header-path.')
  if not args.header_path and jni_mode.is_muxing:
    parser.error('--enable-jni-multiplexing requires --header-path.')
  if args.remove_uncalled_methods and not args.native_sources_file:
    parser.error('--remove-uncalled-methods requires --native-sources-file.')
  if args.priority_java_sources_file:
    if not jni_mode.is_muxing:
      parser.error('--priority-java-sources is only for multiplexing.')
    if not args.never_omit_switch_num:
      # We could arguably just set this rather than error out, but it's
      # important that the flag also be set on the library that does not set
      # --priority-java-sources.
      parser.error('--priority-java-sources requires --never-omit-switch-num.')

  java_sources = _ParseSourceList(args.java_sources_file)
  if args.native_sources_file:
    native_sources = _ParseSourceList(args.native_sources_file)
  else:
    if args.add_stubs_for_missing_native:
      # This will create a fully stubbed out GEN_JNI.
      native_sources = []
    else:
      # Just treating it like we have perfect alignment between native and java
      # when only looking at java.
      native_sources = java_sources
  if args.priority_java_sources_file:
    priority_java_sources = _ParseSourceList(args.priority_java_sources_file)
  else:
    priority_java_sources = None

  _Generate(args,
            jni_mode,
            native_sources,
            java_sources,
            priority_java_sources,
            never_omit_switch_num=args.never_omit_switch_num)

  if args.depfile:
    # GN does not declare a dep on the sources files to avoid circular
    # dependencies, so they need to be listed here.
    all_inputs = native_sources + java_sources + [args.java_sources_file]
    if args.native_sources_file:
      all_inputs.append(args.native_sources_file)
    _write_depfile(args.depfile, args.srcjar_path, all_inputs)
