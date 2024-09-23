// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_LINUX_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_LINUX_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/base/buildflags.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
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
using WindowEventFilterClass = WindowEventFilterLinux;

// Contains Linux specific implementation, which supports both X11 and Wayland
// backend.
class VIEWS_EXPORT DesktopWindowTreeHostLinux
    : public DesktopWindowTreeHostPlatform,
      public ui::X11ExtensionDelegate {
 public:
  static const char kWindowKey[];

  DesktopWindowTreeHostLinux(
      internal::NativeWidgetDelegate* native_widget_delegate,
      DesktopNativeWidgetAura* desktop_native_widget_aura);

  DesktopWindowTreeHostLinux(const DesktopWindowTreeHostLinux&) = delete;
  DesktopWindowTreeHostLinux& operator=(const DesktopWindowTreeHostLinux&) =
      delete;

  ~DesktopWindowTreeHostLinux() override;

  // Returns the current bounds in terms of the X11 Root Window including the
  // borders provided by the window manager (if any). Not in use for Wayland.
  gfx::Rect GetXRootWindowOuterBounds() const;

  // DesktopWindowTreeHostPlatform:
  void LowerWindow() override;
  // Disables event listening to make |dialog| modal.
  base::OnceClosure DisableEventListening();

  // Sets hints for the WM/compositor that reflect the extents of the
  // client-drawn shadow.
  virtual void UpdateFrameHints();

 protected:
  // Overridden from DesktopWindowTreeHost:
  void Init(const Widget::InitParams& params) override;
  void OnNativeWidgetCreated(const Widget::InitParams& params) override;
  void InitModalType(ui::mojom::ModalType modal_type) override;
  Widget::MoveLoopResult RunMoveLoop(
      const gfx::Vector2d& drag_offset,
      Widget::MoveLoopSource source,
      Widget::MoveLoopEscapeBehavior escape_behavior) override;

  // PlatformWindowDelegate:
  void DispatchEvent(ui::Event* event) override;
  void OnClosed() override;
  void OnBoundsChanged(const BoundsChange& change) override;

  ui::X11Extension* GetX11Extension();
  const ui::X11Extension* GetX11Extension() const;

  // DesktopWindowTreeHostPlatform:
  void AddAdditionalInitProperties(
      const Widget::InitParams& params,
      ui::PlatformWindowInitProperties* properties) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(DesktopWindowTreeHostPlatformImplTestWithTouch,
                           HitTest);

  // DesktopWindowTreeHostPlatform:
  base::flat_map<std::string, std::string> GetKeyboardLayoutMap() override;

  // Called back by compositor_observer_ if the latter is set.
  virtual void OnCompleteSwapWithNewSize(const gfx::Size& size);

  void CreateNonClientEventFilter();
  void DestroyNonClientEventFilter();

  // X11ExtensionDelegate overrides:
  void OnLostMouseGrab() override;
#if BUILDFLAG(USE_ATK)
  bool OnAtkKeyEvent(AtkKeyEventStruct* atk_key_event, bool transient) override;
#endif  // BUILDFLAG(USE_ATK)
  bool IsOverrideRedirect() const override;
  gfx::Rect GetGuessedFullScreenSizeInPx() const override;

  // Enables event listening after closing |dialog|.
  void EnableEventListening();

  // A handler for events intended for non client area.
  // A posthandler for events intended for non client area. Handles events if no
  // other consumer handled them.
  std::unique_ptr<WindowEventFilterClass> non_client_window_event_filter_;

  std::unique_ptr<CompositorObserver> compositor_observer_;

  std::unique_ptr<aura::ScopedWindowTargeter> targeter_for_modal_;

  uint32_t modal_dialog_counter_ = 0;

  // The display and the native X window hosting the root window.
  base::WeakPtrFactory<DesktopWindowTreeHostLinux> weak_factory_{this};
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_LINUX_H_
