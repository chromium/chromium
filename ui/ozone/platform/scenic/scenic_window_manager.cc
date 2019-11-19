// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_window_manager.h"

#include <lib/sys/cpp/component_context.h>
#include <memory>

#include "base/fuchsia/default_context.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "ui/ozone/platform/scenic/ozone_platform_scenic.h"

namespace ui {

ScenicWindowManager::ScenicWindowManager() = default;
ScenicWindowManager::~ScenicWindowManager() = default;

std::unique_ptr<PlatformScreen> ScenicWindowManager::CreateScreen() {
  DCHECK(windows_.IsEmpty());
  auto screen = std::make_unique<ScenicScreen>();
  screen_ = screen->GetWeakPtr();
  return screen;
}

fuchsia::ui::scenic::Scenic* ScenicWindowManager::GetScenic() {
  if (!scenic_) {
    scenic_ = base::fuchsia::ComponentContextForCurrentProcess()
                  ->svc()
                  ->Connect<fuchsia::ui::scenic::Scenic>();
    scenic_.set_error_handler(
        [](zx_status_t status) { ZX_LOG(FATAL, status) << " Scenic lost."; });
  }
  return scenic_.get();
}

int32_t ScenicWindowManager::AddWindow(ScenicWindow* window) {
  int32_t id = windows_.Add(window);
  if (screen_)
    screen_->OnWindowAdded(id);
  return id;
}

void ScenicWindowManager::RemoveWindow(int32_t window_id,
                                       ScenicWindow* window) {
  DCHECK_EQ(window, windows_.Lookup(window_id));
  windows_.Remove(window_id);
  if (screen_)
    screen_->OnWindowRemoved(window_id);
}

ScenicWindow* ScenicWindowManager::GetWindow(int32_t window_id) {
  return windows_.Lookup(window_id);
}

}  // namespace ui
