# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
#
# LOCAL SETUP:
# Make sure you have bazel installed first.
#  For Debian like environments:
#    `sudo apt-get install bazel`
#  For others see:
#    https://bazel.build/start
# This script assumes a x86-64 execution environment.
#
# WHAT:
# This script creates the BUILD.gn file for XNNPACK by reverse-engineering the
# bazel build.

# HOW:
# By using the bazel aquery command, the script extracts source files and
# flags from CppCompile actions for each configured architecture. These are
# then put into a configuration that gn will accept. The source_set's are kept
# separate for each architecture, with the "xnnpack" source_set selecting the
# correct dependencies based on "current_cpu".
#
# WHY:
# The biggest difficulty of this process is that gn expects each source's
# basename to be unique within a single source set. For example, including both
# "bar/foo.c" and "baz/foo.c" in the same source set is illegal. Therefore, each
# source set will only contain sources from a single directory.
# However, some sources within the same directory may need different compiler
# flags set, so source sets are further split by their flags.

import atexit
import collections
import json
import logging
import os
import platform
import shutil
import subprocess
import sys

_HEADER = '''
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#
#     THIS FILE IS AUTO-GENERATED. DO NOT EDIT.
#
#     See //third_party/xnnpack/generate_build_gn.py
#
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

import("//build/config/android/config.gni")

config("xnnpack_config") {
  include_dirs = [
    "//third_party/pthreadpool/src/include",
    "src/deps/clog/include",
    "src/include",
    "src/src",
  ]

  cflags=[
    "-Wno-unused-function",
    "-Wno-deprecated-comma-subscript",
  ]

  defines = [
    # Don't enable this without first talking to Chrome Security!
    # XNNPACK runs in the browser process. The hardening and fuzzing needed
    # to ensure JIT can be used safely is not in place yet.
    "XNN_ENABLE_JIT=0",

    "XNN_ENABLE_ASSEMBLY=1",
    "XNN_ENABLE_GEMM_M_SPECIALIZATION=1",
    "XNN_ENABLE_MEMOPT=1",
    "XNN_ENABLE_CPUINFO=1",
    "XNN_ENABLE_SPARSE=1",
    "XNN_LOG_LEVEL=0",
    "XNN_LOG_TO_STDIO=0",
  ]

  if (current_cpu == "arm64") {
    defines += [
      "XNN_ENABLE_ARM_DOTPROD=1",
      "XNN_ENABLE_ARM_I8MM=1",
    ]
  }
}
'''.strip()

_MAIN_TMPL = '''
source_set("xnnpack") {
  public = [ "src/include/xnnpack.h" ]

  configs -= [ "//build/config/compiler:chromium_code" ]
  configs += [ "//build/config/compiler:no_chromium_code" ]
  configs += [ "//build/config/sanitizers:cfi_icall_generalize_pointers" ]

  sources = [
%SRCS%
  ]

  deps = xnnpack_deps + [
    "//third_party/cpuinfo",
    "//third_party/fp16",
    "//third_party/fxdiv",
    "//third_party/pthreadpool",
  ]

  public_configs = [ ":xnnpack_config" ]
}

# This is a target that cannot depend on //base.
source_set("xnnpack_standalone") {
  public = [ "src/include/xnnpack.h" ]

  configs -= [ "//build/config/compiler:chromium_code" ]
  configs += [ "//build/config/compiler:no_chromium_code" ]
  configs += [ "//build/config/sanitizers:cfi_icall_generalize_pointers" ]

  sources = [
%SRCS%
  ]

  deps = xnnpack_standalone_deps + [
    "//third_party/cpuinfo",
    "//third_party/fp16",
    "//third_party/fxdiv",
    "//third_party/pthreadpool:pthreadpool_standalone",
  ]

  public_configs = [ ":xnnpack_config" ]

  if (!(is_android && use_order_profiling)) {
    assert_no_deps = [ "//base" ]
  }
}
'''.strip()

