#!/usr/bin/env python
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script will check out llvm and clang, and then package the results up
to a tgz file."""

from __future__ import print_function

import argparse
import fnmatch
import itertools
import os
import shutil
import subprocess
import sys
import tarfile

from update import RELEASE_VERSION, STAMP_FILE

# Path constants.
THIS_DIR = os.path.dirname(__file__)
CHROMIUM_DIR = os.path.abspath(os.path.join(THIS_DIR, '..', '..', '..'))
THIRD_PARTY_DIR = os.path.join(THIS_DIR, '..', '..', '..', 'third_party')
BUILDTOOLS_DIR = os.path.join(THIS_DIR, '..', '..', '..', 'buildtools')
LLVM_DIR = os.path.join(THIRD_PARTY_DIR, 'llvm')
LLVM_BOOTSTRAP_DIR = os.path.join(THIRD_PARTY_DIR, 'llvm-bootstrap')
LLVM_BOOTSTRAP_INSTALL_DIR = os.path.join(THIRD_PARTY_DIR,
                                          'llvm-bootstrap-install')
LLVM_BUILD_DIR = os.path.join(THIRD_PARTY_DIR, 'llvm-build')
LLVM_RELEASE_DIR = os.path.join(LLVM_BUILD_DIR, 'Release+Asserts')
EU_STRIP = os.path.join(BUILDTOOLS_DIR, 'third_party', 'eu-strip', 'bin',
                        'eu-strip')


def Tee(output, logfile):
  logfile.write(output)
  print(output, end=' ')


def TeeCmd(cmd, logfile, fail_hard=True):
  """Runs cmd and writes the output to both stdout and logfile."""
  # Reading from PIPE can deadlock if one buffer is full but we wait on a
  # different one.  To work around this, pipe the subprocess's stderr to
  # its stdout buffer and don't give it a stdin.
  # shell=True is required in cmd.exe since depot_tools has an svn.bat, and
  # bat files only work with shell=True set.
  proc = subprocess.Popen(cmd, bufsize=1, shell=sys.platform == 'win32',
                          stdin=open(os.devnull), stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT)
  for line in iter(proc.stdout.readline,''):
    Tee(line, logfile)
    if proc.poll() is not None:
      break
  exit_code = proc.wait()
  if exit_code != 0 and fail_hard:
    print('Failed:', cmd)
    sys.exit(1)


def PrintTarProgress(tarinfo):
  print('Adding', tarinfo.name)
  return tarinfo


def GetExpectedStamp():
  rev_cmd = [sys.executable, os.path.join(THIS_DIR, 'update.py'),
             '--print-revision']
  return subprocess.check_output(rev_cmd).rstrip()


def GetGsutilPath():
  if not 'find_depot_tools' in sys.modules:
    sys.path.insert(0, os.path.join(CHROMIUM_DIR, 'build'))
    global find_depot_tools
    import find_depot_tools
  depot_path = find_depot_tools.add_depot_tools_to_path()
  if depot_path is None:
    print ('depot_tools are not found in PATH. '
           'Follow the instructions in this document '
           'http://dev.chromium.org/developers/how-tos/install-depot-tools'
           ' to install depot_tools and then try again.')
    sys.exit(1)
  gsutil_path = os.path.join(depot_path, 'gsutil.py')
  return gsutil_path


def RunGsutil(args):
  return subprocess.call([sys.executable, GetGsutilPath()] + args)


def MaybeUpload(do_upload, filename, platform, extra_gsutil_args=[]):
  gsutil_args = ['cp'] + extra_gsutil_args + ['-a', 'public-read', filename,
      'gs://chromium-browser-clang-staging/%s/%s' % (platform, filename)]
  if do_upload:
    print('Uploading %s to Google Cloud Storage...' % filename)
    exit_code = RunGsutil(gsutil_args)
    if exit_code != 0:
      print("gsutil failed, exit_code: %s" % exit_code)
      sys.exit(exit_code)
  else:
    print('To upload, run:')
    print('gsutil %s' % ' '.join(gsutil_args))


def UploadPDBsToSymbolServer(binaries):
  assert sys.platform == 'win32'
  # Upload PDB and binary to the symbol server on Windows.  Put them into the
  # chromium-browser-symsrv bucket, since chrome devs have that in their
  # _NT_SYMBOL_PATH already. Executable and PDB must be at paths following a
  # certain pattern for the Microsoft debuggers to be able to load them.
  # Executable:
  #  chromium-browser-symsrv/clang-cl.exe/ABCDEFAB01234/clang-cl.ex_
  #    ABCDEFAB is the executable's timestamp in %08X format, 01234 is the
  #    executable's image size in %x format. tools/symsrc/img_fingerprint.py
  #    can compute this ABCDEFAB01234 string for us, so use that.
  #    The .ex_ instead of .exe at the end means that the file is compressed.
  # PDB:
  # gs://chromium-browser-symsrv/clang-cl.exe.pdb/AABBCCDD/clang-cl.exe.pd_
  #   AABBCCDD here is computed from the output of
  #      dumpbin /all mybinary.exe | find "Format: RSDS"
  #   but tools/symsrc/pdb_fingerprint_from_img.py can compute it already, so
  #   again just use that.
  sys.path.insert(0, os.path.join(CHROMIUM_DIR, 'tools', 'symsrc'))
  import img_fingerprint, pdb_fingerprint_from_img

  for binary_path in binaries:
    binary_path = os.path.join(LLVM_RELEASE_DIR, binary_path)
    binary_id = img_fingerprint.GetImgFingerprint(binary_path)
    (pdb_id, pdb_path) = pdb_fingerprint_from_img.GetPDBInfoFromImg(binary_path)

    # The build process builds clang.exe and then copies it to clang-cl.exe
    # (both are the same binary and they behave differently on what their
    # filename is).  Hence, the pdb is at clang.pdb, not at clang-cl.pdb.
    # Likewise, lld-link.exe's PDB file is called lld.pdb.

    # Compress and upload.
    for f, f_id in ((binary_path, binary_id), (pdb_path, pdb_id)):
      subprocess.check_call(
          ['makecab', '/D', 'CompressionType=LZX', '/D', 'CompressionMemory=21',
           f, '/L', os.path.dirname(f)], stdout=open(os.devnull, 'w'))
      f_cab = f[:-1] + '_'

      dest = '%s/%s/%s' % (os.path.basename(f), f_id, os.path.basename(f_cab))
      print('Uploading %s to Google Cloud Storage...' % dest)
      gsutil_args = ['cp', '-n', '-a', 'public-read', f_cab,
                     'gs://chromium-browser-symsrv/' + dest]
      exit_code = RunGsutil(gsutil_args)
      if exit_code != 0:
        print("gsutil failed, exit_code: %s" % exit_code)
        sys.exit(exit_code)


def main():
  parser = argparse.ArgumentParser(description='build and package clang')
  parser.add_argument('--upload', action='store_true',
                      help='Upload the target archive to Google Cloud Storage.')
  args = parser.parse_args()

  expected_stamp = GetExpectedStamp()
  pdir = 'clang-' + expected_stamp
  print(pdir)

  if sys.platform == 'darwin':
    platform = 'Mac'
  elif sys.platform == 'win32':
    platform = 'Win'
  else:
    platform = 'Linux_x64'

  with open('buildlog.txt', 'w') as log:
    Tee('Starting build\n', log)

    # Do a clobber build.
    shutil.rmtree(LLVM_BOOTSTRAP_DIR, ignore_errors=True)
    shutil.rmtree(LLVM_BOOTSTRAP_INSTALL_DIR, ignore_errors=True)
    shutil.rmtree(LLVM_BUILD_DIR, ignore_errors=True)

    build_cmd = [
        sys.executable,
        os.path.join(THIS_DIR, 'build.py'), '--bootstrap', '--disable-asserts',
        '--run-tests', '--pgo'
    ]
    if sys.platform != 'darwin':
      build_cmd.append('--thinlto')

    TeeCmd(build_cmd, log)

  stamp = open(STAMP_FILE).read().rstrip()
  if stamp != expected_stamp:
    print('Actual stamp (%s) != expected stamp (%s).' % (stamp, expected_stamp))
    return 1

  shutil.rmtree(pdir, ignore_errors=True)

  # Copy a whitelist of files to the directory we're going to tar up.
  # This supports the same patterns that the fnmatch module understands.
  # '$V' is replaced by RELEASE_VERSION further down.
  exe_ext = '.exe' if sys.platform == 'win32' else ''
  want = [
    'bin/llvm-pdbutil' + exe_ext,
    'bin/llvm-symbolizer' + exe_ext,
    'bin/llvm-undname' + exe_ext,
    # Copy built-in headers (lib/clang/3.x.y/include).
    'lib/clang/$V/include/*',
    'lib/clang/$V/share/asan_blacklist.txt',
    'lib/clang/$V/share/cfi_blacklist.txt',
  ]
  if sys.platform == 'win32':
    want.extend([
      'bin/clang-cl.exe',
      'bin/lld-link.exe',
    ])
  else:
    want.extend([
      'bin/clang',

      # Include libclang_rt.builtins.a for Fuchsia targets.
      'lib/clang/$V/lib/aarch64-fuchsia/libclang_rt.builtins.a',
      'lib/clang/$V/lib/x86_64-fuchsia/libclang_rt.builtins.a',
      'lib/clang/$V/lib/x86_64-fuchsia/libclang_rt.profile.a',
    ])
  if sys.platform == 'darwin':
    want.extend([
      # AddressSanitizer runtime.
      'lib/clang/$V/lib/darwin/libclang_rt.asan_iossim_dynamic.dylib',
      'lib/clang/$V/lib/darwin/libclang_rt.asan_osx_dynamic.dylib',

      # OS X and iOS builtin libraries for the _IsOSVersionAtLeast runtime
      # function.
      'lib/clang/$V/lib/darwin/libclang_rt.ios.a',
      'lib/clang/$V/lib/darwin/libclang_rt.iossim.a',
      'lib/clang/$V/lib/darwin/libclang_rt.osx.a',

      # Profile runtime (used by profiler and code coverage).
      'lib/clang/$V/lib/darwin/libclang_rt.profile_iossim.a',
      'lib/clang/$V/lib/darwin/libclang_rt.profile_osx.a',

      # UndefinedBehaviorSanitizer runtime.
      'lib/clang/$V/lib/darwin/libclang_rt.ubsan_iossim_dynamic.dylib',
      'lib/clang/$V/lib/darwin/libclang_rt.ubsan_osx_dynamic.dylib',
    ])
  elif sys.platform.startswith('linux'):
    want.extend([
      # Copy the stdlibc++.so.6 we linked the binaries against.
      'lib/libstdc++.so.6',

      # Add LLD.
      'bin/lld',

      # Add llvm-ar for LTO.
      'bin/llvm-ar',

      # Add llvm-objcopy for partition extraction on Android.
      'bin/llvm-objcopy',

      # AddressSanitizer C runtime (pure C won't link with *_cxx).
      'lib/clang/$V/lib/linux/libclang_rt.asan-i386.a',
      'lib/clang/$V/lib/linux/libclang_rt.asan-x86_64.a',
      'lib/clang/$V/lib/linux/libclang_rt.asan-x86_64.a.syms',

      # AddressSanitizer C++ runtime.
      'lib/clang/$V/lib/linux/libclang_rt.asan_cxx-i386.a',
      'lib/clang/$V/lib/linux/libclang_rt.asan_cxx-x86_64.a',
      'lib/clang/$V/lib/linux/libclang_rt.asan_cxx-x86_64.a.syms',

      # AddressSanitizer Android runtime.
      'lib/clang/$V/lib/linux/libclang_rt.asan-aarch64-android.so',
      'lib/clang/$V/lib/linux/libclang_rt.asan-arm-android.so',
      'lib/clang/$V/lib/linux/libclang_rt.asan-i686-android.so',

      # HWASAN Android runtime.
      'lib/clang/$V/lib/linux/libclang_rt.hwasan-aarch64-android.so',

      # MemorySanitizer C runtime (pure C won't link with *_cxx).
      'lib/clang/$V/lib/linux/libclang_rt.msan-x86_64.a',
      'lib/clang/$V/lib/linux/libclang_rt.msan-x86_64.a.syms',

      # MemorySanitizer C++ runtime.
      'lib/clang/$V/lib/linux/libclang_rt.msan_cxx-x86_64.a',
      'lib/clang/$V/lib/linux/libclang_rt.msan_cxx-x86_64.a.syms',

      # Profile runtime (used by profiler and code coverage).
      'lib/clang/$V/lib/linux/libclang_rt.profile-i386.a',
      'lib/clang/$V/lib/linux/libclang_rt.profile-x86_64.a',
      'lib/clang/$V/lib/linux/libclang_rt.profile-aarch64-android.a',
      'lib/clang/$V/lib/linux/libclang_rt.profile-arm-android.a',

      # ThreadSanitizer C runtime (pure C won't link with *_cxx).
      'lib/clang/$V/lib/linux/libclang_rt.tsan-x86_64.a',
      'lib/clang/$V/lib/linux/libclang_rt.tsan-x86_64.a.syms',

      # ThreadSanitizer C++ runtime.
      'lib/clang/$V/lib/linux/libclang_rt.tsan_cxx-x86_64.a',
      'lib/clang/$V/lib/linux/libclang_rt.tsan_cxx-x86_64.a.syms',

      # UndefinedBehaviorSanitizer C runtime (pure C won't link with *_cxx).
      'lib/clang/$V/lib/linux/libclang_rt.ubsan_standalone-i386.a',
      'lib/clang/$V/lib/linux/libclang_rt.ubsan_standalone-x86_64.a',
      'lib/clang/$V/lib/linux/libclang_rt.ubsan_standalone-x86_64.a.syms',

      # UndefinedBehaviorSanitizer C++ runtime.
      'lib/clang/$V/lib/linux/libclang_rt.ubsan_standalone_cxx-i386.a',
      'lib/clang/$V/lib/linux/libclang_rt.ubsan_standalone_cxx-x86_64.a',
      'lib/clang/$V/lib/linux/libclang_rt.ubsan_standalone_cxx-x86_64.a.syms',

      # UndefinedBehaviorSanitizer Android runtime, needed for CFI.
      'lib/clang/$V/lib/linux/libclang_rt.ubsan_standalone-aarch64-android.so',
      'lib/clang/$V/lib/linux/libclang_rt.ubsan_standalone-arm-android.so',

      # Blacklist for MemorySanitizer (used on Linux only).
      'lib/clang/$V/share/msan_blacklist.txt',
    ])
  elif sys.platform == 'win32':
    want.extend([
      # AddressSanitizer C runtime (pure C won't link with *_cxx).
      'lib/clang/$V/lib/windows/clang_rt.asan-x86_64.lib',

      # AddressSanitizer C++ runtime.
      'lib/clang/$V/lib/windows/clang_rt.asan_cxx-x86_64.lib',

      # Thunk for AddressSanitizer needed for static build of a shared lib.
      'lib/clang/$V/lib/windows/clang_rt.asan_dll_thunk-x86_64.lib',

      # AddressSanitizer runtime for component build.
      'lib/clang/$V/lib/windows/clang_rt.asan_dynamic-x86_64.dll',
      'lib/clang/$V/lib/windows/clang_rt.asan_dynamic-x86_64.lib',

      # Thunk for AddressSanitizer for component build of a shared lib.
      'lib/clang/$V/lib/windows/clang_rt.asan_dynamic_runtime_thunk-x86_64.lib',

      # Profile runtime (used by profiler and code coverage).
      'lib/clang/$V/lib/windows/clang_rt.profile-i386.lib',
      'lib/clang/$V/lib/windows/clang_rt.profile-x86_64.lib',

      # UndefinedBehaviorSanitizer C runtime (pure C won't link with *_cxx).
      'lib/clang/$V/lib/windows/clang_rt.ubsan_standalone-x86_64.lib',

      # UndefinedBehaviorSanitizer C++ runtime.
      'lib/clang/$V/lib/windows/clang_rt.ubsan_standalone_cxx-x86_64.lib',
    ])

  # Check all non-glob wanted files exist on disk.
  want = [w.replace('$V', RELEASE_VERSION) for w in want]
  for w in want:
    if '*' in w: continue
    if os.path.exists(os.path.join(LLVM_RELEASE_DIR, w)): continue
    print('wanted file "%s" but it did not exist' % w, file=sys.stderr)
    return 1

  # TODO(thakis): Try walking over want and copying the files in there instead
  # of walking the directory and doing fnmatch() against want.
  for root, dirs, files in os.walk(LLVM_RELEASE_DIR):
    # root: third_party/llvm-build/Release+Asserts/lib/..., rel_root: lib/...
    rel_root = root[len(LLVM_RELEASE_DIR)+1:]
    rel_files = [os.path.join(rel_root, f) for f in files]
    wanted_files = list(set(itertools.chain.from_iterable(
        fnmatch.filter(rel_files, p) for p in want)))
    if wanted_files:
      # Guaranteed to not yet exist at this point:
      os.makedirs(os.path.join(pdir, rel_root))
    for f in wanted_files:
      src = os.path.join(LLVM_RELEASE_DIR, f)
      dest = os.path.join(pdir, f)
      shutil.copy(src, dest)
      # Strip libraries.
      if sys.platform == 'darwin' and f.endswith('.dylib'):
        subprocess.call(['strip', '-x', dest])
      elif (sys.platform.startswith('linux') and
            os.path.splitext(f)[1] in ['.so', '.a']):
        subprocess.call([EU_STRIP, '-g', dest])

  stripped_binaries = ['clang',
                       'llvm-pdbutil',
                       'llvm-symbolizer',
                       'llvm-undname',
                       ]
  if sys.platform.startswith('linux'):
    stripped_binaries.append('lld')
    stripped_binaries.append('llvm-ar')
    stripped_binaries.append('llvm-objcopy')
  for f in stripped_binaries:
    if sys.platform != 'win32':
      subprocess.call(['strip', os.path.join(pdir, 'bin', f)])

  # Set up symlinks.
  if sys.platform != 'win32':
    os.symlink('clang', os.path.join(pdir, 'bin', 'clang++'))
    os.symlink('clang', os.path.join(pdir, 'bin', 'clang-cl'))

  if sys.platform.startswith('linux'):
    os.symlink('lld', os.path.join(pdir, 'bin', 'ld.lld'))
    os.symlink('lld', os.path.join(pdir, 'bin', 'ld64.lld'))
    os.symlink('lld', os.path.join(pdir, 'bin', 'ld64.lld.darwinnew'))
    os.symlink('lld', os.path.join(pdir, 'bin', 'lld-link'))

  # Copy libc++ headers.
  if sys.platform == 'darwin':
    shutil.copytree(os.path.join(LLVM_BOOTSTRAP_INSTALL_DIR, 'include', 'c++'),
                    os.path.join(pdir, 'include', 'c++'))

  # Create main archive.
  tar_entries = ['bin', 'lib' ]
  if sys.platform == 'darwin':
    tar_entries += ['include']
  with tarfile.open(pdir + '.tgz', 'w:gz') as tar:
    for entry in tar_entries:
      tar.add(os.path.join(pdir, entry), arcname=entry, filter=PrintTarProgress)
  MaybeUpload(args.upload, pdir + '.tgz', platform)

  # Upload build log next to it.
  os.rename('buildlog.txt', pdir + '-buildlog.txt')
  MaybeUpload(args.upload, pdir + '-buildlog.txt', platform,
              extra_gsutil_args=['-z', 'txt'])
  os.remove(pdir + '-buildlog.txt')

  # Zip up llvm-code-coverage for code coverage.
  code_coverage_dir = 'llvm-code-coverage-' + stamp
  shutil.rmtree(code_coverage_dir, ignore_errors=True)
  os.makedirs(os.path.join(code_coverage_dir, 'bin'))
  for filename in ['llvm-cov', 'llvm-profdata']:
    shutil.copy(os.path.join(LLVM_RELEASE_DIR, 'bin', filename + exe_ext),
                os.path.join(code_coverage_dir, 'bin'))
  with tarfile.open(code_coverage_dir + '.tgz', 'w:gz') as tar:
    tar.add(os.path.join(code_coverage_dir, 'bin'), arcname='bin',
            filter=PrintTarProgress)
  MaybeUpload(args.upload, code_coverage_dir + '.tgz', platform)

  # Zip up llvm-objdump and related tools for sanitizer coverage and Supersize.
  objdumpdir = 'llvmobjdump-' + stamp
  shutil.rmtree(objdumpdir, ignore_errors=True)
  os.makedirs(os.path.join(objdumpdir, 'bin'))
  for filename in ['llvm-bcanalyzer', 'llvm-cxxfilt', 'llvm-nm', 'llvm-objdump',
                   'llvm-readobj']:
    shutil.copy(os.path.join(LLVM_RELEASE_DIR, 'bin', filename + exe_ext),
                os.path.join(objdumpdir, 'bin'))
  llvmobjdump_stamp_file_base = 'llvmobjdump_build_revision'
  llvmobjdump_stamp_file = os.path.join(objdumpdir, llvmobjdump_stamp_file_base)
  with open(llvmobjdump_stamp_file, 'w') as f:
    f.write(expected_stamp)
    f.write('\n')
  if sys.platform != 'win32':
    os.symlink('llvm-readobj', os.path.join(objdumpdir, 'bin', 'llvm-readelf'))
  with tarfile.open(objdumpdir + '.tgz', 'w:gz') as tar:
    tar.add(os.path.join(objdumpdir, 'bin'), arcname='bin',
            filter=PrintTarProgress)
    tar.add(llvmobjdump_stamp_file, arcname=llvmobjdump_stamp_file_base,
            filter=PrintTarProgress)
  MaybeUpload(args.upload, objdumpdir + '.tgz', platform)

  # Zip up clang-tidy for users who opt into it, and Tricium.
  clang_tidy_dir = 'clang-tidy-' + stamp
  shutil.rmtree(clang_tidy_dir, ignore_errors=True)
  os.makedirs(os.path.join(clang_tidy_dir, 'bin'))
  shutil.copy(os.path.join(LLVM_RELEASE_DIR, 'bin', 'clang-tidy' + exe_ext),
              os.path.join(clang_tidy_dir, 'bin'))
  with tarfile.open(clang_tidy_dir + '.tgz', 'w:gz') as tar:
    tar.add(os.path.join(clang_tidy_dir, 'bin'), arcname='bin',
            filter=PrintTarProgress)
  MaybeUpload(args.upload, clang_tidy_dir + '.tgz', platform)

  # On Mac, lld isn't part of the main zip.  Upload it in a separate zip.
  if sys.platform == 'darwin':
    llddir = 'lld-' + stamp
    shutil.rmtree(llddir, ignore_errors=True)
    os.makedirs(os.path.join(llddir, 'bin'))
    shutil.copy(os.path.join(LLVM_RELEASE_DIR, 'bin', 'lld'),
                os.path.join(llddir, 'bin'))
    shutil.copy(os.path.join(LLVM_RELEASE_DIR, 'bin', 'llvm-ar'),
                os.path.join(llddir, 'bin'))
    os.symlink('lld', os.path.join(llddir, 'bin', 'ld.lld'))
    os.symlink('lld', os.path.join(llddir, 'bin', 'ld64.lld'))
    os.symlink('lld', os.path.join(llddir, 'bin', 'ld64.lld.darwinnew'))
    os.symlink('lld', os.path.join(llddir, 'bin', 'lld-link'))
    with tarfile.open(llddir + '.tgz', 'w:gz') as tar:
      tar.add(os.path.join(llddir, 'bin'), arcname='bin',
              filter=PrintTarProgress)
    MaybeUpload(args.upload, llddir + '.tgz', platform)

    # dsymutil isn't part of the main zip either, and it gets periodically
    # deployed to CIPD (manually, not as part of clang rolls) for use in the
    # Mac build toolchain.
    dsymdir = 'dsymutil-' + stamp
    shutil.rmtree(dsymdir, ignore_errors=True)
    os.makedirs(os.path.join(dsymdir, 'bin'))
    shutil.copy(os.path.join(LLVM_RELEASE_DIR, 'bin', 'dsymutil'),
                os.path.join(dsymdir, 'bin'))
    with tarfile.open(dsymdir + '.tgz', 'w:gz') as tar:
      tar.add(os.path.join(dsymdir, 'bin'), arcname='bin',
              filter=PrintTarProgress)
    MaybeUpload(args.upload, dsymdir + '.tgz', platform)

  # Zip up the translation_unit tool.
  translation_unit_dir = 'translation_unit-' + stamp
  shutil.rmtree(translation_unit_dir, ignore_errors=True)
  os.makedirs(os.path.join(translation_unit_dir, 'bin'))
  shutil.copy(os.path.join(LLVM_RELEASE_DIR, 'bin', 'translation_unit' +
                           exe_ext),
              os.path.join(translation_unit_dir, 'bin'))
  with tarfile.open(translation_unit_dir + '.tgz', 'w:gz') as tar:
    tar.add(os.path.join(translation_unit_dir, 'bin'), arcname='bin',
            filter=PrintTarProgress)
  MaybeUpload(args.upload, translation_unit_dir + '.tgz', platform)

  # Zip up the libclang binaries.
  libclang_dir = 'libclang-' + stamp
  shutil.rmtree(libclang_dir, ignore_errors=True)
  os.makedirs(os.path.join(libclang_dir, 'bin'))
  os.makedirs(os.path.join(libclang_dir, 'bindings', 'python', 'clang'))
  if sys.platform == 'win32':
    shutil.copy(os.path.join(LLVM_RELEASE_DIR, 'bin', 'libclang.dll'),
                os.path.join(libclang_dir, 'bin'))
  for filename in ['__init__.py', 'cindex.py', 'enumerations.py']:
    shutil.copy(os.path.join(LLVM_DIR, 'clang', 'bindings', 'python', 'clang',
                             filename),
                os.path.join(libclang_dir, 'bindings', 'python', 'clang'))
  tar_entries = ['bin', 'bindings' ]
  with tarfile.open(libclang_dir + '.tgz', 'w:gz') as tar:
    for entry in tar_entries:
      tar.add(os.path.join(libclang_dir, entry), arcname=entry,
              filter=PrintTarProgress)
  MaybeUpload(args.upload, libclang_dir + '.tgz', platform)

  if sys.platform == 'win32' and args.upload:
    binaries = [f for f in want if f.endswith('.exe') or f.endswith('.dll')]
    assert 'bin/clang-cl.exe' in binaries
    assert 'bin/lld-link.exe' in binaries
    UploadPDBsToSymbolServer(binaries)

  # FIXME: Warn if the file already exists on the server.


if __name__ == '__main__':
  sys.exit(main())
