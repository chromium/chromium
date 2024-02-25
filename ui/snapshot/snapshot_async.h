// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SNAPSHOT_SNAPSHOT_ASYNC_H_
#define UI_SNAPSHOT_SNAPSHOT_ASYNC_H_

#include <memory>

#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "ui/snapshot/snapshot.h"

namespace gfx {
class Size;
}

namespace ui {

// Helper methods for async snapshots to convert a viz::CopyOutputResult into a
// ui::GrabWindowSnapshot callback.
class SnapshotAsync {
 public:
  SnapshotAsync() = delete;
  SnapshotAsync(const SnapshotAsync&) = delete;
  SnapshotAsync& operator=(const SnapshotAsync&) = delete;

  static void ScaleCopyOutputResult(
      GrabSnapshotImageCallback callback,
      const gfx::Size& target_size,
      std::unique_ptr<viz::CopyOutputResult> result);

  static void RunCallbackWithCopyOutputResult(
      GrabSnapshotImageCallback callback,
      std::unique_ptr<viz::CopyOutputResult> result);
};

}  // namespace ui

#endif  // UI_SNAPSHOT_SNAPSHOT_ASYNC_H_
