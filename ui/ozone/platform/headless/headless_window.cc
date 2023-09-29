// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/headless/headless_window.h"

#include <string>

#include "build/build_config.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/platform/headless/headless_window_manager.h"

namespace ui {

HeadlessWindow::HeadlessWindow(PlatformWindowDelegate* delegate,
                               HeadlessWindowManager* manager,
                               const gfx::Rect& bounds)
    : StubWindow(delegate, false, bounds), manager_(manager) {
  widget_ = manager_->AddWindow(this);
  delegate->OnAcceleratedWidgetAvailable(widget_);
}

HeadlessWindow::~HeadlessWindow() {
  manager_->RemoveWindow(widget_, this);
}

}  // namespace ui
