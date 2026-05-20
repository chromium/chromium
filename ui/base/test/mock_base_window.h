// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_MOCK_BASE_WINDOW_H_
#define UI_BASE_TEST_MOCK_BASE_WINDOW_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/base_window.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_ui_types.h"

namespace ui {

class MockBaseWindow : public BaseWindow {
 public:
  MockBaseWindow();
  ~MockBaseWindow();

  MOCK_METHOD(bool, IsActive, (), (const, override));
  MOCK_METHOD(bool, IsMaximized, (), (const, override));
  MOCK_METHOD(bool, IsMinimized, (), (const, override));
  MOCK_METHOD(bool, IsFullscreen, (), (const, override));
  MOCK_METHOD(gfx::NativeWindow, GetNativeWindow, (), (const, override));
  MOCK_METHOD(gfx::Rect, GetRestoredBounds, (), (const, override));
  MOCK_METHOD(ui::mojom::WindowShowState,
              GetRestoredState,
              (),
              (const, override));
  MOCK_METHOD(gfx::Rect, GetBounds, (), (const, override));
  MOCK_METHOD(void, Show, (), (override));
  MOCK_METHOD(void, Hide, (), (override));
  MOCK_METHOD(bool, IsVisible, (), (const, override));
  MOCK_METHOD(void, ShowInactive, (), (override));
  MOCK_METHOD(void, Close, (), (override));
  MOCK_METHOD(void, Activate, (), (override));
  MOCK_METHOD(void, Deactivate, (), (override));
  MOCK_METHOD(void, Maximize, (), (override));
  MOCK_METHOD(void, Minimize, (), (override));
  MOCK_METHOD(void, Restore, (), (override));
  MOCK_METHOD(void, SetBounds, (const gfx::Rect&), (override));
  MOCK_METHOD(void, FlashFrame, (bool), (override));
  MOCK_METHOD(ui::ZOrderLevel, GetZOrderLevel, (), (const, override));
  MOCK_METHOD(void, SetZOrderLevel, (ui::ZOrderLevel), (override));
};

}  // namespace ui

#endif  // UI_BASE_TEST_MOCK_BASE_WINDOW_H_
