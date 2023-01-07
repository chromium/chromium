#!/usr/bin/env python
#
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
r"""This script downloads / packages & uploads Android SDK packages.

   It could be run when we need to update sdk packages to latest version.
   It has 2 usages:
     1) download: downloading a new version of the SDK via sdkmanager
     2) package: wrapping SDK directory into CIPD-compatible packages and
                 uploading the new packages via CIPD to server.
                 Providing '--dry-run' option to show what packages to be
                 created and uploaded without actually doing either.

   Both downloading and uploading allows to either specify a package, or
   deal with default packages (build-tools, platform-tools, platforms and
   tools).

   Example usage:
     1) updating default packages:
        $ update_sdk.py download
        (optional) $ update_sdk.py package --dry-run
        $ update_sdk.py package
     2) updating a specified package:
        $ update_sdk.py download -p "build-tools;27.0.3"
        (optional) $ update_sdk.py package --dry-run -p build-tools \
                     --version 27.0.3
        $ update_sdk.py package -p build-tools --version 27.0.3

   Note that `package` could update the package argument to the checkout
   version in .gn file //build/config/android/config.gni. If having git
   changes, please prepare to upload a CL that updates the SDK version.
"""

from __future__ import print_function

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

_SRC_ROOT = os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))

_SRC_DEPS_PATH = os.path.join(_SRC_ROOT, 'DEPS')

_SDK_PUBLIC_ROOT = os.path.join(_SRC_ROOT, 'third_party', 'android_sdk',
                                'public')

_SDKMANAGER_PATH = os.path.join(_SRC_ROOT, 'third_party', 'android_sdk',
                                'public', 'tools', 'bin', 'sdkmanager')

_ANDROID_CONFIG_GNI_PATH = os.path.join(_SRC_ROOT, 'build', 'config', 'android',
                                        'config.gni')

_TOOLS_LIB_PATH = os.path.join(_SDK_PUBLIC_ROOT, 'tools', 'lib')

_DEFAULT_DOWNLOAD_PACKAGES = [
    'build-tools', 'platform-tools', 'platforms', 'tools'
]

# TODO(shenghuazhang): Search package versions from available packages through
# the sdkmanager, instead of hardcoding the package names w/ version.
# TODO(yliuyliu): we might not need the latest version if unstable,
# will double-check this later.
_DEFAULT_PACKAGES_DICT = {
    'build-tools': 'build-tools;27.0.3',
    'platforms': 'platforms;android-28',
    'sources': 'sources;android-28',
}

_GN_ARGUMENTS_TO_UPDATE = {
    'build-tools': 'default_android_sdk_build_tools_version',
    'tools': 'android_sdk_tools_version_suffix',
    'platforms': 'default_android_sdk_version',
}

_COMMON_JAR_SUFFIX_PATTERN = re.compile(
    r'^common'  # file name begins with 'common'
    r'(-[\d\.]+(-dev)?)'  # group of suffix e.g.'-26.0.0-dev', '-25.3.2'
    r'\.jar$'  # ends with .jar
)


def _DownloadSdk(arguments):
  """Download sdk package from sdkmanager.

  If package isn't provided, update build-tools, platform-tools, platforms,
  and tools.

  Args:
    arguments: The arguments parsed from argparser.
  """
  for pkg in arguments.package:
    # If package is not a sdk-style path, try to match a default path to it.
    if pkg in _DEFAULT_PACKAGES_DICT:
      print('Coercing %s to %s' % (pkg, _DEFAULT_PACKAGES_DICT[pkg]))
      pkg = _DEFAULT_PACKAGES_DICT[pkg]

    download_sdk_cmd = [
        _SDKMANAGER_PATH, '--install',
        '--sdk_root=%s' % arguments.sdk_root, pkg
    ]
    if arguments.verbose:
      download_sdk_cmd.append('--verbose')

    subprocess.check_call(download_sdk_cmd)


