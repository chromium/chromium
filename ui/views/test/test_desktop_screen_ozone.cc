// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/test_desktop_screen_ozone.h"

namespace views::test {
namespace {
TestDesktopScreenOzone* g_instance = nullptr;
}

// static
std::unique_ptr<display::Screen> TestDesktopScreenOzone::Create() {
  return std::make_unique<TestDesktopScreenOzone>();
}

TestDesktopScreenOzone* TestDesktopScreenOzone::GetInstance() {
  DCHECK_EQ(display::Screen::GetScreen(), g_instance);
  return g_instance;
}

gfx::Point TestDesktopScreenOzone::GetCursorScreenPoint() {
  return cursor_screen_point_;
}

TestDesktopScreenOzone::TestDesktopScreenOzone() {
  DCHECK(!g_instance);
  g_instance = this;
}

TestDesktopScreenOzone::~TestDesktopScreenOzone() {
  g_instance = nullptr;
}

}  // namespace views::test
