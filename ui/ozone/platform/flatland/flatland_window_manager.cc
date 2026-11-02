// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_window_manager.h"

#include <lib/sys/cpp/component_context.h>
#include <memory>

#include "base/fuchsia/process_context.h"

namespace ui {

FlatlandWindowManager::FlatlandWindowManager() = default;

FlatlandWindowManager::~FlatlandWindowManager() {
  Shutdown();
}

void FlatlandWindowManager::Shutdown() {
  CHECK(windows_.IsEmpty(), base::NotFatalUntil::M163);
}

std::unique_ptr<PlatformScreen> FlatlandWindowManager::CreateScreen() {
  CHECK(windows_.IsEmpty(), base::NotFatalUntil::M163);
  return std::make_unique<FlatlandScreen>();
}

int32_t FlatlandWindowManager::AddWindow(FlatlandWindow* window) {
  return windows_.Add(window);
}

void FlatlandWindowManager::RemoveWindow(int32_t window_id,
                                         FlatlandWindow* window) {
  CHECK_EQ(window, windows_.Lookup(window_id), base::NotFatalUntil::M163);
  windows_.Remove(window_id);
}

FlatlandWindow* FlatlandWindowManager::GetWindow(int32_t window_id) {
  return windows_.Lookup(window_id);
}

}  // namespace ui
