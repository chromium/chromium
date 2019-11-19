// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_CAST_PLATFORM_WINDOW_CAST_H_
#define UI_OZONE_PLATFORM_CAST_PLATFORM_WINDOW_CAST_H_

#include "base/macros.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/stub/stub_window.h"

namespace ui {

class PlatformWindowCast : public StubWindow, public PlatformEventDispatcher {
 public:
  PlatformWindowCast(PlatformWindowDelegate* delegate, const gfx::Rect& bounds);
  ~PlatformWindowCast() override;

  // PlatformEventDispatcher implementation:
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PlatformWindowCast);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_CAST_PLATFORM_WINDOW_CAST_H_
