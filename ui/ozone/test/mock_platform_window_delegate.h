// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_TEST_MOCK_PLATFORM_WINDOW_DELEGATE_H_
#define UI_OZONE_TEST_MOCK_PLATFORM_WINDOW_DELEGATE_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

class MockPlatformWindowDelegate : public PlatformWindowDelegate {
 public:
  MockPlatformWindowDelegate();

  MockPlatformWindowDelegate(const MockPlatformWindowDelegate&) = delete;
  MockPlatformWindowDelegate& operator=(const MockPlatformWindowDelegate&) =
      delete;

  ~MockPlatformWindowDelegate();

  MOCK_METHOD1(OnBoundsChanged, void(const BoundsChange& change));
  MOCK_METHOD1(OnDamageRect, void(const gfx::Rect& damaged_region));
  MOCK_METHOD1(DispatchEvent, void(Event* event));
  MOCK_METHOD0(OnCloseRequest, void());
  MOCK_METHOD0(OnClosed, void());
  MOCK_METHOD2(OnWindowStateChanged,
               void(PlatformWindowState old_state,
                    PlatformWindowState new_state));
  MOCK_METHOD0(OnLostCapture, void());
  MOCK_METHOD1(OnAcceleratedWidgetAvailable,
               void(gfx::AcceleratedWidget widget));
  MOCK_METHOD0(OnWillDestroyAcceleratedWidget, void());
  MOCK_METHOD0(OnAcceleratedWidgetDestroyed, void());
  MOCK_METHOD1(OnActivationChanged, void(bool active));
  MOCK_METHOD0(GetMinimumSizeForWindow, absl::optional<gfx::Size>());
  MOCK_METHOD0(GetMaximumSizeForWindow, absl::optional<gfx::Size>());
  MOCK_METHOD0(GetMenuType, absl::optional<MenuType>());
  MOCK_METHOD0(GetOwnedWindowAnchorAndRectInDIP,
               absl::optional<OwnedWindowAnchor>());
  MOCK_METHOD0(OnMouseEnter, void());
};

bool operator==(const PlatformWindowDelegate::BoundsChange& a,
                const PlatformWindowDelegate::BoundsChange& b);

}  // namespace ui

#endif  // UI_OZONE_TEST_MOCK_PLATFORM_WINDOW_DELEGATE_H_
