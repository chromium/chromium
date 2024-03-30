// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_STUB_STUB_WINDOW_H_
#define UI_PLATFORM_WINDOW_STUB_STUB_WINDOW_H_

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/stub/stub_window_export.h"

namespace ui {

// StubWindow is useful for tests, as well as implementations that only care
// about bounds and activation state.
class STUB_WINDOW_EXPORT StubWindow : public PlatformWindow {
 public:
  explicit StubWindow(PlatformWindowDelegate* delegate,
                      bool use_default_accelerated_widget = true,
                      const gfx::Rect& bounds = gfx::Rect());
  explicit StubWindow(const gfx::Rect& bounds);
  StubWindow(const StubWindow&) = delete;
  StubWindow& operator=(const StubWindow&) = delete;

  ~StubWindow() override;

  void InitDelegate(PlatformWindowDelegate* delegate,
                    bool use_default_accelerated_widget = true);
  void InitDelegateWithWidget(PlatformWindowDelegate* delegate,
                              gfx::AcceleratedWidget widget);

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

  raw_ptr<PlatformWindowDelegate> delegate_ = nullptr;
  gfx::Rect bounds_;
  ui::PlatformWindowState window_state_ = ui::PlatformWindowState::kUnknown;
  ActivationState activation_state_ = ActivationState::kUnknown;
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_STUB_STUB_WINDOW_H_
