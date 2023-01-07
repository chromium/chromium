// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/headless/headless_window_manager.h"

namespace ui {

HeadlessWindowManager::HeadlessWindowManager() = default;

HeadlessWindowManager::~HeadlessWindowManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

int32_t HeadlessWindowManager::AddWindow(HeadlessWindow* window) {
  return windows_.Add(window);
}

void HeadlessWindowManager::RemoveWindow(int32_t window_id,
                                         HeadlessWindow* window) {
  DCHECK_EQ(window, windows_.Lookup(window_id));
  windows_.Remove(window_id);
}

HeadlessWindow* HeadlessWindowManager::GetWindow(int32_t window_id) {
  return windows_.Lookup(window_id);
}

}  // namespace ui
