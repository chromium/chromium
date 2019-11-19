// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TREE_HOST_PLATFORM_H_
#define UI_AURA_WINDOW_TREE_HOST_PLATFORM_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {
enum class DomCode;
class PlatformWindowBase;
class KeyboardHook;
struct PlatformWindowInitProperties;
}  // namespace ui

namespace aura {

// The unified WindowTreeHost implementation for platforms
// that implement PlatformWindow.
class AURA_EXPORT WindowTreeHostPlatform : public WindowTreeHost,
                                           public ui::PlatformWindowDelegate {
 public:
  // See Compositor() for details on |trace_environment_name|.
  explicit WindowTreeHostPlatform(
      ui::PlatformWindowInitProperties properties,
      std::unique_ptr<Window> = nullptr,
      const char* trace_environment_name = nullptr,
      bool use_external_begin_frame_control = false);
  ~WindowTreeHostPlatform() override;

  // WindowTreeHost:
  ui::EventSource* GetEventSource() override;
  gfx::AcceleratedWidget GetAcceleratedWidget() override;
  void ShowImpl() override;
  void HideImpl() override;
  gfx::Rect GetBoundsInPixels() const override;
  void SetBoundsInPixels(const gfx::Rect& bounds) override;
  gfx::Point GetLocationOnScreenInPixels() const override;
  void SetCapture() override;
  void ReleaseCapture() override;
  void SetCursorNative(gfx::NativeCursor cursor) override;
  void MoveCursorToScreenLocationInPixels(
      const gfx::Point& location_in_pixels) override;
  void OnCursorVisibilityChangedNative(bool show) override;

 protected:
  // NOTE: this does not call CreateCompositor(); subclasses must call
  // CreateCompositor() at the appropriate time.
  explicit WindowTreeHostPlatform(std::unique_ptr<Window> window = nullptr);

  // Creates a ui::PlatformWindow appropriate for the current platform and
  // installs it at as the PlatformWindow for this WindowTreeHostPlatform.
  void CreateAndSetPlatformWindow(ui::PlatformWindowInitProperties properties);

  void SetPlatformWindow(std::unique_ptr<ui::PlatformWindowBase> window);
  ui::PlatformWindowBase* platform_window() { return platform_window_.get(); }
  const ui::PlatformWindowBase* platform_window() const {
    return platform_window_.get();
  }

  // ui::PlatformWindowDelegate:
  void OnBoundsChanged(const gfx::Rect& new_bounds) override;
  void OnDamageRect(const gfx::Rect& damaged_region) override;
  void DispatchEvent(ui::Event* event) override;
  void OnCloseRequest() override;
  void OnClosed() override;
  void OnWindowStateChanged(ui::PlatformWindowState new_state) override;
  void OnLostCapture() override;
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override;
  void OnAcceleratedWidgetDestroyed() override;
  void OnActivationChanged(bool active) override;
  void OnMouseEnter() override;

  // Overridden from aura::WindowTreeHost:
  bool CaptureSystemKeyEventsImpl(
      base::Optional<base::flat_set<ui::DomCode>> dom_codes) override;
  void ReleaseSystemKeyEventCapture() override;
  bool IsKeyLocked(ui::DomCode dom_code) override;
  base::flat_map<std::string, std::string> GetKeyboardLayoutMap() override;

  // This function is only for test purpose.
  gfx::NativeCursor* GetCursorNative() { return &current_cursor_; }

 private:
  gfx::AcceleratedWidget widget_;
  std::unique_ptr<ui::PlatformWindowBase> platform_window_;
  gfx::NativeCursor current_cursor_;
  gfx::Rect bounds_in_pixels_;

  std::unique_ptr<ui::KeyboardHook> keyboard_hook_;

  gfx::Size pending_size_;

  // Tracks how nested OnBoundsChanged() is. That is, on entering
  // OnBoundsChanged() this is incremented and on leaving OnBoundsChanged() this
  // is decremented.
  int on_bounds_changed_recursion_depth_ = 0;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostPlatform);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TREE_HOST_PLATFORM_H_
