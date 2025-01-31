# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dataclasses
import hashlib
import os
import pickle
from typing import Optional, Any, Dict


@dataclasses.dataclass(frozen=True)
class _PresubmitCheckContext:
  """Describes and identifies a context of a specific presubmit check.

  This is used as a key to cache results of a presubmit check. The histograms
  directory hash is used to lower the probability of changes in one file
  impacing health status of presubmit checks in the other. This still doesn't
  eliminate the risk of changes from outside of this directory affecting the
  health status, but given how PRESUBMIT is triggered, this seems to be inline
  with the risk of that happening because of PRESUBMIT not being triggered.

  Attributes:
    histograms_directory_hash: A sha256 hash of the entire contents of the
      histograms directory (combined hash of all the files) that is used to
      key the cache and invalidate it when the directory content changes.
    check_id: A unique identifier of the check that is being cached. As the
      single directory presubmit can have more then one check in it, this id
      is used to determine which cache content should be used for which check.
  """
  histograms_directory_hash: str
  check_id: int

  def key(self):
    return f"{self.histograms_directory_hash}:{self.check_id}"


_CURRENT_CACHE_FILE_SCHEMA_VERSION = 1


@dataclasses.dataclass(frozen=True)
class CacheFileSchema:
  """Describes the schema of the cache file."""
  version: int
  data: Dict[str, Any]


def _CalculateCombinedDirectoryHash(directory_path):
  """Calculates a sha256 hash of the entire contents of a directory."""
  hasher = hashlib.sha256()
  for root, _, files in os.walk(directory_path):
    for file in sorted(files):
      file_path = os.path.join(root, file)
      # Read the file in chunks to avoid loading the entire file into memory.
      # Chunk of 4kb is arbitrary multiple of sha256 block size of 64b.
      with open(file_path, "rb") as f:
        chunk = f.read(4096)
        while chunk:
          hasher.update(chunk)
          chunk = f.read(4096)
  return hasher.hexdigest()


class PresubmitCache:
  """Stores and retrieves results of a presubmit checks for presubmits."""

  _cache_contents: CacheFileSchema
  _storage_file_path: str
  _observed_directory: str

  def __init__(self, storage_file_path: str, observed_directory_path: str):
    self._storage_file_path = storage_file_path
    self._observed_directory = observed_directory_path
    self._cache_contents = CacheFileSchema(
        version=_CURRENT_CACHE_FILE_SCHEMA_VERSION,
        data={},
    )

    if not os.path.exists(self._storage_file_path) or os.path.getsize(
        self._storage_file_path) == 0:
      return

    # Attempt to restore the cache from the file. If it fails we will just
    # create a new, empty cache.
    with open(self._storage_file_path, "rb") as f:
      try:
        loaded_cache = pickle.load(f)
        if loaded_cache.version == _CURRENT_CACHE_FILE_SCHEMA_VERSION:
          self._cache_contents = loaded_cache
      except pickle.PickleError:
        pass

  def _GetForContext(self, context: _PresubmitCheckContext) -> Optional[str]:
    if context.key() not in self._cache_contents.data:
      return None
    return self._cache_contents.data[context.key()]

  def _StoreForContext(self, context: _PresubmitCheckContext,
                       check_result: Any):
    self._cache_contents.data[context.key()] = check_result
    self._SaveCurrentCache()

  def _SaveCurrentCache(self):
    with open(self._storage_file_path, "wb") as f:
      pickle.dump(self._cache_contents, f)

  def InspectCacheForTesting(self) -> CacheFileSchema:
    return self._cache_contents

  def RetrieveResultFromCache(self, check_id: int) -> Optional[Any]:
    return self._GetForContext(
        _PresubmitCheckContext(
            histograms_directory_hash=_CalculateCombinedDirectoryHash(
                self._observed_directory),
            check_id=check_id,
        ))

  def StoreResultInCache(self, check_id: int, check_result: Any):
    self._StoreForContext(
        _PresubmitCheckContext(
            histograms_directory_hash=_CalculateCombinedDirectoryHash(
                self._observed_directory),
            check_id=check_id,
        ), check_result)
