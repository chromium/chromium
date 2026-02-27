// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/headless/headless_screen_manager.h"

#include <algorithm>

#include "base/notimplemented.h"
#include "ui/display/types/display_constants.h"

namespace display {

// static
HeadlessScreenManager* HeadlessScreenManager::Get() {
  static base::NoDestructor<HeadlessScreenManager> headless_screen_manager;
  return headless_screen_manager.get();
}

// static
int64_t HeadlessScreenManager::GetNewDisplayId() {
  static int64_t headless_display_id = 1;
  return headless_display_id++;
}

void HeadlessScreenManager::SetDelegate(Delegate* delegate,
                                        const base::Location& location) {
  CHECK(!delegate_ || delegate == nullptr)
      << "Delegate is already set by " << location_.ToString();

  delegate_ = delegate;
  location_ = location;
}

int64_t HeadlessScreenManager::AddDisplay(const Display& display) {
  if (!delegate_) {
    NOTIMPLEMENTED();
    return kInvalidDisplayId;
  }

  return delegate_->AddDisplay(display);
}

void HeadlessScreenManager::RemoveDisplay(int64_t display_id) {
  if (!delegate_) {
    NOTIMPLEMENTED();
    return;
  }

  delegate_->RemoveDisplay(display_id);
}

void HeadlessScreenManager::SetPrimaryDisplay(int64_t display_id) {
  if (!delegate_) {
    NOTIMPLEMENTED();
    return;
  }

  delegate_->SetPrimaryDisplay(display_id);
}

}  // namespace display
