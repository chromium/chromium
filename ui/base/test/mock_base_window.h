// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_MOCK_BASE_WINDOW_H_
#define UI_BASE_TEST_MOCK_BASE_WINDOW_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/base_window.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
namespace test {

class MockBaseWindow : public BaseWindow {
 public:
  MockBaseWindow();
  ~MockBaseWindow();
  MockBaseWindow(const MockBaseWindow&) = delete;
  MockBaseWindow(MockBaseWindow&&) = delete;
  MockBaseWindow& operator=(const MockBaseWindow&) = delete;
  MockBaseWindow& operator=(MockBaseWindow&&) = delete;

  MOCK_METHOD(bool, IsActive, (), (const));
  MOCK_METHOD(bool, IsMaximized, (), (const));
  MOCK_METHOD(bool, IsMinimized, (), (const));
  MOCK_METHOD(bool, IsFullscreen, (), (const));
  MOCK_METHOD(gfx::NativeWindow, GetNativeWindow, (), (const));
  MOCK_METHOD(gfx::Rect, GetRestoredBounds, (), (const));
  MOCK_METHOD(ui::mojom::WindowShowState, GetRestoredState, (), (const));
  MOCK_METHOD(gfx::Rect, GetBounds, (), (const));
  MOCK_METHOD(void, Show, (), ());
  MOCK_METHOD(void, Hide, (), ());
  MOCK_METHOD(bool, IsVisible, (), (const));
  MOCK_METHOD(void, ShowInactive, (), ());
  MOCK_METHOD(void, Close, (), ());
  MOCK_METHOD(void, Activate, (), ());
  MOCK_METHOD(void, Deactivate, (), ());
  MOCK_METHOD(void, Maximize, (), ());
  MOCK_METHOD(void, Minimize, (), ());
  MOCK_METHOD(void, Restore, (), ());
  MOCK_METHOD(void, SetBounds, (const gfx::Rect& bounds), ());
  MOCK_METHOD(void, FlashFrame, (bool flash), ());
  MOCK_METHOD(ZOrderLevel, GetZOrderLevel, (), (const));
  MOCK_METHOD(void, SetZOrderLevel, (ZOrderLevel order), ());
};

}  // namespace test
}  // namespace ui

#endif  // UI_BASE_TEST_MOCK_BASE_WINDOW_H_