def _FindPackageVersion(package, sdk_root):
  """Find sdk package version.

  Two options for package version:
    1) Use the version in name if package name contains ';version'
    2) For simple name package, search its version from 'Installed packages'
       via `sdkmanager --list`

  Args:
    package: The Android SDK package.
    sdk_root: The Android SDK root path.

  Returns:
    The version of package.

  Raises:
    Exception: cannot find the version of package.
  """
  sdkmanager_list_cmd = [
      _SDKMANAGER_PATH,
      '--list',
      '--sdk_root=%s' % sdk_root,
  ]

  if package in _DEFAULT_PACKAGES_DICT:
    # Get the version after ';' from package name
    package = _DEFAULT_PACKAGES_DICT[package]
    return package.split(';')[1]
  else:
    # Get the package version via `sdkmanager --list`. The logic is:
    # Check through 'Installed packages' which is at the first section of
    # `sdkmanager --list` output, example:
    #   Installed packages:=====================] 100% Computing updates...
    #     Path                        | Version | Description
    #     -------                     | ------- | -------
    #     build-tools;27.0.3          | 27.0.3  | Android SDK Build-Tools 27.0.3
    #     emulator                    | 26.0.3  | Android Emulator
    #     platforms;android-27        | 1       | Android SDK Platform 27
    #     tools                       | 26.1.1  | Android SDK Tools
    #
    #   Available Packages:
    #   ....
    # When found a line containing the package path, grap its version between
    # the first and second '|'. Since the 'Installed packages' list section ends
    # by the first new line, the check loop should be ended when reaches a '\n'.
    output = subprocess.check_output(sdkmanager_list_cmd)
    for line in output.splitlines():
      if ' ' + package + ' ' in line:
        # if found package path, catch its version which in the first '|...|'
        return line.split('|')[1].strip()
      if line == '\n':  # Reaches the end of 'Installed packages' list
        break
    raise Exception('Cannot find the version of package %s' % package)


def _ReplaceVersionInFile(file_path, pattern, version, dry_run=False):
  """Replace the version of sdk package argument in file.

  Check whether the version in file is the same as the new version first.
  Replace the version if not dry run.

  Args:
    file_path: Path to the file to update the version of sdk package argument.
    pattern: Pattern for the sdk package argument. Must capture at least one
      group that the first group is the argument line excluding version.
    version: The new version of the package.
    dry_run: Bool. To show what packages would be created and packages, without
      actually doing either.
  """
  with tempfile.NamedTemporaryFile() as temp_file:
    with open(file_path) as f:
      for line in f:
        new_line = re.sub(pattern, r'\g<1>\g<2>%s\g<3>\n' % version, line)
        if new_line != line:
          print('    Note: file "%s" argument ' % file_path +
                '"%s" would be updated to "%s".' % (line.strip(), version))
        temp_file.write(new_line)
    if not dry_run:
      temp_file.flush()
      shutil.move(temp_file.name, file_path)
      temp_file.delete = False


def GetCipdPackagePath(pkg_yaml_file):
  """Find CIPD package path in .yaml file.

  There should one line in .yaml file, e.g.:
  "package: chrome_internal/third_party/android_sdk/internal/q/add-ons" or
  "package: chromium/third_party/android_sdk/public/platforms"

  Args:
    pkg_yaml_file: The yaml file to find CIPD package path.

  Returns:
    The CIPD package path in yaml file.
  """
  cipd_package_path = ''
  with open(pkg_yaml_file) as f:
    pattern = re.compile(
        # Match the argument with "package: "
        r'(^\s*package:\s*)'
        # The CIPD package path we want
        r'([\w\/-]+)'
        # End of string
        r'(\s*?$)')
    for line in f:
      found = re.match(pattern, line)
      if found:
        cipd_package_path = found.group(2)
        break
  return cipd_package_path


def UploadSdkPackage(sdk_root, dry_run, service_url, package, yaml_file,
                     verbose):
  """Build and upload a package instance file to CIPD.

  This would also update gn and ensure files to the package version as
  uploading to CIPD.

  Args:
    sdk_root: Root of the sdk packages.
    dry_run: Bool. To show what packages would be created and packages, without
      actually doing either.
    service_url: The url of the CIPD service.
    package: The package to be uploaded to CIPD.
    yaml_file: Path to the yaml file that defines what to put into the package.
      Default as //third_party/android_sdk/public/cipd_*.yaml
    verbose: Enable more logging.

  Returns:
    New instance ID when CIPD package created.

  Raises:
    IOError: cannot find .yaml file, CIPD package path or instance ID for
    package.
    CalledProcessError: cipd command failed to create package.
  """
  pkg_yaml_file = yaml_file or os.path.join(sdk_root, 'cipd_%s.yaml' % package)
  if not os.path.exists(pkg_yaml_file):
    raise IOError('Cannot find .yaml file for package %s' % package)

  cipd_package_path = GetCipdPackagePath(pkg_yaml_file)
  if not cipd_package_path:
    raise IOError('Cannot find CIPD package path in %s' % pkg_yaml_file)

  if dry_run:
    print('This `package` command (without -n/--dry-run) would create and ' +
          'upload the package %s to CIPD.' % package)
  else:
    upload_sdk_cmd = [
        'cipd', 'create', '-pkg-def', pkg_yaml_file, '-service-url', service_url
    ]
    if verbose:
      upload_sdk_cmd.extend(['-log-level', 'debug'])

    output = subprocess.check_output(upload_sdk_cmd)

    # Need to match pattern to find new instance ID.
    # e.g.: chromium/third_party/android_sdk/public/platforms:\
    # Kg2t9p0YnQk8bldUv4VA3o156uPXLUfIFAmVZ-Gm5ewC
    pattern = re.compile(
        # Match the argument with "Instance: %s:" for cipd_package_path
        (r'(^\s*Instance: %s:)' % cipd_package_path) +
        # instance ID e.g. DLK621q5_Bga5EsOr7cp6bHWWxFKx6UHLu_Ix_m3AckC.
        r'([-\w.]+)'
        # End of string
        r'(\s*?$)')
    for line in output.splitlines():
      found = re.match(pattern, line)
      if found:
        # Return new instance ID.
        return found.group(2)
    # Raises error if instance ID not found.
    raise IOError('Cannot find instance ID by creating package %s' % package)


