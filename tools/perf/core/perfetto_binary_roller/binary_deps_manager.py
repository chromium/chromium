# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import posixpath
import stat
import subprocess
import sys
import tempfile
import six

# When py_utils from the catapult repo are not available (as is the case
# on autorollers), import a small replacement.
try:
  from py_utils import cloud_storage
except ImportError:
  from core.perfetto_binary_roller import cloud_storage


# Binaries are publicly readable, data files are for internal use only.
BINARY_BUCKET = cloud_storage.PUBLIC_BUCKET
BINARY_CS_FOLDER = 'perfetto_binaries'
DATA_BUCKET = cloud_storage.INTERNAL_BUCKET
DATA_CS_FOLDER = 'perfetto_data'
LATEST_FILENAME = 'latest'

# This is the bucket where Perfetto LUCI prebuilts are stored
PERFETTO_BINARY_BUCKET = 'perfetto-luci-artifacts'
PLATFORM_TO_PERFETTO_FOLDER = {
    'linux': 'linux-amd64',
    'linux_arm': 'linux-arm',
    'linux_arm64': 'linux-arm64',
    'mac': 'mac-amd64',
    'mac_arm': 'mac-arm',
    'mac_arm64': 'mac-arm64',
    'win': 'windows-amd64',
}

LOCAL_STORAGE_FOLDER = os.path.abspath(
  os.path.join(os.path.dirname(__file__), 'bin'))
CONFIG_PATH = os.path.abspath(
  os.path.join(os.path.dirname(__file__), 'binary_deps.json'))


def _IsRunningOnCrosDevice():
  """Returns True if we're on a ChromeOS device."""
  lsb_release = '/etc/lsb-release'
  if sys.platform.startswith('linux') and os.path.exists(lsb_release):
    with open(lsb_release, 'r') as f:
      res = f.read()
      if res.count('CHROMEOS_RELEASE_NAME'):
        return True
  return False


def _GetHostOsName():
  if _IsRunningOnCrosDevice():
    return 'chromeos'
  if sys.platform.startswith('linux'):
    return 'linux'
  if sys.platform == 'darwin':
    return 'mac'
  if sys.platform == 'win32':
    return 'win'
  return None


def _GetHostArch():
  uname_arch = six.ensure_str(subprocess.check_output(['uname', '-m']).strip())
  if uname_arch == 'armv7l':
    return 'arm'
  if uname_arch == 'aarch64':
    return 'arm64'
  return uname_arch


def _GetLinuxBinaryArch(binary_name):
  file_output = six.ensure_str(subprocess.check_output(['file', binary_name]))
  file_arch = file_output.split(',')[1].strip()
  if file_arch == 'x86-64':
    return 'x86_64'
  if file_arch == 'ARM':
    return 'arm'
  if file_arch == 'ARM aarch64':
    return 'arm64'
  return file_arch


def _GetMacBinaryArch(binary_name):
  file_output = six.ensure_str(subprocess.check_output(['file', binary_name]))
  return file_output.split()[-1].strip()


def _GetHostPlatform():
  os_name = _GetHostOsName()
  # If we're running directly on a Chrome OS device, fetch the binaries for
  # linux instead, which should be compatible with CrOS.
  if os_name in ['chromeos', 'linux']:
    arch = _GetHostArch()
    if arch == 'x86_64':
      return 'linux'
    return 'linux_' + arch
  if os_name == 'mac':
    return 'mac_arm64' if _GetHostArch() == 'arm64' else 'mac'
  return os_name


def _GetBinaryPlatform(binary_name):
  host_platform = _GetHostPlatform()

  # Binaries built on linux may be for linux or chromeos on different
  # architectures.
  if host_platform.startswith('linux'):
    arch = _GetLinuxBinaryArch(binary_name)
    if arch == 'x86_64':
      return 'linux'
    return 'linux' + '_' + arch

  # Binaries built on mac may be either for arm64 or intel.
  if host_platform.startswith('mac'):
    arch = _GetMacBinaryArch(binary_name)
    if arch == 'x86_64':
      return 'mac'
    return 'mac' + '_' + arch

  # Binaries built on windows are for windows intel always.
  return host_platform


def _CalculateHash(full_remote_path):
  bucket, remote_path = full_remote_path.split('/', 1)
  with tempfile.NamedTemporaryFile(delete=False) as f:
    f.close()
    cloud_storage.Get(bucket, remote_path, f.name)
    return cloud_storage.CalculateHash(f.name)


def _SetLatestPathForBinaryChromium(binary_name, platform, latest_path):
  with tempfile.NamedTemporaryFile(mode='w', delete=False) as latest_file:
    latest_file.write(latest_path)
    latest_file.close()
    remote_latest_file = posixpath.join(BINARY_CS_FOLDER, binary_name, platform,
                                        LATEST_FILENAME)
    cloud_storage.Insert(BINARY_BUCKET,
                         remote_latest_file,
                         latest_file.name,
                         publicly_readable=True)


