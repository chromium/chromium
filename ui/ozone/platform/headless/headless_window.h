// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_HEADLESS_HEADLESS_WINDOW_H_
#define UI_OZONE_PLATFORM_HEADLESS_HEADLESS_WINDOW_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

class HeadlessWindowManager;

class HeadlessWindow : public PlatformWindow {
 public:
  explicit HeadlessWindow(PlatformWindowDelegate* delegate,
                          HeadlessWindowManager* manager,
                          const gfx::Rect& bounds);

  HeadlessWindow(const HeadlessWindow&) = delete;
  HeadlessWindow& operator=(const HeadlessWindow&) = delete;

  ~HeadlessWindow() override;

 protected:
  PlatformWindowDelegate* delegate() { return delegate_; }

 private:
  enum class ActivationState {
    kUnknown,
    kActive,
    kInactive,
  };

  // PlatformWindow:
  void Show(bool inactive) override;
  void Hide() override;
  void Close() override;
  bool IsVisible() const override;
  void PrepareForShutdown() override;
  void SetBoundsInPixels(const gfx::Rect& bounds) override;
  gfx::Rect GetBoundsInPixels() const override;
  void SetBoundsInDIP(const gfx::Rect& bounds) override;
  gfx::Rect GetBoundsInDIP() const override;
  void SetTitle(const std::u16string& title) override;
  void SetCapture() override;
  void ReleaseCapture() override;
  void SetFullscreen(bool fullscreen, int64_t target_display_id) override;
  bool HasCapture() const override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  PlatformWindowState GetPlatformWindowState() const override;
  void Activate() override;
  void Deactivate() override;
  void SetUseNativeFrame(bool use_native_frame) override;
  bool ShouldUseNativeFrame() const override;
  void SetCursor(scoped_refptr<PlatformCursor> cursor) override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;
  void SetRestoredBoundsInDIP(const gfx::Rect& bounds) override;
  gfx::Rect GetRestoredBoundsInDIP() const override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
  void SizeConstraintsChanged() override;

  void ZoomWindowBounds();
  void RestoreWindowBounds();
  void UpdateBounds(const gfx::Rect& bounds);
  void UpdateWindowState(PlatformWindowState new_window_state);

  raw_ptr<PlatformWindowDelegate> delegate_ = nullptr;
  raw_ptr<HeadlessWindowManager> manager_;
  gfx::Rect bounds_;

  gfx::AcceleratedWidget widget_;

  bool visible_ = false;
  std::optional<gfx::Rect> restored_bounds_;
  PlatformWindowState window_state_ = PlatformWindowState::kUnknown;
  ActivationState activation_state_ = ActivationState::kUnknown;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_HEADLESS_HEADLESS_WINDOW_H_