def UpdateInstanceId(package,
                     deps_path,
                     dry_run,
                     new_instance_id,
                     release_version=None):
  """Find the sdk pkg version in DEPS and modify it as cipd uploading version.

  TODO(shenghuazhang): use DEPS edition operations after issue crbug.com/760633
  fixed.

  DEPS file hooks sdk package with version with suffix -crX, e.g. '26.0.2-cr1'.
  If pkg_version is the base number of the existing version in DEPS, e.g.
  '26.0.2', return '26.0.2-cr2' as the version uploading to CIPD. If not the
  base number, return ${pkg_version}-cr0.

  Args:
    package: The name of the package.
    deps_path: Path to deps file which gclient hooks sdk pkg w/ versions.
    dry_run: Bool. To show what packages would be created and packages, without
      actually doing either.
    new_instance_id: New instance ID after CIPD package created.
    release_version: Android sdk release version e.g. 'o_mr1', 'p'.
  """
  var_package = package
  if release_version:
    var_package = release_version + '_' + var_package
  package_var_pattern = re.compile(
      # Match the argument with "'android_sdk_*_version': '" with whitespaces.
      r'(^\s*\'android_sdk_%s_version\'\s*:\s*\')' % var_package +
      # instance ID e.g. DLK621q5_Bga5EsOr7cp6bHWWxFKx6UHLu_Ix_m3AckC.
      r'([-\w.]+)'
      # End of string
      r'(\',?$)')

  with tempfile.NamedTemporaryFile() as temp_file:
    with open(deps_path) as f:
      for line in f:
        new_line = line
        found = re.match(package_var_pattern, line)
        if found:
          instance_id = found.group(2)
          new_line = re.sub(package_var_pattern,
                            r'\g<1>%s\g<3>' % new_instance_id, line)
          print(
              '    Note: deps file "%s" argument ' % deps_path +
              '"%s" would be updated to "%s".' % (instance_id, new_instance_id))
        temp_file.write(new_line)

    if not dry_run:
      temp_file.flush()
      shutil.move(temp_file.name, deps_path)
      temp_file.delete = False


def ChangeVersionInGNI(package, arg_version, gn_args_dict, gni_file_path,
                       dry_run):
  """Change the sdk package version in config.gni file."""
  if package in gn_args_dict:
    version_config_name = gn_args_dict.get(package)
    # Regex to parse the line of sdk package version gn argument, e.g.
    # '  default_android_sdk_version = "27"'. Capture a group for the line
    # excluding the version.
    gn_arg_pattern = re.compile(
        # Match the argument with '=' and whitespaces. Capture a group for it.
        r'(^\s*%s\s*=\s*)' % version_config_name +
        # Optional quote.
        r'("?)' +
        # Version number. E.g. 27, 27.0.3, -26.0.0-dev
        r'(?:[-\w\s.]+)' +
        # Optional quote.
        r'("?)' +
        # End of string
        r'$')

    _ReplaceVersionInFile(gni_file_path, gn_arg_pattern, arg_version, dry_run)


def GetToolsSuffix(tools_lib_path):
  """Get the gn config of package 'tools' suffix.

  Check jar file name of 'common*.jar' in tools/lib, which could be
  'common.jar', common-<version>-dev.jar' or 'common-<version>.jar'.
  If suffix exists, return the suffix.

  Args:
    tools_lib_path: The path of tools/lib.

  Returns:
    The suffix of tools package.
  """
  tools_lib_jars_list = os.listdir(tools_lib_path)
  for file_name in tools_lib_jars_list:
    found = re.match(_COMMON_JAR_SUFFIX_PATTERN, file_name)
    if found:
      return found.group(1)


