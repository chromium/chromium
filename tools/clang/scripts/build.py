#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is used to build clang binaries. It is used by package.py to
create the prebuilt binaries downloaded by update.py and used by developers.

The expectation is that update.py downloads prebuilt binaries for everyone, and
nobody should run this script as part of normal development.

DEAR MAC USER: YOU NEED XCODE INSTALLED TO BUILD LLVM/CLANG WITH THIS SCRIPT.
The Xcode command line tools that are installed as part of the Chromium
development setup process are not sufficient. CMake will fail to configure, as
the non-system Clang we use will not find any standard library headers. To use
this build script on Mac:
1. Download Xcode. (Visit http://go/xcode for googlers.)
2. Install to /Applications
3. sudo xcode-select --switch /Applications/Xcode.app
"""

import argparse
import glob
import io
import json
import multiprocessing
import os
import shlex
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import urllib

from update import (CDS_URL, CHROMIUM_DIR, CLANG_REVISION, LLVM_BUILD_DIR,
                    FORCE_HEAD_REVISION_FILE, PACKAGE_VERSION, RELEASE_VERSION,
                    STAMP_FILE, THIS_DIR, DownloadUrl, DownloadAndUnpack,
                    DownloadAndUnpackPackage, EnsureDirExists, GetDefaultHostOs,
                    ReadStampFile, RmTree, WriteStampFile)

# Path constants. (All of these should be absolute paths.)
THIRD_PARTY_DIR = os.path.join(CHROMIUM_DIR, 'third_party')
LLVM_DIR = os.path.join(THIRD_PARTY_DIR, 'llvm')
COMPILER_RT_DIR = os.path.join(LLVM_DIR, 'compiler-rt')
LLVM_GIT_URL = ('https://chromium.googlesource.com/external/' +
                'github.com/llvm/llvm-project')
LLVM_BOOTSTRAP_DIR = os.path.join(THIRD_PARTY_DIR, 'llvm-bootstrap')
LLVM_BOOTSTRAP_INSTALL_DIR = os.path.join(THIRD_PARTY_DIR,
                                          'llvm-bootstrap-install')
LLVM_INSTRUMENTED_DIR = os.path.join(THIRD_PARTY_DIR, 'llvm-instrumented')
LLVM_PROFDATA_FILE = os.path.join(LLVM_INSTRUMENTED_DIR, 'profdata.prof')
LLVM_BUILD_TOOLS_DIR = os.path.abspath(
    os.path.join(LLVM_DIR, '..', 'llvm-build-tools'))
ANDROID_NDK_DIR = os.path.join(CHROMIUM_DIR, 'third_party',
                               'android_toolchain', 'ndk')
ANDROID_NDK_TOOLCHAIN_RELATIVE_DIR = os.path.join('toolchains', 'llvm',
                                                  'prebuilt', 'linux-x86_64')
ANDROID_NDK_TOOLCHAIN_DIR = os.path.join(ANDROID_NDK_DIR,
                                         ANDROID_NDK_TOOLCHAIN_RELATIVE_DIR)
FUCHSIA_SDK_DIR = os.path.join(CHROMIUM_DIR, 'third_party', 'fuchsia-sdk',
                               'sdk')
PINNED_CLANG_DIR = os.path.join(LLVM_BUILD_TOOLS_DIR, 'pinned-clang')

BUG_REPORT_URL = ('https://crbug.com in the Tools>LLVM component,'
                  ' run tools/clang/scripts/process_crashreports.py'
                  ' (only if inside Google) to upload crash related files,')

LIBXML2_VERSION = 'libxml2-v2.9.12'
ZSTD_VERSION = 'zstd-1.5.5'

win_sdk_dir = None
def GetWinSDKDir():
  """Get the location of the current SDK."""
  global win_sdk_dir
  if win_sdk_dir:
    return win_sdk_dir

  # Don't let vs_toolchain overwrite our environment.
  environ_bak = dict(os.environ)

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

  os.environ.clear()
  os.environ.update(environ_bak)
  return win_sdk_dir


def RunCommand(command, setenv=False, env=None, fail_hard=True):
  """Run command and return success (True) or failure; or if fail_hard is
     True, exit on failure.  If setenv is True, runs the command in a
     shell with the msvc tools for x64 architecture."""

  if setenv and sys.platform == 'win32':
    command = [os.path.join(CHROMIUM_DIR, 'tools', 'win', 'setenv.bat'), '&&'
               ] + command

  # https://docs.python.org/2/library/subprocess.html:
  # "On Unix with shell=True [...] if args is a sequence, the first item
  # specifies the command string, and any additional items will be treated as
  # additional arguments to the shell itself.  That is to say, Popen does the
  # equivalent of:
  #   Popen(['/bin/sh', '-c', args[0], args[1], ...])"
  #
  # We want to pass additional arguments to command[0], not to the shell,
  # so manually join everything into a single string.
  # Annoyingly, for "svn co url c:\path", shlex.quote() thinks that it should
  # quote c:\path but svn can't handle quoted paths on Windows.  Since on
  # Windows follow-on args are passed to args[0] instead of the shell, don't
  # do the single-string transformation there.
  if sys.platform != 'win32':
    command = ' '.join([shlex.quote(c) for c in command])
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


def CheckoutGitRepo(name, git_url, commit, dir):
  """Checkout the git repo at a certain git commit in dir. Any local
  modifications in dir will be lost."""

  print(f'Checking out {name} {commit} into {dir}')

  # Try updating the current repo if it exists and has no local diff.
  if os.path.isdir(dir):
    os.chdir(dir)
    # git diff-index --exit-code returns 0 when there is no diff.
    # Also check that the first commit is reachable.
    if (RunCommand(['git', 'diff-index', '--exit-code', 'HEAD'],
                   fail_hard=False)
        and RunCommand(['git', 'fetch'], fail_hard=False)
        and RunCommand(['git', 'checkout', commit], fail_hard=False)
        and RunCommand(['git', 'clean', '-f'], fail_hard=False)):
      return

    # If we can't use the current repo, delete it.
    os.chdir(CHROMIUM_DIR)  # Can't remove dir if we're in it.
    print('Removing %s.' % dir)
    RmTree(dir)

  clone_cmd = ['git', 'clone', git_url, dir]

  if RunCommand(clone_cmd, fail_hard=False):
    os.chdir(dir)
    if RunCommand(['git', 'checkout', commit], fail_hard=False):
      return

  print('CheckoutGitRepo failed.')
  sys.exit(1)


def GitCherryPick(git_repository, git_remote, commit):
  print(f'Cherry-picking {commit} in {git_repository} from {git_remote}')
  git_cmd = ['git', '-C', git_repository]
  RunCommand(git_cmd + ['remote', 'add', 'github', git_remote], fail_hard=False)
  RunCommand(git_cmd + ['fetch', '--recurse-submodules=no', 'github', commit])
  is_ancestor = RunCommand(git_cmd +
                           ['merge-base', '--is-ancestor', commit, 'HEAD'],
                           fail_hard=False)
  if is_ancestor:
    print('Commit already an ancestor; skipping.')
    return
  RunCommand([
      'git', '-C', git_repository, 'cherry-pick', '--keep-redundant-commits',
      commit
  ])


def GetLatestLLVMCommit():
  """Get the latest commit hash in the LLVM monorepo."""
  main = json.loads(
      urllib.request.urlopen('https://chromium.googlesource.com/external/' +
                             'github.com/llvm/llvm-project/' +
                             '+/refs/heads/main?format=JSON').read().decode(
                                 "utf-8").replace(")]}'", ""))
  return main['commit']


def GetCommitDescription(commit):
  """Get the output of `git describe`.

  Needs to be called from inside the git repository dir."""
  git_exe = 'git.bat' if sys.platform.startswith('win') else 'git'
  return subprocess.check_output([
      git_exe, 'describe', '--long', '--abbrev=8', '--match=*llvmorg-*-init',
      commit
  ], universal_newlines=True).rstrip()


def AddCMakeToPath():
  """Download CMake and add it to PATH."""
  if sys.platform == 'win32':
    zip_name = 'cmake-3.26.4-windows-x86_64.zip'
    dir_name = ['cmake-3.26.4-windows-x86_64', 'bin']
  elif sys.platform == 'darwin':
    zip_name = 'cmake-3.26.4-macos-universal.tar.gz'
    dir_name = ['cmake-3.26.4-macos-universal', 'CMake.app', 'Contents', 'bin']
  else:
    zip_name = 'cmake-3.26.4-linux-x86_64.tar.gz'
    dir_name = ['cmake-3.26.4-linux-x86_64', 'bin']

  cmake_dir = os.path.join(LLVM_BUILD_TOOLS_DIR, *dir_name)
  if not os.path.exists(cmake_dir):
    DownloadAndUnpack(CDS_URL + '/tools/' + zip_name, LLVM_BUILD_TOOLS_DIR)
  os.environ['PATH'] = cmake_dir + os.pathsep + os.environ.get('PATH', '')


def AddGnuWinToPath():
  """Download some GNU win tools and add them to PATH."""
  assert sys.platform == 'win32'

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


def AddZlibToPath(dry_run = False):
  """Download and build zlib, and add to PATH."""
  zlib_dir = os.path.join(LLVM_BUILD_TOOLS_DIR, 'zlib-1.2.11')
  if dry_run:
    return zlib_dir

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
  RunCommand(['cl.exe'] + [f + '.c' for f in zlib_files] + cl_flags,
             setenv=True)
  RunCommand(['lib.exe'] + [f + '.obj'
                            for f in zlib_files] + ['/nologo', '/out:zlib.lib'],
             setenv=True)
  # Remove the test directory so it isn't found when trying to find
  # test.exe.
  shutil.rmtree('test')

  os.environ['PATH'] = zlib_dir + os.pathsep + os.environ.get('PATH', '')
  return zlib_dir


class LibXmlDirs:
  def __init__(self):
    self.unzip_dir = LLVM_BUILD_TOOLS_DIR
    # When unpacked in `unzip_dir`, this will be the directory where the
    # sources are found.
    self.src_dir = os.path.join(self.unzip_dir, LIBXML2_VERSION)
    # The lib is built in a directory under its sources.
    self.build_dir = os.path.join(self.src_dir, 'build')
    # The lib is installed in a directory under where its built.
    self.install_dir = os.path.join(self.build_dir, 'install')
    # The full path to installed include files.
    self.include_dir = os.path.join(self.install_dir, 'include', 'libxml2')
    # The full path to installed lib files.
    self.lib_dir = os.path.join(self.install_dir, 'lib')


def GetLibXml2Dirs():
  """Gets the set of directories where LibXml2 is located.

  Includes the diractories where the source is unpacked, where it is built,
  and installed."""
  return LibXmlDirs()


def BuildLibXml2():
  """Download and build libxml2"""
  # The .tar.gz on GCS was uploaded as follows.
  # The gitlab page has more up-to-date packages than http://xmlsoft.org/,
  # and the official releases on xmlsoft.org are only available over ftp too.
  # $ VER=v2.9.12
  # $ curl -O \
  #   https://gitlab.gnome.org/GNOME/libxml2/-/archive/$VER/libxml2-$VER.tar.gz
  # $ gsutil cp -n -a public-read libxml2-$VER.tar.gz \
  #   gs://chromium-browser-clang/tools

  dirs = GetLibXml2Dirs()
  if os.path.exists(dirs.src_dir):
    RmTree(dirs.src_dir)
  zip_name = LIBXML2_VERSION + '.tar.gz'
  DownloadAndUnpack(CDS_URL + '/tools/' + zip_name, dirs.unzip_dir)
  os.mkdir(dirs.build_dir)
  os.chdir(dirs.build_dir)

  # Disable everything except WITH_TREE and WITH_OUTPUT, both needed by LLVM's
  # WindowsManifestMerger.
  # Also enable WITH_THREADS, else libxml doesn't compile on Linux.
  RunCommand(
      [
          'cmake',
          '-GNinja',
          '-DCMAKE_BUILD_TYPE=Release',
          '-DCMAKE_INSTALL_PREFIX=install',
          '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded',  # /MT to match LLVM.
          '-DBUILD_SHARED_LIBS=OFF',
          '-DLIBXML2_WITH_C14N=OFF',
          '-DLIBXML2_WITH_CATALOG=OFF',
          '-DLIBXML2_WITH_DEBUG=OFF',
          '-DLIBXML2_WITH_DOCB=OFF',
          '-DLIBXML2_WITH_FTP=OFF',
          '-DLIBXML2_WITH_HTML=OFF',
          '-DLIBXML2_WITH_HTTP=OFF',
          '-DLIBXML2_WITH_ICONV=OFF',
          '-DLIBXML2_WITH_ICU=OFF',
          '-DLIBXML2_WITH_ISO8859X=OFF',
          '-DLIBXML2_WITH_LEGACY=OFF',
          '-DLIBXML2_WITH_LZMA=OFF',
          '-DLIBXML2_WITH_MEM_DEBUG=OFF',
          '-DLIBXML2_WITH_MODULES=OFF',
          '-DLIBXML2_WITH_OUTPUT=ON',
          '-DLIBXML2_WITH_PATTERN=OFF',
          '-DLIBXML2_WITH_PROGRAMS=OFF',
          '-DLIBXML2_WITH_PUSH=OFF',
          '-DLIBXML2_WITH_PYTHON=OFF',
          '-DLIBXML2_WITH_READER=OFF',
          '-DLIBXML2_WITH_REGEXPS=OFF',
          '-DLIBXML2_WITH_RUN_DEBUG=OFF',
          '-DLIBXML2_WITH_SAX1=OFF',
          '-DLIBXML2_WITH_SCHEMAS=OFF',
          '-DLIBXML2_WITH_SCHEMATRON=OFF',
          '-DLIBXML2_WITH_TESTS=OFF',
          '-DLIBXML2_WITH_THREADS=ON',
          '-DLIBXML2_WITH_THREAD_ALLOC=OFF',
          '-DLIBXML2_WITH_TREE=ON',
          '-DLIBXML2_WITH_VALID=OFF',
          '-DLIBXML2_WITH_WRITER=OFF',
          '-DLIBXML2_WITH_XINCLUDE=OFF',
          '-DLIBXML2_WITH_XPATH=OFF',
          '-DLIBXML2_WITH_XPTR=OFF',
          '-DLIBXML2_WITH_ZLIB=OFF',
          '..',
      ],
      setenv=True)
  RunCommand(['ninja', 'install'], setenv=True)

  if sys.platform == 'win32':
    libxml2_lib = os.path.join(dirs.lib_dir, 'libxml2s.lib')
  else:
    libxml2_lib = os.path.join(dirs.lib_dir, 'libxml2.a')
  extra_cmake_flags = [
      '-DLLVM_ENABLE_LIBXML2=FORCE_ON',
      '-DLIBXML2_INCLUDE_DIR=' + dirs.include_dir.replace('\\', '/'),
      '-DLIBXML2_LIBRARIES=' + libxml2_lib.replace('\\', '/'),
      '-DLIBXML2_LIBRARY=' + libxml2_lib.replace('\\', '/'),

      # This hermetic libxml2 has enough features enabled for lld-link, but not
      # for the libxml2 usage in libclang. We don't need libxml2 support in
      # libclang, so just turn that off.
      '-DCLANG_ENABLE_LIBXML2=NO',
  ]
  extra_cflags = ['-DLIBXML_STATIC']

  return extra_cmake_flags, extra_cflags


class ZStdDirs:
  """
  The set of directories where zstd is located.

  Includes the diractories where the source is unpacked, where it is built,
  and installed.
  """
  def __init__(self):
    self.unzip_dir = LLVM_BUILD_TOOLS_DIR
    # When unpacked in `unzip_dir`, this will be the directory where the
    # sources are found.
    self.src_dir = os.path.join(self.unzip_dir, ZSTD_VERSION)
    # The lib is built in a directory under its sources. Note, zstd uses
    # build/cmake for cmake.
    self.build_dir = os.path.join(self.src_dir, 'cmake_build')
    # The lib is installed in a directory under where its built.
    self.install_dir = os.path.join(self.build_dir, 'install')
    # The full path to installed include files.
    self.include_dir = os.path.join(self.install_dir, 'include')
    # The full path to installed lib files.
    self.lib_dir = os.path.join(self.install_dir, 'lib')


def BuildZStd():
  """Download and build zstd lib"""
  # The zstd-1.5.5.tar.gz was downloaded from
  #   https://github.com/facebook/zstd/releases/
  # and uploaded as follows.
  # $ gsutil cp -n -a public-read zstd-$VER.tar.gz \
  #   gs://chromium-browser-clang/tools

  dirs = ZStdDirs()
  if os.path.exists(dirs.src_dir):
    RmTree(dirs.src_dir)
  zip_name = ZSTD_VERSION + '.tar.gz'
  DownloadAndUnpack(CDS_URL + '/tools/' + zip_name, dirs.unzip_dir)
  os.mkdir(dirs.build_dir)
  os.chdir(dirs.build_dir)

  RunCommand(
      [
          'cmake',
          '-GNinja',
          '-DCMAKE_BUILD_TYPE=Release',
          '-DCMAKE_INSTALL_PREFIX=install',
          '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded',  # /MT to match LLVM.
          '-DZSTD_BUILD_SHARED=OFF',
          '../build/cmake',
      ],
      setenv=True)
  RunCommand(['ninja', 'install'], setenv=True)

  if sys.platform == 'win32':
    zstd_lib = os.path.join(dirs.lib_dir, 'zstd_static.lib')
  else:
    zstd_lib = os.path.join(dirs.lib_dir, 'libzstd.a')
  extra_cmake_flags = [
      '-DLLVM_ENABLE_ZSTD=ON',
      '-DLLVM_USE_STATIC_ZSTD=ON',
      '-Dzstd_INCLUDE_DIR=' + dirs.include_dir.replace('\\', '/'),
      '-Dzstd_LIBRARY=' + zstd_lib.replace('\\', '/'),
  ]
  extra_cflags = []

  return extra_cmake_flags, extra_cflags


def DownloadPinnedClang():
  PINNED_CLANG_VERSION = 'llvmorg-17-init-16420-g0c545a44-1'
  DownloadAndUnpackPackage('clang', PINNED_CLANG_DIR, GetDefaultHostOs(),
                           PINNED_CLANG_VERSION)


def VerifyVersionOfBuiltClangMatchesVERSION():
  """Checks that `clang --version` outputs RELEASE_VERSION. If this
  fails, update.RELEASE_VERSION is out-of-date and needs to be updated (possibly
  in an `if args.llvm_force_head_revision:` block inupdate. main() first)."""
  clang = os.path.join(LLVM_BUILD_DIR, 'bin', 'clang')
  if sys.platform == 'win32':
    clang += '-cl.exe'
  version_out = subprocess.check_output([clang, '--version'],
                                        universal_newlines=True)
  version_out = re.match(r'clang version ([0-9]+)', version_out).group(1)
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
    clang += '.exe'
    test_file = 'nul'

  print('Checking for zlib support')
  clang_out = subprocess.check_output([
      clang, '-target', 'x86_64-unknown-linux-gnu', '-gz', '-c', '-###', '-x',
      'c', test_file
  ],
                                      stderr=subprocess.STDOUT,
                                      universal_newlines=True)
  if (re.search(r'--compress-debug-sections', clang_out)):
    print('OK')
  else:
    print(('Failed to detect zlib support!\n\n(driver output: %s)') % clang_out)
    sys.exit(1)


def VerifyZStdSupport():
  """Check that lld was built with zstd support enabled."""
  lld = os.path.join(LLVM_BUILD_DIR, 'bin')
  if sys.platform == 'win32':
    lld = os.path.join(lld, 'lld-link.exe')
  elif sys.platform == 'linux':
    lld = os.path.join(lld, 'ld.lld')
  else:
    print('zstd support check cannot be performed on the unsupported ' \
          'platform ' + sys.platform)
    return

  print('Checking for zstd support')
  lld_out = subprocess.run([lld, '--compress-debug-sections=zstd'],
                           check=False,
                           stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT,
                           universal_newlines=True).stdout
  if '--compress-debug-sections: zstd is not available' in lld_out:
    print(('Failed to detect zlib support!\n\n(driver output: %s)') % lld_out)
    sys.exit(1)
  else:
    print('OK')


def DownloadDebianSysroot(platform_name, skip_download=False):
  # Download sysroots. This uses basically Chromium's sysroots, but with
  # minor changes:
  # - glibc version bumped to 2.18 to make __cxa_thread_atexit_impl
  #   work (clang can require 2.18; chromium currently doesn't)
  # - libcrypt.so.1 reversioned so that crypt() is picked up from glibc
  # The sysroot was built at
  # https://chromium-review.googlesource.com/c/chromium/src/+/5506275/1
  # and the hashes here are from sysroots.json in that CL.
  toolchain_bucket = 'https://commondatastorage.googleapis.com/chrome-linux-sysroot/'

  hashes = {
      # hash from https://chromium-review.googlesource.com/c/chromium/src/+/5506275/1/build/linux/sysroot_scripts/sysroots.json#3
      'amd64': 'dec7a3a0fc5b83b909cba1b6d119077e0429a138eadef6bf5a0f2e03b1904631',
      # hash from https://chromium-review.googlesource.com/c/chromium/src/+/5506275/1/build/linux/sysroot_scripts/sysroots.json#21
      'i386': 'b53933120bb08ffc38140a817e3f0f99782254a6bf9622271574fa004e8783a4',
      # hash from https://chromium-review.googlesource.com/c/chromium/src/+/5506275/1/build/linux/sysroot_scripts/sysroots.json#15
      'arm': 'fe81e7114b97440262bce004caf02c1514732e2fa7f99693b2836932ad1c4626',
      # hash from https://chromium-review.googlesource.com/c/chromium/src/+/5506275/1/build/linux/sysroot_scripts/sysroots.json#21
      'arm64': '308e23faba3174bd01accfe358467b8a40fad4db4c49ef629da30219f65a275f',
  }

  toolchain_name = f'debian_bullseye_{platform_name}_sysroot'
  output = os.path.join(LLVM_BUILD_TOOLS_DIR, toolchain_name)
  U = toolchain_bucket + hashes[platform_name]
  if not skip_download:
    DownloadAndUnpack(U, output)

  return output


def compiler_rt_cmake_flags(*, sanitizers, profile):
  # Don't set -DCOMPILER_RT_BUILD_BUILTINS=ON/OFF as it interferes with the
  # runtimes logic of building builtins.
  args = [
      # Build crtbegin/crtend. It's just two tiny TUs, so just enable this
      # everywhere, even though we only need it on Linux.
      'COMPILER_RT_BUILD_CRT=ON',
      'COMPILER_RT_BUILD_LIBFUZZER=OFF',
      # Turn off ctx_profile because it depends on the sanitizer libraries,
      # which we don't always build.
      'COMPILER_RT_BUILD_CTX_PROFILE=OFF',
      'COMPILER_RT_BUILD_MEMPROF=OFF',
      'COMPILER_RT_BUILD_ORC=OFF',
      'COMPILER_RT_BUILD_PROFILE=' + ('ON' if profile else 'OFF'),
      'COMPILER_RT_BUILD_SANITIZERS=' + ('ON' if sanitizers else 'OFF'),
      'COMPILER_RT_BUILD_XRAY=OFF',
      # See crbug.com/1205046: don't build scudo (and others we don't need).
      'COMPILER_RT_SANITIZERS_TO_BUILD=asan;dfsan;msan;hwasan;tsan;cfi',
      # We explicitly list all targets we want to build, do not autodetect
      # targets.
      'COMPILER_RT_DEFAULT_TARGET_ONLY=ON',
  ]
  return args


def gn_arg(v):
  if v == 'True':
    return True
  if v == 'False':
    return False
  raise argparse.ArgumentTypeError('Expected one of %r or %r' % (
      'True', 'False'))


def main():
  parser = argparse.ArgumentParser(description='Build Clang.')
  parser.add_argument('--bootstrap',
                      action='store_true',
                      help='first build clang with CC, then with itself.')
  parser.add_argument('--disable-asserts', action='store_true',
                      help='build with asserts disabled')
  parser.add_argument('--host-cc',
                      help='build with host C compiler, requires --host-cxx as '
                      'well')
  parser.add_argument('--host-cxx',
                      help='build with host C++ compiler, requires --host-cc '
                      'as well')
  parser.add_argument('--pgo', action='store_true', help='build with PGO')
  parser.add_argument('--thinlto',
                      action='store_true',
                      help='build with ThinLTO')
  parser.add_argument('--bolt', action='store_true', help='build with BOLT')
  parser.add_argument('--llvm-force-head-revision', action='store_true',
                      help='build the latest revision')
  parser.add_argument('--run-tests', action='store_true',
                      help='run tests after building')
  parser.add_argument('--skip-build', action='store_true',
                      help='do not build anything')
  parser.add_argument('--skip-checkout', action='store_true',
                      help='do not create or update any checkouts')
  parser.add_argument('--build-dir',
                      help='Override build directory')
  parser.add_argument('--install-dir',
                      help='override the install directory for the final '
                      'compiler. If not specified, no install happens for '
                      'the compiler.')
  parser.add_argument('--no-tools',
                      action='store_true',
                      help='don\'t build any chromium tools or '
                      'clang-extra-tools. Overrides --extra-tools.')
  parser.add_argument('--extra-tools', nargs='*', default=[],
                      help='select additional chrome tools to build')
  parser.add_argument('--no-runtimes',
                      action='store_true',
                      help='don\'t build compiler-rt, sanitizer and profile '
                      'runtimes. This is incompatible with --pgo. On Mac, '
                      'compiler-rt is always built regardless.')
  parser.add_argument('--use-system-cmake',
                      action='store_true',
                      help='use the cmake from PATH instead of downloading '
                      'and using prebuilt cmake binaries')
  parser.add_argument('--tf-path',
                      help='path to python tensorflow pip package. '
                      'Used for embedding an MLGO model')
  parser.add_argument(
      '--with-ml-inliner-model',
      help='path to MLGO inliner model to embed. Setting to '
      '\'default\', will download an official model which was '
      'trained for Chrome on Android',
      default='default' if sys.platform.startswith('linux') else '')
  parser.add_argument('--with-android', type=gn_arg, nargs='?', const=True,
                      help='build the Android ASan runtime (linux only)',
                      default=sys.platform.startswith('linux'))
  parser.add_argument('--pic',
                      action='store_true',
                      help='Uses PIC when building LLVM')
  parser.add_argument('--with-fuchsia',
                      type=gn_arg,
                      nargs='?',
                      const=True,
                      help='build the Fuchsia runtimes (linux only)',
                      default=sys.platform.startswith('linux'))
  parser.add_argument('--without-android', action='store_false',
                      help='don\'t build Android ASan runtime (linux only)',
                      dest='with_android')
  parser.add_argument('--without-fuchsia', action='store_false',
                      help='don\'t build Fuchsia clang_rt runtime (linux/mac)',
                      dest='with_fuchsia',
                      default=sys.platform in ('linux2', 'darwin'))
  parser.add_argument('--with-ccache',
                      action='store_true',
                      help='Use ccache to build the stage 1 compiler')
  parser.add_argument('--without-zstd',
                      dest='with_zstd',
                      action='store_false',
                      help='Disable zstd in the build')

  args = parser.parse_args()

  global CLANG_REVISION, PACKAGE_VERSION, LLVM_BUILD_DIR

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
  if args.no_runtimes and args.pgo:
    print('--pgo requires runtimes, can\'t use --no-runtimes')
    return 1

  if args.with_fuchsia and not os.path.exists(FUCHSIA_SDK_DIR):
    print('Fuchsia SDK not found at ' + FUCHSIA_SDK_DIR)
    print('The Fuchsia SDK is needed to build libclang_rt for Fuchsia.')
    print('Install the Fuchsia SDK by adding fuchsia to the ')
    print('target_os section in your .gclient and running hooks, ')
    print('or pass --without-fuchsia.')
    print(
        'https://chromium.googlesource.com/chromium/src/+/main/docs/fuchsia/build_instructions.md'
    )
    print('for general Fuchsia build instructions.')
    return 1

  if args.with_ml_inliner_model and not sys.platform.startswith('linux'):
    print('--with-ml-inliner-model only supports linux hosts')
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


  if args.build_dir:
    LLVM_BUILD_DIR = args.build_dir

  if args.llvm_force_head_revision:
    checkout_revision = GetLatestLLVMCommit()
  else:
    checkout_revision = CLANG_REVISION

  if not args.skip_checkout:
    CheckoutGitRepo('LLVM monorepo', LLVM_GIT_URL, checkout_revision, LLVM_DIR)

  if args.llvm_force_head_revision:
    CLANG_REVISION = GetCommitDescription(checkout_revision)
    PACKAGE_VERSION = '%s-0' % CLANG_REVISION

  print('Locally building clang %s...' % PACKAGE_VERSION)
  WriteStampFile('', STAMP_FILE)
  WriteStampFile('', FORCE_HEAD_REVISION_FILE)

  if not args.use_system_cmake:
    AddCMakeToPath()

  if sys.platform == 'win32':
    # CMake on Windows doesn't like depot_tools's ninja.bat wrapper.
    ninja_dir = os.path.join(THIRD_PARTY_DIR, 'ninja')
    os.environ['PATH'] = ninja_dir + os.pathsep + os.environ.get('PATH', '')

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

  targets = 'AArch64;ARM;LoongArch;Mips;PowerPC;RISCV;SystemZ;WebAssembly;X86'
  projects = 'clang;lld'
  if not args.no_tools:
    projects += ';clang-tools-extra'
  if args.bolt:
    projects += ';bolt'

  runtimes = ''
  # On macOS, we always need to build compiler-rt because dsymutil's link needs
  # libclang_rt.osx.a.
  if not args.no_runtimes or sys.platform == 'darwin':
    runtimes = 'compiler-rt'

  pic_default = sys.platform == 'win32'
  pic_mode = 'ON' if args.pic or pic_default else 'OFF'

  base_cmake_args = [
      '-GNinja',
      '-DCMAKE_BUILD_TYPE=Release',
      '-DLLVM_ENABLE_ASSERTIONS=%s' % ('OFF' if args.disable_asserts else 'ON'),
      f'-DLLVM_ENABLE_PROJECTS={projects}',
      f'-DLLVM_ENABLE_RUNTIMES={runtimes}',
      f'-DLLVM_TARGETS_TO_BUILD={targets}',
      f'-DLLVM_ENABLE_PIC={pic_mode}',
      '-DLLVM_ENABLE_TERMINFO=OFF',
      '-DLLVM_ENABLE_Z3_SOLVER=OFF',
      '-DCLANG_PLUGIN_SUPPORT=OFF',
      '-DCLANG_ENABLE_STATIC_ANALYZER=OFF',
      '-DCLANG_ENABLE_ARCMT=OFF',
      '-DBUG_REPORT_URL=' + BUG_REPORT_URL,
      # See crbug.com/1126219: Use native symbolizer instead of DIA
      '-DLLVM_ENABLE_DIA_SDK=OFF',
      # Link all binaries with lld. Effectively passes -fuse-ld=lld to the
      # compiler driver. On Windows, cmake calls the linker directly, so there
      # the same is achieved by passing -DCMAKE_LINKER=$lld below.
      '-DLLVM_ENABLE_LLD=ON',
      # The default value differs per platform, force it off everywhere.
      '-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF',
      # Don't use curl.
      '-DLLVM_ENABLE_CURL=OFF',
      # Build libclang.a as well as libclang.so
      '-DLIBCLANG_BUILD_STATIC=ON',
      # The Rust build (on Mac ARM at least if not others) depends on the
      # FileCheck tool which is built but not installed by default, this
      # puts it in the path for the Rust build to find and matches the
      # `bootstrap` tool:
      # https://github.com/rust-lang/rust/blob/021861aea8de20c76c7411eb8ada7e8235e3d9b5/src/bootstrap/src/core/build_steps/llvm.rs#L348
      '-DLLVM_INSTALL_UTILS=ON',
      '-DLLVM_ENABLE_ZSTD=%s' % ('ON' if args.with_zstd else 'OFF'),
  ]

  if sys.platform == 'darwin':
    isysroot = subprocess.check_output(['xcrun', '--show-sdk-path'],
                                       universal_newlines=True).rstrip()
  base_cmake_args += ['-DLLVM_ENABLE_UNWIND_TABLES=OFF']

  ccache_cmake_args = []
  if args.with_ccache:
    ccache_cmake_args.append('-DCMAKE_C_COMPILER_LAUNCHER=ccache')
    ccache_cmake_args.append('-DCMAKE_CXX_COMPILER_LAUNCHER=ccache')

  if args.host_cc or args.host_cxx:
    assert args.host_cc and args.host_cxx, \
           "--host-cc and --host-cxx need to be used together"
    cc = args.host_cc
    cxx = args.host_cxx
  else:
    if not args.skip_checkout:
      DownloadPinnedClang()
    if sys.platform == 'win32':
      cc = os.path.join(PINNED_CLANG_DIR, 'bin', 'clang-cl.exe')
      cxx = os.path.join(PINNED_CLANG_DIR, 'bin', 'clang-cl.exe')
      lld = os.path.join(PINNED_CLANG_DIR, 'bin', 'lld-link.exe')
      # CMake has a hard time with backslashes in compiler paths:
      # https://stackoverflow.com/questions/13050827
      cc = cc.replace('\\', '/')
      cxx = cxx.replace('\\', '/')
      lld = lld.replace('\\', '/')
    else:
      cc = os.path.join(PINNED_CLANG_DIR, 'bin', 'clang')
      cxx = os.path.join(PINNED_CLANG_DIR, 'bin', 'clang++')

    if sys.platform.startswith('linux'):
      base_cmake_args += [ '-DLLVM_STATIC_LINK_CXX_STDLIB=ON' ]

  if sys.platform.startswith('linux'):
    sysroot_amd64 = DownloadDebianSysroot('amd64', args.skip_checkout)
    sysroot_i386 = DownloadDebianSysroot('i386', args.skip_checkout)
    sysroot_arm = DownloadDebianSysroot('arm', args.skip_checkout)
    sysroot_arm64 = DownloadDebianSysroot('arm64', args.skip_checkout)

    # Add the sysroot to base_cmake_args.
    if platform.machine() == 'aarch64':
      base_cmake_args.append('-DCMAKE_SYSROOT=' + sysroot_arm64)
    else:
      # amd64 is the default toolchain.
      base_cmake_args.append('-DCMAKE_SYSROOT=' + sysroot_amd64)

  if sys.platform == 'win32':
    AddGnuWinToPath()

    base_cmake_args.append('-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded')

    # Require zlib compression.
    zlib_dir = AddZlibToPath()
    cflags.append('-I' + zlib_dir)
    cxxflags.append('-I' + zlib_dir)
    ldflags.append('-LIBPATH:' + zlib_dir)

    # Use rpmalloc. For faster ThinLTO linking.
    base_cmake_args.append('-DLLVM_ENABLE_RPMALLOC=ON')

    # Set a sysroot to make the build more hermetic.
    base_cmake_args.append('-DLLVM_WINSYSROOT="%s"' %
                           os.path.dirname(os.path.dirname(GetWinSDKDir())))

  # Statically link libxml2 to make lld-link not require mt.exe on Windows,
  # and to make sure lld-link output on other platforms is identical to
  # lld-link on Windows (for cross-builds).
  libxml_cmake_args, libxml_cflags = BuildLibXml2()
  base_cmake_args += libxml_cmake_args
  cflags += libxml_cflags
  cxxflags += libxml_cflags

  if args.with_zstd:
    # Statically link zstd to make lld support zstd compression for debug info.
    zstd_cmake_args, zstd_cflags = BuildZStd()
    base_cmake_args += zstd_cmake_args
    cflags += zstd_cflags
    cxxflags += zstd_cflags

  lit_excludes = []
  if sys.platform.startswith('linux'):
    lit_excludes += [
        # fstat and sunrpc tests fail due to sysroot/host mismatches
        # (crbug.com/1459187).
        '^MemorySanitizer-.* f?stat(at)?(64)?.cpp$',
        '^.*Sanitizer-.*sunrpc.*cpp$',
        # sysroot/host glibc version mismatch, crbug.com/1506551
        '^.*Sanitizer.*mallinfo2.cpp$',
    ]
  elif sys.platform == 'darwin':
    lit_excludes += [
        # Fails on macOS 14, crbug.com/332589870
        '^.*Sanitizer.*Darwin/malloc_zone.cpp$',
        # Fails with a recent ld, crbug.com/332589870
        '^.*ContinuousSyncMode/darwin-proof-of-concept.c$',
        '^.*instrprof-darwin-exports.c$',
        # Fails on our mac builds, crbug.com/346289767
        '^.*Interpreter/pretty-print.c$',
    ]
    if platform.machine() == 'arm64':
      lit_excludes += [
          # TODO(https://crbug.com/40270881): fix and re-enable
          '^.*tools/dsymutil.*$',
          '^.*AddressSanitizer-arm64-darwin.*$',
          '^.*SanitizerCommon-lsan-arm64-Darwin.*$',
          '^.*SanitizerCommon-ubsan-arm64-Darwin.*Posix/dedup_token_length_test.cpp$',
      ]

  test_env = None
  if lit_excludes:
    test_env = os.environ.copy()
    test_env['LIT_FILTER_OUT'] = '|'.join(lit_excludes)

  if args.bootstrap:
    print('Building bootstrap compiler')
    if os.path.exists(LLVM_BOOTSTRAP_DIR):
      RmTree(LLVM_BOOTSTRAP_DIR)
    EnsureDirExists(LLVM_BOOTSTRAP_DIR)
    os.chdir(LLVM_BOOTSTRAP_DIR)

    runtimes = []
    if args.pgo or sys.platform == 'darwin':
      # Need libclang_rt.profile for PGO.
      # On macOS, the bootstrap toolchain needs to have compiler-rt because
      # dsymutil's link needs libclang_rt.osx.a. Only the x86_64 osx
      # libraries are needed though, and only libclang_rt (i.e.
      # COMPILER_RT_BUILD_BUILTINS).
      runtimes.append('compiler-rt')

    bootstrap_targets = 'X86'
    if sys.platform == 'darwin':
      # Need ARM and AArch64 for building the ios clang_rt.
      bootstrap_targets += ';ARM;AArch64'
    bootstrap_args = base_cmake_args + ccache_cmake_args + [
        '-DLLVM_TARGETS_TO_BUILD=' + bootstrap_targets,
        '-DLLVM_ENABLE_PROJECTS=clang;lld',
        '-DLLVM_ENABLE_RUNTIMES=' + ';'.join(runtimes),
        '-DCMAKE_INSTALL_PREFIX=' + LLVM_BOOTSTRAP_INSTALL_DIR,
        '-DCMAKE_C_FLAGS=' + ' '.join(cflags),
        '-DCMAKE_CXX_FLAGS=' + ' '.join(cxxflags),
        '-DCMAKE_EXE_LINKER_FLAGS=' + ' '.join(ldflags),
        '-DCMAKE_SHARED_LINKER_FLAGS=' + ' '.join(ldflags),
        '-DCMAKE_MODULE_LINKER_FLAGS=' + ' '.join(ldflags),
        # Ignore args.disable_asserts for the bootstrap compiler.
        '-DLLVM_ENABLE_ASSERTIONS=ON',
    ]
    # PGO needs libclang_rt.profile but none of the other compiler-rt stuff.
    bootstrap_args.extend([
        '-D' + f
        for f in compiler_rt_cmake_flags(sanitizers=False, profile=args.pgo)
    ])
    if sys.platform == 'darwin':
      bootstrap_args.extend([
          '-DCOMPILER_RT_ENABLE_IOS=OFF',
          '-DCOMPILER_RT_ENABLE_WATCHOS=OFF',
          '-DCOMPILER_RT_ENABLE_TVOS=OFF',
          ])
      if platform.machine() == 'arm64':
        bootstrap_args.extend(['-DDARWIN_osx_ARCHS=arm64'])
      else:
        bootstrap_args.extend(['-DDARWIN_osx_ARCHS=x86_64'])

    if cc is not None:  bootstrap_args.append('-DCMAKE_C_COMPILER=' + cc)
    if cxx is not None: bootstrap_args.append('-DCMAKE_CXX_COMPILER=' + cxx)
    if lld is not None: bootstrap_args.append('-DCMAKE_LINKER=' + lld)
    RunCommand(['cmake'] + bootstrap_args + [os.path.join(LLVM_DIR, 'llvm')],
               setenv=True)
    RunCommand(['ninja'], setenv=True)
    if args.run_tests:
      RunCommand(['ninja', 'check-all'], env=test_env, setenv=True)
    RunCommand(['ninja', 'install'], setenv=True)

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

    print('Bootstrap compiler installed.')

  if args.pgo:
    print('Building instrumented compiler')
    if os.path.exists(LLVM_INSTRUMENTED_DIR):
      RmTree(LLVM_INSTRUMENTED_DIR)
    EnsureDirExists(LLVM_INSTRUMENTED_DIR)
    os.chdir(LLVM_INSTRUMENTED_DIR)

    # Disable -Wbackend-plugin for PGO builds as these warnings are harmless
    # (https://llvm.org/docs/HowToBuildWithPGO.html#building-clang-with-pgo)
    cflags += ['-Wno-backend-plugin']
    cxxflags += ['-Wno-backend-plugin']

    instrument_args = base_cmake_args + [
        '-DLLVM_ENABLE_PROJECTS=clang',
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

    RunCommand(['cmake'] + instrument_args + [os.path.join(LLVM_DIR, 'llvm')],
               setenv=True)
    RunCommand(['ninja', 'clang'], setenv=True)
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
      train_cmd.extend(['-isysroot', isysroot])
    RunCommand(train_cmd, setenv=True)

    # Merge profiles.
    profdata = os.path.join(LLVM_BOOTSTRAP_INSTALL_DIR, 'bin', 'llvm-profdata')
    RunCommand(
        [profdata, 'merge', '-output=' + LLVM_PROFDATA_FILE] +
        glob.glob(os.path.join(LLVM_INSTRUMENTED_DIR, 'profiles', '*.profraw')),
        setenv=True)
    print('Profile generated.')

  deployment_target = '10.12'

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

  # Keep static relocations in the executable for BOLT to analyze. Resolve all
  # symbols on program start to allow BOLT's PLT optimization.
  if args.bolt:
    ldflags += ['-Wl,--emit-relocs', '-Wl,-znow']

  chrome_tools = []
  if not args.no_tools:
    default_tools = [
        'plugins', 'blink_gc_plugin', 'raw_ptr_plugin', 'translation_unit'
    ]
    chrome_tools = list(set(default_tools + args.extra_tools))
  if cc is not None:  base_cmake_args.append('-DCMAKE_C_COMPILER=' + cc)
  if cxx is not None: base_cmake_args.append('-DCMAKE_CXX_COMPILER=' + cxx)
  if lld is not None: base_cmake_args.append('-DCMAKE_LINKER=' + lld)
  final_install_dir = args.install_dir if args.install_dir else LLVM_BUILD_DIR
  cmake_args = base_cmake_args + [
      '-DCMAKE_C_FLAGS=' + ' '.join(cflags),
      '-DCMAKE_CXX_FLAGS=' + ' '.join(cxxflags),
      '-DCMAKE_EXE_LINKER_FLAGS=' + ' '.join(ldflags),
      '-DCMAKE_SHARED_LINKER_FLAGS=' + ' '.join(ldflags),
      '-DCMAKE_MODULE_LINKER_FLAGS=' + ' '.join(ldflags),
      '-DCMAKE_INSTALL_PREFIX=' + final_install_dir,
  ]
  if not args.no_tools:
    cmake_args.extend([
        '-DLLVM_EXTERNAL_PROJECTS=chrometools',
        '-DLLVM_EXTERNAL_CHROMETOOLS_SOURCE_DIR=' +
        os.path.join(CHROMIUM_DIR, 'tools', 'clang'),
        '-DCHROMIUM_TOOLS=%s' % ';'.join(chrome_tools)
    ])
  if args.pgo:
    cmake_args.append('-DLLVM_PROFDATA_FILE=' + LLVM_PROFDATA_FILE)
  if args.thinlto:
    cmake_args.append('-DLLVM_ENABLE_LTO=Thin')
  if sys.platform == 'win32':
    cmake_args.append('-DLLVM_ENABLE_ZLIB=FORCE_ON')

  # The default LLVM_DEFAULT_TARGET_TRIPLE depends on the host machine.
  # Set it explicitly to make the build of clang more hermetic, and also to
  # set it to arm64 when cross-building clang for mac/arm.
  if sys.platform == 'darwin':
    if platform.machine() == 'arm64':
      cmake_args.append('-DLLVM_DEFAULT_TARGET_TRIPLE=arm64-apple-darwin')
    else:
      cmake_args.append('-DLLVM_DEFAULT_TARGET_TRIPLE=x86_64-apple-darwin')
  elif sys.platform.startswith('linux'):
    if platform.machine() == 'aarch64':
      cmake_args.append(
          '-DLLVM_DEFAULT_TARGET_TRIPLE=aarch64-unknown-linux-gnu')
    elif platform.machine() == 'riscv64':
      cmake_args.append(
          '-DLLVM_DEFAULT_TARGET_TRIPLE=riscv64-unknown-linux-gnu')
    elif platform.machine() == 'loongarch64':
      cmake_args.append(
          '-DLLVM_DEFAULT_TARGET_TRIPLE=loongarch64-unknown-linux-gnu')
    else:
      cmake_args.append('-DLLVM_DEFAULT_TARGET_TRIPLE=x86_64-unknown-linux-gnu')
    cmake_args.append('-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=ON')
  elif sys.platform == 'win32':
    cmake_args.append('-DLLVM_DEFAULT_TARGET_TRIPLE=x86_64-pc-windows-msvc')

  # Map from triple to {
  #   "args": list of CMake vars without '-D' common to builtins and runtimes
  #   "profile": bool # build profile runtime
  #   "sanitizers": bool # build sanitizer runtimes
  # }
  runtimes_triples_args = {}

  if sys.platform.startswith('linux'):
    runtimes_triples_args['i386-unknown-linux-gnu'] = {
        "args": [
            'CMAKE_SYSROOT=%s' % sysroot_i386,
            # TODO(crbug.com/40242553): pass proper flags to i386 tests so they compile correctly
            'LLVM_INCLUDE_TESTS=OFF',
        ],
        "profile":
        True,
        "sanitizers":
        True,
    }
    runtimes_triples_args['x86_64-unknown-linux-gnu'] = {
        "args": [
            'CMAKE_SYSROOT=%s' % sysroot_amd64,
        ],
        "profile": True,
        "sanitizers": True,
    }
    # Using "armv7a-unknown-linux-gnueabhihf" confuses the compiler-rt
    # builtins build, since compiler-rt/cmake/builtin-config-ix.cmake
    # doesn't include "armv7a" in its `ARM32` list.
    # TODO(thakis): It seems to work for everything else though, see try
    # results on
    # https://chromium-review.googlesource.com/c/chromium/src/+/3702739/4
    # Maybe it should work for builtins too?
    runtimes_triples_args['armv7-unknown-linux-gnueabihf'] = {
        "args": [
            'CMAKE_SYSROOT=%s' % sysroot_arm,
            # Can't run tests on x86 host.
            'LLVM_INCLUDE_TESTS=OFF',
        ],
        "profile":
        True,
        "sanitizers":
        True,
    }
    runtimes_triples_args['aarch64-unknown-linux-gnu'] = {
        "args": [
            'CMAKE_SYSROOT=%s' % sysroot_arm64,
            # Can't run tests on x86 host.
            'LLVM_INCLUDE_TESTS=OFF',
        ],
        "profile":
        True,
        "sanitizers":
        True,
    }
  elif sys.platform == 'win32':
    sysroot = os.path.dirname(os.path.dirname(GetWinSDKDir()))
    runtimes_triples_args['i386-pc-windows-msvc'] = {
        "args": [
            'LLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF',
            'LLVM_WINSYSROOT="%s"' % sysroot,
        ],
        "profile":
        True,
        "sanitizers":
        False,
    }
    runtimes_triples_args['x86_64-pc-windows-msvc'] = {
        "args": [
            'LLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF',
            'LLVM_WINSYSROOT="%s"' % sysroot,
        ],
        "profile":
        True,
        "sanitizers":
        True,
    }
    runtimes_triples_args['aarch64-pc-windows-msvc'] = {
        "args": [
            'LLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF',
            'LLVM_WINSYSROOT="%s"' % sysroot,
            # Can't run tests on x86 host.
            'LLVM_INCLUDE_TESTS=OFF',
        ],
        "profile":
        True,
        "sanitizers":
        False,
    }
  elif sys.platform == 'darwin':
    # compiler-rt is built for all platforms/arches with a single
    # configuration, we should only specify one target triple. 'default' is
    # specially handled.
    runtimes_triples_args['default'] = {
        "args": [
            'SANITIZER_MIN_OSX_VERSION=' + deployment_target,
            'COMPILER_RT_ENABLE_MACCATALYST=ON',
            'COMPILER_RT_ENABLE_IOS=ON',
            'COMPILER_RT_ENABLE_WATCHOS=ON',
            'COMPILER_RT_ENABLE_TVOS=OFF',
            'COMPILER_RT_ENABLE_XROS=ON',
            'DARWIN_ios_ARCHS=arm64',
            'DARWIN_iossim_ARCHS=arm64;x86_64',
            'DARWIN_osx_ARCHS=arm64;x86_64',
        ],
        "sanitizers":
        True,
        "profile":
        True
    }

  if args.with_android:
    for target_arch in ['aarch64', 'arm', 'i686', 'riscv64', 'x86_64']:
      toolchain_dir = ANDROID_NDK_TOOLCHAIN_DIR
      target_triple = target_arch
      if target_arch == 'arm':
        target_triple = 'armv7'
      api_level = '21'
      if target_arch == 'riscv64':
        api_level = '35'
      target_triple += '-linux-android' + api_level
      android_cflags = [
          '--sysroot=%s/sysroot' % toolchain_dir,

          # We don't have an unwinder ready, and don't need it either.
          '--unwindlib=none',
      ]

      if target_arch == 'aarch64':
        # Use PAC/BTI instructions for AArch64
        android_cflags += ['-mbranch-protection=standard']

      android_args = [
          'LLVM_ENABLE_RUNTIMES=compiler-rt',
          # On Android, we want DWARF info for the builtins for unwinding. See
          # crbug.com/1311807.
          'CMAKE_BUILD_TYPE=RelWithDebInfo',
          'CMAKE_C_FLAGS=' + ' '.join(android_cflags),
          'CMAKE_CXX_FLAGS=' + ' '.join(android_cflags),
          'CMAKE_ASM_FLAGS=' + ' '.join(android_cflags),
          'COMPILER_RT_USE_BUILTINS_LIBRARY=ON',
          'SANITIZER_CXX_ABI=libcxxabi',
          'CMAKE_SHARED_LINKER_FLAGS=-Wl,-u__cxa_demangle',
          'ANDROID=1',
          'LLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF',
          'LLVM_INCLUDE_TESTS=OFF',
          # This prevents static_asserts from firing in 32-bit builds.
          # TODO: remove once we only support API >=24.
          'ANDROID_NATIVE_API_LEVEL=' + api_level,
      ]
      runtimes_triples_args[target_triple] = {
          "args": android_args,
          "sanitizers": True,
          "profile": True
      }

  if args.with_fuchsia:
    # Fuchsia links against libclang_rt.builtins-<arch>.a instead of libgcc.a.
    for target_arch in ['aarch64', 'x86_64']:
      fuchsia_arch_name = {'aarch64': 'arm64', 'x86_64': 'x64'}[target_arch]
      toolchain_dir = os.path.join(
          FUCHSIA_SDK_DIR, 'arch', fuchsia_arch_name, 'sysroot')
      target_triple = target_arch + '-unknown-fuchsia'
      # Build the Fuchsia profile and asan runtimes.  This is done after the rt
      # builtins have been created because the CMake build runs link checks that
      # require that the builtins already exist to succeed.
      build_profile = target_arch == 'x86_64'
      # Build the asan runtime only on non-Mac platforms.  Macs are excluded
      # because the asan install changes library RPATHs which CMake only
      # supports on ELF platforms and MacOS uses Mach-O instead of ELF.
      build_sanitizers = build_profile and sys.platform != 'darwin'
      # TODO(thakis): Might have to pass -B here once sysroot contains
      # binaries (e.g. gas for arm64?)
      fuchsia_args = [
          'LLVM_ENABLE_RUNTIMES=compiler-rt',
          'CMAKE_SYSTEM_NAME=Fuchsia',
          'CMAKE_SYSROOT=%s' % toolchain_dir,
          # TODO(thakis|scottmg): Use PER_TARGET_RUNTIME_DIR for all platforms.
          # https://crbug.com/882485.
          'LLVM_ENABLE_PER_TARGET_RUNTIME_DIR=ON',
      ]
      if build_sanitizers:
        fuchsia_args.append('SANITIZER_NO_UNDEFINED_SYMBOLS=OFF')

      runtimes_triples_args[target_triple] = {
          "args": fuchsia_args,
          "sanitizers": build_sanitizers,
          "profile": build_profile
      }

  # Embed MLGO inliner model. If tf_path is not specified, a vpython3 env
  # will be created which contains the necessary source files for compilation.
  # MLGO is only officially supported on linux. This condition is checked at
  # the top of main()
  if args.with_ml_inliner_model:
    if args.with_ml_inliner_model == 'default':
      model_path = ('https://commondatastorage.googleapis.com/'
                    'chromium-browser-clang/tools/mlgo_model3.tgz')
    else:
      model_path = args.with_ml_inliner_model
    if not args.tf_path:
      tf_path = subprocess.check_output(
          ['vpython3', os.path.join(THIS_DIR, 'get_tensorflow.py')],
          universal_newlines=True).rstrip()
    else:
      tf_path = args.tf_path
    print('Embedding MLGO inliner model at %s using Tensorflow at %s' %
          (model_path, tf_path))
    cmake_args += [
        '-DLLVM_INLINER_MODEL_PATH=%s' % model_path,
        '-DTENSORFLOW_AOT_PATH=%s' % tf_path,
        # Disable Regalloc model generation since it is unused
        '-DLLVM_RAEVICT_MODEL_PATH=none'
    ]

  # Convert FOO=BAR CMake flags per triple into
  # -DBUILTINS_$triple_FOO=BAR/-DRUNTIMES_$triple_FOO=BAR and build up
  # -DLLVM_BUILTIN_TARGETS/-DLLVM_RUNTIME_TARGETS.
  all_triples = ''
  for triple in sorted(runtimes_triples_args.keys()):
    all_triples += triple + ';'
    for arg in runtimes_triples_args[triple]["args"]:
      assert not arg.startswith('-')
      # 'default' is specially handled to pass through relevant CMake flags.
      if triple == 'default':
        cmake_args.append('-D' + arg)
      else:
        cmake_args.append('-DRUNTIMES_' + triple + '_' + arg)
        cmake_args.append('-DBUILTINS_' + triple + '_' + arg)
    if not args.no_runtimes:
      profile = runtimes_triples_args[triple]["profile"],
      sanitizers = runtimes_triples_args[triple]["sanitizers"]
    else:
      profile = False
      sanitizers = False
    for arg in compiler_rt_cmake_flags(profile=profile, sanitizers=sanitizers):
      # 'default' is specially handled to pass through relevant CMake flags.
      if triple == 'default':
        cmake_args.append('-D' + arg)
      else:
        cmake_args.append('-DRUNTIMES_' + triple + '_' + arg)

  cmake_args.append('-DLLVM_BUILTIN_TARGETS=' + all_triples)
  cmake_args.append('-DLLVM_RUNTIME_TARGETS=' + all_triples)

  if not args.bootstrap:
    cmake_args.extend(ccache_cmake_args)

  if os.path.exists(LLVM_BUILD_DIR):
    RmTree(LLVM_BUILD_DIR)
  EnsureDirExists(LLVM_BUILD_DIR)
  os.chdir(LLVM_BUILD_DIR)
  RunCommand(['cmake'] + cmake_args + [os.path.join(LLVM_DIR, 'llvm')],
             setenv=True,
             env=deployment_env)
  RunCommand(['ninja'], setenv=True)

  if chrome_tools:
    # If any Chromium tools were built, install those now.
    RunCommand(['ninja', 'cr-install'], setenv=True)

  if args.bolt:
    print('Performing BOLT post-link optimizations.')
    bolt_profiles_dir = os.path.join(LLVM_BUILD_DIR, 'bolt-profiles')
    os.mkdir(bolt_profiles_dir)

    # Instrument.
    RunCommand([
        'bin/llvm-bolt', 'bin/clang', '-o', 'bin/clang-bolt.inst',
        '-instrument', '--instrumentation-file-append-pid',
        '--instrumentation-file=' +
        os.path.join(bolt_profiles_dir, 'prof.fdata')
    ])
    RunCommand([
        'ln', '-s',
        os.path.join(LLVM_BUILD_DIR, 'bin', 'clang-bolt.inst'),
        os.path.join(LLVM_BUILD_DIR, 'bin', 'clang++-bolt.inst')
    ])

    # Train by building a part of Clang.
    os.mkdir('bolt-training')
    os.chdir('bolt-training')
    bolt_train_cmake_args = base_cmake_args + [
        '-DLLVM_TARGETS_TO_BUILD=X86',
        '-DLLVM_ENABLE_PROJECTS=clang',
        '-DCMAKE_C_FLAGS=' + ' '.join(cflags),
        '-DCMAKE_CXX_FLAGS=' + ' '.join(cxxflags),
        '-DCMAKE_EXE_LINKER_FLAGS=' + ' '.join(ldflags),
        '-DCMAKE_SHARED_LINKER_FLAGS=' + ' '.join(ldflags),
        '-DCMAKE_MODULE_LINKER_FLAGS=' + ' '.join(ldflags),
        '-DCMAKE_C_COMPILER=' +
        os.path.join(LLVM_BUILD_DIR, 'bin/clang-bolt.inst'),
        '-DCMAKE_CXX_COMPILER=' +
        os.path.join(LLVM_BUILD_DIR, 'bin/clang++-bolt.inst'),
        '-DCMAKE_ASM_COMPILER=' +
        os.path.join(LLVM_BUILD_DIR, 'bin/clang-bolt.inst'),
        '-DCMAKE_ASM_COMPILER_ID=Clang',
    ]
    RunCommand(['cmake'] + bolt_train_cmake_args +
               [os.path.join(LLVM_DIR, 'llvm')])
    RunCommand([
        'ninja', 'tools/clang/lib/Sema/CMakeFiles/obj.clangSema.dir/Sema.cpp.o'
    ])
    os.chdir(LLVM_BUILD_DIR)

    # Optimize.
    RunCommand([
        sys.executable,
        os.path.join(LLVM_DIR, 'clang', 'utils', 'perf-training',
                     'perf-helper.py'), 'merge-fdata', 'bin/merge-fdata',
        'merged.fdata', bolt_profiles_dir
    ])
    RunCommand([
        'bin/llvm-bolt', 'bin/clang', '-o', 'bin/clang-bolt.opt', '-data',
        'merged.fdata', '-reorder-blocks=ext-tsp', '-reorder-functions=hfsort+',
        '-split-functions', '-split-all-cold', '-split-eh', '-dyno-stats',
        '-icf=1', '-use-gnu-stack', '-use-old-text'
    ])

    # Overwrite clang, preserving its timestamp so ninja doesn't rebuild it.
    RunCommand(['touch', '-r', 'bin/clang', 'bin/clang-bolt.opt'])
    RunCommand(['mv', 'bin/clang-bolt.opt', 'bin/clang'])

  VerifyVersionOfBuiltClangMatchesVERSION()
  VerifyZlibSupport()
  if args.with_zstd:
    VerifyZStdSupport()

  # Run tests.
  if (chrome_tools and (args.run_tests or args.llvm_force_head_revision)):
    RunCommand(['ninja', '-C', LLVM_BUILD_DIR, 'cr-check-all'], setenv=True)

  if args.run_tests:
    RunCommand(['ninja', '-C', LLVM_BUILD_DIR, 'check-all'],
               env=test_env,
               setenv=True)
  if args.install_dir:
    RunCommand(['ninja', 'install'], setenv=True)

  WriteStampFile(PACKAGE_VERSION, STAMP_FILE)
  WriteStampFile(PACKAGE_VERSION, FORCE_HEAD_REVISION_FILE)
  print('Clang build was successful.')
  return 0


if __name__ == '__main__':
  sys.exit(main())
