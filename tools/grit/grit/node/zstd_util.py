# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Framework for compressing resources using Zstandard."""

import re
import subprocess

__zstd_executable = None

# Matches compress="zstd", compress="zstd19", compress="zstd-2", etc.
_COMPRESS_ATTR_RE = re.compile(r'zstd(-?\d+)?')

# Level used for compress="zstd". Chosen to achieve close to the maximum
# compression ratio without the steepest compression-time cost.
_DEFAULT_LEVEL = 15

# Levels above this require --ultra.
_MAX_NON_ULTRA_LEVEL = 19


def SetZstdCommand(zstd):
  # zstd is a list. In production it contains the path to the zstd executable.
  # During testing it contains [python, mock_zstd.py].
  global __zstd_executable
  __zstd_executable = zstd


def ParseCompressLevel(value):
  """Returns the zstd level for a compress="" value, or None if not zstd."""
  m = _COMPRESS_ATTR_RE.fullmatch(value)
  if not m:
    return None
  return int(m.group(1)) if m.group(1) is not None else _DEFAULT_LEVEL


def ZstdCompress(data, level):
  """Compresses |data| into a single zstd frame that contains its size."""
  if not __zstd_executable:
    raise Exception(
      'Pass --zstd to grit (or set "use_zstd = true" in GN) to use zstd.'
    )
  # --stream-size causes the decompressed size to be stored in the frame
  # header. The checksum is omitted (saving 4 bytes per resource) since .pak
  # files are not prone to corruption.
  args = ['-q', '-c', '--no-check', '--stream-size=%d' % len(data)]
  if level < 0:
    args.append('--fast=%d' % -level)
  else:
    if level > _MAX_NON_ULTRA_LEVEL:
      args.append('--ultra')
    args.append('-%d' % level)
  compress = subprocess.Popen(
    __zstd_executable + args,
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
  )
  stdout, stderr = compress.communicate(data)
  if compress.returncode != 0:
    raise Exception(
      'zstd compression failed with return code %d: %s'
      % (compress.returncode, stderr.decode('utf-8', errors='replace'))
    )
  return stdout
