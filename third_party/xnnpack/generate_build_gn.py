# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
#
# LOCAL SETUP:
# Make sure you have bazel installed first.
#  `sudo apt-get install bazel`
# This script is designed to work on Linux only, but may work on other platforms
# with small changes.
#
# WHAT:
# This script creates the BUILD.gn file for XNNPACK by reverse-engineering the
# bazel build.

# HOW:
# By setting the -s option on the bazel build command, bazel logs each compiler
# invocation to the console which is then scraped and put into a configuration
# that gn will accept.
#
# WHY:
# The biggest difficulty of this process is that gn expects each source's
# basename to be unique within a single source set. For example, including both
# "bar/foo.c" and "baz/foo.c" in the same source set is illegal. Therefore, each
# source set will only contain sources from a single directory.
# However, some sources within the same directory may need different compiler
# flags set, so source sets are further split by their flags.

import collections
import logging
import os
import shutil
import subprocess
from operator import attrgetter


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
    "XNN_ENABLE_SPARSE=1",
    "XNN_LOG_LEVEL=0",
    "XNN_LOG_TO_STDIO=0",
]
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

  deps = [
%TARGETS%,
    "//third_party/cpuinfo",
    "//third_party/fp16",
    "//third_party/fxdiv",
    "//third_party/pthreadpool",
  ]

  public_configs = [ ":xnnpack_config" ]
}
'''.strip()

_TARGET_TMPL = '''
source_set("%TARGET_NAME%") {
  cflags = [
%ARGS%
  ]

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
'''.strip()

# This is the latest version of the Android NDK that is compatible with
# XNNPACK.
_ANDROID_NDK_VERSION = 'android-ndk-r19c'
_ANDROID_NDK_URL = 'https://dl.google.com/android/repository/android-ndk-r19c-linux-x86_64.zip'

g_android_ndk = None
def _ensure_android_ndk_available():
  """
  Ensures that the Android NDK is available to bazel, downloading a new copy if
  needed. Raises an Exception if any command fails.

  Returns: the full path to the Android NDK
  """
  global g_android_ndk
  if g_android_ndk:
    return g_android_ndk
  g_android_ndk = '/tmp/'+_ANDROID_NDK_VERSION
  if os.path.exists(g_android_ndk):
    logging.info('Using existing Android NDK at ' + g_android_ndk)
    return g_android_ndk
  logging.info('Downloading new copy of the Android NDK')
  zipfile = '/tmp/{ndk}.zip'.format(ndk=_ANDROID_NDK_VERSION)
  subprocess.run(['wget', '-O', zipfile, _ANDROID_NDK_URL],
    stdout=subprocess.DEVNULL,
    stderr=subprocess.DEVNULL,
    check=True)
  subprocess.run(['unzip', '-o', zipfile, '-d', '/tmp'],
    stdout=subprocess.DEVNULL,
    stderr=subprocess.DEVNULL,
    check=True)
  return g_android_ndk

# A SourceSet corresponds to a single source_set() gn tuple.
SourceSet = collections.namedtuple(
  'SourceSet',
  ['dir', 'srcs', 'args'],
  defaults=['', [], []])

def NameForSourceSet(source_set):
  """
  Returns the name to use for a SourceSet in the gn target.
  """
  if source_set.dir == 'xnnpack':
    return 'xnnpack'
  if len(source_set.args) == 0:
    return source_set.dir
  return '{dir}_{args}'.format(**{
    'dir': source_set.dir,
    'args': '-'.join([arg[2:] for arg in source_set.args]),
  })

# An ObjectBuild corresponds to a single built object, which is parsed from a
# single bazel compiler invocation on a single source file.
ObjectBuild = collections.namedtuple(
  'ObjectBuild',
  ['src', 'dir', 'args'],
  defaults=['', '', []])

def _objectbuild_from_bazel_log(log_line):
  """
  Attempts to scrape a compiler invocation from a single bazel build output
  line. If no invocation is present, None is returned.
  """
  split = log_line.strip().split(' ')
  if not split[0].endswith('gcc'):
    return None

  src = ''
  dir = ''
  args = []
  for i, arg in enumerate(split):
    if arg == '-c' and split[i + 1].startswith("external/XNNPACK/"):
      src = os.path.join('src', split[i + 1][len("external/XNNPACK/"):])
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

def _run_bazel_cmd(args):
  """
  Runs a bazel command in the form of bazel <args...>. Returns the stdout and
  stderr concatenated, raising an Exception if the command failed.
  """
  exec_path = shutil.which("bazel")
  if not exec_path:
    raise Exception("bazel is not installed. Please run `sudo apt-get install "
                   + "bazel` or put the bazel executable in $PATH")
  cmd = [exec_path]
  cmd.extend(args)
  env = os.environ
  env.update({
    'ANDROID_NDK_HOME': _ensure_android_ndk_available(),
  })
  proc = subprocess.Popen(cmd,
    text=True,
    cwd=_tflite_dir(),
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    env=env)
  stdout, stderr = proc.communicate()
  if proc.returncode != 0:
    raise Exception("bazel command returned non-zero return code:\n"
      "cmd: {cmd}\n"
      "status: {status}\n"
      "stdout: {stdout}\n"
      "stderr: {stderr}".format(**{
        'cmd': str(cmd),
        'status': proc.returncode,
        'stdout': stdout,
        'stderr': stderr,
      })
    )
  return stdout + "\n" + stderr

def ListAllSrcs():
  """
  Runs a bazel command to query and and return all source files for XNNPACK, but
  not any dependencies, as relative paths to //third_party/xnnpack/.
  """
  logging.info('Querying for the list of all XNNPACK srcs in :tensorflowlite')
  out = _run_bazel_cmd([
    'cquery',
    'kind("source file", deps(:tensorflowlite))',
    '--define',
    'xnn_enable_jit=false',
  ])
  srcs = []
  for line in out.split('\n'):
    if line.startswith('@XNNPACK//:'):
      srcs.append(os.path.join('src', line.split()[0][len("@XNNPACK//:"):]))
  return srcs

def GenerateObjectBuilds(srcs):
  """
  Builds XNNPACK with bazel and scrapes out all the ObjectBuild's for all
  source files in srcs.
  """
  logging.info('Running `bazel clean`')
  _run_bazel_cmd(['clean'])
  logging.info('Building xnnpack with bazel...')
  logs = _run_bazel_cmd([
    'build',
    ':tensorflowlite',
    '-s',
    '-c', 'opt',
    '--define',
    'xnn_enable_jit=false',
  ])
  logging.info('scraping %d log lines from bazel build...'
                % len(logs.split('\n')))
  obs = []
  for log in logs.split('\n'):
    ob = _objectbuild_from_bazel_log(log)
    if ob and ob.src in srcs:
      obs.append(ob)
  logging.info('Scraped %d built objects' % len(obs))
  return obs

def CombineObjectBuildsIntoSourceSets(obs):
  """
  Combines all the given ObjectBuild's into SourceSet's by combining source
  files whose SourceSet name's (that is thier directory and compiler flags)
  match.
  """
  sss = {}
  for ob in obs:
    single = SourceSet(dir=ob.dir, srcs=[ob.src], args=ob.args)
    name = NameForSourceSet(single)
    if name not in sss:
      sss[name] = single
    else:
      ss = sss[name]
      ss = ss._replace(srcs=list(set(ss.srcs + [ob.src])))
      sss[name] = ss
  xxnpack_ss = sss.pop('xnnpack')
  logging.info('Generated %d sub targets for xnnpack' % len(sss))
  return xxnpack_ss, sorted(
    list(sss.values()), key=lambda ss: NameForSourceSet(ss))

def MakeTargetSourceSet(ss):
  """
  Generates the BUILD file text for a build target that supports the main
  XNNPACK target, returning it as a string.
  """
  target = _TARGET_TMPL
  target = target.replace('%ARGS%', ',\n'.join([
    '    "%s"' % arg for arg in sorted(ss.args)
  ]))
  target = target.replace('%SRCS%', ',\n'.join([
    '    "%s"' % src for src in sorted(ss.srcs)
  ]))
  target = target.replace('%TARGET_NAME%', NameForSourceSet(ss))
  return target

def MakeXNNPACKSourceSet(ss, other_targets):
  """
  Generates the BUILD file text for the main XNNPACK build target, given the
  XNNPACK SourceSet and the names of all its supporting targets.
  """
  target = _MAIN_TMPL
  target = target.replace('%SRCS%', ',\n'.join([
    '    "%s"' % src for src in sorted(ss.srcs)
  ]))
  target = target.replace('%TARGETS%', ',\n'.join([
    '    ":%s"' % t for t in sorted(other_targets)
  ]))
  return target

def main():
  logging.basicConfig(level=logging.INFO)

  srcs = ListAllSrcs()
  obs = GenerateObjectBuilds(srcs)
  xnnpack_ss, other_sss = CombineObjectBuildsIntoSourceSets(obs)

  sub_targets = []
  for ss in other_sss:
    sub_targets.append(MakeTargetSourceSet(ss))
  xnnpack_target = MakeXNNPACKSourceSet(
      xnnpack_ss,
      [NameForSourceSet(ss) for ss in other_sss])

  out_path = os.path.join(_xnnpack_dir(), 'BUILD.gn')
  logging.info('Writing to ' + out_path)
  with open(out_path, 'w') as f:
    f.write(_HEADER)
    f.write('\n')
    f.write(xnnpack_target)
    f.write('\n\n')
    for target in sub_targets:
      f.write(target)
      f.write('\n\n')

  logging.info('Done! Please run `git cl format`')


if __name__ == "__main__":
  main()
