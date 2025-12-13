// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/display_observer.h"

#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"

namespace display {

DisplayObserver::~DisplayObserver() {}

ScopedOptionalDisplayObserver::ScopedOptionalDisplayObserver(
    DisplayObserver* observer) {
  if (auto* screen = display::Screen::Get()) {
    observer_ = observer;
    screen->AddObserver(observer_);
  }
}

ScopedOptionalDisplayObserver::~ScopedOptionalDisplayObserver() {
  if (!observer_)
    return;
  if (auto* screen = display::Screen::Get()) {
    screen->RemoveObserver(observer_);
  }
}

ScopedDisplayObserver::ScopedDisplayObserver(DisplayObserver* observer)
    : ScopedOptionalDisplayObserver(observer) {
  CHECK(Screen::Get());
}

}  // namespace display
