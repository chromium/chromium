// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_observer.h"

#include "base/check.h"

namespace aura {

WindowObserver::WindowObserver() = default;

WindowObserver::~WindowObserver() {
  CHECK(!IsInObserverList());
}

}  // namespace aura
