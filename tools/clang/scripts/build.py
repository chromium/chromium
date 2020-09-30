#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is used to build clang binaries. It is used by package.py to
create the prebuilt binaries downloaded by update.py and used by developers.

The expectation is that update.py downloads prebuilt binaries for everyone, and
nobody should run this script as part of normal development.
"""

from __future__ import print_function

import argparse
import glob
import io
import json
import os
import pipes
import re
import shutil
import subprocess
import sys

from update import (CDS_URL, CHROMIUM_DIR, CLANG_REVISION, LLVM_BUILD_DIR,
                    FORCE_HEAD_REVISION_FILE, PACKAGE_VERSION, RELEASE_VERSION,
                    STAMP_FILE, DownloadUrl, DownloadAndUnpack, EnsureDirExists,
                    ReadStampFile, RmTree, WriteStampFile)

# Path constants. (All of these should be absolute paths.)
THIRD_PARTY_DIR = os.path.join(CHROMIUM_DIR, 'third_party')
LLVM_DIR = os.path.join(THIRD_PARTY_DIR, 'llvm')
COMPILER_RT_DIR = os.path.join(LLVM_DIR, 'compiler-rt')
LLVM_BOOTSTRAP_DIR = os.path.join(THIRD_PARTY_DIR, 'llvm-bootstrap')
LLVM_BOOTSTRAP_INSTALL_DIR = os.path.join(THIRD_PARTY_DIR,
                                          'llvm-bootstrap-install')
LLVM_INSTRUMENTED_DIR = os.path.join(THIRD_PARTY_DIR, 'llvm-instrumented')
LLVM_PROFDATA_FILE = os.path.join(LLVM_INSTRUMENTED_DIR, 'profdata.prof')
CHROME_TOOLS_SHIM_DIR = os.path.join(LLVM_DIR, 'llvm', 'tools', 'chrometools')
COMPILER_RT_BUILD_DIR = os.path.join(LLVM_BUILD_DIR, 'compiler-rt')
LLVM_BUILD_TOOLS_DIR = os.path.abspath(
    os.path.join(LLVM_DIR, '..', 'llvm-build-tools'))
ANDROID_NDK_DIR = os.path.join(
    CHROMIUM_DIR, 'third_party', 'android_ndk')
FUCHSIA_SDK_DIR = os.path.join(CHROMIUM_DIR, 'third_party', 'fuchsia-sdk',
                               'sdk')

BUG_REPORT_URL = ('https://crbug.com and run'
                  ' tools/clang/scripts/process_crashreports.py'
                  ' (only works inside Google) which will upload a report')


win_sdk_dir = None
def GetWinSDKDir():
  """Get the location of the current SDK."""
  global win_sdk_dir
  if win_sdk_dir:
    return win_sdk_dir

  # Don't let vs_toolchain overwrite our environment.
  environ_bak = os.environ

  sys.path.append(os.path.join(CHROMIUM_DIR, 'build'))
  import vs_toolchain
  win_sdk_dir = vs_toolchain.SetEnvironmentAndGetSDKDir()
  msvs_version = vs_toolchain.GetVisualStudioVersion()

  if bool(int(os.environ.get('DEPOT_TOOLS_WIN_TOOLCHAIN', '1'))):
    dia_path = os.path.join(win_sdk_dir, '..', 'DIA SDK', 'bin', 'amd64')
  else:
    if 'GYP_MSVS_OVERRIDE_PATH' not in os.environ:
      vs_path = vs_toolchain.DetectVisualStudioPath()
    else:
      vs_path = os.environ['GYP_MSVS_OVERRIDE_PATH']
    dia_path = os.path.join(vs_path, 'DIA SDK', 'bin', 'amd64')

  os.environ = environ_bak
  return win_sdk_dir


def RunCommand(command, msvc_arch=None, env=None, fail_hard=True):
  """Run command and return success (True) or failure; or if fail_hard is
     True, exit on failure.  If msvc_arch is set, runs the command in a
     shell with the msvc tools for that architecture."""

  if msvc_arch and sys.platform == 'win32':
    command = [os.path.join(GetWinSDKDir(), 'bin', 'SetEnv.cmd'),
               "/" + msvc_arch, '&&'] + command

  # https://docs.python.org/2/library/subprocess.html:
  # "On Unix with shell=True [...] if args is a sequence, the first item
  # specifies the command string, and any additional items will be treated as
  # additional arguments to the shell itself.  That is to say, Popen does the
  # equivalent of:
  #   Popen(['/bin/sh', '-c', args[0], args[1], ...])"
  #
  # We want to pass additional arguments to command[0], not to the shell,
  # so manually join everything into a single string.
  # Annoyingly, for "svn co url c:\path", pipes.quote() thinks that it should
  # quote c:\path but svn can't handle quoted paths on Windows.  Since on
  # Windows follow-on args are passed to args[0] instead of the shell, don't
  # do the single-string transformation there.
  if sys.platform != 'win32':
    command = ' '.join([pipes.quote(c) for c in command])
  print('Running', command)
  if subprocess.call(command, env=env, shell=True) == 0:
    return True
  print('Failed.')
  if fail_hard:
    sys.exit(1)
  return False


def CopyFile(src, dst):
  """Copy a file from src to dst."""
  print("Copying %s to %s" % (src, dst))
  shutil.copy(src, dst)


def CopyDirectoryContents(src, dst):
  """Copy the files from directory src to dst."""
  dst = os.path.realpath(dst)  # realpath() in case dst ends in /..
  EnsureDirExists(dst)
  for f in os.listdir(src):
    CopyFile(os.path.join(src, f), dst)


def CheckoutLLVM(commit, dir):
  """Checkout the LLVM monorepo at a certain git commit in dir. Any local
  modifications in dir will be lost."""

  print('Checking out LLVM monorepo %s into %s' % (commit, dir))

  # Try updating the current repo if it exists and has no local diff.
  if os.path.isdir(dir):
    os.chdir(dir)
    # git diff-index --quiet returns success when there is no diff.
    # Also check that the first commit is reachable.
    if (RunCommand(['git', 'diff-index', '--quiet', 'HEAD'], fail_hard=False)
        and RunCommand(['git', 'fetch'], fail_hard=False)
        and RunCommand(['git', 'checkout', commit], fail_hard=False)):
      return

    # If we can't use the current repo, delete it.
    os.chdir(CHROMIUM_DIR)  # Can't remove dir if we're in it.
    print('Removing %s.' % dir)
    RmTree(dir)

  clone_cmd = ['git', 'clone', 'https://github.com/llvm/llvm-project/', dir]

  if RunCommand(clone_cmd, fail_hard=False):
    os.chdir(dir)
    if RunCommand(['git', 'checkout', commit], fail_hard=False):
      return

  print('CheckoutLLVM failed.')
  sys.exit(1)


def UrlOpen(url):
  # TODO(crbug.com/1067752): Use urllib once certificates are fixed.
  return subprocess.check_output(['curl', '--silent', url])


def GetLatestLLVMCommit():
  """Get the latest commit hash in the LLVM monorepo."""
  ref = json.loads(UrlOpen(('https://api.github.com/repos/'
                            'llvm/llvm-project/git/refs/heads/master')))
  assert ref['object']['type'] == 'commit'
  return ref['object']['sha']


def GetCommitDescription(commit):
  """Get the output of `git describe`.

  Needs to be called from inside the git repository dir."""
  return subprocess.check_output(
      ['git', 'describe', '--long', '--abbrev=8', commit]).rstrip()


def DeleteChromeToolsShim():
  OLD_SHIM_DIR = os.path.join(LLVM_DIR, 'tools', 'zzz-chrometools')
  shutil.rmtree(OLD_SHIM_DIR, ignore_errors=True)
  shutil.rmtree(CHROME_TOOLS_SHIM_DIR, ignore_errors=True)


def CreateChromeToolsShim():
  """Hooks the Chrome tools into the LLVM build.

  Several Chrome tools have dependencies on LLVM/Clang libraries. The LLVM build
  detects implicit tools in the tools subdirectory, so this helper install a
  shim CMakeLists.txt that forwards to the real directory for the Chrome tools.

  Note that the shim directory name intentionally has no - or _. The implicit
  tool detection logic munges them in a weird way."""
  assert not any(i in os.path.basename(CHROME_TOOLS_SHIM_DIR) for i in '-_')
  os.mkdir(CHROME_TOOLS_SHIM_DIR)
  with open(os.path.join(CHROME_TOOLS_SHIM_DIR, 'CMakeLists.txt'), 'w') as f:
    f.write('# Automatically generated by tools/clang/scripts/update.py. ' +
            'Do not edit.\n')
    f.write('# Since tools/clang is located in another directory, use the \n')
    f.write('# two arg version to specify where build artifacts go. CMake\n')
    f.write('# disallows reuse of the same binary dir for multiple source\n')
    f.write('# dirs, so the build artifacts need to go into a subdirectory.\n')
    f.write('if (CHROMIUM_TOOLS_SRC)\n')
    f.write('  add_subdirectory(${CHROMIUM_TOOLS_SRC} ' +
              '${CMAKE_CURRENT_BINARY_DIR}/a)\n')
    f.write('endif (CHROMIUM_TOOLS_SRC)\n')


def AddCMakeToPath(args):
  """Download CMake and add it to PATH."""
  if args.use_system_cmake:
    return

  if sys.platform == 'win32':
    zip_name = 'cmake-3.17.1-win64-x64.zip'
    dir_name = ['cmake-3.17.1-win64-x64', 'bin']
  elif sys.platform == 'darwin':
    zip_name = 'cmake-3.17.1-Darwin-x86_64.tar.gz'
    dir_name = ['cmake-3.17.1-Darwin-x86_64', 'CMake.app', 'Contents', 'bin']
  else:
    zip_name = 'cmake-3.17.1-Linux-x86_64.tar.gz'
    dir_name = ['cmake-3.17.1-Linux-x86_64', 'bin']

  cmake_dir = os.path.join(LLVM_BUILD_TOOLS_DIR, *dir_name)
  if not os.path.exists(cmake_dir):
    DownloadAndUnpack(CDS_URL + '/tools/' + zip_name, LLVM_BUILD_TOOLS_DIR)
  os.environ['PATH'] = cmake_dir + os.pathsep + os.environ.get('PATH', '')


def AddGnuWinToPath():
  """Download some GNU win tools and add them to PATH."""
  if sys.platform != 'win32':
    return

  gnuwin_dir = os.path.join(LLVM_BUILD_TOOLS_DIR, 'gnuwin')
  GNUWIN_VERSION = '14'
  GNUWIN_STAMP = os.path.join(gnuwin_dir, 'stamp')
  if ReadStampFile(GNUWIN_STAMP) == GNUWIN_VERSION:
    print('GNU Win tools already up to date.')
  else:
    zip_name = 'gnuwin-%s.zip' % GNUWIN_VERSION
    DownloadAndUnpack(CDS_URL + '/tools/' + zip_name, LLVM_BUILD_TOOLS_DIR)
    WriteStampFile(GNUWIN_VERSION, GNUWIN_STAMP)

  os.environ['PATH'] = gnuwin_dir + os.pathsep + os.environ.get('PATH', '')

  # find.exe, mv.exe and rm.exe are from MSYS (see crrev.com/389632). MSYS uses
  # Cygwin under the hood, and initializing Cygwin has a race-condition when
  # getting group and user data from the Active Directory is slow. To work
  # around this, use a horrible hack telling it not to do that.
  # See https://crbug.com/905289
  etc = os.path.join(gnuwin_dir, '..', '..', 'etc')
  EnsureDirExists(etc)
  with open(os.path.join(etc, 'nsswitch.conf'), 'w') as f:
    f.write('passwd: files\n')
    f.write('group: files\n')


def AddZlibToPath():
  """Download and build zlib, and add to PATH."""
  zlib_dir = os.path.join(LLVM_BUILD_TOOLS_DIR, 'zlib-1.2.11')
  if os.path.exists(zlib_dir):
    RmTree(zlib_dir)
  zip_name = 'zlib-1.2.11.tar.gz'
  DownloadAndUnpack(CDS_URL + '/tools/' + zip_name, LLVM_BUILD_TOOLS_DIR)
  os.chdir(zlib_dir)
  zlib_files = [
      'adler32', 'compress', 'crc32', 'deflate', 'gzclose', 'gzlib', 'gzread',
      'gzwrite', 'inflate', 'infback', 'inftrees', 'inffast', 'trees',
      'uncompr', 'zutil'
  ]
  cl_flags = [
      '/nologo', '/O2', '/DZLIB_DLL', '/c', '/D_CRT_SECURE_NO_DEPRECATE',
      '/D_CRT_NONSTDC_NO_DEPRECATE'
  ]
  RunCommand(
      ['cl.exe'] + [f + '.c' for f in zlib_files] + cl_flags, msvc_arch='x64')
  RunCommand(
      ['lib.exe'] + [f + '.obj'
                     for f in zlib_files] + ['/nologo', '/out:zlib.lib'],
      msvc_arch='x64')
  # Remove the test directory so it isn't found when trying to find
  # test.exe.
  shutil.rmtree('test')

  os.environ['PATH'] = zlib_dir + os.pathsep + os.environ.get('PATH', '')
  return zlib_dir


def DownloadRPMalloc():
  """Download rpmalloc."""
  rpmalloc_dir = os.path.join(LLVM_BUILD_TOOLS_DIR, 'rpmalloc')
  if os.path.exists(rpmalloc_dir):
    RmTree(rpmalloc_dir)

  # Using rpmalloc bc1923f rather than the latest release (1.4.1) because
  # it contains the fix for https://github.com/mjansson/rpmalloc/pull/186
  # which would cause lld to deadlock.
  # The zip file was created and uploaded as follows:
  # $ mkdir rpmalloc
  # $ curl -L https://github.com/mjansson/rpmalloc/archive/bc1923f436539327707b08ef9751a7a87bdd9d2f.tar.gz \
  #     | tar -C rpmalloc --strip-components=1 -xzf -
  # $ GZIP=-9 tar vzcf rpmalloc-bc1923f.tgz rpmalloc
  # $ gsutil.py cp -n -a public-read rpmalloc-bc1923f.tgz \
  #     gs://chromium-browser-clang/tools/
  zip_name = 'rpmalloc-bc1923f.tgz'
  DownloadAndUnpack(CDS_URL + '/tools/' + zip_name, LLVM_BUILD_TOOLS_DIR)
  rpmalloc_dir = rpmalloc_dir.replace('\\', '/')
  return rpmalloc_dir


def MaybeDownloadHostGcc(args):
  """Download a modern GCC host compiler on Linux."""
  if not sys.platform.startswith('linux') or args.gcc_toolchain:
    return
  gcc_dir = os.path.join(LLVM_BUILD_TOOLS_DIR, 'gcc530trusty')
  if not os.path.exists(gcc_dir):
    DownloadAndUnpack(CDS_URL + '/tools/gcc530trusty.tgz', gcc_dir)
  args.gcc_toolchain = gcc_dir


def VerifyVersionOfBuiltClangMatchesVERSION():
  """Checks that `clang --version` outputs RELEASE_VERSION. If this
  fails, update.RELEASE_VERSION is out-of-date and needs to be updated (possibly
  in an `if args.llvm_force_head_revision:` block inupdate. main() first)."""
  clang = os.path.join(LLVM_BUILD_DIR, 'bin', 'clang')
  if sys.platform == 'win32':
    clang += '-cl.exe'
  version_out = subprocess.check_output([clang, '--version'],
                                        universal_newlines=True)
  version_out = re.match(r'clang version ([0-9.]+)', version_out).group(1)
  if version_out != RELEASE_VERSION:
    print(('unexpected clang version %s (not %s), '
           'update RELEASE_VERSION in update.py')
          % (version_out, RELEASE_VERSION))
    sys.exit(1)


def VerifyZlibSupport():
  """Check that clang was built with zlib support enabled."""
  clang = os.path.join(LLVM_BUILD_DIR, 'bin', 'clang')
  test_file = '/dev/null'
  if sys.platform == 'win32':
    clang += '-cl.exe'
    test_file = 'nul'

  print('Checking for zlib support')
  clang_out = subprocess.check_output([
      clang, '--driver-mode=gcc', '-target', 'x86_64-unknown-linux-gnu', '-gz',
      '-c', '-###', '-x', 'c', test_file ],
      stderr=subprocess.STDOUT, universal_newlines=True)
  if (re.search(r'--compress-debug-sections', clang_out)):
    print('OK')
  else:
    print(('Failed to detect zlib support!\n\n(driver output: %s)') % clang_out)
    sys.exit(1)


def CopyLibstdcpp(args, build_dir):
  if not args.gcc_toolchain:
    return
  # Find libstdc++.so.6
  libstdcpp = subprocess.check_output([
      os.path.join(args.gcc_toolchain, 'bin', 'g++'),
      '-print-file-name=libstdc++.so.6'
  ],
                                      universal_newlines=True).rstrip()

  # Copy libstdc++.so.6 into the build dir so that the built binaries can find
  # it. Binaries get their rpath set to $origin/../lib/. For clang, lld,
  # etc. that live in the bin/ directory, this means they expect to find the .so
  # in their neighbouring lib/ dir.
  # For unit tests we pass -Wl,-rpath to the linker pointing to the lib64 dir
  # in the gcc toolchain, via LLVM_LOCAL_RPATH below.
  # The two fuzzer tests are weird in that they copy the fuzzer binary from bin/
  # into the test tree under a different name. To make the relative rpath in
  # them work, copy libstdc++ to the copied location for now.
  # TODO(thakis): Instead, make the upstream lit.local.cfg.py for these 2 tests
  # check if the binary contains an rpath and if so disable the tests.
  for d in ['lib',
            'test/tools/llvm-isel-fuzzer/lib',
            'test/tools/llvm-opt-fuzzer/lib']:
    EnsureDirExists(os.path.join(build_dir, d))
    CopyFile(libstdcpp, os.path.join(build_dir, d))

def gn_arg(v):
  if v == 'True':
    return True
  if v == 'False':
    return False
  raise argparse.ArgumentTypeError('Expected one of %r or %r' % (
      'True', 'False'))


def main():
  parser = argparse.ArgumentParser(description='Build Clang.')
  parser.add_argument('--bootstrap', action='store_true',
                      help='first build clang with CC, then with itself.')
  parser.add_argument('--disable-asserts', action='store_true',
                      help='build with asserts disabled')
  parser.add_argument('--gcc-toolchain', help='what gcc toolchain to use for '
                      'building; --gcc-toolchain=/opt/foo picks '
                      '/opt/foo/bin/gcc')
  parser.add_argument('--pgo', action='store_true', help='build with PGO')
  parser.add_argument('--thinlto',
                      action='store_true',
                      help='build with ThinLTO')
  parser.add_argument('--llvm-force-head-revision', action='store_true',
                      help='build the latest revision')
  parser.add_argument('--run-tests', action='store_true',
                      help='run tests after building')
  parser.add_argument('--skip-build', action='store_true',
                      help='do not build anything')
  parser.add_argument('--skip-checkout', action='store_true',
                      help='do not create or update any checkouts')
  parser.add_argument('--extra-tools', nargs='*', default=[],
                      help='select additional chrome tools to build')
  parser.add_argument('--use-system-cmake', action='store_true',
                      help='use the cmake from PATH instead of downloading '
                      'and using prebuilt cmake binaries')
  parser.add_argument('--with-android', type=gn_arg, nargs='?', const=True,
                      help='build the Android ASan runtime (linux only)',
                      default=sys.platform.startswith('linux'))
  parser.add_argument('--without-android', action='store_false',
                      help='don\'t build Android ASan runtime (linux only)',
                      dest='with_android')
  parser.add_argument('--without-fuchsia', action='store_false',
                      help='don\'t build Fuchsia clang_rt runtime (linux/mac)',
                      dest='with_fuchsia',
                      default=sys.platform in ('linux2', 'darwin'))
  args = parser.parse_args()

  if (args.pgo or args.thinlto) and not args.bootstrap:
    print('--pgo/--thinlto requires --bootstrap')
    return 1
  if args.with_android and not os.path.exists(ANDROID_NDK_DIR):
    print('Android NDK not found at ' + ANDROID_NDK_DIR)
    print('The Android NDK is needed to build a Clang whose -fsanitize=address')
    print('works on Android. See ')
    print('https://www.chromium.org/developers/how-tos/android-build-instructions')
    print('for how to install the NDK, or pass --without-android.')
    return 1

  if args.llvm_force_head_revision:
    # Don't build fuchsia runtime on ToT bots at all.
    args.with_fuchsia = False

  if args.with_fuchsia and not os.path.exists(FUCHSIA_SDK_DIR):
    print('Fuchsia SDK not found at ' + FUCHSIA_SDK_DIR)
    print('The Fuchsia SDK is needed to build libclang_rt for Fuchsia.')
    print('Install the Fuchsia SDK by adding fuchsia to the ')
    print('target_os section in your .gclient and running hooks, ')
    print('or pass --without-fuchsia.')
    print('https://chromium.googlesource.com/chromium/src/+/master/docs/fuchsia_build_instructions.md')
    print('for general Fuchsia build instructions.')
    return 1


  # Don't buffer stdout, so that print statements are immediately flushed.
  # LLVM tests print output without newlines, so with buffering they won't be
  # immediately printed.
  major, _, _, _, _ = sys.version_info
  if major == 3:
    # Python3 only allows unbuffered output for binary streams. This
    # workaround comes from https://stackoverflow.com/a/181654/4052492.
    sys.stdout = io.TextIOWrapper(open(sys.stdout.fileno(), 'wb', 0),
                                  write_through=True)
  else:
    sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)

  # The gnuwin package also includes curl, which is needed to interact with the
  # github API below.
  # TODO(crbug.com/1067752): Use urllib once certificates are fixed, and
  # move this down to where we fetch other build tools.
  AddGnuWinToPath()

  # TODO(crbug.com/929645): Remove once we build on host systems with a modern
  # enough GCC to build Clang.
  MaybeDownloadHostGcc(args)

  if sys.platform == 'darwin':
    isysroot = subprocess.check_output(['xcrun', '--show-sdk-path']).rstrip()

  global CLANG_REVISION, PACKAGE_VERSION
  if args.llvm_force_head_revision:
    checkout_revision = GetLatestLLVMCommit()
  else:
    checkout_revision = CLANG_REVISION

  if not args.skip_checkout:
    CheckoutLLVM(checkout_revision, LLVM_DIR)

  if args.llvm_force_head_revision:
    CLANG_REVISION = GetCommitDescription(checkout_revision)
    PACKAGE_VERSION = '%s-0' % CLANG_REVISION

  print('Locally building clang %s...' % PACKAGE_VERSION)
  WriteStampFile('', STAMP_FILE)
  WriteStampFile('', FORCE_HEAD_REVISION_FILE)

  AddCMakeToPath(args)
  DeleteChromeToolsShim()


  if args.skip_build:
    return 0

  # The variable "lld" is only used on Windows because only there does setting
  # CMAKE_LINKER have an effect: On Windows, the linker is called directly,
  # while elsewhere it's called through the compiler driver, and we pass
  # -fuse-ld=lld there to make the compiler driver call the linker (by setting
  # LLVM_ENABLE_LLD).
  cc, cxx, lld = None, None, None

  cflags = []
  cxxflags = []
  ldflags = []

  targets = 'AArch64;ARM;Mips;PowerPC;SystemZ;WebAssembly;X86'

  projects = 'clang;compiler-rt;lld;chrometools;clang-tools-extra'

  if sys.platform == 'darwin':
    # clang needs libc++, else -stdlib=libc++ won't find includes
    # (this is needed for bootstrap builds and for building the fuchsia runtime)
    projects += ';libcxx'

  base_cmake_args = [
      '-GNinja',
      '-DCMAKE_BUILD_TYPE=Release',
      '-DLLVM_ENABLE_ASSERTIONS=%s' % ('OFF' if args.disable_asserts else 'ON'),
      '-DLLVM_ENABLE_PROJECTS=' + projects,
      '-DLLVM_TARGETS_TO_BUILD=' + targets,
      '-DLLVM_ENABLE_PIC=OFF',
      '-DLLVM_ENABLE_UNWIND_TABLES=OFF',
      '-DLLVM_ENABLE_TERMINFO=OFF',
      '-DLLVM_ENABLE_Z3_SOLVER=OFF',
      '-DCLANG_PLUGIN_SUPPORT=OFF',
      '-DCLANG_ENABLE_STATIC_ANALYZER=OFF',
      '-DCLANG_ENABLE_ARCMT=OFF',
      '-DBUG_REPORT_URL=' + BUG_REPORT_URL,
      # See PR41956: Don't link libcxx into libfuzzer.
      '-DCOMPILER_RT_USE_LIBCXX=NO',
      # Don't run Go bindings tests; PGO makes them confused.
      '-DLLVM_INCLUDE_GO_TESTS=OFF',
      # TODO(crbug.com/1113475): Update binutils.
      '-DENABLE_X86_RELAX_RELOCATIONS=NO',
      # See crbug.com/1126219: Use native symbolizer instead of DIA
      '-DLLVM_ENABLE_DIA_SDK=OFF',
  ]

  if args.gcc_toolchain:
    # Use the specified gcc installation for building.
    cc = os.path.join(args.gcc_toolchain, 'bin', 'gcc')
    cxx = os.path.join(args.gcc_toolchain, 'bin', 'g++')
    if not os.access(cc, os.X_OK):
      print('Invalid --gcc-toolchain: ' + args.gcc_toolchain)
      return 1
    base_cmake_args += [
        '-DLLVM_LOCAL_RPATH=' + os.path.join(args.gcc_toolchain, 'lib64')
    ]


  if sys.platform == 'darwin':
    # For libc++, we only want the headers.
    base_cmake_args.extend([
        '-DLIBCXX_ENABLE_SHARED=OFF',
        '-DLIBCXX_ENABLE_STATIC=OFF',
        '-DLIBCXX_INCLUDE_TESTS=OFF',
        '-DLIBCXX_ENABLE_EXPERIMENTAL_LIBRARY=OFF',
    ])
    # Prefer Python 2. TODO(crbug.com/1076834): Remove this.
    base_cmake_args.append('-DPython3_EXECUTABLE=/nonexistent')

  if args.gcc_toolchain:
    # Don't use the custom gcc toolchain when building compiler-rt tests; those
    # tests are built with the just-built Clang, and target both i386 and x86_64
    # for example, so should use the system's libstdc++.
    base_cmake_args.append(
        '-DCOMPILER_RT_TEST_COMPILER_CFLAGS=--gcc-toolchain=')

  if sys.platform == 'win32':
    base_cmake_args.append('-DLLVM_USE_CRT_RELEASE=MT')

    # Require zlib compression.
    zlib_dir = AddZlibToPath()
    cflags.append('-I' + zlib_dir)
    cxxflags.append('-I' + zlib_dir)
    ldflags.append('-LIBPATH:' + zlib_dir)

    # Use rpmalloc. For faster ThinLTO linking.
    rpmalloc_dir = DownloadRPMalloc()
    base_cmake_args.append('-DLLVM_INTEGRATED_CRT_ALLOC=' + rpmalloc_dir)

  if sys.platform != 'win32':
    # libxml2 is required by the Win manifest merging tool used in cross-builds.
    base_cmake_args.append('-DLLVM_ENABLE_LIBXML2=FORCE_ON')

  if args.bootstrap:
    print('Building bootstrap compiler')
    if os.path.exists(LLVM_BOOTSTRAP_DIR):
      RmTree(LLVM_BOOTSTRAP_DIR)
    EnsureDirExists(LLVM_BOOTSTRAP_DIR)
    os.chdir(LLVM_BOOTSTRAP_DIR)

    projects = 'clang'
    if args.pgo:
      # Need libclang_rt.profile
      projects += ';compiler-rt'
    if sys.platform != 'darwin':
      projects += ';lld'
    if sys.platform == 'darwin':
      # Need libc++ and compiler-rt for the bootstrap compiler on mac.
      projects += ';libcxx;compiler-rt'

    bootstrap_targets = 'X86'
    if sys.platform == 'darwin':
      # Need ARM and AArch64 for building the ios clang_rt.
      bootstrap_targets += ';ARM;AArch64'
    bootstrap_args = base_cmake_args + [
        '-DLLVM_TARGETS_TO_BUILD=' + bootstrap_targets,
        '-DLLVM_ENABLE_PROJECTS=' + projects,
        '-DCMAKE_INSTALL_PREFIX=' + LLVM_BOOTSTRAP_INSTALL_DIR,
        '-DCMAKE_C_FLAGS=' + ' '.join(cflags),
        '-DCMAKE_CXX_FLAGS=' + ' '.join(cxxflags),
        '-DCMAKE_EXE_LINKER_FLAGS=' + ' '.join(ldflags),
        '-DCMAKE_SHARED_LINKER_FLAGS=' + ' '.join(ldflags),
        '-DCMAKE_MODULE_LINKER_FLAGS=' + ' '.join(ldflags),
        # Ignore args.disable_asserts for the bootstrap compiler.
        '-DLLVM_ENABLE_ASSERTIONS=ON',
    ]
    if sys.platform == 'darwin':
      # On macOS, the bootstrap toolchain needs to have compiler-rt because
      # dsymutil's link needs libclang_rt.osx.a. Only the x86_64 osx
      # libraries are needed though, and only libclang_rt (i.e.
      # COMPILER_RT_BUILD_BUILTINS).
      bootstrap_args.extend([
          '-DDARWIN_osx_ARCHS=x86_64',
          '-DCOMPILER_RT_BUILD_BUILTINS=ON',
          '-DCOMPILER_RT_BUILD_CRT=OFF',
          '-DCOMPILER_RT_BUILD_LIBFUZZER=OFF',
          '-DCOMPILER_RT_BUILD_SANITIZERS=OFF',
          '-DCOMPILER_RT_BUILD_XRAY=OFF',
          '-DCOMPILER_RT_ENABLE_IOS=OFF',
          '-DCOMPILER_RT_ENABLE_WATCHOS=OFF',
          '-DCOMPILER_RT_ENABLE_TVOS=OFF',
          ])
    elif args.pgo:
      # PGO needs libclang_rt.profile but none of the other compiler-rt stuff.
      bootstrap_args.extend([
          '-DCOMPILER_RT_BUILD_BUILTINS=OFF',
          '-DCOMPILER_RT_BUILD_CRT=OFF',
          '-DCOMPILER_RT_BUILD_LIBFUZZER=OFF',
          '-DCOMPILER_RT_BUILD_PROFILE=ON',
          '-DCOMPILER_RT_BUILD_SANITIZERS=OFF',
          '-DCOMPILER_RT_BUILD_XRAY=OFF',
          ])

    if cc is not None:  bootstrap_args.append('-DCMAKE_C_COMPILER=' + cc)
    if cxx is not None: bootstrap_args.append('-DCMAKE_CXX_COMPILER=' + cxx)
    if lld is not None: bootstrap_args.append('-DCMAKE_LINKER=' + lld)
    RunCommand(['cmake'] + bootstrap_args + [os.path.join(LLVM_DIR, 'llvm')],
               msvc_arch='x64')
    CopyLibstdcpp(args, LLVM_BOOTSTRAP_DIR)
    CopyLibstdcpp(args, LLVM_BOOTSTRAP_INSTALL_DIR)
    RunCommand(['ninja'], msvc_arch='x64')
    if args.run_tests:
      test_targets = [ 'check-all' ]
      if sys.platform == 'darwin':
        # TODO(crbug.com/731375): Run check-all on Darwin too.
        test_targets = [ 'check-llvm', 'check-clang', 'check-builtins' ]
      RunCommand(['ninja'] + test_targets, msvc_arch='x64')
    RunCommand(['ninja', 'install'], msvc_arch='x64')

    if sys.platform == 'win32':
      cc = os.path.join(LLVM_BOOTSTRAP_INSTALL_DIR, 'bin', 'clang-cl.exe')
      cxx = os.path.join(LLVM_BOOTSTRAP_INSTALL_DIR, 'bin', 'clang-cl.exe')
      lld = os.path.join(LLVM_BOOTSTRAP_INSTALL_DIR, 'bin', 'lld-link.exe')
      # CMake has a hard time with backslashes in compiler paths:
      # https://stackoverflow.com/questions/13050827
      cc = cc.replace('\\', '/')
      cxx = cxx.replace('\\', '/')
      lld = lld.replace('\\', '/')
    else:
      cc = os.path.join(LLVM_BOOTSTRAP_INSTALL_DIR, 'bin', 'clang')
      cxx = os.path.join(LLVM_BOOTSTRAP_INSTALL_DIR, 'bin', 'clang++')
    if sys.platform.startswith('linux'):
      base_cmake_args.append('-DLLVM_ENABLE_LLD=ON')

    if args.gcc_toolchain:
      # Tell the bootstrap compiler where to find the standard library headers
      # and shared object files.
      cflags.append('--gcc-toolchain=' + args.gcc_toolchain)
      cxxflags.append('--gcc-toolchain=' + args.gcc_toolchain)

    print('Bootstrap compiler installed.')

  if args.pgo:
    print('Building instrumented compiler')
    if os.path.exists(LLVM_INSTRUMENTED_DIR):
      RmTree(LLVM_INSTRUMENTED_DIR)
    EnsureDirExists(LLVM_INSTRUMENTED_DIR)
    os.chdir(LLVM_INSTRUMENTED_DIR)

    projects = 'clang'
    if sys.platform == 'darwin':
      projects += ';libcxx;compiler-rt'

    instrument_args = base_cmake_args + [
        '-DLLVM_ENABLE_PROJECTS=' + projects,
        '-DCMAKE_C_FLAGS=' + ' '.join(cflags),
        '-DCMAKE_CXX_FLAGS=' + ' '.join(cxxflags),
        '-DCMAKE_EXE_LINKER_FLAGS=' + ' '.join(ldflags),
        '-DCMAKE_SHARED_LINKER_FLAGS=' + ' '.join(ldflags),
        '-DCMAKE_MODULE_LINKER_FLAGS=' + ' '.join(ldflags),
        # Build with instrumentation.
        '-DLLVM_BUILD_INSTRUMENTED=IR',
    ]
    # Build with the bootstrap compiler.
    if cc is not None:  instrument_args.append('-DCMAKE_C_COMPILER=' + cc)
    if cxx is not None: instrument_args.append('-DCMAKE_CXX_COMPILER=' + cxx)
    if lld is not None: instrument_args.append('-DCMAKE_LINKER=' + lld)
    if args.thinlto:
      instrument_args.append('-DLLVM_ENABLE_LTO=Thin')

    RunCommand(['cmake'] + instrument_args + [os.path.join(LLVM_DIR, 'llvm')],
               msvc_arch='x64')
    CopyLibstdcpp(args, LLVM_INSTRUMENTED_DIR)
    RunCommand(['ninja'], msvc_arch='x64')
    print('Instrumented compiler built.')

    # Train by building some C++ code.
    #
    # pgo_training-1.ii is a preprocessed (on Linux) version of
    # src/third_party/blink/renderer/core/layout/layout_object.cc, selected
    # because it's a large translation unit in Blink, which is normally the
    # slowest part of Chromium to compile. Using this, we get ~20% shorter
    # build times for Linux, Android, and Mac, which is also what we got when
    # training by actually building a target in Chromium. (For comparison, a
    # C++-y "Hello World" program only resulted in 14% faster builds.)
    # See https://crbug.com/966403#c16 for all numbers.
    #
    # Although the training currently only exercises Clang, it does involve LLVM
    # internals, and so LLD also benefits when used for ThinLTO links.
    #
    # NOTE: Tidy uses binaries built with this profile, but doesn't seem to
    # gain much from it. If tidy's execution time becomes a concern, it might
    # be good to investigate that.
    #
    # TODO(hans): Enhance the training, perhaps by including preprocessed code
    # from more platforms, and by doing some linking so that lld can benefit
    # from PGO as well. Perhaps the training could be done asynchronously by
    # dedicated buildbots that upload profiles to the cloud.
    training_source = 'pgo_training-1.ii'
    with open(training_source, 'wb') as f:
      DownloadUrl(CDS_URL + '/' + training_source, f)
    train_cmd = [os.path.join(LLVM_INSTRUMENTED_DIR, 'bin', 'clang++'),
                '-target', 'x86_64-unknown-unknown', '-O2', '-g', '-std=c++14',
                 '-fno-exceptions', '-fno-rtti', '-w', '-c', training_source]
    if sys.platform == 'darwin':
      train_cmd.extend(['-stdlib=libc++', '-isysroot', isysroot])
    RunCommand(train_cmd, msvc_arch='x64')

    # Merge profiles.
    profdata = os.path.join(LLVM_BOOTSTRAP_INSTALL_DIR, 'bin', 'llvm-profdata')
    RunCommand([profdata, 'merge', '-output=' + LLVM_PROFDATA_FILE] +
                glob.glob(os.path.join(LLVM_INSTRUMENTED_DIR, 'profiles',
                                       '*.profraw')), msvc_arch='x64')
    print('Profile generated.')

  compiler_rt_args = [
    '-DCOMPILER_RT_BUILD_CRT=OFF',
    '-DCOMPILER_RT_BUILD_LIBFUZZER=OFF',
    '-DCOMPILER_RT_BUILD_PROFILE=ON',
    '-DCOMPILER_RT_BUILD_SANITIZERS=ON',
    '-DCOMPILER_RT_BUILD_XRAY=OFF',
  ]
  if sys.platform == 'darwin':
    compiler_rt_args.extend([
        '-DCOMPILER_RT_BUILD_BUILTINS=ON',
        '-DCOMPILER_RT_ENABLE_IOS=ON',
        '-DCOMPILER_RT_ENABLE_WATCHOS=OFF',
        '-DCOMPILER_RT_ENABLE_TVOS=OFF',
        # armv7 is A5 and earlier, armv7s is A6+ (2012 and later, before 64-bit
        # iPhones). armv7k is Apple Watch, which we don't need.
        '-DDARWIN_ios_ARCHS=armv7;armv7s;arm64',
        '-DDARWIN_iossim_ARCHS=i386;x86_64',
        ])
    if args.bootstrap:
      # mac/arm64 needs MacOSX11.0.sdk. System Xcode (+ SDK) on the chrome bots
      # is something much older.
      # Options:
      # - temporarily set system Xcode to Xcode 12 beta while running this
      #   script, (cf build/swarming_xcode_install.py, but it looks unused)
      # - use Xcode 12 beta for everything on tot bots, only need to fuzz with
      #   scripts/slave/recipes/chromium_upload_clang.py then (but now the
      #   chrome/ios build will use the 11.0 SDK too and we'd be on the hook for
      #   keeping it green -- if it's currently green, who knows)
      # - pass flags to cmake to try to coax it into using Xcode 12 beta for the
      #   LLVM build without it being system Xcode.
      #
      # The last option seems best, so let's go with that. We need to pass
      # -isysroot to the 11.0 SDK and -B to the /usr/bin so that the new ld64 is
      # used.
      # The compiler-rt build overrides -isysroot flags set via cflags, and we
      # only need to use the 11 SDK for the compiler-rt build. So set only
      # DARWIN_macosx_CACHED_SYSROOT to the 11.0 SDK and use the regular SDK
      # for the rest of the build. (The new ld is used for all links.)
      sys.path.insert(1, os.path.join(CHROMIUM_DIR, 'build'))
      import mac_toolchain
      LLVM_XCODE = os.path.join(THIRD_PARTY_DIR, 'llvm-xcode')
      mac_toolchain.InstallXcodeBinaries('xcode_12_beta', LLVM_XCODE)
      isysroot_11 = os.path.join(LLVM_XCODE, 'Contents', 'Developer',
                                 'Platforms', 'MacOSX.platform', 'Developer',
                                 'SDKs', 'MacOSX11.0.sdk')
      xcode_bin = os.path.join(LLVM_XCODE, 'Contents', 'Developer',
                               'Toolchains', 'XcodeDefault.xctoolchain', 'usr',
                               'bin')
      # Include an arm64 slice for libclang_rt.osx.a. This requires using
      # MacOSX11.0.sdk (via -isysroot, via DARWIN_macosx_CACHED_SYSROOT) and
      # the new ld, via -B
      compiler_rt_args.extend([
          # We don't need 32-bit intel support for macOS, we only ship 64-bit.
          '-DDARWIN_osx_ARCHS=arm64;x86_64',
          '-DDARWIN_macosx_CACHED_SYSROOT=' + isysroot_11,
      ])
      ldflags += ['-B', xcode_bin]
    else:
      compiler_rt_args.extend(['-DDARWIN_osx_ARCHS=x86_64'])
  else:
    compiler_rt_args.append('-DCOMPILER_RT_BUILD_BUILTINS=OFF')

  # LLVM uses C++11 starting in llvm 3.5. On Linux, this means libstdc++4.7+ is
  # needed, on OS X it requires libc++. clang only automatically links to libc++
  # when targeting OS X 10.9+, so add stdlib=libc++ explicitly so clang can run
  # on OS X versions as old as 10.7.
  deployment_target = ''

  if sys.platform == 'darwin' and args.bootstrap:
    # When building on 10.9, /usr/include usually doesn't exist, and while
    # Xcode's clang automatically sets a sysroot, self-built clangs don't.
    cflags = ['-isysroot', isysroot]
    cxxflags = ['-stdlib=libc++'] + cflags
    ldflags += ['-stdlib=libc++']
    deployment_target = '10.7'

  # If building at head, define a macro that plugins can use for #ifdefing
  # out code that builds at head, but not at CLANG_REVISION or vice versa.
  if args.llvm_force_head_revision:
    cflags += ['-DLLVM_FORCE_HEAD_REVISION']
    cxxflags += ['-DLLVM_FORCE_HEAD_REVISION']

  # Build PDBs for archival on Windows.  Don't use RelWithDebInfo since it
  # has different optimization defaults than Release.
  # Also disable stack cookies (/GS-) for performance.
  if sys.platform == 'win32':
    cflags += ['/Zi', '/GS-']
    cxxflags += ['/Zi', '/GS-']
    ldflags += ['/DEBUG', '/OPT:REF', '/OPT:ICF']

  deployment_env = None
  if deployment_target:
    deployment_env = os.environ.copy()
    deployment_env['MACOSX_DEPLOYMENT_TARGET'] = deployment_target

  print('Building final compiler.')

  default_tools = ['plugins', 'blink_gc_plugin', 'translation_unit']
  chrome_tools = list(set(default_tools + args.extra_tools))
  if cc is not None:  base_cmake_args.append('-DCMAKE_C_COMPILER=' + cc)
  if cxx is not None: base_cmake_args.append('-DCMAKE_CXX_COMPILER=' + cxx)
  if lld is not None: base_cmake_args.append('-DCMAKE_LINKER=' + lld)
  cmake_args = base_cmake_args + compiler_rt_args + [
      '-DCMAKE_C_FLAGS=' + ' '.join(cflags),
      '-DCMAKE_CXX_FLAGS=' + ' '.join(cxxflags),
      '-DCMAKE_EXE_LINKER_FLAGS=' + ' '.join(ldflags),
      '-DCMAKE_SHARED_LINKER_FLAGS=' + ' '.join(ldflags),
      '-DCMAKE_MODULE_LINKER_FLAGS=' + ' '.join(ldflags),
      '-DCMAKE_INSTALL_PREFIX=' + LLVM_BUILD_DIR,
      '-DCHROMIUM_TOOLS_SRC=%s' % os.path.join(CHROMIUM_DIR, 'tools', 'clang'),
      '-DCHROMIUM_TOOLS=%s' % ';'.join(chrome_tools)]
  if args.pgo:
    cmake_args.append('-DLLVM_PROFDATA_FILE=' + LLVM_PROFDATA_FILE)
  if args.thinlto:
    cmake_args.append('-DLLVM_ENABLE_LTO=Thin')
  if sys.platform == 'win32':
    cmake_args.append('-DLLVM_ENABLE_ZLIB=FORCE_ON')
  if sys.platform == 'darwin':
    cmake_args += ['-DCOMPILER_RT_ENABLE_IOS=ON',
                   '-DSANITIZER_MIN_OSX_VERSION=10.7']

  # TODO(crbug.com/962988): Use -DLLVM_EXTERNAL_PROJECTS instead.
  CreateChromeToolsShim()

  if os.path.exists(LLVM_BUILD_DIR):
    RmTree(LLVM_BUILD_DIR)
  EnsureDirExists(LLVM_BUILD_DIR)
  os.chdir(LLVM_BUILD_DIR)
  RunCommand(['cmake'] + cmake_args + [os.path.join(LLVM_DIR, 'llvm')],
             msvc_arch='x64', env=deployment_env)
  CopyLibstdcpp(args, LLVM_BUILD_DIR)
  RunCommand(['ninja'], msvc_arch='x64')

  if chrome_tools:
    # If any Chromium tools were built, install those now.
    RunCommand(['ninja', 'cr-install'], msvc_arch='x64')

  VerifyVersionOfBuiltClangMatchesVERSION()
  VerifyZlibSupport()

  if sys.platform == 'win32':
    platform = 'windows'
  elif sys.platform == 'darwin':
    platform = 'darwin'
  else:
    assert sys.platform.startswith('linux')
    platform = 'linux'
  rt_lib_dst_dir = os.path.join(LLVM_BUILD_DIR, 'lib', 'clang',
                                RELEASE_VERSION, 'lib', platform)

  # Do an out-of-tree build of compiler-rt for 32-bit Win clang_rt.profile.lib.
  if sys.platform == 'win32':
    if os.path.isdir(COMPILER_RT_BUILD_DIR):
      RmTree(COMPILER_RT_BUILD_DIR)
    os.makedirs(COMPILER_RT_BUILD_DIR)
    os.chdir(COMPILER_RT_BUILD_DIR)
    if args.bootstrap:
      # The bootstrap compiler produces 64-bit binaries by default.
      cflags += ['-m32']
      cxxflags += ['-m32']

    compiler_rt_args = base_cmake_args + [
        '-DCMAKE_C_FLAGS=' + ' '.join(cflags),
        '-DCMAKE_CXX_FLAGS=' + ' '.join(cxxflags),
        '-DCMAKE_EXE_LINKER_FLAGS=' + ' '.join(ldflags),
        '-DCMAKE_SHARED_LINKER_FLAGS=' + ' '.join(ldflags),
        '-DCMAKE_MODULE_LINKER_FLAGS=' + ' '.join(ldflags),
        '-DCOMPILER_RT_BUILD_BUILTINS=OFF',
        '-DCOMPILER_RT_BUILD_CRT=OFF',
        '-DCOMPILER_RT_BUILD_LIBFUZZER=OFF',
        '-DCOMPILER_RT_BUILD_PROFILE=ON',
        '-DCOMPILER_RT_BUILD_SANITIZERS=OFF',
        '-DCOMPILER_RT_BUILD_XRAY=OFF',
    ]
    RunCommand(['cmake'] + compiler_rt_args +
               [os.path.join(LLVM_DIR, 'llvm')],
               msvc_arch='x86', env=deployment_env)
    RunCommand(['ninja', 'compiler-rt'], msvc_arch='x86')

    # Copy select output to the main tree.
    rt_lib_src_dir = os.path.join(COMPILER_RT_BUILD_DIR, 'lib', 'clang',
                                  RELEASE_VERSION, 'lib', platform)
    # Static and dynamic libraries:
    CopyDirectoryContents(rt_lib_src_dir, rt_lib_dst_dir)

  if args.with_android:
    # TODO(thakis): Now that the NDK uses clang, try to build all archs in
    # one LLVM build instead of building 3 times.
    toolchain_dir = ANDROID_NDK_DIR + '/toolchains/llvm/prebuilt/linux-x86_64'
    for target_arch in ['aarch64', 'arm', 'i686']:
      # Build compiler-rt runtimes needed for Android in a separate build tree.
      build_dir = os.path.join(LLVM_BUILD_DIR, 'android-' + target_arch)
      if not os.path.exists(build_dir):
        os.mkdir(os.path.join(build_dir))
      os.chdir(build_dir)
      target_triple = target_arch
      if target_arch == 'arm':
        target_triple = 'armv7'
      api_level = '21' if target_arch == 'aarch64' else '19'
      target_triple += '-linux-android' + api_level
      cflags = [
          '--target=' + target_triple,
          '--sysroot=%s/sysroot' % toolchain_dir,
          '--gcc-toolchain=' + toolchain_dir,
      ]
      android_args = base_cmake_args + [
        '-DCMAKE_C_COMPILER=' + os.path.join(LLVM_BUILD_DIR, 'bin/clang'),
        '-DCMAKE_CXX_COMPILER=' + os.path.join(LLVM_BUILD_DIR, 'bin/clang++'),
        '-DLLVM_CONFIG_PATH=' + os.path.join(LLVM_BUILD_DIR, 'bin/llvm-config'),
        '-DCMAKE_C_FLAGS=' + ' '.join(cflags),
        '-DCMAKE_CXX_FLAGS=' + ' '.join(cflags),
        '-DCMAKE_ASM_FLAGS=' + ' '.join(cflags),
        '-DCOMPILER_RT_BUILD_BUILTINS=OFF',
        '-DCOMPILER_RT_BUILD_CRT=OFF',
        '-DCOMPILER_RT_BUILD_LIBFUZZER=OFF',
        '-DCOMPILER_RT_BUILD_PROFILE=ON',
        '-DCOMPILER_RT_BUILD_SANITIZERS=ON',
        '-DCOMPILER_RT_BUILD_XRAY=OFF',
        '-DSANITIZER_CXX_ABI=libcxxabi',
        '-DCMAKE_SHARED_LINKER_FLAGS=-Wl,-u__cxa_demangle',
        '-DANDROID=1']
      RunCommand(['cmake'] + android_args + [COMPILER_RT_DIR])

      # We use ASan i686 build for fuzzing.
      libs_want = ['lib/linux/libclang_rt.asan-{0}-android.so']
      if target_arch in ['aarch64', 'arm']:
        libs_want += [
            'lib/linux/libclang_rt.ubsan_standalone-{0}-android.so',
            'lib/linux/libclang_rt.profile-{0}-android.a',
        ]
      if target_arch == 'aarch64':
        libs_want += ['lib/linux/libclang_rt.hwasan-{0}-android.so']
      libs_want = [lib.format(target_arch) for lib in libs_want]
      RunCommand(['ninja'] + libs_want)

      # And copy them into the main build tree.
      for p in libs_want:
        shutil.copy(p, rt_lib_dst_dir)

  if args.with_fuchsia:
    # Fuchsia links against libclang_rt.builtins-<arch>.a instead of libgcc.a.
    for target_arch in ['aarch64', 'x86_64']:
      fuchsia_arch_name = {'aarch64': 'arm64', 'x86_64': 'x64'}[target_arch]
      toolchain_dir = os.path.join(
          FUCHSIA_SDK_DIR, 'arch', fuchsia_arch_name, 'sysroot')
      # Build clang_rt runtime for Fuchsia in a separate build tree.
      build_dir = os.path.join(LLVM_BUILD_DIR, 'fuchsia-' + target_arch)
      if not os.path.exists(build_dir):
        os.mkdir(os.path.join(build_dir))
      os.chdir(build_dir)
      target_spec = target_arch + '-fuchsia'
      # TODO(thakis): Might have to pass -B here once sysroot contains
      # binaries (e.g. gas for arm64?)
      fuchsia_args = base_cmake_args + [
        '-DCMAKE_C_COMPILER=' + os.path.join(LLVM_BUILD_DIR, 'bin/clang'),
        '-DCMAKE_CXX_COMPILER=' + os.path.join(LLVM_BUILD_DIR, 'bin/clang++'),
        '-DCMAKE_LINKER=' + os.path.join(LLVM_BUILD_DIR, 'bin/clang'),
        '-DCMAKE_AR=' + os.path.join(LLVM_BUILD_DIR, 'bin/llvm-ar'),
        '-DLLVM_CONFIG_PATH=' + os.path.join(LLVM_BUILD_DIR, 'bin/llvm-config'),
        '-DCMAKE_SYSTEM_NAME=Fuchsia',
        '-DCMAKE_C_COMPILER_TARGET=%s-fuchsia' % target_arch,
        '-DCMAKE_ASM_COMPILER_TARGET=%s-fuchsia' % target_arch,
        '-DCOMPILER_RT_BUILD_BUILTINS=ON',
        '-DCOMPILER_RT_BUILD_CRT=OFF',
        '-DCOMPILER_RT_BUILD_LIBFUZZER=OFF',
        '-DCOMPILER_RT_BUILD_PROFILE=OFF',
        '-DCOMPILER_RT_BUILD_SANITIZERS=OFF',
        '-DCOMPILER_RT_BUILD_XRAY=OFF',
        '-DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON',
        '-DCMAKE_SYSROOT=%s' % toolchain_dir,
        # TODO(thakis|scottmg): Use PER_TARGET_RUNTIME_DIR for all platforms.
        # https://crbug.com/882485.
        '-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=ON',

        # These are necessary because otherwise CMake tries to build an
        # executable to test to see if the compiler is working, but in doing so,
        # it links against the builtins.a that we're about to build.
        '-DCMAKE_C_COMPILER_WORKS=ON',
        '-DCMAKE_ASM_COMPILER_WORKS=ON',
        ]
      RunCommand(['cmake'] +
                 fuchsia_args +
                 [os.path.join(COMPILER_RT_DIR, 'lib', 'builtins')])
      builtins_a = 'libclang_rt.builtins.a'
      RunCommand(['ninja', builtins_a])

      # And copy it into the main build tree.
      fuchsia_lib_dst_dir = os.path.join(LLVM_BUILD_DIR, 'lib', 'clang',
                                         RELEASE_VERSION, 'lib', target_spec)
      if not os.path.exists(fuchsia_lib_dst_dir):
        os.makedirs(fuchsia_lib_dst_dir)
      CopyFile(os.path.join(build_dir, 'lib', target_spec, builtins_a),
               fuchsia_lib_dst_dir)

  # Run tests.
  if args.run_tests or args.llvm_force_head_revision:
    RunCommand(['ninja', '-C', LLVM_BUILD_DIR, 'cr-check-all'], msvc_arch='x64')

  if args.run_tests:
    test_targets = [ 'check-all' ]
    if sys.platform == 'darwin':
      # TODO(thakis): Run check-all on Darwin too, https://crbug.com/959361
      test_targets = [ 'check-llvm', 'check-clang', 'check-lld' ]
    RunCommand(['ninja', '-C', LLVM_BUILD_DIR] + test_targets, msvc_arch='x64')

  if sys.platform == 'darwin':
    for dylib in glob.glob(os.path.join(rt_lib_dst_dir, '*.dylib')):
      # Fix LC_ID_DYLIB for the ASan dynamic libraries to be relative to
      # @executable_path.
      # Has to happen after running tests.
      # TODO(glider): this is transitional. We'll need to fix the dylib
      # name either in our build system, or in Clang. See also
      # http://crbug.com/344836.
      subprocess.call(['install_name_tool', '-id',
                       '@executable_path/' + os.path.basename(dylib), dylib])

  WriteStampFile(PACKAGE_VERSION, STAMP_FILE)
  WriteStampFile(PACKAGE_VERSION, FORCE_HEAD_REVISION_FILE)
  print('Clang build was successful.')
  return 0


if __name__ == '__main__':
  sys.exit(main())
