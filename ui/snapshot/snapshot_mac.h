// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SNAPSHOT_SNAPSHOT_MAC_H_
#define UI_SNAPSHOT_SNAPSHOT_MAC_H_

#include "ui/snapshot/snapshot_export.h"

namespace ui {

enum class SnapshotAPI {
  kUnspecified,
  kOldAPI,  // CGWindowListCreateImage()
  kNewAPI   // ScreenCaptureKit (requires macOS 14.4 or later)
};

// Forces the usage of a specific snapshotting API for testing purposes. Do not
// force `kNewAPI` on a macOS release before 14.4. Use `kUnspecified` to reset a
// forced value.
SNAPSHOT_EXPORT void ForceAPIUsageForTesting(SnapshotAPI api);

}  // namespace ui

#endif  // UI_SNAPSHOT_SNAPSHOT_MAC_H_
