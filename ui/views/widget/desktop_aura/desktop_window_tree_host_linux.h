// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_LINUX_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_LINUX_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/base/buildflags.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/platform_window/extensions/x11_extension_delegate.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

namespace aura {
class ScopedWindowTargeter;
}  // namespace aura

namespace ui {
class X11Extension;
}  // namespace ui

namespace views {

class WindowEventFilterLinux;

// Contains Linux specific implementation.
class VIEWS_EXPORT DesktopWindowTreeHostLinux
    : public DesktopWindowTreeHostPlatform,
      public ui::X11ExtensionDelegate {
 public:
  DesktopWindowTreeHostLinux(
      internal::NativeWidgetDelegate* native_widget_delegate,
      DesktopNativeWidgetAura* desktop_native_widget_aura);
  ~DesktopWindowTreeHostLinux() override;

  // Get all open top-level windows. This includes windows that may not be
  // visible. This list is sorted in their stacking order, i.e. the first window
  // is the topmost window.
  static std::vector<aura::Window*> GetAllOpenWindows();

  // Runs the |func| callback for each content-window, and deallocates the
  // internal list of open windows.
  static void CleanUpWindowList(void (*func)(aura::Window* window));

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
  base::flat_map<std::string, std::string> GetKeyboardLayoutMap() override;
  void InitModalType(ui::ModalType modal_type) override;
  Widget::MoveLoopResult RunMoveLoop(
      const gfx::Vector2d& drag_offset,
      Widget::MoveLoopSource source,
      Widget::MoveLoopEscapeBehavior escape_behavior) override;

  // PlatformWindowDelegate:
  void DispatchEvent(ui::Event* event) override;
  void OnClosed() override;
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override;
  void OnActivationChanged(bool active) override;

  ui::X11Extension* GetX11Extension();
  const ui::X11Extension* GetX11Extension() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(DesktopWindowTreeHostLinuxTest, HitTest);
  FRIEND_TEST_ALL_PREFIXES(DesktopWindowTreeHostLinuxTest, MouseNCEvents);
  FRIEND_TEST_ALL_PREFIXES(DesktopWindowTreeHostLinuxHighDPITest,
                           MouseNCEvents);

  // DesktopWindowTreeHostPlatform overrides:
  void AddAdditionalInitProperties(
      const Widget::InitParams& params,
      ui::PlatformWindowInitProperties* properties) override;

  // Called back by compositor_observer_ if the latter is set.
  virtual void OnCompleteSwapWithNewSize(const gfx::Size& size);

  void CreateNonClientEventFilter();
  void DestroyNonClientEventFilter();

  // X11ExtensionDelegate overrides:
  void OnLostMouseGrab() override;
#if BUILDFLAG(USE_ATK)
  bool OnAtkKeyEvent(AtkKeyEventStruct* atk_key_event, bool transient) override;
#endif
  bool IsOverrideRedirect(bool is_tiling_wm) const override;

  // Enables event listening after closing |dialog|.
  void EnableEventListening();

  // See comment for variable open_windows_.
  static std::list<gfx::AcceleratedWidget>& open_windows();

  // A handler for events intended for non client area.
  // A posthandler for events intended for non client area. Handles events if no
  // other consumer handled them.
  std::unique_ptr<WindowEventFilterLinux> non_client_window_event_filter_;

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
