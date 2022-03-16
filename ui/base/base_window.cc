// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/base_window.h"

namespace ui {

namespace {
bool g_is_fullscreen_faked_for_testing = false;
}  // namespace

bool BaseWindow::IsRestored(const BaseWindow& window) {
  return !window.IsMaximized() &&
     !window.IsMinimized() &&
     !window.IsFullscreen();
}

void BaseWindow::SetFullscreenFakedForTesting(
    bool is_fullscreen_faked_for_testing) {
  g_is_fullscreen_faked_for_testing = is_fullscreen_faked_for_testing;
}

bool BaseWindow::IsFullscreenFakedForTesting() {
  return g_is_fullscreen_faked_for_testing;
}

}  // namespace ui

