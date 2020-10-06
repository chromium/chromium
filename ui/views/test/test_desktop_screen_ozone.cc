// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/test_desktop_screen_ozone.h"

#include <memory>

#include "base/memory/singleton.h"

namespace views {
namespace test {

TestDesktopScreenOzone* TestDesktopScreenOzone::GetInstance() {
  return base::Singleton<TestDesktopScreenOzone>::get();
}

gfx::Point TestDesktopScreenOzone::GetCursorScreenPoint() {
  return cursor_screen_point_;
}

TestDesktopScreenOzone::TestDesktopScreenOzone() = default;
TestDesktopScreenOzone::~TestDesktopScreenOzone() = default;

}  // namespace test
}  // namespace views
