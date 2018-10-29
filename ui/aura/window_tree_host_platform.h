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
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {
enum class DomCode;
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
  explicit WindowTreeHostPlatform(ui::PlatformWindowInitProperties properties,
                                  std::unique_ptr<Window> = nullptr,
                                  const char* trace_environment_name = nullptr);
  ~WindowTreeHostPlatform() override;

  // WindowTreeHost:
  ui::EventSource* GetEventSource() override;
  gfx::AcceleratedWidget GetAcceleratedWidget() override;
  void ShowImpl() override;
  void HideImpl() override;
  gfx::Rect GetBoundsInPixels() const override;
  void SetBoundsInPixels(
      const gfx::Rect& bounds,
      const viz::LocalSurfaceId& local_surface_id,
      base::TimeTicks local_surface_id_allocation_time) override;
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

  void SetPlatformWindow(std::unique_ptr<ui::PlatformWindow> window);
  ui::PlatformWindow* platform_window() { return platform_window_.get(); }
  const ui::PlatformWindow* platform_window() const {
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
  std::unique_ptr<ui::PlatformWindow> platform_window_;
  gfx::NativeCursor current_cursor_;
  gfx::Rect bounds_;

  std::unique_ptr<ui::KeyboardHook> keyboard_hook_;

  // |pending_local_surface_id_|, |pending_local_surface_id_allocation_time_|
  // and |pending_size_| are set when the PlatformWindow instance is requested
  // to adopt a new size (in SetBoundsInPixels()). When the platform confirms
  // the new size (by way of OnBoundsChanged() callback), the LocalSurfaceId and
  // its allocation time is set on the compositor, by WindowTreeHost.
  viz::LocalSurfaceId pending_local_surface_id_;
  base::TimeTicks pending_local_surface_id_allocation_time_;
  gfx::Size pending_size_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostPlatform);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TREE_HOST_PLATFORM_H_
