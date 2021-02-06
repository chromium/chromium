// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_STUB_STUB_WINDOW_H_
#define UI_PLATFORM_WINDOW_STUB_STUB_WINDOW_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/stub/stub_window_export.h"

namespace ui {

// StubWindow is useful for tests, as well as implementations that only care
// about bounds.
class STUB_WINDOW_EXPORT StubWindow : public PlatformWindow {
 public:
  explicit StubWindow(PlatformWindowDelegate* delegate,
                      bool use_default_accelerated_widget = true,
                      const gfx::Rect& bounds = gfx::Rect());
  ~StubWindow() override;

 protected:
  PlatformWindowDelegate* delegate() { return delegate_; }

 private:
  // PlatformWindow:
  void Show(bool inactive) override;
  void Hide() override;
  void Close() override;
  bool IsVisible() const override;
  void PrepareForShutdown() override;
  void SetBounds(const gfx::Rect& bounds) override;
  gfx::Rect GetBounds() const override;
  void SetTitle(const base::string16& title) override;
  void SetCapture() override;
  void ReleaseCapture() override;
  void ToggleFullscreen() override;
  bool HasCapture() const override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  PlatformWindowState GetPlatformWindowState() const override;
  void Activate() override;
  void Deactivate() override;
  void SetUseNativeFrame(bool use_native_frame) override;
  bool ShouldUseNativeFrame() const override;
  void SetCursor(PlatformCursor cursor) override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;
  void SetRestoredBoundsInPixels(const gfx::Rect& bounds) override;
  gfx::Rect GetRestoredBoundsInPixels() const override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
  void SizeConstraintsChanged() override;

  PlatformWindowDelegate* delegate_;
  gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(StubWindow);
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_STUB_STUB_WINDOW_H_