_TARGET_TMPL = '''
source_set("%TARGET_NAME%") {
  cflags = [
%CFLAGS%
  ]
%ASMFLAGS%
  sources = [
%SRCS%
  ]

  configs -= [ "//build/config/compiler:chromium_code" ]
  configs += [ "//build/config/compiler:no_chromium_code" ]
  configs += [ "//build/config/sanitizers:cfi_icall_generalize_pointers" ]

  deps = [
    "//third_party/cpuinfo",
    "//third_party/fp16",
    "//third_party/fxdiv",
    "//third_party/pthreadpool",
  ]

  public_configs = [ ":xnnpack_config" ]
}

# This is a target that cannot depend on //base.
source_set("%TARGET_NAME%_standalone") {
  cflags = [
%CFLAGS%
  ]
%ASMFLAGS%
  sources = [
%SRCS%
  ]

  configs -= [ "//build/config/compiler:chromium_code" ]
  configs += [ "//build/config/compiler:no_chromium_code" ]
  configs += [ "//build/config/sanitizers:cfi_icall_generalize_pointers" ]

  deps = [
    "//third_party/cpuinfo",
    "//third_party/fp16",
    "//third_party/fxdiv",
    "//third_party/pthreadpool:pthreadpool_standalone",
  ]

  public_configs = [ ":xnnpack_config" ]

  if (!(is_android && use_order_profiling)) {
    assert_no_deps = [ "//base" ]
  }
}
'''.strip()

# This is a minimized version of
# https://bazel.build/tutorials/ccp-toolchain-config with cc_toolchain_config()
# definitions for aarch64-linux-gnu and x86_64-linux-gnu. It will not
# successfully build tflite (it's missing default_linker_flags and
# cxx_builtin_include_directories), but will allow the compile actions to be
# queried.
# Should XNNPACK be updated to use @platforms:cpu additional platform() and
# toolchain() definitions will be needed. See:
#   https://bazel.build/extending/platforms
#   https://bazel.build/extending/toolchains
_AARCH64_LINUX_GCC = "/usr/bin/aarch64-linux-gnu-gcc"
_AARCH64_LINUX_LD = "/usr/bin/aarch64-linux-gnu-ld"
_X86_64_LINUX_GCC = "/usr/bin/x86_64-linux-gnu-gcc"
_X86_64_LINUX_LD = "/usr/bin/x86_64-linux-gnu-ld"

_TOOLCHAIN_BUILD = '''
load(":cc_toolchain_config.bzl", "cc_toolchain_config")

package(default_visibility = ["//visibility:public"])

cc_toolchain_suite(
    name = "cc_suite",
    toolchains = {
        "k8": ":linux_k8_toolchain",
        "aarch64": ":linux_aarch64_toolchain",
    },
)

filegroup(name = "empty")

cc_toolchain(
    name = "linux_k8_toolchain",
    toolchain_identifier = "linux-k8-toolchain",
    toolchain_config = ":linux_k8_toolchain_config",
    all_files = ":empty",
    compiler_files = ":empty",
    dwp_files = ":empty",
    linker_files = ":empty",
    objcopy_files = ":empty",
    strip_files = ":empty",
    supports_param_files = 0,
)

cc_toolchain_config(name = "linux_k8_toolchain_config")

cc_toolchain(
    name = "linux_aarch64_toolchain",
    toolchain_identifier = "linux-aarch64-toolchain",
    toolchain_config = ":linux_aarch64_toolchain_config",
    all_files = ":empty",
    compiler_files = ":empty",
    dwp_files = ":empty",
    linker_files = ":empty",
    objcopy_files = ":empty",
    strip_files = ":empty",
    supports_param_files = 0,
)

cc_toolchain_config(name = "linux_aarch64_toolchain_config")
'''.strip()

_CC_TOOLCHAIN_CONFIG_BZL = f'''
load("@bazel_tools//tools/build_defs/cc:action_names.bzl", "ACTION_NAMES")
load(
    "@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl",
    "feature",
    "flag_group",
    "flag_set",
    "tool_path",
)

all_link_actions = [
    ACTION_NAMES.cpp_link_executable,
    ACTION_NAMES.cpp_link_dynamic_library,
    ACTION_NAMES.cpp_link_nodeps_dynamic_library,
]

all_compile_actions = [
    ACTION_NAMES.assemble,
    ACTION_NAMES.c_compile,
    ACTION_NAMES.clif_match,
    ACTION_NAMES.cpp_compile,
    ACTION_NAMES.cpp_header_parsing,
    ACTION_NAMES.cpp_module_codegen,
    ACTION_NAMES.cpp_module_compile,
    ACTION_NAMES.linkstamp_compile,
    ACTION_NAMES.lto_backend,
    ACTION_NAMES.preprocess_assemble,
]

def _impl(ctx):
    if ctx.label.name == "linux_aarch64_toolchain_config":
        cpu = "aarch64"
        gcc = "{_AARCH64_LINUX_GCC}"
        ld = "{_AARCH64_LINUX_LD}"
    else:
        cpu = "k8"
        gcc = "{_X86_64_LINUX_GCC}"
        ld = "{_X86_64_LINUX_LD}"

    tool_paths = [
        tool_path(
            name = "gcc",
            path = gcc,
        ),
        tool_path(
            name = "ld",
            path = ld,
        ),
        tool_path(
            name = "ar",
            path = "/bin/false",
        ),
        tool_path(
            name = "cpp",
            path = "/bin/false",
        ),
        tool_path(
            name = "nm",
            path = "/bin/false",
        ),
        tool_path(
            name = "objdump",
            path = "/bin/false",
        ),
        tool_path(
            name = "strip",
            path = "/bin/false",
        ),
    ]

    return cc_common.create_cc_toolchain_config_info(
        ctx = ctx,
        features = [],
        cxx_builtin_include_directories = [],
        toolchain_identifier = "local",
        host_system_name = "local",
        target_system_name = "local",
        target_cpu = cpu,
        target_libc = "unknown",
        compiler = "gcc",
        abi_version = "unknown",
        abi_libc_version = "unknown",
        tool_paths = tool_paths,
    )

cc_toolchain_config = rule(
    implementation = _impl,
    attrs = {{}},
    provides = [CcToolchainConfigInfo],
)

'''.strip()