def UploadHostBinaryChromium(binary_name, binary_path, version):
  """Upload the binary to the cloud.

  This function uploads the host binary (e.g. trace_processor_shell) to the
  cloud and updates the 'latest' file for the host platform to point to the
  newly uploaded file. Note that it doesn't modify the config and so doesn't
  affect which binaries will be downloaded by FetchHostBinary.
  """
  filename = os.path.basename(binary_path)
  platform = _GetBinaryPlatform(binary_path)
  remote_path = posixpath.join(BINARY_CS_FOLDER, binary_name, platform, version,
                               filename)
  if not cloud_storage.Exists(BINARY_BUCKET, remote_path):
    cloud_storage.Insert(BINARY_BUCKET,
                         remote_path,
                         binary_path,
                         publicly_readable=True)
  _SetLatestPathForBinaryChromium(binary_name, platform, remote_path)


def GetLatestFullPathChromium(binary_name, platform):
  with tempfile.NamedTemporaryFile(delete=False) as latest_file:
    latest_file.close()
    remote_path = posixpath.join(BINARY_CS_FOLDER, binary_name, platform,
                                 LATEST_FILENAME)
    cloud_storage.Get(BINARY_BUCKET, remote_path, latest_file.name)
    with open(latest_file.name) as latest:
      return posixpath.join(BINARY_BUCKET, latest.read())


def GetLatestFullPathPerfetto(binary_name, platform):
  path_wildcard = ('*/%s/%s' %
                   (PLATFORM_TO_PERFETTO_FOLDER[platform], binary_name))
  path_list = cloud_storage.ListFiles(PERFETTO_BINARY_BUCKET,
                                      path_wildcard,
                                      sort_by='time')
  if not path_list:
    raise RuntimeError('No pre-built binary found for platform %s.' % platform)
  return PERFETTO_BINARY_BUCKET + path_list[-1]


def GetCurrentFullPath(binary_name, platform):
  with open(CONFIG_PATH) as f:
    config = json.load(f)
  return config[binary_name][platform]['full_remote_path']


def SwitchBinaryToNewFullPath(binary_name, platform, new_full_path):
  """Switch the binary version in use to the latest one.

  This function updates the config file to contain the path to the latest
  available binary version. This will make FetchHostBinary download the latest
  file.
  """
  with open(CONFIG_PATH) as f:
    config = json.load(f)
  config.setdefault(binary_name,
                    {}).setdefault(platform,
                                   {})['full_remote_path'] = new_full_path
  config.setdefault(binary_name,
                    {}).setdefault(platform,
                                   {})['hash'] = _CalculateHash(new_full_path)
  with open(CONFIG_PATH, 'w') as f:
    json.dump(config, f, indent=4, separators=(',', ': '))


def FetchHostBinary(binary_name):
  """Download the binary from the cloud.

  This function fetches the binary for the host platform from the cloud.
  The cloud path is read from the config.
  """
  with open(CONFIG_PATH) as f:
    config = json.load(f)
  platform = _GetHostPlatform()
  full_remote_path = config[binary_name][platform]['full_remote_path']
  bucket, remote_path = full_remote_path.split('/', 1)
  expected_hash = config[binary_name][platform]['hash']
  filename = posixpath.basename(remote_path)
  local_path = os.path.join(LOCAL_STORAGE_FOLDER, filename)
  cloud_storage.Get(bucket, remote_path, local_path)
  if cloud_storage.CalculateHash(local_path) != expected_hash:
    raise RuntimeError('The downloaded binary has wrong hash.')
  mode = os.stat(local_path).st_mode
  os.chmod(local_path, mode | stat.S_IXUSR)
  return local_path


def FetchDataFile(data_file_name):
  """Download the file from the cloud."""
  with open(CONFIG_PATH) as f:
    config = json.load(f)
  full_remote_path = config[data_file_name]['full_remote_path']
  bucket, remote_path = full_remote_path.split('/', 1)
  expected_hash = config[data_file_name]['hash']
  filename = posixpath.basename(remote_path)
  local_path = os.path.join(LOCAL_STORAGE_FOLDER, filename)
  cloud_storage.Get(bucket, remote_path, local_path)
  if cloud_storage.CalculateHash(local_path) != expected_hash:
    raise RuntimeError('The downloaded data file has wrong hash.')
  return local_path


def UploadAndSwitchDataFile(data_file_name, data_file_path, version):
  """Upload the script to the cloud and update config to use the new version."""
  filename = os.path.basename(data_file_path)
  bucket = DATA_BUCKET
  remote_path = posixpath.join(DATA_CS_FOLDER, data_file_name, version,
                               filename)
  full_remote_path = posixpath.join(bucket, remote_path)
  if not cloud_storage.Exists(DATA_BUCKET, remote_path):
    cloud_storage.Insert(bucket,
                         remote_path,
                         data_file_path,
                         publicly_readable=False)

  with open(CONFIG_PATH) as f:
    config = json.load(f)
  config[data_file_name]['full_remote_path'] = full_remote_path
  config[data_file_name]['hash'] = cloud_storage.CalculateHash(data_file_path)
  with open(CONFIG_PATH, 'w') as f:
    json.dump(config, f, indent=4, separators=(',', ': '))
