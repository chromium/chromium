// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_HEADLESS_HEADLESS_WINDOW_H_
#define UI_OZONE_PLATFORM_HEADLESS_HEADLESS_WINDOW_H_

#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/stub/stub_window.h"

namespace ui {

class HeadlessWindowManager;

class HeadlessWindow : public StubWindow {
 public:
  HeadlessWindow(PlatformWindowDelegate* delegate,
                 HeadlessWindowManager* manager,
                 const gfx::Rect& bounds);
  ~HeadlessWindow() override;

 private:
  HeadlessWindowManager* manager_;
  gfx::AcceleratedWidget widget_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessWindow);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_HEADLESS_HEADLESS_WINDOW_H_
