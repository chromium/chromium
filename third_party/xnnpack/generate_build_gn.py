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

import itertools
import json
import logging
import os
import platform as pyplatform
import shutil
import subprocess
import sys

from dataclasses import dataclass, field

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
    "src",
  ]

  cflags=[
    "-Wno-unused-function",
    "-Wno-deprecated-comma-subscript",
  ]

  if (is_android && current_cpu == "arm64") {
    asmflags = [ "-mmark-bti-property" ]
  }

  defines = [
    "CHROMIUM",
    "XNN_ENABLE_ASSEMBLY=1",
    "XNN_ENABLE_GEMM_M_SPECIALIZATION=1",
    "XNN_ENABLE_MEMOPT=1",
    "XNN_ENABLE_CPUINFO=1",
    "XNN_ENABLE_SPARSE=1",
    "XNN_LOG_LEVEL=0",
    "XNN_LOG_TO_STDIO=0",
    "XNN_ENABLE_AVX512BF16=0",
  ]

  if (current_cpu == "arm64") {
    defines += [
      "XNN_ENABLE_ARM_DOTPROD=1",
      "XNN_ENABLE_ARM_I8MM=1",
    ]
  }

  if (current_cpu == "x86" || current_cpu == "x64") {
    defines += [
      "XNN_ENABLE_AVXVNNI=1",
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
  "src/include/xnnpack.h",
  "build_identifier.c",
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
  "src/include/xnnpack.h",
  "build_identifier.c",
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
    "src/include/xnnpack.h",
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
    "src/include/xnnpack.h",
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


@dataclass(frozen=True)
class _Platform:
    gn_cpu: str
    bazel_cpu: str
    bazel_platform: str

    @property
    def gn_condition(self):
        """An expression that conditions on this platform in GN build."""
        # x86-64 and x86 are treated equivalently in XNNPACK's build.
        if self.gn_cpu == 'x64':
            return 'current_cpu == "x64" || current_cpu == "x86"'
        else:
            return f'current_cpu == "{self.gn_cpu}"'


# N.B. that XNNPACK's Bazel doesn't know about these platforms yet, they're
# purely for the dummy toolchain in bazelroot/ to pull the file list out.
_PLATFORMS = [
    _Platform(gn_cpu='x64', bazel_cpu='k8', bazel_platform='//:linux_x64'),
    _Platform(gn_cpu='arm64',
              bazel_cpu='aarch64',
              bazel_platform='//:linux_aarch64')
]


@dataclass(frozen=True)
class ObjectBuild:
    """An action in the bazel build to construct an object."""
    platform: _Platform
    dir: str
    src: str
    args: list[str]

    def GnName(self) -> str:
        """Name of the gn target that should build this object."""
        if self.dir == 'xnnpack':
            return 'xnnpack'
        # Note this creates some target redundancy when the source set is
        # the same between architectures.
        dir = f'{self.dir}_{self.platform.gn_cpu}'
        if not self.args:
            return dir
        args = '-'.join([arg[2:] for arg in self.args])
        return f'{self.dir}_{args}'


@dataclass(frozen=True)
class SourceSet:
    """A SourceSet corresponds to a single source_set() gn tuple."""
    objects: list[ObjectBuild]

    def GnName(self) -> str:
        """Name of the gn target for this source_set."""
        return self.objects[0].GnName()

    @property
    def srcs(self) -> list[str]:
        return sorted(ob.src for ob in self.objects)

    @property
    def args(self) -> list[str]:
        return sorted(self.objects[0].args)

    @property
    def platform(self) -> _Platform:
        return self.objects[0].platform


@dataclass
class PlatformSourceSets:
    """The SourceSets for a single platform."""
    xnnpack: SourceSet
    # All other targets sorted by GnName().
    other: list[SourceSet]

    @property
    def platform(self) -> _Platform:
        return self.xnnpack.platform


def _xnnpack_dir() -> str:
    """Returns the absolute path of //third_party/xnnpack/."""
    return os.path.dirname(os.path.realpath(__file__))


def _bazelroot() -> str:
    """Returns the absolute path of //third_party/xnnpack/bazelroot."""
    return os.path.join(_xnnpack_dir(), "bazelroot")


def _objectbuild_from_bazel_log(action, platform: _Platform) -> ObjectBuild:
    """Extracts compile information from a bazel action.

    Returns:
      An ObjectBuild with the target and compiler flags, or None if the action
      does not represent a compilation of an XNNPACK source file (e.g. it's
      a compile of dependency source like cpuinfo).
    """
    action_args = action["arguments"]
    src_index = action_args.index('-c') + 1
    PREFIX = 'external/xnnpack+/src/'
    if not action_args[src_index].startswith(PREFIX):
        # Not an XNNPACK source file.
        return None
    # Get the //third_party/xnnpack relative path.
    src = os.path.join('src', 'src', action_args[src_index][len(PREFIX):])
    src_path = src.split('/')
    if len(src_path) == 3:
        dir = 'xnnpack'  # src files in the root belong in the main target.
    else:
        dir = src_path[2]

    args = [arg for arg in action_args if arg.startswith('-m')]
    ob = ObjectBuild(platform=platform, src=src, dir=dir, args=args)
    if ob.GnName() in (
            'bf16-f32-gemm_f16c-fma-avx512f-avx512cd-avx512bw-avx512dq-avx512vl-avx512vnni-gfni',
            'f32-gemm_f16c-fma-avx512f-avx512cd-avx512bw-avx512dq-avx512vl-avx512vnni-gfni',
            'qd8-f32-qc8w-gemm_f16c-fma-avx512f-avx512cd-avx512bw-avx512dq-avx512vl-avx512vnni-gfni',
    ):
        # TODO: crbug.com/395969334 - These target breaks windows builds.
        return None
    return ob


def _run_bazel_cmd(args: list[str]) -> str:
    """Runs a bazel command in the form of bazel <args...>.

    Returns:
      The stdout of the command.
    Raises:
      Exception if the command failed.
    """
    # Use standard Bazel install instead of the one included with depot_tools.
    exec_path = "/usr/bin/bazel"
    if not exec_path:
        raise Exception(
            "bazel is not installed. Please run `sudo apt-get install " +
            "bazel` or put the bazel executable in $PATH")

    cmd = [exec_path]
    cmd.extend(args)
    logging.info('Running: %s', cmd)
    proc = subprocess.Popen(cmd,
                            text=True,
                            cwd=_bazelroot(),
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    stdout, stderr = proc.communicate()
    if proc.returncode != 0:
        raise Exception("bazel command returned non-zero return code:\n"
                        f"cmd: {str(cmd)}\n"
                        f"status: {proc.returncode}\n"
                        f"stdout: {stdout}\n"
                        f"stderr: {stderr}")
    return stdout


def _query_object_builds(platform: _Platform) -> list[ObjectBuild]:
    """Queries bazel for the compile commands to build XNNPACK for the platform.

      Returns:
        ObjectBuilds for each compile command on XNNPACK sources.
    """
    logging.info('Querying xnnpack compile commands '
                 'for {platform.bazel_platform} with bazel...')
    # Make sure we have a clean start, this is important if the Android NDK
    # version changed.
    _run_bazel_cmd(['clean'])

    logs = _run_bazel_cmd([
        'aquery',
        f'--platforms={platform.bazel_platform}',
        f'--cpu={platform.bazel_cpu}',
        'mnemonic("CppCompile", filter("//:", deps(@xnnpack//:XNNPACK)))',
        "--output=jsonproto",
    ])
    logging.info('parsing actions from bazel aquery...')
    aquery_json = json.loads(logs)

    obs = []
    for action in aquery_json["actions"]:
        if ob := _objectbuild_from_bazel_log(action, platform):
            obs.append(ob)
    logging.info('Scraped %d built objects' % len(obs))
    return obs


def _combine_object_builds(obs: list[ObjectBuild]) -> PlatformSourceSets:
    """Combines ObjectBuilds for one platform into PlatformSourceSets.

    ObjectBuilds with the same directory, flags and platform are combined into
    SourceSets, and the root source is seperated from those.

    Returns:
      A PlatformSourceSets containing the combined SourceSets.
    """
    key_func = lambda ob: ob.GnName()
    obs = sorted(obs, key=key_func)
    source_sets = {
        name: SourceSet(list(group))
        for name, group in itertools.groupby(obs, key=key_func)
    }
    xxnpack_source_set = source_sets.pop('xnnpack')
    logging.info('Generated %d sub targets for xnnpack' % len(source_sets))
    return PlatformSourceSets(xnnpack=xxnpack_source_set,
                              other=sorted(source_sets.values(),
                                           key=lambda ss: ss.GnName()))


def _generate_supporting_source_set(ss: SourceSet) -> str:
    """Generates the BUILD file text for one supporting source_set."""
    target = _TARGET_TMPL
    target = target.replace(
        '%CFLAGS%', ',\n'.join(['    "%s"' % arg for arg in sorted(ss.args)]))
    have_asm_files = any(src.endswith('.S') for src in ss.srcs)
    target = target.replace('%ASMFLAGS%',
                            '\n  asmflags = cflags\n' if have_asm_files else '')
    target = target.replace(
        '%SRCS%', ',\n'.join(['    "%s"' % src for src in sorted(ss.srcs)]))
    target = target.replace('%TARGET_NAME%', ss.GnName())
    return target


def _generate_per_platform_dep_lists(
        platform_source_sets: list[PlatformSourceSets]) -> str:
    """Creates xnnpack_deps[] and xnnpack_standalone_deps[] definitions.

    Each platform gets a seperate set of deps, in an if-else-if block.

    The declared variables are referenced later in the main xnnpack targets.

    Returns:
      A str with the block defining the variables.
    """
    deps_list = ''
    for pss in platform_source_sets:
        targets = [ss.GnName() for ss in pss.other]
        if (deps_list):
            deps_list += '} else '
        deps_list += f'\nif ({pss.platform.gn_condition}) {{\n'
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


def _generate_main_source_set(
        platform_source_sets: list[PlatformSourceSets]) -> str:
    """Generates the BUILD file text for the main XNNPACK build target."""
    # The sources for the "xnnpack" target are assumed to be the same for each
    # target architecture.
    for pss in platform_source_sets[1:]:
        assert pss.xnnpack.srcs == platform_source_sets[0].xnnpack.srcs
        assert pss.xnnpack.args == platform_source_sets[0].xnnpack.args
    srcs = ',\n'.join(
        ['    "%s"' % src for src in platform_source_sets[0].xnnpack.srcs])
    target = _MAIN_TMPL
    target = target.replace('%SRCS%', srcs)
    return target


def _generate_build_identifier() -> None:
    """Generates and copies the `build_identifier.c`"""
    _run_bazel_cmd(['build', '@xnnpack//:generate_build_identifier'])
    bazel_bin_dir = _run_bazel_cmd(['info', 'bazel-bin']).strip()
    build_identifier_src = os.path.join(
        bazel_bin_dir, 'external/xnnpack+/src/build_identifier.c')
    assert os.path.exists(build_identifier_src)
    build_identifier_dst = os.path.join(_xnnpack_dir(), 'build_identifier.c')
    logging.info(f'Copying {build_identifier_src} to {build_identifier_dst}')
    shutil.copyfile(build_identifier_src, build_identifier_dst)


def CleanupBazelroot() -> None:
    """Remove convenience links and lockfile from bazelroot."""
    for filename in [
            'bazel-bazelroot',
            'bazel-bin',
            'bazel-out',
            'bazel-testlogs',
            'MODULE.bazel.lock',
    ]:
        os.remove(os.path.join(_bazelroot(), filename))


def _generate_build_gn(platform_source_sets: list[PlatformSourceSets]) -> None:
    """Write out the BUILD.gn file."""
    # Create a dependency list containing the source_set's for use by the main
    # "xnnpack" target.
    xnnpack_deps = _generate_per_platform_dep_lists(platform_source_sets)

    xnnpack_target = _generate_main_source_set(platform_source_sets)

    out_path = os.path.join(_xnnpack_dir(), 'BUILD.gn')
    logging.info('Writing to ' + out_path)
    with open(out_path, 'w') as f:
        f.write(_HEADER)
        f.write('\n\n')
        f.write(xnnpack_deps)
        f.write('\n\n')
        f.write(xnnpack_target)
        f.write('\n')
        for pss in platform_source_sets:
            f.write(f'\nif ({pss.platform.gn_condition}) {{\n')
            for source_set in pss.other:
                f.write(_generate_supporting_source_set(source_set))
                f.write('\n\n')
            f.write('}\n')


def main():
    logging.basicConfig(level=logging.INFO)

    if pyplatform.system() != 'Linux':
        logging.error('This script only supports running under Linux!')
        sys.exit(1)

    # Create SourceSet's for each target architecture.
    platform_source_sets = [
        _combine_object_builds(_query_object_builds(platform))
        for platform in _PLATFORMS
    ]

    _generate_build_gn(platform_source_sets)

    _generate_build_identifier()

    CleanupBazelroot()

    logging.info('Running `git cl format` for you.')

    subprocess.check_output(['git', 'cl', 'format'], cwd=_xnnpack_dir())

    logging.info('Done')


if __name__ == "__main__":
    main()