# A SourceSet corresponds to a single source_set() gn tuple.
SourceSet = collections.namedtuple('SourceSet', ['dir', 'srcs', 'args'],
                                   defaults=['', [], []])


def NameForSourceSet(source_set, arch):
  """
  Returns the name to use for a SourceSet in the gn target.
  """
  if source_set.dir == 'xnnpack':
    return 'xnnpack'
  if not source_set.args:
    # Note this creates some target redundancy when the source set is the same
    # between architectures.
    return f'{source_set.dir}_{arch}'
  return '{dir}_{args}'.format(
      **{
          'dir': source_set.dir,
          'args': '-'.join([arg[2:] for arg in source_set.args]),
      })


# An ObjectBuild corresponds to a single built object, which is parsed from a
# single bazel compiler invocation on a single source file.
ObjectBuild = collections.namedtuple('ObjectBuild', ['src', 'dir', 'args'],
                                     defaults=['', '', []])


def _objectbuild_from_bazel_log(action):
  """
  Attempts to scrape a compiler invocation from a single bazel build output
  line. If no invocation is present, None is returned.
  """
  action_args = action["arguments"]

  src = ''
  dir = ''
  args = []
  for i, arg in enumerate(action_args):
    if arg == '-c' and action_args[i + 1].startswith("external/XNNPACK/"):
      src = os.path.join('src', action_args[i + 1][len("external/XNNPACK/"):])
      # |src| should look like 'src/src/...'.
      src_path = src.split('/')
      if len(src_path) == 3:
        dir = 'xnnpack'
      else:
        dir = src_path[2]
    if arg.startswith('-m'):
      args.append(arg)
  return ObjectBuild(src=src, dir=dir, args=args)


def _xnnpack_dir():
  """
  Returns the absolute path of //third_party/xnnpack/.
  """
  return os.path.dirname(os.path.realpath(__file__))


def _tflite_dir():
  """
  Returns the absolute path of //third_party/tflite/src/tensorflow/lite/.
  """
  tp_dir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
  return os.path.join(tp_dir, "tflite", "src", "tensorflow", "lite")


_TOOLCHAIN_DIR = os.path.join(_tflite_dir(),
                              "xnnpack-generate_build_gn-toolchain")


def _cleanup():
  shutil.rmtree(_TOOLCHAIN_DIR)


def CreateToolchainFiles():
  logging.info(f"Creating temporary toolchain files in '{_TOOLCHAIN_DIR}'")
  try:
    os.mkdir(_TOOLCHAIN_DIR)
  except FileExistsError:
    pass
  atexit.register(_cleanup)

  build_path = os.path.join(_TOOLCHAIN_DIR, 'BUILD')
  with open(build_path, 'w') as f:
    f.write(_TOOLCHAIN_BUILD)
    f.write('\n')

  cc_toolchain_config_bzl_path = os.path.join(_TOOLCHAIN_DIR,
                                              'cc_toolchain_config.bzl')
  with open(cc_toolchain_config_bzl_path, 'w') as f:
    f.write(_CC_TOOLCHAIN_CONFIG_BZL)
    f.write('\n')


