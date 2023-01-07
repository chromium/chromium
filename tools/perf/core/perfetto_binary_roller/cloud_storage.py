# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A small subset of commands for interaction with Google Cloud Storage.
It's used when py_utils from the catapult repo are not available (as is the case
on autorollers).
"""

import hashlib
import os
import subprocess

PUBLIC_BUCKET = 'chromium-telemetry'
INTERNAL_BUCKET = 'chrome-telemetry'


def _RunCommand(args):
  gsutil_command = 'gsutil'
  pathenv = os.getenv('PATH')
  for path in pathenv.split(os.path.pathsep):
    gsutil_path = os.path.join(path, gsutil_command)
    if os.path.exists(gsutil_path):
      break

  gsutil = subprocess.Popen([gsutil_path] + args,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
  stdout, stderr = gsutil.communicate()
  if gsutil.returncode:
    raise RuntimeError(stderr.decode('utf-8'))
  return stdout.decode('utf-8')


def Get(bucket, remote_path, local_path):
  url = 'gs://%s/%s' % (bucket, remote_path)
  _RunCommand(['cp', url, local_path])


def Exists(bucket, remote_path):
  try:
    _RunCommand(['ls', 'gs://%s/%s' % (bucket, remote_path)])
    return True
  except RuntimeError:
    return False


def ListFiles(bucket, path='', sort_by='name'):
  """Returns files matching the given path in bucket.

  Args:
    bucket: Name of cloud storage bucket to look at.
    path: Path within the bucket to filter to. Path can include wildcards.
    sort_by: 'name' (default), 'time' or 'size'.

  Returns:
    A sorted list of files.
  """
  bucket_prefix = 'gs://%s' % bucket
  full_path = '%s/%s' % (bucket_prefix, path)
  stdout = _RunCommand(['ls', '-l', '-d', full_path])

  # Filter out directories and the summary line.
  file_infos = [
      line.split(None, 2) for line in stdout.splitlines() if len(line) > 0
      and not line.startswith("TOTAL") and not line.endswith('/')
  ]

  # The first field in the info is size, the second is time, the third is name.
  if sort_by == 'size':
    file_infos.sort(key=lambda info: int(info[0]))
  elif sort_by == 'time':
    file_infos.sort(key=lambda info: info[1])
  elif sort_by == 'name':
    file_infos.sort(key=lambda info: info[2])
  else:
    raise ValueError("Wrong sort_by value: %s" % sort_by)

  return [url[len(bucket_prefix):] for _, _, url in file_infos]


def Insert(bucket, remote_path, local_path, publicly_readable):
  """Upload file in |local_path| to cloud storage.

  Newer version of 'Insert()' returns an object instead of a string.

  Args:
    bucket: the google cloud storage bucket name.
    remote_path: the remote file path in |bucket|.
    local_path: path of the local file to be uploaded.
    publicly_readable: whether the uploaded file has publicly readable
    permission.

  Returns:
    A CloudFilepath object providing the location of the object in various
    formats.
  """
  url = 'gs://%s/%s' % (bucket, remote_path)
  command_and_args = ['cp']
  if publicly_readable:
    command_and_args += ['-a', 'public-read']
  command_and_args += [local_path, url]
  _RunCommand(command_and_args)


def CalculateHash(file_path):
  """Calculates and returns the hash of the file at file_path."""
  sha1 = hashlib.sha1()
  with open(file_path, 'rb') as f:
    while True:
      # Read in 1mb chunks, so it doesn't all have to be loaded into memory.
      chunk = f.read(1024 * 1024)
      if not chunk:
        break
      sha1.update(chunk)
  return sha1.hexdigest()
