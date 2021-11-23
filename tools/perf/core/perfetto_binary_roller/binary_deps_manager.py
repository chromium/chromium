# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import posixpath
import stat
import subprocess
import six

import py_utils
from py_utils import cloud_storage
from py_utils import tempfile_ext

# Binaries are publicly readable, data files are for internal use only.
BINARY_BUCKET = cloud_storage.PUBLIC_BUCKET
BINARY_CS_FOLDER = 'perfetto_binaries'
DATA_BUCKET = cloud_storage.INTERNAL_BUCKET
DATA_CS_FOLDER = 'perfetto_data'
LATEST_FILENAME = 'latest'
LOCAL_STORAGE_FOLDER = os.path.abspath(
  os.path.join(os.path.dirname(__file__), 'bin'))
CONFIG_PATH = os.path.abspath(
  os.path.join(os.path.dirname(__file__), 'binary_deps.json'))


def _GetHostArch():
  uname_arch = subprocess.check_output(['uname', '-m']).strip()
  # TODO(b/206008069): Remove the check when fixed.
  if hasattr(six, 'ensure_str'):
    uname_arch = six.ensure_str(uname_arch)
  if uname_arch == 'armv7l':
    return 'arm'
  elif uname_arch == 'aarch64':
    return 'arm64'
  return uname_arch


def _GetBinaryArch(binary_name):
  file_output = subprocess.check_output(['file', binary_name])
  # TODO(b/206008069): Remove the check when fixed.
  if hasattr(six, 'ensure_str'):
    file_output = six.ensure_str(file_output)
  file_arch = file_output.split(',')[1].strip()
  if file_arch == 'x86-64':
    return 'x86_64'
  elif file_arch == 'ARM':
    return 'arm'
  elif file_arch == 'ARM aarch64':
    return 'arm64'
  return file_arch


def _GetHostPlatform():
  os_name = py_utils.GetHostOsName()
  # If we're running directly on a Chrome OS device, fetch the binaries for
  # linux instead, which should be compatible with CrOS.
  if os_name in ['chromeos', 'linux']:
    arch = _GetHostArch()
    if arch == 'x86_64':
      return 'linux'
    return 'linux_' + arch
  return os_name


def _GetBinaryPlatform(binary_name):
  host_platform = _GetHostPlatform()
  # Binaries built on mac/windows are for mac/windows respectively. Binaries
  # built on linux may be for linux or chromeos on different architectures.
  if not host_platform.startswith('linux'):
    return host_platform
  arch = _GetBinaryArch(binary_name)
  if arch == 'x86_64':
    return 'linux'
  return 'linux' + '_' + arch


def _CalculateHash(remote_path):
  with tempfile_ext.NamedTemporaryFile() as f:
    f.close()
    cloud_storage.Get(BINARY_BUCKET, remote_path, f.name)
    return cloud_storage.CalculateHash(f.name)


def _SetLatestPathForBinary(binary_name, platform, latest_path):
  with tempfile_ext.NamedTemporaryFile(mode='w') as latest_file:
    latest_file.write(latest_path)
    latest_file.close()
    remote_latest_file = posixpath.join(BINARY_CS_FOLDER, binary_name, platform,
                                        LATEST_FILENAME)
    cloud_storage.Insert(BINARY_BUCKET,
                         remote_latest_file,
                         latest_file.name,
                         publicly_readable=True)


def UploadHostBinary(binary_name, binary_path, version):
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
  _SetLatestPathForBinary(binary_name, platform, remote_path)


def GetLatestPath(binary_name, platform):
  with tempfile_ext.NamedTemporaryFile() as latest_file:
    latest_file.close()
    remote_path = posixpath.join(BINARY_CS_FOLDER, binary_name, platform,
                                 LATEST_FILENAME)
    cloud_storage.Get(BINARY_BUCKET, remote_path, latest_file.name)
    with open(latest_file.name) as latest:
      return latest.read()


def GetCurrentPath(binary_name, platform):
  with open(CONFIG_PATH) as f:
    config = json.load(f)
  return config[binary_name][platform]['remote_path']


def SwitchBinaryToNewPath(binary_name, platform, new_path):
  """Switch the binary version in use to the latest one.

  This function updates the config file to contain the path to the latest
  available binary version. This will make FetchHostBinary download the latest
  file.
  """
  with open(CONFIG_PATH) as f:
    config = json.load(f)
  config.setdefault(binary_name, {}).setdefault(platform,
                                                {})['remote_path'] = new_path
  config.setdefault(binary_name,
                    {}).setdefault(platform,
                                   {})['hash'] = _CalculateHash(new_path)
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
  remote_path = config[binary_name][platform]['remote_path']
  expected_hash = config[binary_name][platform]['hash']
  filename = posixpath.basename(remote_path)
  local_path = os.path.join(LOCAL_STORAGE_FOLDER, filename)
  cloud_storage.Get(BINARY_BUCKET, remote_path, local_path)
  if cloud_storage.CalculateHash(local_path) != expected_hash:
    raise RuntimeError('The downloaded binary has wrong hash.')
  mode = os.stat(local_path).st_mode
  os.chmod(local_path, mode | stat.S_IXUSR)
  return local_path


def FetchDataFile(data_file_name):
  """Download the file from the cloud."""
  with open(CONFIG_PATH) as f:
    config = json.load(f)
  remote_path = config[data_file_name]['remote_path']
  expected_hash = config[data_file_name]['hash']
  filename = posixpath.basename(remote_path)
  local_path = os.path.join(LOCAL_STORAGE_FOLDER, filename)
  cloud_storage.Get(DATA_BUCKET, remote_path, local_path)
  if cloud_storage.CalculateHash(local_path) != expected_hash:
    raise RuntimeError('The downloaded data file has wrong hash.')
  return local_path


def UploadAndSwitchDataFile(data_file_name, data_file_path, version):
  """Upload the script to the cloud and update config to use the new version."""
  filename = os.path.basename(data_file_path)
  remote_path = posixpath.join(DATA_CS_FOLDER, data_file_name, version,
                               filename)
  if not cloud_storage.Exists(DATA_BUCKET, remote_path):
    cloud_storage.Insert(DATA_BUCKET,
                         remote_path,
                         data_file_path,
                         publicly_readable=False)

  with open(CONFIG_PATH) as f:
    config = json.load(f)
  config[data_file_name]['remote_path'] = remote_path
  config[data_file_name]['hash'] = cloud_storage.CalculateHash(data_file_path)
  with open(CONFIG_PATH, 'w') as f:
    json.dump(config, f, indent=4, separators=(',', ': '))
