# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dataclasses
import hashlib
import os
import pickle
from typing import Optional, Any, Dict
from pathlib import Path

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


_CURRENT_CACHE_FILE_SCHEMA_VERSION = "v2"


@dataclasses.dataclass(frozen=True)
class CacheFileSchema:
  """Describes the schema of the cache file."""
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

  def __init__(self, storage_directory_path: str, observed_directory_path: str):
    base_dir_path = Path(storage_directory_path)
    versioned_path = base_dir_path.joinpath(_CURRENT_CACHE_FILE_SCHEMA_VERSION)
    versioned_path.mkdir(parents=True, exist_ok=True)

    self._storage_file_path = str(versioned_path.joinpath("cache.json"))
    self._observed_directory = observed_directory_path
    self._cache_contents = CacheFileSchema(data={})

    if not os.path.exists(self._storage_file_path) or os.path.getsize(
        self._storage_file_path) == 0:
      return

    # Attempt to restore the cache from the file. If it fails we will just
    # create a new, empty cache.
    with open(self._storage_file_path, "rb") as f:
      try:
        self._cache_contents = pickle.load(f)
        cache_needs_invalidation = False
      except Exception as e:
        # Blank exception - it's better to ignore cache then to break
        # the presubmit because of cache failure.
        print(f"Cache is being as it failed to finish reading: {e}")
        cache_needs_invalidation = True

      if not cache_needs_invalidation:
        return

    try:
      os.remove(self._storage_file_path)
    except Exception as e:
      # Again using blank exception, just because failing here
      # causes issues with presubmits.
      print(f"Failed to delete the cache file ({e}). To invalidate cache,"
            f" please try to remove {self._storage_file_path} manually.")


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
