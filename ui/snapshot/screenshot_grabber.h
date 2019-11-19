// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SNAPSHOT_SCREENSHOT_GRABBER_H_
#define UI_SNAPSHOT_SCREENSHOT_GRABBER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/snapshot/snapshot_export.h"

namespace ui {

// Result of the entire screenshotting attempt. This enum is fat for various
// file operations which could happen in the chrome layer.
enum class ScreenshotResult {
  SUCCESS,
  GRABWINDOW_PARTIAL_FAILED,
  GRABWINDOW_FULL_FAILED,
  CREATE_DIR_FAILED,
  GET_DIR_FAILED,
  CHECK_DIR_FAILED,
  CREATE_FILE_FAILED,
  WRITE_FILE_FAILED,
  DISABLED
};

class SNAPSHOT_EXPORT ScreenshotGrabber {
 public:
  ScreenshotGrabber();
  ~ScreenshotGrabber();

  // Callback for the new system, which ignores the observer crud.
  using ScreenshotCallback =
      base::OnceCallback<void(ui::ScreenshotResult screenshot_result,
                              scoped_refptr<base::RefCountedMemory> png_data)>;

  // Takes a screenshot of |rect| in |window| in that window's coordinate space
  // and return it to |callback|.
  void TakeScreenshot(gfx::NativeWindow window,
                      const gfx::Rect& rect,
                      ScreenshotCallback callback);

  bool CanTakeScreenshot();

 private:
#if defined(USE_AURA)
  class ScopedCursorHider;
#endif

  void GrabWindowSnapshotAsyncCallback(
      const std::string& window_identifier,
      bool is_partial,
      ScreenshotCallback callback,
      scoped_refptr<base::RefCountedMemory> png_data);

  // The timestamp when the screenshot task was issued last time.
  base::TimeTicks last_screenshot_timestamp_;

#if defined(USE_AURA)
  // The object to hide cursor when taking screenshot.
  std::unique_ptr<ScopedCursorHider> cursor_hider_;
#endif

  base::WeakPtrFactory<ScreenshotGrabber> factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ScreenshotGrabber);
};

}  // namespace ui

#endif  // UI_SNAPSHOT_SCREENSHOT_GRABBER_H_
