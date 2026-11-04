// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/headless/headless_window_manager.h"

#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/headless/headless_window.h"

namespace ui {

HeadlessWindowManager::HeadlessWindowManager() = default;

HeadlessWindowManager::~HeadlessWindowManager() {
  CHECK(thread_checker_.CalledOnValidThread(), base::NotFatalUntil::M163);
}

gfx::AcceleratedWidget HeadlessWindowManager::AddWindow(
    HeadlessWindow* window) {
  return windows_.Add(window);
}

void HeadlessWindowManager::RemoveWindow(gfx::AcceleratedWidget widget,
                                         HeadlessWindow* window) {
  CHECK_EQ(window, windows_.Lookup(widget), base::NotFatalUntil::M163);
  windows_.Remove(widget);
}

HeadlessWindow* HeadlessWindowManager::GetWindow(
    gfx::AcceleratedWidget widget) {
  return windows_.Lookup(widget);
}

gfx::AcceleratedWidget HeadlessWindowManager::GetAcceleratedWidgetAtScreenPoint(
    const gfx::Point& point) {
  for (base::IDMap<HeadlessWindow*>::const_iterator it(&windows_);
       !it.IsAtEnd(); it.Advance()) {
    const HeadlessWindow* window = it.GetCurrentValue();
    gfx::Rect bounds = window->GetBoundsInPixels();
    if (bounds.Contains(point)) {
      return window->widget();
    }
  }

  return gfx::kNullAcceleratedWidget;
}

}  // namespace ui
