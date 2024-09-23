// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TREE_HOST_PLATFORM_H_
#define UI_AURA_WINDOW_TREE_HOST_PLATFORM_H_

#include <memory>

#include "build/chromeos_buildflags.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {
enum class DomCode : uint32_t;
class PlatformWindow;
class KeyboardHook;
struct PlatformWindowInitProperties;
}  // namespace ui

namespace aura {

// The unified WindowTreeHost implementation for platforms
// that implement PlatformWindow.
class AURA_EXPORT WindowTreeHostPlatform : public WindowTreeHost,
                                           public ui::PlatformWindowDelegate {
 public:
  explicit WindowTreeHostPlatform(ui::PlatformWindowInitProperties properties,
                                  std::unique_ptr<Window> = nullptr);

  WindowTreeHostPlatform(const WindowTreeHostPlatform&) = delete;
  WindowTreeHostPlatform& operator=(const WindowTreeHostPlatform&) = delete;

  ~WindowTreeHostPlatform() override;

  static WindowTreeHostPlatform* GetHostForWindow(aura::Window* window);

  // WindowTreeHost:
  ui::EventSource* GetEventSource() override;
  gfx::AcceleratedWidget GetAcceleratedWidget() override;
  void ShowImpl() override;
  void HideImpl() override;
  gfx::Rect GetBoundsInPixels() const override;
  void SetBoundsInPixels(const gfx::Rect& bounds) override;
  void SetCapture() override;
  void ReleaseCapture() override;
  void SetCursorNative(gfx::NativeCursor cursor) override;
  void MoveCursorToScreenLocationInPixels(
      const gfx::Point& location_in_pixels) override;
  void OnCursorVisibilityChangedNative(bool show) override;
  void LockMouse(Window* window) override;

  ui::PlatformWindow* platform_window() { return platform_window_.get(); }
  const ui::PlatformWindow* platform_window() const {
    return platform_window_.get();
  }

  // Returns `PlatformWindow` for the platform. If
  // `PlatformWindowFactoryDelegateForTesting` is set, it uses the delegate.
  std::unique_ptr<ui::PlatformWindow> CreatePlatformWindow(
      ui::PlatformWindowInitProperties properties);

  class PlatformWindowFactoryDelegateForTesting {
   public:
    virtual ~PlatformWindowFactoryDelegateForTesting() = default;
    virtual std::unique_ptr<ui::PlatformWindow> Create(
        WindowTreeHostPlatform*) = 0;
  };
  static void SetPlatformWindowFactoryDelegateForTesting(
      PlatformWindowFactoryDelegateForTesting* delegate);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::string GetUniqueId() const override;
#endif

 protected:
  // NOTE: this does not call CreateCompositor(); subclasses must call
  // CreateCompositor() at the appropriate time.
  explicit WindowTreeHostPlatform(std::unique_ptr<Window> window = nullptr);

  // Creates a ui::PlatformWindow appropriate for the current platform and
  // installs it at as the PlatformWindow for this WindowTreeHostPlatform.
  void CreateAndSetPlatformWindow(ui::PlatformWindowInitProperties properties);

  void SetPlatformWindow(std::unique_ptr<ui::PlatformWindow> window);

  // ui::PlatformWindowDelegate:
  void OnBoundsChanged(const BoundsChange& change) override;
  void OnDamageRect(const gfx::Rect& damaged_region) override;
  void DispatchEvent(ui::Event* event) override;
  void OnCloseRequest() override;
  void OnClosed() override;
  void OnWindowStateChanged(ui::PlatformWindowState old_state,
                            ui::PlatformWindowState new_state) override;
  void OnLostCapture() override;
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override;
  void OnWillDestroyAcceleratedWidget() override;
  void OnAcceleratedWidgetDestroyed() override;
  void OnActivationChanged(bool active) override;
  void OnMouseEnter() override;
  void OnOcclusionStateChanged(
      ui::PlatformWindowOcclusionState occlusion_state) override;
  int64_t OnStateUpdate(const PlatformWindowDelegate::State& old,
                        const PlatformWindowDelegate::State& latest) override;
  void SetFrameRateThrottleEnabled(bool enabled) override;
  void DisableNativeWindowOcclusion() override;

  // Overridden from aura::WindowTreeHost:
  gfx::Point GetLocationOnScreenInPixels() const override;
  bool CaptureSystemKeyEventsImpl(
      std::optional<base::flat_set<ui::DomCode>> dom_codes) override;
  void ReleaseSystemKeyEventCapture() override;
  bool IsKeyLocked(ui::DomCode dom_code) override;
  base::flat_map<std::string, std::string> GetKeyboardLayoutMap() override;

  void OnVideoCaptureLockCreated() override;
  void OnVideoCaptureLockDestroyed() override;

 private:
  gfx::AcceleratedWidget widget_;
  std::unique_ptr<ui::PlatformWindow> platform_window_;
  gfx::NativeCursor current_cursor_;
  // TODO: use compositor's size.
  gfx::Size size_in_pixels_;

  std::unique_ptr<ui::KeyboardHook> keyboard_hook_;

  // Prop to hold mapping to and `WindowTreeHostPlatform`. Used by
  // `GetHostForWindow`.
  std::unique_ptr<ui::ViewProp> prop_;

  // Tracks how nested OnBoundsChanged() is. That is, on entering
  // OnBoundsChanged() this is incremented and on leaving OnBoundsChanged() this
  // is decremented.
  int on_bounds_changed_recursion_depth_ = 0;
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TREE_HOST_PLATFORM_H_
