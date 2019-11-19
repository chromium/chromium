// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/screenshot_grabber.h"

#include <stddef.h>

#include <climits>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop_current.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "ui/snapshot/snapshot.h"

#if defined(USE_AURA)
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#endif

namespace ui {

namespace {
// The minimum interval between two screenshot commands.  It has to be
// more than 1000 to prevent the conflict of filenames.
const int kScreenshotMinimumIntervalInMS = 1000;

}  // namespace

#if defined(USE_AURA)
class ScreenshotGrabber::ScopedCursorHider {
 public:
  // The nullptr might be returned when GetCursorClient is nullptr.
  static std::unique_ptr<ScopedCursorHider> Create(aura::Window* window) {
    DCHECK(window->IsRootWindow());
    aura::client::CursorClient* cursor_client =
        aura::client::GetCursorClient(window);
    if (!cursor_client)
      return nullptr;
    cursor_client->HideCursor();
    return std::unique_ptr<ScopedCursorHider>(
        base::WrapUnique(new ScopedCursorHider(window)));
  }

  ~ScopedCursorHider() {
    aura::client::CursorClient* cursor_client =
        aura::client::GetCursorClient(window_);
    cursor_client->ShowCursor();
  }

 private:
  explicit ScopedCursorHider(aura::Window* window) : window_(window) {}
  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCursorHider);
};
#endif

ScreenshotGrabber::ScreenshotGrabber() {}

ScreenshotGrabber::~ScreenshotGrabber() {
}

void ScreenshotGrabber::TakeScreenshot(gfx::NativeWindow window,
                                       const gfx::Rect& rect,
                                       ScreenshotCallback callback) {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  last_screenshot_timestamp_ = base::TimeTicks::Now();

  bool is_partial = true;
  // Window identifier is used to log a message on failure to capture a full
  // screen (i.e. non partial) screenshot. The only time is_partial can be
  // false, we will also have an identification string for the window.
  std::string window_identifier;
#if defined(USE_AURA)
  aura::Window* aura_window = static_cast<aura::Window*>(window);
  is_partial = rect.size() != aura_window->bounds().size();
  window_identifier = aura_window->GetBoundsInScreen().ToString();

  cursor_hider_ = ScopedCursorHider::Create(aura_window->GetRootWindow());
#endif
  ui::GrabWindowSnapshotAsyncPNG(
      window, rect,
      base::BindOnce(&ScreenshotGrabber::GrabWindowSnapshotAsyncCallback,
                     factory_.GetWeakPtr(), window_identifier, is_partial,
                     std::move(callback)));
}

bool ScreenshotGrabber::CanTakeScreenshot() {
  return last_screenshot_timestamp_.is_null() ||
         base::TimeTicks::Now() - last_screenshot_timestamp_ >
             base::TimeDelta::FromMilliseconds(kScreenshotMinimumIntervalInMS);
}

void ScreenshotGrabber::GrabWindowSnapshotAsyncCallback(
    const std::string& window_identifier,
    bool is_partial,
    ScreenshotCallback callback,
    scoped_refptr<base::RefCountedMemory> png_data) {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());

#if defined(USE_AURA)
  cursor_hider_.reset();
#endif

  if (!png_data.get()) {
    if (is_partial) {
      LOG(ERROR) << "Failed to grab the window screenshot";
      std::move(callback).Run(ScreenshotResult::GRABWINDOW_PARTIAL_FAILED,
                              nullptr);
    } else {
      LOG(ERROR) << "Failed to grab the window screenshot for "
                 << window_identifier;
      std::move(callback).Run(ScreenshotResult::GRABWINDOW_FULL_FAILED,
                              nullptr);
    }
    return;
  }

  std::move(callback).Run(ScreenshotResult::SUCCESS, std::move(png_data));
}

}  // namespace ui
