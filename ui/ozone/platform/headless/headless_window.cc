// Copyright 2014 The Chromium Authors. All rights reserved.
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
#if defined(OS_WIN)
  widget_ = reinterpret_cast<gfx::AcceleratedWidget>(manager_->AddWindow(this));
#else
  widget_ = manager_->AddWindow(this);
#endif
  delegate->OnAcceleratedWidgetAvailable(widget_);
}

HeadlessWindow::~HeadlessWindow() {
#if defined(OS_WIN)
  manager_->RemoveWindow(reinterpret_cast<uint64_t>(widget_), this);
#else
  manager_->RemoveWindow(widget_, this);
#endif
}

}  // namespace ui