def _run_bazel_cmd(args):
  """
  Runs a bazel command in the form of bazel <args...>. Returns the stdout,
  raising an Exception if the command failed.
  """
  exec_path = shutil.which("bazel")
  if not exec_path:
    raise Exception(
        "bazel is not installed. Please run `sudo apt-get install " +
        "bazel` or put the bazel executable in $PATH")
  cmd = [exec_path]
  cmd.extend(args)
  proc = subprocess.Popen(cmd,
                          text=True,
                          cwd=_tflite_dir(),
                          stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE)
  stdout, stderr = proc.communicate()
  if proc.returncode != 0:
    raise Exception("bazel command returned non-zero return code:\n"
                    "cmd: {cmd}\n"
                    "status: {status}\n"
                    "stdout: {stdout}\n"
                    "stderr: {stderr}".format(
                        **{
                            'cmd': str(cmd),
                            'status': proc.returncode,
                            'stdout': stdout,
                            'stderr': stderr,
                        }))
  return stdout


def GenerateObjectBuilds(cpu):
  """
  Queries bazel for the compile commands needed for the XNNPACK source files
  necessary to fulfill the :tensorflowlite target's dependencies for the given
  cpu.

  Args:
    cpu: aarch64 or k8
  """
  logging.info(f'Querying xnnpack compile commands for {cpu} with bazel...')
  basename = os.path.basename(_TOOLCHAIN_DIR)
  crosstool_top = f'//tensorflow/lite/{basename}:cc_suite'
  logs = _run_bazel_cmd([
      'aquery',
      f'--crosstool_top={crosstool_top}',
      '--host_crosstool_top=@bazel_tools//tools/cpp:toolchain',
      f'--cpu={cpu}',
      ('mnemonic("CppCompile",'
       'filter("@XNNPACK//:", deps(:tensorflowlite)))'),
      '--define',
      'xnn_enable_jit=false',
      "--output=jsonproto",
  ])
  logging.info('parsing actions from bazel aquery...')
  obs = []
  aquery_json = json.loads(logs)

  # TODO: b/305060707 - This can be removed when the submodule is updated past:
  #   2007bc117 BUILD.bazel,:operators: depend on :jit conditionally
  jit_target_id = -1
  for target in aquery_json["targets"]:
    if target["label"] == "@XNNPACK//:jit":
      jit_target_id = target["id"]
      break

  for action in aquery_json["actions"]:
    if action["targetId"] == jit_target_id:
      continue
    ob = _objectbuild_from_bazel_log(action)
    if ob:
      obs.append(ob)
  logging.info('Scraped %d built objects' % len(obs))
  return obs


def CombineObjectBuildsIntoSourceSets(obs, arch):
  """
  Combines all the given ObjectBuild's into SourceSet's by combining source
  files whose SourceSet name's (that is their directory and compiler flags)
  match.

  Args:
    obs: a list of ObjectBuild's
    arch: CPU architecture, arm64 or x64
  """
  sss = {}
  for ob in obs:
    single = SourceSet(dir=ob.dir, srcs=[ob.src], args=ob.args)
    name = NameForSourceSet(single, arch)
    if name not in sss:
      sss[name] = single
    else:
      ss = sss[name]
      ss = ss._replace(srcs=list(set(ss.srcs + [ob.src])))
      sss[name] = ss
  xxnpack_ss = sss.pop('xnnpack')
  logging.info('Generated %d sub targets for xnnpack' % len(sss))
  return xxnpack_ss, sorted(list(sss.values()),
                            key=lambda ss: NameForSourceSet(ss, arch))


def MakeTargetSourceSet(ss, arch):
  """
  Generates the BUILD file text for a build target that supports the main
  XNNPACK target, returning it as a string.

  Args:
    ss: a SourceSet
    arch: CPU architecture, arm64 or x64
  """
  target = _TARGET_TMPL
  target = target.replace(
      '%CFLAGS%', ',\n'.join(['    "%s"' % arg for arg in sorted(ss.args)]))
  have_asm_files = False
  for src in ss.srcs:
    if src.endswith('.S'):
      have_asm_files = True
      break
  if have_asm_files:
    target = target.replace('%ASMFLAGS%', '\n  asmflags = cflags\n')
  else:
    target = target.replace('%ASMFLAGS%', '')

  target = target.replace(
      '%SRCS%', ',\n'.join(['    "%s"' % src for src in sorted(ss.srcs)]))
  target = target.replace('%TARGET_NAME%', NameForSourceSet(ss, arch))
  return target


