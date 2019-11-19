// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_x11.h"

#include <algorithm>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/null_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/buildflags.h"
#include "ui/base/class_property.h"
#include "ui/base/dragdrop/os_exchange_data_provider_aurax11.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/layout.h"
#include "ui/base/x/x11_pointer_grab.h"
#include "ui/base/x/x11_util.h"
#include "ui/base/x/x11_util_internal.h"
#include "ui/display/screen.h"
#include "ui/events/devices/x11/device_list_cache_x11.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/x/events_x_utils.h"
#include "ui/events/x/x11_window_event_manager.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_path.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/views_switches.h"
#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_aurax11.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_observer_x11.h"
#include "ui/views/widget/desktop_aura/x11_desktop_handler.h"
#include "ui/views/widget/desktop_aura/x11_desktop_window_move_client.h"
#include "ui/views/window/native_frame_view.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/core/window_util.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostX11, public:
DesktopWindowTreeHostX11::DesktopWindowTreeHostX11(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura)
    : DesktopWindowTreeHostLinux(native_widget_delegate,
                                 desktop_native_widget_aura) {}

DesktopWindowTreeHostX11::~DesktopWindowTreeHostX11() {
  wm::SetWindowMoveClient(window(), nullptr);

  // ~DWTHPlatform notifies the DestkopNativeWidgetAura about destruction and
  // also destroyes the dispatcher.
}

void DesktopWindowTreeHostX11::AddObserver(
    DesktopWindowTreeHostObserverX11* observer) {
  observer_list_.AddObserver(observer);
}

void DesktopWindowTreeHostX11::RemoveObserver(
    DesktopWindowTreeHostObserverX11* observer) {
  observer_list_.RemoveObserver(observer);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostX11, DesktopWindowTreeHost implementation:

void DesktopWindowTreeHostX11::Init(const Widget::InitParams& params) {
  DesktopWindowTreeHostLinux::Init(params);

  // Set XEventDelegate to receive selection, drag&drop and raw key events.
  //
  // TODO(https://crbug.com/990756): There are two cases of this delegate:
  // XEvents for DragAndDrop client and raw key events. DragAndDrop could be
  // unified so that DragAndrDropClientOzone is used and XEvent are handled on
  // platform level.
  static_cast<ui::X11Window*>(platform_window())->SetXEventDelegate(this);
}

void DesktopWindowTreeHostX11::OnNativeWidgetCreated(
    const Widget::InitParams& params) {
  // Ensure that the X11DesktopHandler exists so that it tracks create/destroy
  // notify events.
  X11DesktopHandler::get();

  x11_window_move_client_ = std::make_unique<X11DesktopWindowMoveClient>();
  wm::SetWindowMoveClient(window(), x11_window_move_client_.get());

  DesktopWindowTreeHostLinux::OnNativeWidgetCreated(params);
}

std::unique_ptr<aura::client::DragDropClient>
DesktopWindowTreeHostX11::CreateDragDropClient(
    DesktopNativeCursorManager* cursor_manager) {
  drag_drop_client_ = new DesktopDragDropClientAuraX11(window(), cursor_manager,
                                                       GetXWindow()->display(),
                                                       GetXWindow()->window());
  drag_drop_client_->Init();
  return base::WrapUnique(drag_drop_client_);
}

Widget::MoveLoopResult DesktopWindowTreeHostX11::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  wm::WindowMoveSource window_move_source =
      source == Widget::MOVE_LOOP_SOURCE_MOUSE ? wm::WINDOW_MOVE_SOURCE_MOUSE
                                               : wm::WINDOW_MOVE_SOURCE_TOUCH;
  if (x11_window_move_client_->RunMoveLoop(GetContentWindow(), drag_offset,
                                           window_move_source) ==
      wm::MOVE_SUCCESSFUL)
    return Widget::MOVE_LOOP_SUCCESSFUL;

  return Widget::MOVE_LOOP_CANCELED;
}

void DesktopWindowTreeHostX11::EndMoveLoop() {
  x11_window_move_client_->EndMoveLoop();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostX11 implementation:

void DesktopWindowTreeHostX11::OnXWindowMapped() {
  for (DesktopWindowTreeHostObserverX11& observer : observer_list_)
    observer.OnWindowMapped(GetXWindow()->window());
}

void DesktopWindowTreeHostX11::OnXWindowUnmapped() {
  for (DesktopWindowTreeHostObserverX11& observer : observer_list_)
    observer.OnWindowUnmapped(GetXWindow()->window());
}

void DesktopWindowTreeHostX11::OnXWindowSelectionEvent(XEvent* xev) {
  DCHECK(xev);
  DCHECK(drag_drop_client_);
  drag_drop_client_->OnSelectionNotify(xev->xselection);
}

void DesktopWindowTreeHostX11::OnXWindowDragDropEvent(XEvent* xev) {
  DCHECK(xev);
  DCHECK(drag_drop_client_);

  ::Atom message_type = xev->xclient.message_type;
  if (message_type == gfx::GetAtom("XdndEnter")) {
    drag_drop_client_->OnXdndEnter(xev->xclient);
  } else if (message_type == gfx::GetAtom("XdndLeave")) {
    drag_drop_client_->OnXdndLeave(xev->xclient);
  } else if (message_type == gfx::GetAtom("XdndPosition")) {
    drag_drop_client_->OnXdndPosition(xev->xclient);
  } else if (message_type == gfx::GetAtom("XdndStatus")) {
    drag_drop_client_->OnXdndStatus(xev->xclient);
  } else if (message_type == gfx::GetAtom("XdndFinished")) {
    drag_drop_client_->OnXdndFinished(xev->xclient);
  } else if (message_type == gfx::GetAtom("XdndDrop")) {
    drag_drop_client_->OnXdndDrop(xev->xclient);
  }
}

const ui::XWindow* DesktopWindowTreeHostX11::GetXWindow() const {
  DCHECK(platform_window());
  // ui::X11Window inherits both PlatformWindow and ui::XWindow.
  return static_cast<const ui::XWindow*>(
      static_cast<const ui::X11Window*>(platform_window()));
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHost, public:

// static
DesktopWindowTreeHost* DesktopWindowTreeHost::Create(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura) {
  return new DesktopWindowTreeHostX11(native_widget_delegate,
                                      desktop_native_widget_aura);
}

}  // namespace views
