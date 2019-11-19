#!/usr/bin/env python

# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A script used to manage Google Maven dependencies for Chromium.

This script creates a temporary build directory, where it will, for each
of the dependencies specified in `build.gradle`, take care of the following:

  - Download the library
  - Generate a README.chromium file
  - Download the LICENSE
  - Generate a GN target in BUILD.gn
  - Generate .info files for AAR libraries
  - Generate CIPD yaml files describing the packages
  - Generate a 'deps' entry in DEPS.

It will then compare the build directory with your current workspace, and
print the differences (i.e. new/updated/deleted packages names).

The --update-all option can be used to update the current workspace with any
relevant changes, and will print commands to create the relevant CIPD packages
as well.

The --reset-workspace option can be used to reset your workspace to its HEAD
state if you're not satisfied with the results of --update-all. Note that
this preserves local modifications to your build.gradle file.
"""

from __future__ import print_function

import argparse
import collections
import contextlib
import fnmatch
import logging
import tempfile
import os
import re
import shutil
import subprocess
import zipfile

# Assume this script is stored under tools/android/roll/android_deps/
_CHROMIUM_SRC = os.path.abspath(
    os.path.join(__file__, '..', '..', '..', '..', '..'))

# Location of the android_deps directory from a root checkout.
_ANDROID_DEPS_SUBDIR = 'third_party/android_deps'

# Path to BUILD.gn file under android_deps/
_ANDROID_DEPS_BUILD_GN = _ANDROID_DEPS_SUBDIR + '/BUILD.gn'

# Path to custom licenses under android_deps/
_ANDROID_DEPS_LICENSE_SUBDIR = _ANDROID_DEPS_SUBDIR + '/licenses'

# Path to additional_readme_paths.json
_ANDROID_DEPS_ADDITIONAL_README_PATHS = (
    _ANDROID_DEPS_SUBDIR + '/additional_readme_paths.json')

# Location of the android_deps libs directory from a root checkout.
_ANDROID_DEPS_LIBS_SUBDIR = _ANDROID_DEPS_SUBDIR + '/libs'

# Location of the buildSrc directory used implement our gradle task.
_GRADLE_BUILDSRC_PATH = 'tools/android/roll/android_deps/buildSrc'

# The list of git-controlled files that are checked or updated by this tool.
_UPDATED_GIT_FILES = [
  'DEPS',
  _ANDROID_DEPS_BUILD_GN,
  _ANDROID_DEPS_ADDITIONAL_README_PATHS,
]

# If this file exists in an aar file then it is appended to LICENSE
_THIRD_PARTY_LICENSE_FILENAME = 'third_party_licenses.txt'

@contextlib.contextmanager
def BuildDir(dirname=None):
  """Helper function used to manage a build directory.

  Args:
    dirname: Optional build directory path. If not provided, a temporary
      directory will be created and cleaned up on exit.
  Returns:
    A python context manager modelling a directory path. The manager
    removes the directory if necessary on exit.
  """
  delete = False
  if not dirname:
    dirname = tempfile.mkdtemp()
    delete = True
  try:
    yield dirname
  finally:
    if delete:
      shutil.rmtree(dirname)


def RaiseCommandException(args, returncode, output, error):
  """Raise an exception whose message describing a command failure.

  Args:
    args: shell command-line (as passed to subprocess.call())
    returncode: status code.
    error: standard error output.
  Raises:
    a new Exception.
  """
  message = 'Command failed with status %d: %s\n' % (returncode, args)
  if output:
    message += 'Output:-------------------------------------------\n%s\n' \
               '--------------------------------------------------\n' % output
  if error:
    message += 'Error message: -----------------------------------\n%s\n' \
               '--------------------------------------------------\n' % error
  raise Exception(message)


def RunCommand(args, print_stdout=False):
  """Run a new shell command.

  This function runs without printing anything.

  Args:
    args: A string or a list of strings for the shell command.
  Raises:
    On failure, raise an Exception that contains the command's arguments,
    return status, and standard output + error merged in a single message.
  """
  logging.debug('Run %s', args)
  stdout = None if print_stdout else subprocess.PIPE
  p = subprocess.Popen(args, stdout=stdout)
  pout, _ = p.communicate()
  if p.returncode != 0:
    RaiseCommandException(args, p.returncode, None, pout)


def RunCommandAndGetOutput(args):
  """Run a new shell command. Return its output. Exception on failure.

  This function runs without printing anything.

  Args:
    args: A string or a list of strings for the shell command.
  Returns:
    The command's output.
  Raises:
    On failure, raise an Exception that contains the command's arguments,
    return status, and standard output, and standard error as separate
    messages.
  """
  logging.debug('Run %s', args)
  p = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  pout, perr = p.communicate()
  if p.returncode != 0:
    RaiseCommandException(args, p.returncode, pout, perr)
  return pout


def MakeDirectory(dir_path):
  """Make directory |dir_path| recursively if necessary."""
  if not os.path.isdir(dir_path):
    logging.debug('mkdir [%s]', dir_path)
    os.makedirs(dir_path)


def DeleteDirectory(dir_path):
  """Recursively delete a directory if it exists."""
  if os.path.exists(dir_path):
    logging.debug('rmdir [%s]', dir_path)
    shutil.rmtree(dir_path)


def CopyFileOrDirectory(src_path, dst_path):
  """Copy file or directory |src_path| into |dst_path| exactly."""
  logging.debug('copy [%s -> %s]', src_path, dst_path)
  MakeDirectory(os.path.dirname(dst_path))
  if os.path.isdir(src_path):
    # Copy directory recursively.
    DeleteDirectory(dst_path)
    shutil.copytree(src_path, dst_path)
  else:
    shutil.copy(src_path, dst_path)


def ReadFile(file_path):
  """Read a file, return its content."""
  with open(file_path) as f:
    return f.read()


def ReadFileAsLines(file_path):
  """Read a file as a series of lines."""
  with open(file_path) as f:
    return f.readlines()


def WriteFile(file_path, file_data):
  """Write a file."""
  MakeDirectory(os.path.dirname(file_path))
  with open(file_path, 'w') as f:
    f.write(file_data)


def ReadGitHeadFile(git_root, src_path):
  """Read the HEAD version of a git-controlled file.

  Args:
    git_root: Git root directory.
    src_path: Relative path of source file under |git_root|.
  Returns:
    file data.
  """
  git_args = ['git', '-C', git_root, 'show', 'HEAD:%s' % src_path]
  return RunCommandAndGetOutput(git_args)


def FindInDirectory(directory, filename_filter):
  """Find all files in a directory that matches a given filename filter."""
  files = []
  for root, _dirnames, filenames in os.walk(directory):
    matched_files = fnmatch.filter(filenames, filename_filter)
    files.extend((os.path.join(root, f) for f in matched_files))
  return files


# Named tuple describing a CIPD package.
# - path: Path to cipd.yaml file.
# - name: cipd package name.
# - tag: cipd tag.
CipdPackageInfo = collections.namedtuple(
    'CipdPackageInfo', ['path', 'name', 'tag'])

# Regular expressions used to extract useful info from cipd.yaml files
# generated by Gradle. See BuildConfigGenerator.groovy:makeCipdYaml()
_RE_CIPD_CREATE = re.compile('cipd create --pkg-def cipd.yaml -tag (\S*)')
_RE_CIPD_PACKAGE = re.compile('package: (\S*)')


def GetCipdPackageInfo(cipd_yaml_path):
  """Returns the CIPD package name corresponding to a given cipd.yaml file.

  Args:
    cipd_yaml_path: Path of input cipd.yaml file.
  Returns:
    A (package_name, package_tag) tuple.
  Raises:
    Exception if the file could not be read.
  """
  package_name = None
  package_tag = None
  for line in ReadFileAsLines(cipd_yaml_path):
    m = _RE_CIPD_PACKAGE.match(line)
    if m:
      package_name = m.group(1)

    m = _RE_CIPD_CREATE.search(line)
    if m:
      package_tag = m.group(1)

  if not package_name or not package_tag:
    raise Exception('Invalid cipd.yaml format: ' + cipd_yaml_path)

  return (package_name, package_tag)


def ParseDeps(root_dir, libs_dir):
  """Parse an android_deps/libs and retrieve package information.

  Args:
    root_dir: Path to a root Chromium or build directory.
  Returns:
    A directory mapping package names to tuples of
    (cipd_yaml_file, package_name, package_tag), where |cipd_yaml_file|
    is the path to the cipd.yaml file, related to |libs_dir|,
    and |package_name| and |package_tag| are the extracted from it.
  """
  result = {}
  libs_dir = os.path.abspath(os.path.join(root_dir, libs_dir))
  for cipd_file in FindInDirectory(libs_dir, 'cipd.yaml'):
    pkg_name, pkg_tag = GetCipdPackageInfo(cipd_file)
    cipd_path = os.path.dirname(cipd_file)
    cipd_path = cipd_path[len(root_dir) + 1:]
    result[pkg_name] = CipdPackageInfo(cipd_path, pkg_name, pkg_tag)

  return result


def PrintPackageList(packages, list_name):
  """Print a list of packages to standard output.

  Args:
    packages: list of package names.
    list_name: a simple word describing the package list (e.g. 'new')
  """
  print('  %d %s packages:' % (len(packages), list_name))
  print('\n'.join(['    - %s' % p for p in packages]))


def GenerateCipdUploadCommand(cipd_pkg_info):
  """Generate a shell command to create a cipd package.

  Args:
    cipd_pkg_info: A CipdPackageInfo instance.
  Returns:
    A string holding a shell command to upload the package through cipd.
  """
  pkg_path, pkg_name, pkg_tag = cipd_pkg_info
  return ('(cd "{0}"; '
          # Need to skip create step if an instance already exists with the
          # same package name and version tag (thus the use of ||).
          'cipd describe "{1}" -version "{2}" || '
          'cipd create --pkg-def cipd.yaml -tag "{2}")').format(
              pkg_path, pkg_name, pkg_tag)


def main():
  parser = argparse.ArgumentParser(
      description=__doc__,
      formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument('--build-dir',
      help='Path to build directory (default is temporary directory).')
  parser.add_argument('--chromium-dir',
      help='Path to Chromium source tree (auto-detect by default).')
  parser.add_argument('--gradle-wrapper',
      help='Path to custom gradle wrapper (auto-detect by default).')
  parser.add_argument(
      '--build-gradle',
      help='Path to build.gradle relative to src/.',
      default='tools/android/roll/android_deps/build.gradle')
  parser.add_argument(
      '--git-dir', help='Path to git subdir from chromium-dir.', default='.')
  parser.add_argument(
      '--ignore-licenses',
      help='Ignores licenses for these deps.',
      action='store_true')
  parser.add_argument('--update-all', action='store_true',
      help='Update current checkout in case of build.gradle changes.'
      'This will also print a list of commands to upload new and updated '
      'packages through cipd, if needed.')
  parser.add_argument('--reset-workspace', action='store_true',
      help='Reset your Chromium workspace to its HEAD state, but preserves '
      'build.gradle changes. Use this to undo previous --update-all changes.')
  parser.add_argument(
      '--debug', action='store_true', help='Enable debug logging')
  args = parser.parse_args()

  # Determine Chromium source tree.
  chromium_src = args.chromium_dir
  if not chromium_src:
    # Assume this script is stored under tools/android/roll/android_deps/
    chromium_src = _CHROMIUM_SRC

  chromium_src = os.path.abspath(chromium_src)

  abs_git_dir = os.path.normpath(os.path.join(chromium_src, args.git_dir))

  if not os.path.isdir(chromium_src):
    raise Exception('Not a directory: ' + chromium_src)
  if not os.path.isdir(abs_git_dir):
    raise Exception('Not a directory: ' + abs_git_dir)

  # The list of files and dirs that are copied to the build directory by this
  # script. Should not include _UPDATED_GIT_FILES.
  copied_paths = {
      args.build_gradle:
          args.build_gradle,
      _GRADLE_BUILDSRC_PATH:
          os.path.join(os.path.dirname(args.build_gradle), "buildSrc"),
  }

  if not args.ignore_licenses:
    copied_paths[_ANDROID_DEPS_LICENSE_SUBDIR] = _ANDROID_DEPS_LICENSE_SUBDIR

  logging.basicConfig(format='%(message)s')
  logger = logging.getLogger()
  if args.debug:
    logger.setLevel('DEBUG')

  # Handle --reset-workspace here.
  if args.reset_workspace:
    print('# Removing .cipd directory.')
    cipd_dir = os.path.join(chromium_src, '..', '.cipd')
    if os.path.isdir(cipd_dir):
      RunCommand(['rm', '-rf', cipd_dir])

    print('# Saving build.gradle content')
    build_gradle_path = os.path.join(chromium_src, args.build_gradle)
    build_gradle = ReadFile(build_gradle_path)

    print('# Resetting and re-syncing workspace. (may take a while).')
    RunCommand(['gclient', 'sync', '--reset', '--nohooks', '-r', 'src@HEAD'],
               print_stdout=args.debug)

    print('# Restoring build.gradle.')
    WriteFile(build_gradle_path, build_gradle)
    return

  missing_files = []
  for src_path in copied_paths.keys():
    if not os.path.exists(os.path.join(chromium_src, src_path)):
      missing_files.append(src_path)
  for src_path in _UPDATED_GIT_FILES:
    if not os.path.exists(os.path.join(abs_git_dir, src_path)):
      missing_files.append(src_path)
  if missing_files:
    raise Exception('Missing files from %s: %s' % (chromium_src, missing_files))

  # Path to the gradlew script used to run build.gradle.
  gradle_wrapper_path = args.gradle_wrapper or os.path.join(
        chromium_src, 'third_party', 'gradle_wrapper', 'gradlew')

  # Path to the aar.py script used to generate .info files.
  aar_py = os.path.join(chromium_src, 'build', 'android', 'gyp', 'aar.py')
  if not os.path.exists(aar_py):
    raise Exception('Missing required python script: ' + aar_py)

  with BuildDir(args.build_dir) as build_dir:
    print('# Setup build directory.')
    logging.debug('Using build directory: ' + build_dir)
    for git_file in _UPDATED_GIT_FILES:
      git_data = ReadGitHeadFile(abs_git_dir, git_file)
      WriteFile(os.path.join(build_dir, args.git_dir, git_file), git_data)

    for path, dest in copied_paths.iteritems():
      CopyFileOrDirectory(
          os.path.join(chromium_src, path), os.path.join(build_dir, dest))

    print('# Use Gradle to download packages and edit/create relevant files.')
    # This gradle command generates the new DEPS and BUILD.gn files, it can also
    # handle special cases. Edit BuildConfigGenerator.groovy#addSpecialTreatment
    # for such cases.
    gradle_cmd = [
        gradle_wrapper_path,
        '-b',
        os.path.join(build_dir, args.build_gradle),
        'setupRepository',
        '--stacktrace',
    ]
    if args.debug:
      gradle_cmd.append('--debug')

    RunCommand(gradle_cmd, print_stdout=args.debug)

    libs_dir = os.path.join(build_dir, args.git_dir, _ANDROID_DEPS_LIBS_SUBDIR)

    print('# Reformat %s.' % _ANDROID_DEPS_BUILD_GN)
    gn_args = [
        'gn', 'format',
        os.path.join(build_dir, args.git_dir, _ANDROID_DEPS_BUILD_GN)
    ]
    RunCommand(gn_args, print_stdout=args.debug)

    print('# Generate Android .aar info and third-party license files.')
    aar_files = FindInDirectory(libs_dir, '*.aar')
    for aar_file in aar_files:
      aar_dirname = os.path.dirname(aar_file)
      aar_info_name = os.path.basename(aar_dirname) + '.info'
      aar_info_path = os.path.join(aar_dirname, aar_info_name)
      if not os.path.exists(aar_info_path):
        logging.info('- %s' % aar_info_name)
        RunCommand([aar_py, 'list', aar_file, '--output', aar_info_path])
      if not args.ignore_licenses:
        with zipfile.ZipFile(aar_file) as z:
          if _THIRD_PARTY_LICENSE_FILENAME in z.namelist():
            license_path = os.path.join(aar_dirname, 'LICENSE')
            # Make sure to append as we don't want to lose the existing license.
            with open(license_path, 'a') as f:
              f.write(z.read(_THIRD_PARTY_LICENSE_FILENAME))


    print('# Compare CIPD packages.')
    existing_packages = ParseDeps(abs_git_dir, _ANDROID_DEPS_LIBS_SUBDIR)
    build_packages = ParseDeps(
        build_dir, os.path.join(args.git_dir, _ANDROID_DEPS_LIBS_SUBDIR))

    deleted_packages = []
    updated_packages = []
    for pkg in sorted(existing_packages):
      if pkg not in build_packages:
        logging.info('- %s' % pkg)
        deleted_packages.append(pkg)
      else:
        existing_info = existing_packages[pkg]
        build_info = build_packages[pkg]
        if existing_info.tag != build_info.tag:
          logging.info('U %s (%s -> %s)', pkg, existing_info.tag,
                          build_info.tag)
          updated_packages.append(pkg)
        else:
          logging.info('= %s', pkg)  # Unchanged.

    new_packages = sorted(set(build_packages) - set(existing_packages))
    for pkg in new_packages:
      logging.info('+ %s', pkg)

    # Generate CIPD package upload commands.
    cipd_packages_to_upload = sorted(updated_packages + new_packages)
    if cipd_packages_to_upload:
      # TODO(wnwen): Check CIPD to make sure that no other package with the
      #              same tag exists, print error otherwise.
      cipd_commands = [GenerateCipdUploadCommand(build_packages[pkg])
        for pkg in cipd_packages_to_upload]
      # Print them to the log for debugging.
      logging.info('CIPD update commands\n%s\n',
                   '\n'.join(cipd_commands))

    if not args.update_all:
      if not (deleted_packages or new_packages or updated_packages):
        print('No changes detected. All good.')
      else:
        print('Changes detected:')
        if new_packages:
          PrintPackageList(new_packages, 'new')
        if updated_packages:
          PrintPackageList(updated_packages, 'updated')
        if deleted_packages:
          PrintPackageList(deleted_packages, 'deleted')
        print('')
        print('Run with --update-all to update your checkout!')
      return

    # Copy updated DEPS and BUILD.gn to build directory.
    update_cmds = []
    for updated_file in _UPDATED_GIT_FILES:
      CopyFileOrDirectory(
          os.path.join(build_dir, args.git_dir, updated_file),
          os.path.join(abs_git_dir, updated_file))

    # Delete obsolete or updated package directories.
    for pkg in deleted_packages + updated_packages:
      pkg_path = os.path.join(abs_git_dir, existing_packages[pkg].path)
      DeleteDirectory(pkg_path)

    # Copy new and updated packages from build directory.
    for pkg in new_packages + updated_packages:
      pkg_path = build_packages[pkg].path
      dst_pkg_path = os.path.join(chromium_src, pkg_path)
      src_pkg_path = os.path.join(build_dir, pkg_path)
      CopyFileOrDirectory(src_pkg_path, dst_pkg_path)

    if cipd_packages_to_upload:
      print('Run the following to upload new and updated CIPD packages:')
      print('Note: Duplicate instances with the same tag will break the build.')
      print('------------------------ cut here -----------------------------')
      print('\n'.join(cipd_commands))
      print('------------------------ cut here -----------------------------')


if __name__ == "__main__":
  main()
