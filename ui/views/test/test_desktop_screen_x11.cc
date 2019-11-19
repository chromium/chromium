// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/test_desktop_screen_x11.h"

#include <memory>

#include "base/memory/singleton.h"

namespace views {
namespace test {

TestDesktopScreenX11* TestDesktopScreenX11::GetInstance() {
  return base::Singleton<TestDesktopScreenX11>::get();
}

gfx::Point TestDesktopScreenX11::GetCursorScreenPoint() {
  return cursor_screen_point_;
}

TestDesktopScreenX11::TestDesktopScreenX11() {
  DesktopScreenX11::Init();
}

TestDesktopScreenX11::~TestDesktopScreenX11() = default;

}  // namespace test
}  // namespace views
