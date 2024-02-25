// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_mode_observer.h"

#include "base/check.h"

namespace ui {

AXModeObserver::~AXModeObserver() {
  CHECK(!IsInObserverList());
}

}  // namespace ui
