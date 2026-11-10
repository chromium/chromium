// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_wm_move_loop.h"

#include "base/run_loop.h"
#include "ui/base/x/x11_pointer_grab.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xinput.h"

namespace ui {

namespace {

// From the EWMH spec.
constexpr uint32_t kNetWmMoveResizeMove = 8;
constexpr uint32_t kNetWmMoveResizeCancel = 11;

}  // namespace

BASE_FEATURE(kX11WmMoveLoop, base::FEATURE_ENABLED_BY_DEFAULT);

X11WmMoveLoop::X11WmMoveLoop(x11::Connection& connection,
                             x11::Window root_window,
                             x11::Window window)
    : connection_(connection), root_window_(root_window), window_(window) {}

X11WmMoveLoop::~X11WmMoveLoop() {
  End();
}

// static
bool X11WmMoveLoop::IsSupported(x11::Connection& connection) {
  if (!base::FeatureList::IsEnabled(kX11WmMoveLoop) || !IsXInput2Available() ||
      !connection.WmSupportsHint(connection.GetAtom("_NET_WM_MOVERESIZE"))) {
    return false;
  }
  switch (GuessWindowManager()) {
    case WM_COMPIZ:
    case WM_METACITY:
    case WM_MUFFIN:
    case WM_MUTTER:
      return true;
    default:
      // Other window managers either don't constrain client-requested moves
      // or haven't been verified to handle _NET_WM_MOVERESIZE well for tab
      // dragging.
      return false;
  }
}

bool X11WmMoveLoop::Run(const gfx::Vector2d& drag_offset, bool window_mapped) {
  CHECK(!in_move_loop_);
  in_move_loop_ = true;
  drag_offset_ = drag_offset;

  connection_->AddEventObserver(this);
  SelectRawButtonReleaseEvents(true);

  // The button may have been released before raw button events were selected.
  if (!IsLeftButtonDown()) {
    End();
    return true;
  }

  // The window manager ignores _NET_WM_MOVERESIZE for windows that it doesn't
  // manage yet, so wait for newly created windows to be mapped.
  if (window_mapped) {
    StartWmMove();
  }

  base::WeakPtr<X11WmMoveLoop> alive(weak_factory_.GetWeakPtr());
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
  return !!alive;
}

void X11WmMoveLoop::End() {
  if (!in_move_loop_) {
    return;
  }
  in_move_loop_ = false;

  // If the move loop ends before the button is released (e.g. the dragged tabs
  // are attached to another tab strip), the window manager is still moving the
  // window, so cancel the move.
  if (wm_move_started_) {
    SendClientMessage(window_, root_window_,
                      connection_->GetAtom("_NET_WM_MOVERESIZE"),
                      {0, 0, kNetWmMoveResizeCancel, 0, 0});
  }
  SelectRawButtonReleaseEvents(false);
  connection_->RemoveEventObserver(this);
  connection_->Flush();

  if (quit_closure_) {
    std::move(quit_closure_).Run();
  }
}

void X11WmMoveLoop::StartWmMove() {
  if (wm_move_started_) {
    return;
  }
  wm_move_started_ = true;

  // Anchor the move at the point of the window that should stay under the
  // pointer. The window manager moves the window relative to this point, so the
  // window snaps to the pointer even if its position was constrained before.
  gfx::Point anchor;
  if (auto reply = connection_
                       ->TranslateCoordinates(
                           {.src_window = window_, .dst_window = root_window_})
                       .Sync()) {
    anchor.SetPoint(reply->dst_x, reply->dst_y);
  }
  anchor += drag_offset_;

  // The window manager can only grab the pointer once no other grab, including
  // the implicit grab from the button press that started the drag, is active.
  UngrabPointer();
  SendClientMessage(
      window_, root_window_, connection_->GetAtom("_NET_WM_MOVERESIZE"),
      {static_cast<uint32_t>(anchor.x()), static_cast<uint32_t>(anchor.y()),
       kNetWmMoveResizeMove, 0, 0});
  connection_->Flush();
}

void X11WmMoveLoop::OnEvent(const x11::Event& event) {
  if (!in_move_loop_) {
    return;
  }
  if (auto* map = event.As<x11::MapNotifyEvent>()) {
    if (map->window == window_) {
      StartWmMove();
    }
    return;
  }
  auto* raw = event.As<x11::Input::RawDeviceEvent>();
  if (!raw || raw->opcode != x11::Input::RawDeviceEvent::RawButtonRelease) {
    return;
  }
  // Raw events report physical buttons, so check the logical button state
  // instead of `raw->detail`.
  if (!IsLeftButtonDown()) {
    End();
  }
}

void X11WmMoveLoop::SelectRawButtonReleaseEvents(bool select) {
  x11::Input::XIEventMask mask{};
  if (select) {
    mask = x11::Input::XIEventMask::RawButtonRelease;
  }
  connection_->xinput().XISelectEvents(
      {root_window_, {{x11::Input::DeviceId::AllMaster, {mask}}}});
}

bool X11WmMoveLoop::IsLeftButtonDown() {
  auto reply = connection_->QueryPointer({root_window_}).Sync();
  return reply && static_cast<bool>(reply->mask & x11::KeyButMask::Button1);
}

}  // namespace ui
