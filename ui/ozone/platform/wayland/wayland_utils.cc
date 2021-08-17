// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_utils.h"

#include "ui/gfx/image/image_skia.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"

namespace ui {

namespace {

class WaylandScopedDisableClientSideDecorationsForTest
    : public PlatformUtils::ScopedDisableClientSideDecorationsForTest {
 public:
  WaylandScopedDisableClientSideDecorationsForTest() {
    WaylandToplevelWindow::AllowSettingDecorationInsetsForTest(false);
  }

  ~WaylandScopedDisableClientSideDecorationsForTest() override {
    WaylandToplevelWindow::AllowSettingDecorationInsetsForTest(true);
  }
};

}  // namespace

WaylandUtils::WaylandUtils() = default;

WaylandUtils::~WaylandUtils() = default;

gfx::ImageSkia WaylandUtils::GetNativeWindowIcon(intptr_t target_window_id) {
  return {};
}

std::string WaylandUtils::GetWmWindowClass(
    const std::string& desktop_base_name) {
  return desktop_base_name;
}

std::unique_ptr<PlatformUtils::ScopedDisableClientSideDecorationsForTest>
WaylandUtils::DisableClientSideDecorationsForTest() {
  return std::make_unique<WaylandScopedDisableClientSideDecorationsForTest>();
}

}  // namespace ui