def _GetArgVersion(pkg_version, package):
  """Get the argument version.

  Args:
    pkg_version: The package version.
    package: The package name.

  Returns:
    The argument version.
  """
  # Remove all chars except for digits and dots in version
  arg_version = re.sub(r'[^\d\.]', '', pkg_version)

  if package == 'tools':
    suffix = GetToolsSuffix(_TOOLS_LIB_PATH)
    if suffix:
      arg_version = suffix
    else:
      arg_version = '-%s' % arg_version
  return arg_version


def _UploadSdkPackage(arguments):
  """Upload SDK packages to CIPD.

  Args:
    arguments: The arguments parsed by argparser.

  Raises:
    IOError: Don't use --version/--yaml-file for default packages.
  """
  packages = arguments.package
  if not packages:
    packages = _DEFAULT_DOWNLOAD_PACKAGES
    if arguments.version or arguments.yaml_file:
      raise IOError("Don't use --version/--yaml-file for default packages.")

  for package in packages:
    pkg_version = arguments.version
    if not pkg_version:
      pkg_version = _FindPackageVersion(package, arguments.sdk_root)

    # Upload SDK package to CIPD, and update the package instance ID hooking
    # in DEPS file.
    new_instance_id = UploadSdkPackage(
        os.path.join(arguments.sdk_root, '..'), arguments.dry_run,
        arguments.service_url, package, arguments.yaml_file, arguments.verbose)
    UpdateInstanceId(package, _SRC_DEPS_PATH, arguments.dry_run,
                     new_instance_id)

    if package in _GN_ARGUMENTS_TO_UPDATE:
      # Update the package version config in gn file
      arg_version = _GetArgVersion(pkg_version, package)
      ChangeVersionInGNI(package, arg_version, _GN_ARGUMENTS_TO_UPDATE,
                         _ANDROID_CONFIG_GNI_PATH, arguments.dry_run)


def main():
  parser = argparse.ArgumentParser(
      description='A script to download Android SDK packages '
      'via sdkmanager and upload to CIPD.')

  subparsers = parser.add_subparsers(title='commands')

  download_parser = subparsers.add_parser(
      'download',
      help='Download sdk package to the latest version from sdkmanager.')
  download_parser.set_defaults(func=_DownloadSdk)
  download_parser.add_argument(
      '-p',
      '--package',
      nargs=1,
      default=_DEFAULT_DOWNLOAD_PACKAGES,
      help='The package of the SDK needs to be installed/updated. '
      'Note that package name should be a sdk-style path e.g. '
      '"platforms;android-27" or "platform-tools". If package '
      'is not specified, update "build-tools;27.0.3", "tools" '
      '"platform-tools" and "platforms;android-27" by default.')
  download_parser.add_argument(
      '--sdk-root', help='base path to the Android SDK root')
  download_parser.add_argument(
      '-v', '--verbose', action='store_true', help='print debug information')

  package_parser = subparsers.add_parser(
      'package', help='Create and upload package instance file to CIPD.')
  package_parser.set_defaults(func=_UploadSdkPackage)
  package_parser.add_argument(
      '-n',
      '--dry-run',
      action='store_true',
      help='Dry run won\'t trigger creating instances or uploading packages. '
      'It shows what packages would be created and uploaded to CIPD. '
      'It also shows the possible updates of sdk version on files.')
  package_parser.add_argument(
      '-p',
      '--package',
      nargs=1,
      help='The package to be uploaded to CIPD. Note that package '
      'name is a simple path e.g. "platforms" or "build-tools" '
      'which matches package name on CIPD service. Default by '
      'build-tools, platform-tools, platforms and tools')
  package_parser.add_argument(
      '--version',
      help='Version of the uploading package instance through CIPD.')
  package_parser.add_argument(
      '--yaml-file',
      help='Path to *.yaml file that defines what to put into the package.'
      'Default as //third_party/android_sdk/public/cipd_<package>.yaml')
  package_parser.add_argument(
      '--service-url',
      help='The url of the CIPD service.',
      default='https://chrome-infra-packages.appspot.com')
  package_parser.add_argument(
      '--sdk-root', help='base path to the Android SDK root')
  package_parser.add_argument(
      '-v', '--verbose', action='store_true', help='print debug information')

  args = parser.parse_args()

  if not args.sdk_root:
    args.sdk_root = _SDK_PUBLIC_ROOT

  args.func(args)


if __name__ == '__main__':
  sys.exit(main())
