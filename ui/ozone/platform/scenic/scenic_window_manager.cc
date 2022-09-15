// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_window_manager.h"

#include <lib/sys/cpp/component_context.h>
#include <memory>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "ui/ozone/platform/scenic/ozone_platform_scenic.h"

namespace ui {

ScenicWindowManager::ScenicWindowManager() = default;

ScenicWindowManager::~ScenicWindowManager() {
  Shutdown();
}

void ScenicWindowManager::Shutdown() {
  DCHECK(windows_.IsEmpty());
  scenic_ = nullptr;
}

std::unique_ptr<PlatformScreen> ScenicWindowManager::CreateScreen() {
  DCHECK(windows_.IsEmpty());
  return std::make_unique<ScenicScreen>();
}

fuchsia::ui::scenic::Scenic* ScenicWindowManager::GetScenic() {
  if (!scenic_) {
    scenic_ = base::ComponentContextForProcess()
                  ->svc()
                  ->Connect<fuchsia::ui::scenic::Scenic>();
    scenic_.set_error_handler(base::LogFidlErrorAndExitProcess(
        FROM_HERE, "fuchsia.ui.scenic.Scenic"));
  }
  return scenic_.get();
}

int32_t ScenicWindowManager::AddWindow(ScenicWindow* window) {
  return windows_.Add(window);
}

void ScenicWindowManager::RemoveWindow(int32_t window_id,
                                       ScenicWindow* window) {
  DCHECK_EQ(window, windows_.Lookup(window_id));
  windows_.Remove(window_id);
}

ScenicWindow* ScenicWindowManager::GetWindow(int32_t window_id) {
  return windows_.Lookup(window_id);
}

}  // namespace ui
