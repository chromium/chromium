// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_LINUX_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_LINUX_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/base/buildflags.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

class SkPath;

namespace aura {
class ScopedWindowTargeter;
}  // namespace aura

namespace ui {
class PlatformWindowLinux;
}  // namespace ui

namespace views {

class WindowEventFilterLinux;

// Contains Linux specific implementation.
class VIEWS_EXPORT DesktopWindowTreeHostLinux
    : public DesktopWindowTreeHostPlatform {
 public:
  DesktopWindowTreeHostLinux(
      internal::NativeWidgetDelegate* native_widget_delegate,
      DesktopNativeWidgetAura* desktop_native_widget_aura);
  ~DesktopWindowTreeHostLinux() override;

  // A way of converting a |widget| into the content_window()
  // of the associated DesktopNativeWidgetAura.
  static aura::Window* GetContentWindowForWidget(gfx::AcceleratedWidget widget);

  // A way of converting a |widget| into this object.
  static DesktopWindowTreeHostLinux* GetHostForWidget(
      gfx::AcceleratedWidget widget);

  // Get all open top-level windows. This includes windows that may not be
  // visible. This list is sorted in their stacking order, i.e. the first window
  // is the topmost window.
  static std::vector<aura::Window*> GetAllOpenWindows();

  // Runs the |func| callback for each content-window, and deallocates the
  // internal list of open windows.
  static void CleanUpWindowList(void (*func)(aura::Window* window));

  // This must be called before the window is created, because the visual cannot
  // be changed after. Useful for X11. Not in use for Wayland.
  void SetPendingXVisualId(int x_visual_id);

  // Returns the current bounds in terms of the X11 Root Window including the
  // borders provided by the window manager (if any). Not in use for Wayland.
  gfx::Rect GetXRootWindowOuterBounds() const;

  // Tells if the point is within X11 Root Window's region. Not in use for
  // Wayland.
  bool ContainsPointInXRegion(const gfx::Point& point) const;

  // Tells the X server to lower the |platform_window()| owned by this host down
  // the stack so that it does not obscure any sibling windows. Not in use for
  // Wayland.
  void LowerXWindow();

  // Disables event listening to make |dialog| modal.
  base::OnceClosure DisableEventListening();

 protected:
  // Overridden from DesktopWindowTreeHost:
  void Init(const Widget::InitParams& params) override;
  void OnNativeWidgetCreated(const Widget::InitParams& params) override;
  std::string GetWorkspace() const override;
  void SetVisibleOnAllWorkspaces(bool always_visible) override;
  bool IsVisibleOnAllWorkspaces() const override;
  void SetOpacity(float opacity) override;
  base::flat_map<std::string, std::string> GetKeyboardLayoutMap() override;
  void InitModalType(ui::ModalType modal_type) override;

  // PlatformWindowDelegateBase:
  void DispatchEvent(ui::Event* event) override;
  void OnClosed() override;
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override;
  void OnActivationChanged(bool active) override;
#if BUILDFLAG(USE_ATK)
  bool OnAtkKeyEvent(AtkKeyEventStruct* atk_key_event) override;
#endif

 private:
  FRIEND_TEST_ALL_PREFIXES(DesktopWindowTreeHostLinuxTest, HitTest);

  // Overridden from display::DisplayObserver via aura::WindowTreeHost:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // DesktopWindowTreeHostPlatform overrides:
  void AddAdditionalInitProperties(
      const Widget::InitParams& params,
      ui::PlatformWindowInitProperties* properties) override;

  // Called back by compositor_observer_ if the latter is set.
  virtual void OnCompleteSwapWithNewSize(const gfx::Size& size);

  void CreateNonClientEventFilter();
  void DestroyNonClientEventFilter();

  // PlatformWindowDelegateLinux overrides:
  void OnWorkspaceChanged() override;
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override;
  void OnLostMouseGrab() override;

  // Enables event listening after closing |dialog|.
  void EnableEventListening();

  const ui::PlatformWindowLinux* GetPlatformWindowLinux() const;
  ui::PlatformWindowLinux* GetPlatformWindowLinux();

  // See comment for variable open_windows_.
  static std::list<gfx::AcceleratedWidget>& open_windows();

  // A handler for events intended for non client area.
  // A posthandler for events intended for non client area. Handles events if no
  // other consumer handled them.
  std::unique_ptr<WindowEventFilterLinux> non_client_window_event_filter_;

  // X11 may set set a visual id for the system tray icon before the host is
  // initialized. This value will be passed down to PlatformWindow during
  // initialization of the host.
  base::Optional<int> pending_x_visual_id_;

  std::unique_ptr<CompositorObserver> compositor_observer_;

  std::unique_ptr<aura::ScopedWindowTargeter> targeter_for_modal_;

  uint32_t modal_dialog_counter_ = 0;

  // A list of all (top-level) windows that have been created but not yet
  // destroyed.
  static std::list<gfx::AcceleratedWidget>* open_windows_;

  // The display and the native X window hosting the root window.
  base::WeakPtrFactory<DesktopWindowTreeHostLinux> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostLinux);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_LINUX_H_
