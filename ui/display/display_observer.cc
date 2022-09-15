// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/display_observer.h"

#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"

namespace display {

DisplayObserver::~DisplayObserver() {}

void DisplayObserver::OnWillProcessDisplayChanges() {}

void DisplayObserver::OnDidProcessDisplayChanges() {}

void DisplayObserver::OnDisplayAdded(const Display& new_display) {}

void DisplayObserver::OnDisplayRemoved(const Display& old_display) {}

void DisplayObserver::OnDidRemoveDisplays() {}

void DisplayObserver::OnDisplayMetricsChanged(const Display& display,
                                              uint32_t changed_metrics) {}

void DisplayObserver::OnCurrentWorkspaceChanged(
    const std::string& new_workspace) {}

void DisplayObserver::OnDisplayTabletStateChanged(TabletState state) {}

ScopedOptionalDisplayObserver::ScopedOptionalDisplayObserver(
    DisplayObserver* observer) {
  if (auto* screen = display::Screen::GetScreen()) {
    observer_ = observer;
    screen->AddObserver(observer_);
  }
}

ScopedOptionalDisplayObserver::~ScopedOptionalDisplayObserver() {
  if (!observer_)
    return;
  if (auto* screen = display::Screen::GetScreen())
    screen->RemoveObserver(observer_);
}

ScopedDisplayObserver::ScopedDisplayObserver(DisplayObserver* observer)
    : ScopedOptionalDisplayObserver(observer) {
  CHECK(Screen::GetScreen());
}

}  // namespace display
