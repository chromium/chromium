# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Shared-storage-related histograms to measure.
_SHARED_STORAGE_UMA_HISTOGRAMS = [
    "Storage.SharedStorage.Document.Timing.AddModule",
    "Storage.SharedStorage.Document.Timing.Append",
    "Storage.SharedStorage.Document.Timing.Clear",
    "Storage.SharedStorage.Document.Timing.Delete",
    "Storage.SharedStorage.Document.Timing.Run",
    "Storage.SharedStorage.Document.Timing.Run.ExecutedInWorklet",
    "Storage.SharedStorage.Document.Timing.SelectURL",
    "Storage.SharedStorage.Document.Timing.SelectURL.ExecutedInWorklet",
    "Storage.SharedStorage.Document.Timing.Set",
    "Storage.SharedStorage.Worklet.Timing.Append",
    "Storage.SharedStorage.Worklet.Timing.Clear",
    "Storage.SharedStorage.Worklet.Timing.Delete",
    "Storage.SharedStorage.Worklet.Timing.Entries.Next",
    "Storage.SharedStorage.Worklet.Timing.Get",
    "Storage.SharedStorage.Worklet.Timing.Keys.Next",
    "Storage.SharedStorage.Worklet.Timing.Length",
    "Storage.SharedStorage.Worklet.Timing.RemainingBudget",
    "Storage.SharedStorage.Worklet.Timing.Set",
    "Storage.SharedStorage.Worklet.Timing.Values.Next",
]


def GetSharedStorageUmaHistograms():
  return _SHARED_STORAGE_UMA_HISTOGRAMS
