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

_SHARED_STORAGE_ITERATOR_HISTOGRAMS = [
    "Storage.SharedStorage.Worklet.Timing.Entries.Next",
    "Storage.SharedStorage.Worklet.Timing.Keys.Next",
    "Storage.SharedStorage.Worklet.Timing.Values.Next",
]

_EVENT_TYPE_TO_HISTOGRAMS_MAP = {
    "documentAddModule": ["Storage.SharedStorage.Document.Timing.AddModule"],
    "documentAppend": ["Storage.SharedStorage.Document.Timing.Append"],
    "documentClear": ["Storage.SharedStorage.Document.Timing.Clear"],
    "documentDelete": ["Storage.SharedStorage.Document.Timing.Delete"],
    "documentRun": [
        "Storage.SharedStorage.Document.Timing.Run",
        "Storage.SharedStorage.Document.Timing.Run.ExecutedInWorklet",
    ],
    "documentSelectURL": [
        "Storage.SharedStorage.Document.Timing.SelectURL",
        "Storage.SharedStorage.Document.Timing.SelectURL.ExecutedInWorklet",
    ],
    "documentSet": ["Storage.SharedStorage.Document.Timing.Set"],
    "workletAppend": ["Storage.SharedStorage.Worklet.Timing.Append"],
    "workletClear": ["Storage.SharedStorage.Worklet.Timing.Clear"],
    "workletDelete": ["Storage.SharedStorage.Worklet.Timing.Delete"],
    "workletEntries": ["Storage.SharedStorage.Worklet.Timing.Entries.Next"],
    "workletGet": ["Storage.SharedStorage.Worklet.Timing.Get"],
    "workletKeys": ["Storage.SharedStorage.Worklet.Timing.Keys.Next"],
    "workletLength": ["Storage.SharedStorage.Worklet.Timing.Length"],
    "workletRemainingBudget":
    ["Storage.SharedStorage.Worklet.Timing.RemainingBudget"],
    "workletSet": ["Storage.SharedStorage.Worklet.Timing.Set"],
}


def GetSharedStorageUmaHistograms():
  return _SHARED_STORAGE_UMA_HISTOGRAMS


def GetSharedStorageIteratorHistograms():
  return _SHARED_STORAGE_ITERATOR_HISTOGRAMS


def GetHistogramsFromEventType(event_type):
  return _EVENT_TYPE_TO_HISTOGRAMS_MAP.get(event_type, [])
