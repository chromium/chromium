// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_TEST_MOCK_PLATFORM_WINDOW_DELEGATE_H_
#define UI_OZONE_TEST_MOCK_PLATFORM_WINDOW_DELEGATE_H_

#include <optional>

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/owned_window_anchor.h"
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

  ~MockPlatformWindowDelegate() override;

  MOCK_CONST_METHOD1(CalculateInsetsInDIP,
                     gfx::Insets(PlatformWindowState window_state));
  MOCK_METHOD1(OnBoundsChanged, void(const BoundsChange& change));
  MOCK_METHOD1(OnDamageRect, void(const gfx::Rect& damaged_region));
  MOCK_METHOD1(DispatchEvent, void(Event* event));
  MOCK_METHOD0(OnCloseRequest, void());
  MOCK_METHOD0(OnClosed, void());
  MOCK_METHOD2(OnWindowStateChanged,
               void(PlatformWindowState old_state,
                    PlatformWindowState new_state));
#if BUILDFLAG(IS_LINUX)
  MOCK_METHOD1(OnWindowTiledStateChanged,
               void(WindowTiledEdges new_tiled_edges));
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  MOCK_METHOD2(OnFullscreenTypeChanged,
               void(PlatformFullscreenType old_type,
                    PlatformFullscreenType new_type));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  MOCK_METHOD0(OnLostCapture, void());
  MOCK_METHOD1(OnAcceleratedWidgetAvailable,
               void(gfx::AcceleratedWidget widget));
  MOCK_METHOD0(OnWillDestroyAcceleratedWidget, void());
  MOCK_METHOD0(OnAcceleratedWidgetDestroyed, void());
  MOCK_METHOD1(OnActivationChanged, void(bool active));
  MOCK_CONST_METHOD0(GetMinimumSizeForWindow, std::optional<gfx::Size>());
  MOCK_CONST_METHOD0(GetMaximumSizeForWindow, std::optional<gfx::Size>());
  MOCK_METHOD0(GetOwnedWindowAnchorAndRectInDIP,
               std::optional<OwnedWindowAnchor>());
  MOCK_METHOD0(OnMouseEnter, void());
  MOCK_METHOD1(OnOcclusionStateChanged,
               void(PlatformWindowOcclusionState occlusion_state));
  MOCK_METHOD2(OnStateUpdate, int64_t(const State& old, const State& latest));
  MOCK_METHOD1(OnOverviewModeChanged, void(bool overview));
  MOCK_METHOD2(OnRotateFocus,
               bool(PlatformWindowDelegate::RotateDirection, bool));
  MOCK_CONST_METHOD0(CanMaximize, bool());
  MOCK_CONST_METHOD0(CanFullscreen, bool());
};

bool operator==(const PlatformWindowDelegate::BoundsChange& a,
                const PlatformWindowDelegate::BoundsChange& b);

}  // namespace ui

#endif  // UI_OZONE_TEST_MOCK_PLATFORM_WINDOW_DELEGATE_H_
