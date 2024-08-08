// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_LACROS_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_LACROS_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window_observer.h"
#include "ui/base/buildflags.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

namespace ui {
class WaylandToplevelExtension;
class DeskExtension;
class PinnedModeExtension;
}  // namespace ui

namespace views {

class WindowEventFilterLacros;

// Contains Lacros specific implementation.
class VIEWS_EXPORT DesktopWindowTreeHostLacros
    : public DesktopWindowTreeHostPlatform,
      public aura::WindowObserver {
 public:
  // Casts from a base WindowTreeHost instance.
  static DesktopWindowTreeHostLacros* From(WindowTreeHost* wth);

  DesktopWindowTreeHostLacros(
      internal::NativeWidgetDelegate* native_widget_delegate,
      DesktopNativeWidgetAura* desktop_native_widget_aura);

  DesktopWindowTreeHostLacros(const DesktopWindowTreeHostLacros&) = delete;
  DesktopWindowTreeHostLacros& operator=(const DesktopWindowTreeHostLacros&) =
      delete;

  ~DesktopWindowTreeHostLacros() override;

  ui::WaylandToplevelExtension* GetWaylandToplevelExtension();
  const ui::WaylandToplevelExtension* GetWaylandToplevelExtension() const;

  ui::DeskExtension* GetDeskExtension();
  const ui::DeskExtension* GetDeskExtension() const;

  ui::PinnedModeExtension* GetPinnedModeExtension();
  const ui::PinnedModeExtension* GetPinnedModeExtension() const;

 protected:
  // Overridden from DesktopWindowTreeHost:
  void OnNativeWidgetCreated(const Widget::InitParams& params) override;
  void InitModalType(ui::mojom::ModalType modal_type) override;

  // PlatformWindowDelegate:
  void OnClosed() override;
  void OnWindowStateChanged(ui::PlatformWindowState old_state,
                            ui::PlatformWindowState new_state) override;
  void OnFullscreenTypeChanged(ui::PlatformFullscreenType old_type,
                               ui::PlatformFullscreenType new_type) override;
  void OnOverviewModeChanged(bool in_overview) override;
  void OnTooltipShownOnServer(const std::u16string& text,
                              const gfx::Rect& bounds) override;
  void OnTooltipHiddenOnServer() override;
  void OnBoundsChanged(const BoundsChange& change) override;

  // DesktopWindowTreeHostPlatform overrides:
  void AddAdditionalInitProperties(
      const Widget::InitParams& params,
      ui::PlatformWindowInitProperties* properties) override;
  Widget::MoveLoopResult RunMoveLoop(
      const gfx::Vector2d& drag_offset,
      Widget::MoveLoopSource source,
      Widget::MoveLoopEscapeBehavior escape_behavior) override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;

  // DesktopWindowTreeHost:
  void OnWidgetInitDone() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(DesktopWindowTreeHostPlatformImplTestWithTouch,
                           HitTest);

  void CreateNonClientEventFilter();
  void DestroyNonClientEventFilter();

  // Sets hints for the WM/compositor that reflect the rounded corners.
  void UpdateWindowHints();

  // A handler for events intended for non client area.
  // A posthandler for events intended for non client area. Handles events if no
  // other consumer handled them.
  std::unique_ptr<WindowEventFilterLacros> non_client_window_event_filter_;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      content_window_observation_{this};

  // The display and the native X window hosting the root window.
  base::WeakPtrFactory<DesktopWindowTreeHostLacros> weak_factory_{this};
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_LACROS_H_
