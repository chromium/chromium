# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helpers for compressing resources in parallel."""

import concurrent.futures
import contextlib
import os

# Set while within ParallelCompression().
_executor = None


@contextlib.contextmanager
def ParallelCompression():
  """Makes CompressMaybeAsync() return futures for the enclosed scope.

  Compression runs in subprocesses (brotli, zstd) or in zlib (gzip), all of
  which release the GIL, so threads give true parallelism.
  """
  global _executor
  assert _executor is None
  # Capped since grit may itself be running in several forked processes (and
  # alongside other build actions).
  max_workers = min(8, os.cpu_count() or 1)
  with concurrent.futures.ThreadPoolExecutor(max_workers) as executor:
    _executor = executor
    try:
      yield
    finally:
      _executor = None


def _Compress(prefix, func, args):
  return prefix + func(*args)


def CompressMaybeAsync(func, *args, prefix=b''):
  """Returns |prefix| + func(*args), or a future for it when within
  ParallelCompression().
  """
  if _executor is not None:
    return _executor.submit(_Compress, prefix, func, args)
  return _Compress(prefix, func, args)


def Resolve(value):
  """Returns the result of |value| if it is a future, or |value| otherwise."""
  if isinstance(value, concurrent.futures.Future):
    return value.result()
  return value
