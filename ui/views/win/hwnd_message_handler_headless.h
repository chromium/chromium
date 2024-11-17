// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIN_HWND_MESSAGE_HANDLER_HEADLESS_H_
#define UI_VIEWS_WIN_HWND_MESSAGE_HANDLER_HEADLESS_H_

#include <windows.h>

#include <stddef.h>

#include <optional>
#include <string>

#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/views_export.h"
#include "ui/views/win/hwnd_message_handler.h"

namespace views {

class HWNDMessageHandlerDelegate;

// This class is instantiated instead of the original HWNDMessageHandler when
// Chrome is running in headless mode. It overrides certain methods ensuring
// that headless mode platform windows exist but are never shown or change their
// state and that the upper layers observe headless windows the same way as they
// would have observed the visible platform windows.
class VIEWS_EXPORT HWNDMessageHandlerHeadless : public HWNDMessageHandler {
 public:
  HWNDMessageHandlerHeadless(const HWNDMessageHandlerHeadless&) = delete;
  HWNDMessageHandlerHeadless& operator=(const HWNDMessageHandlerHeadless&) =
      delete;

  ~HWNDMessageHandlerHeadless() override;

  void Init(HWND parent, const gfx::Rect& bounds) override;

  gfx::Rect GetWindowBoundsInScreen() const override;
  gfx::Rect GetClientAreaBoundsInScreen() const override;
  gfx::Rect GetRestoredBounds() const override;

  void GetWindowPlacement(
      gfx::Rect* bounds,
      ui::mojom::WindowShowState* show_state) const override;

  void SetSize(const gfx::Size& size) override;
  void CenterWindow(const gfx::Size& size) override;

  void SetRegion(HRGN rgn) override;

  void StackAbove(HWND other_hwnd) override;
  void StackAtTop() override;

  void Show(ui::mojom::WindowShowState show_state,
            const gfx::Rect& pixel_restore_bounds) override;
  void Hide() override;

  void Maximize() override;
  void Minimize() override;
  void Restore() override;

  void Activate() override;
  void Deactivate() override;

  void SetAlwaysOnTop(bool on_top) override;

  bool IsVisible() const override;
  bool IsActive() const override;
  bool IsMinimized() const override;
  bool IsMaximized() const override;
  bool IsFullscreen() const override;
  bool IsAlwaysOnTop() const override;
  bool IsHeadless() const override;

  void FlashFrame(bool flash) override;

  void ClearNativeFocus() override;

  void SetCapture() override;
  void ReleaseCapture() override;
  bool HasCapture() const override;

  FullscreenHandler* fullscreen_handler() override;

  void SetFullscreen(bool fullscreen, int64_t target_display_id) override;

  void SizeConstraintsChanged() override;

 protected:
  friend class HWNDMessageHandler;

  HWNDMessageHandlerHeadless(HWNDMessageHandlerDelegate* delegate,
                             const std::string& debugging_id);

  void SetBoundsInternal(const gfx::Rect& bounds_in_pixels,
                         bool force_size_changed) override;

  void RestoreBounds();

 private:
  // Sets headless window bounds which may be different from the platform window
  // bounds and updates Aura window property that stores headless window bounds
  // for the upper layers to retrieve.
  void SetHeadlessWindowBounds(const gfx::Rect& bounds);

  bool is_visible_ = false;
  bool is_active_ = false;
  bool is_always_on_top_ = false;
  bool was_active_before_minimize_ = false;

  enum class WindowState {
    kNormal,
    kMinimized,
    kMaximized,
    kFullscreen,
  } window_state_ = WindowState::kNormal;

  gfx::Rect bounds_;
  std::optional<gfx::Rect> restored_bounds_;
};

}  // namespace views

#endif  // UI_VIEWS_WIN_HWND_MESSAGE_HANDLER_HEADLESS_H_
