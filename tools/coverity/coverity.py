#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Runs Coverity Prevent on a build of Chromium.

This script should be run in a Visual Studio Command Prompt, so that the
INCLUDE, LIB, and PATH environment variables are set properly for Visual
Studio.

Usage examples:
  coverity.py
  coverity.py --dry-run
  coverity.py --target=debug
  %comspec% /c ""C:\Program Files\Microsoft Visual Studio 8\VC\vcvarsall.bat"
      x86 && C:\Python24\python.exe C:\coverity.py"

For a full list of options, pass the '--help' switch.

See http://support.microsoft.com/kb/308569 for running this script as a
Scheduled Task on Windows XP.

"""

from __future__ import print_function

import optparse
import os
import os.path
import shutil
import subprocess
import sys
import time

# These constants provide default values, but are exposed as command-line
# flags. See the --help for more info. Note that for historical reasons
# (the script started out as Windows-only and has legacy usages which pre-date
# these switches), the constants are all tuned for Windows.
# Usage of this script on Linux pretty much requires explicit
# --source-dir, --coverity-bin-dir, --coverity-intermediate-dir, and
# --coverity-target command line flags.

CHROMIUM_SOURCE_DIR = 'C:\\chromium.latest'

# Relative to CHROMIUM_SOURCE_DIR.
CHROMIUM_SOLUTION_FILE = 'src\\chrome\\chrome.sln'

# Relative to CHROMIUM_SOURCE_DIR.
CHROMIUM_SOLUTION_DIR = 'src\\chrome'

COVERITY_BIN_DIR = 'C:\\coverity\\prevent-win32-4.5.1\\bin'

COVERITY_INTERMEDIATE_DIR = 'C:\\coverity\\cvbuild\\cr_int'

COVERITY_ANALYZE_OPTIONS = ('--cxx --security --concurrency '
                            '--enable ATOMICITY '
                            '--enable MISSING_LOCK '
                            '--enable DELETE_VOID '
                            '--checker-option PASS_BY_VALUE:size_threshold:16 '
                            '--checker-option '
                            'USE_AFTER_FREE:allow_simple_use:false '
                            '--enable-constraint-fpp '
                            '--enable-callgraph-metrics')

# Might need to be changed to FQDN
COVERITY_REMOTE = 'chromecoverity-linux1'

COVERITY_PORT = '5467'

COVERITY_PRODUCT = 'Chromium'

COVERITY_TARGET = 'Windows'

COVERITY_USER = 'admin'
# looking for a PASSWORD constant? Look at --coverity-password-file instead.

# Relative to CHROMIUM_SOURCE_DIR.  Contains the pid of this script.
LOCK_FILE = 'coverity.lock'


def _ReadPassword(pwfilename):
  """Reads the coverity password in from a file where it was stashed"""
  pwfile = open(pwfilename, 'r')
  password = pwfile.readline()
  pwfile.close()
  return password.rstrip()


def _RunCommand(cmd, dry_run, shell=False, echo_cmd=True):
  """Runs the command if dry_run is false, otherwise just prints the command."""
  if echo_cmd:
    print(cmd)
  if not dry_run:
    return subprocess.call(cmd, shell=shell)
  else:
    return 0


def _ReleaseLock(lock_file, lock_filename):
  """Removes the lockfile. Function-ized so we can bail from anywhere"""
  os.close(lock_file)
  os.remove(lock_filename)


def run_coverity(options, args):
  """Runs all the selected tests for the given build type and target."""
  # Create the lock file to prevent another instance of this script from
  # running.
  lock_filename = os.path.join(options.source_dir, LOCK_FILE)
  try:
    lock_file = os.open(lock_filename,
                        os.O_CREAT | os.O_EXCL | os.O_TRUNC | os.O_RDWR)
  except OSError, err:
    print('Failed to open lock file:\n  ' + str(err))
    return 1

  # Write the pid of this script (the python.exe process) to the lock file.
  os.write(lock_file, str(os.getpid()))

  options.target = options.target.title()

  start_time = time.time()

  print('Change directory to ' + options.source_dir)
  os.chdir(options.source_dir)

  # The coverity-password filename may have been a relative path.
  # If so, assume it's relative to the source directory, which means
  # the time to read the password is after we do the chdir().
  coverity_password = _ReadPassword(options.coverity_password_file)

  cmd = 'gclient sync'
  gclient_exit = _RunCommand(cmd, options.dry_run, shell=True)
  if gclient_exit != 0:
    print('gclient aborted with status %s' % gclient_exit)
    _ReleaseLock(lock_file, lock_filename)
    return 1

  print('Elapsed time: %ds' % (time.time() - start_time))

  # Do a clean build.  Remove the build output directory first.
  if sys.platform.startswith('linux'):
    rm_path = os.path.join(options.source_dir,'src','out',options.target)
  elif sys.platform == 'win32':
    rm_path = os.path.join(options.source_dir,options.solution_dir,
                           options.target)
  elif sys.platform == 'darwin':
    rm_path = os.path.join(options.source_dir,'src','xcodebuild')
  else:
    print('Platform "%s" unrecognized, aborting' % sys.platform)
    _ReleaseLock(lock_file, lock_filename)
    return 1

  if options.dry_run:
    print('shutil.rmtree(%s)' % repr(rm_path))
  else:
    shutil.rmtree(rm_path,True)

  if options.preserve_intermediate_dir:
    print('Preserving intermediate directory.')
  else:
    if options.dry_run:
      print('shutil.rmtree(%s)' % repr(options.coverity_intermediate_dir))
      print('os.mkdir(%s)' % repr(options.coverity_intermediate_dir))
    else:
      shutil.rmtree(options.coverity_intermediate_dir,True)
      os.mkdir(options.coverity_intermediate_dir)

  print('Elapsed time: %ds' % (time.time() - start_time))

  use_shell_during_make = False
  if sys.platform.startswith('linux'):
    use_shell_during_make = True
    os.chdir('src')
    _RunCommand('pwd', options.dry_run, shell=True)
    cmd = '%s/cov-build --dir %s make BUILDTYPE=%s chrome' % (
      options.coverity_bin_dir, options.coverity_intermediate_dir,
      options.target)
  elif sys.platform == 'win32':
    cmd = ('%s\\cov-build.exe --dir %s devenv.com %s\\%s /build %s '
           '/project chrome.vcproj') % (
      options.coverity_bin_dir, options.coverity_intermediate_dir,
      options.source_dir, options.solution_file, options.target)
  elif sys.platform == 'darwin':
    use_shell_during_make = True
    os.chdir('src/chrome')
    _RunCommand('pwd', options.dry_run, shell=True)
    cmd = ('%s/cov-build --dir %s xcodebuild -project chrome.xcodeproj '
           '-configuration %s -target chrome') % (
      options.coverity_bin_dir, options.coverity_intermediate_dir,
      options.target)


  _RunCommand(cmd, options.dry_run, shell=use_shell_during_make)
  print('Elapsed time: %ds' % (time.time() - start_time))

  cov_analyze_exe = os.path.join(options.coverity_bin_dir,'cov-analyze')
  cmd = '%s --dir %s %s' % (cov_analyze_exe,
                            options.coverity_intermediate_dir,
                            options.coverity_analyze_options)
  _RunCommand(cmd, options.dry_run, shell=use_shell_during_make)
  print('Elapsed time: %ds' % (time.time() - start_time))

  cov_commit_exe = os.path.join(options.coverity_bin_dir,'cov-commit-defects')

  # On Linux we have started using a Target with a space in it, so we want
  # to quote it. On the other hand, Windows quoting doesn't work quite the
  # same way. To be conservative, I'd like to avoid quoting an argument
  # that doesn't need quoting and which we haven't historically been quoting
  # on that platform. So, only quote the target if we have to.
  coverity_target = options.coverity_target
  if sys.platform != 'win32':
    coverity_target = '"%s"' % coverity_target

  cmd = ('%s --dir %s --remote %s --port %s '
         '--product %s '
         '--target %s '
         '--user %s '
         '--password %s') % (cov_commit_exe,
                             options.coverity_intermediate_dir,
                             options.coverity_dbhost,
                             options.coverity_port,
                             options.coverity_product,
                             coverity_target,
                             options.coverity_user,
                             coverity_password)
  # Avoid echoing the Commit command because it has a password in it
  _RunCommand(cmd, options.dry_run, shell=use_shell_during_make, echo_cmd=False)

  print('Total time: %ds' % (time.time() - start_time))

  _ReleaseLock(lock_file, lock_filename)

  return 0


def main():
  option_parser = optparse.OptionParser()
  option_parser.add_option('', '--dry-run', action='store_true', default=False,
                           help='print but don\'t run the commands')

  option_parser.add_option('', '--target', default='Release',
                           help='build target (Debug or Release)')

  option_parser.add_option('', '--source-dir', dest='source_dir',
                           help='full path to directory ABOVE "src"',
                           default=CHROMIUM_SOURCE_DIR)

  option_parser.add_option('', '--solution-file', dest='solution_file',
                           default=CHROMIUM_SOLUTION_FILE)

  option_parser.add_option('', '--solution-dir', dest='solution_dir',
                           default=CHROMIUM_SOLUTION_DIR)

  option_parser.add_option('', '--coverity-bin-dir', dest='coverity_bin_dir',
                           default=COVERITY_BIN_DIR)

  option_parser.add_option('', '--coverity-intermediate-dir',
                           dest='coverity_intermediate_dir',
                           default=COVERITY_INTERMEDIATE_DIR)

  option_parser.add_option('', '--coverity-analyze-options',
                           dest='coverity_analyze_options',
                           help=('all cov-analyze options, e.g. "%s"'
                                 % COVERITY_ANALYZE_OPTIONS),
                           default=COVERITY_ANALYZE_OPTIONS)

  option_parser.add_option('', '--coverity-db-host',
                           dest='coverity_dbhost',
                           help=('coverity defect db server hostname, e.g. %s'
                                 % COVERITY_REMOTE),
                           default=COVERITY_REMOTE)

  option_parser.add_option('', '--coverity-db-port', dest='coverity_port',
                           help=('port # of coverity web/db server, e.g. %s'
                                 % COVERITY_PORT),
                           default=COVERITY_PORT)

  option_parser.add_option('', '--coverity-product', dest='coverity_product',
                           help=('Product name reported to coverity, e.g. %s'
                                 % COVERITY_PRODUCT),
                           default=COVERITY_PRODUCT)

  option_parser.add_option('', '--coverity-target', dest='coverity_target',
                           help='Platform Target reported to coverity',
                           default=COVERITY_TARGET)

  option_parser.add_option('', '--coverity-user', dest='coverity_user',
                           help='Username used to log into coverity',
                           default=COVERITY_USER)

  option_parser.add_option('', '--coverity-password-file',
                           dest='coverity_password_file',
                           help='file containing the coverity password',
                           default='coverity-password')

  helpmsg = ('By default, the intermediate dir is emptied before analysis. '
             'This switch disables that behavior.')
  option_parser.add_option('', '--preserve-intermediate-dir',
                           action='store_true', help=helpmsg,
                           default=False)

  options, args = option_parser.parse_args()
  return run_coverity(options, args)


if '__main__' == __name__:
  sys.exit(main())