def MakeXNNPACKDepsList(target_sss):
  """
  Creates xnnpack_deps[] and xnnpack_standalone_deps[] for each cpu in
  target_sss. These used by the xnnpack and xnnpack_standalone source_set's to
  set deps[].
  """
  deps_list = ''
  for cpu, sss in target_sss.items():
    targets = sorted([NameForSourceSet(ss, cpu) for ss in sss])
    if (deps_list):
      deps_list += '} else '
    # x86-64 and x86 are treated equivalently in XNNPACK's build.
    if (cpu == 'x64'):
      deps_list += 'if (current_cpu == "x64" || current_cpu == "x86") {'
    else:
      deps_list += f'if (current_cpu == "{cpu}") {{'
    xnnpack_deps = ',\n'.join(['    ":%s"' % t for t in targets])
    xnnpack_standalone_deps = ',\n'.join(
        ['    ":%s_standalone"' % t for t in targets])
    deps_list += f'''
  xnnpack_deps = [
{xnnpack_deps}
  ]

  xnnpack_standalone_deps = [
{xnnpack_standalone_deps}
  ]
'''
  deps_list += '} else {\n'
  deps_list += '  xnnpack_deps = []\n'
  deps_list += '  xnnpack_standalone_deps = []\n'
  deps_list += '}'

  return deps_list


def MakeXNNPACKSourceSet(ss):
  """
  Generates the BUILD file text for the main XNNPACK build target, given the
  XNNPACK SourceSet and the names of all its supporting targets.
  """
  target = _MAIN_TMPL
  target = target.replace(
      '%SRCS%', ',\n'.join(['    "%s"' % src for src in sorted(ss.srcs)]))
  return target


def main():
  logging.basicConfig(level=logging.INFO)

  if platform.system() != 'Linux':
    logging.error('This script only supports running under Linux!')
    sys.exit(1)
  if not (os.access(_AARCH64_LINUX_GCC, os.X_OK)
          and os.access(_X86_64_LINUX_GCC, os.X_OK)):
    logging.error(f'{_AARCH64_LINUX_GCC} and {_X86_64_LINUX_GCC} are required!')
    logging.error('On x86-64 Debian, install gcc-aarch64-linux-gnu and gcc.')
    sys.exit(1)

  CreateToolchainFiles()

  # Create SourceSet's for each target architecture.
  xnnpack_ss = {}
  other_sss = {}
  gn_to_bazel_cpus = {'x64': 'k8', 'arm64': 'aarch64'}
  for gn_cpu, bazel_cpu in gn_to_bazel_cpus.items():
    obs = GenerateObjectBuilds(bazel_cpu)
    xnnpack_ss[gn_cpu], other_sss[gn_cpu] = CombineObjectBuildsIntoSourceSets(
        obs, gn_cpu)

  # Generate sub-target gn source_set's for each target architecture.
  sub_targets = {}
  for gn_cpu, sss in other_sss.items():
    sub_targets[gn_cpu] = []
    for ss in sss:
      sub_targets[gn_cpu].append(MakeTargetSourceSet(ss, gn_cpu))
  # Create a dependency list containing the source_set's for use by the main
  # "xnnpack" target.
  xnnpack_deps = MakeXNNPACKDepsList(other_sss)

  # The sources for the "xnnpack" target are assumed to be the same for each
  # target architecture.
  for cpu in xnnpack_ss:
    if cpu == 'x64': continue
    assert sorted(xnnpack_ss[cpu].srcs) == sorted(xnnpack_ss['x64'].srcs)
    assert sorted(xnnpack_ss[cpu].args) == sorted(xnnpack_ss['x64'].args)
  xnnpack_target = MakeXNNPACKSourceSet(xnnpack_ss['x64'])

  out_path = os.path.join(_xnnpack_dir(), 'BUILD.gn')
  logging.info('Writing to ' + out_path)
  with open(out_path, 'w') as f:
    f.write(_HEADER)
    f.write('\n\n')
    f.write(xnnpack_deps)
    f.write('\n\n')
    f.write(xnnpack_target)
    f.write('\n')
    for gn_cpu in sub_targets:
      # x86-64 and x86 are treated equivalently in XNNPACK's build.
      if (gn_cpu == 'x64'):
        f.write('\nif (current_cpu == "x64" || current_cpu == "x86") {\n')
      else:
        f.write(f'\nif (current_cpu == "{gn_cpu}") {{\n')
      for target in sub_targets[gn_cpu]:
        f.write(target)
        f.write('\n\n')
      f.write('}\n')

  logging.info('Done! Please run `git cl format`')


if __name__ == "__main__":
  main()
