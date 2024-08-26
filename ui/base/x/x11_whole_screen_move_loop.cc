// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_whole_screen_move_loop.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/base/x/x11_pointer_grab.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/scoped_event_dispatcher.h"
#include "ui/events/x/events_x_utils.h"
#include "ui/events/x/x11_event_translation.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/keysyms/keysyms.h"
#include "ui/gfx/x/window_event_manager.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

namespace {

// XGrabKey requires the modifier mask to explicitly be specified.
constexpr x11::ModMask kModifiersMasks[] = {
    {},                  // No additional modifier.
    x11::ModMask::c_2,   // Num lock
    x11::ModMask::Lock,  // Caps lock
    x11::ModMask::c_5,   // Scroll lock
    x11::ModMask::c_2 | x11::ModMask::Lock,
    x11::ModMask::c_2 | x11::ModMask::c_5,
    x11::ModMask::Lock | x11::ModMask::c_5,
    x11::ModMask::c_2 | x11::ModMask::Lock | x11::ModMask::c_5,
};

const char* GrabStatusToString(x11::GrabStatus grab_status) {
  switch (grab_status) {
    case x11::GrabStatus::Success:
      return "Success";
    case x11::GrabStatus::AlreadyGrabbed:
      return "AlreadyGrabbed";
    case x11::GrabStatus::InvalidTime:
      return "InvalidTime";
    case x11::GrabStatus::NotViewable:
      return "NotViewable";
    case x11::GrabStatus::Frozen:
      return "Frozen";
  }
  NOTREACHED();
}

}  // namespace

X11WholeScreenMoveLoop::X11WholeScreenMoveLoop(X11MoveLoopDelegate* delegate)
    : delegate_(delegate),
      in_move_loop_(false),
      grab_input_window_(x11::Window::None),
      grabbed_pointer_(false),
      canceled_(false) {}

X11WholeScreenMoveLoop::~X11WholeScreenMoveLoop() {
  EndMoveLoop();
}

void X11WholeScreenMoveLoop::PostDispatchIfNeeded(const ui::MouseEvent& event) {
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostLinux, ui::PlatformEventDispatcher implementation:

bool X11WholeScreenMoveLoop::CanDispatchEvent(const ui::PlatformEvent& event) {
  return in_move_loop_;
}

uint32_t X11WholeScreenMoveLoop::DispatchEvent(const ui::PlatformEvent& event) {
  DCHECK(base::CurrentUIThread::IsSet());

  // This method processes all events while the move loop is active.
  if (!in_move_loop_) {
    return ui::POST_DISPATCH_PERFORM_DEFAULT;
  }

  switch (event->type()) {
    case ui::EventType::kMouseMoved:
    case ui::EventType::kMouseDragged: {
      auto& current_xevent = *x11::Connection::Get()->dispatching_event();
      x11::Event last_xevent;
      std::unique_ptr<ui::Event> last_motion;
      auto* mouse_event = event->AsMouseEvent();
      if ((current_xevent.As<x11::MotionNotifyEvent>() ||
           current_xevent.As<x11::Input::DeviceEvent>()) &&
          ui::CoalescePendingMotionEvents(current_xevent, &last_xevent)) {
        last_motion = ui::BuildEventFromXEvent(last_xevent);
        mouse_event = last_motion->AsMouseEvent();
      }
      delegate_->OnMouseMovement(mouse_event->root_location(),
                                 mouse_event->flags(),
                                 mouse_event->time_stamp());
      return ui::POST_DISPATCH_NONE;
    }
    case ui::EventType::kMouseReleased: {
      if (event->AsMouseEvent()->IsLeftMouseButton()) {
        // Assume that drags are being done with the left mouse button. Only
        // break the drag if the left mouse button was released.
        delegate_->OnMouseReleased();

        if (!grabbed_pointer_) {
          // If the source widget had capture prior to the move loop starting,
          // it may be relying on views::Widget getting the mouse release and
          // releasing capture in Widget::OnMouseEvent().
          return ui::POST_DISPATCH_PERFORM_DEFAULT;
        }
      }
      return ui::POST_DISPATCH_NONE;
    }
    case ui::EventType::kKeyPressed:
      if (event->AsKeyEvent()->key_code() == ui::VKEY_ESCAPE) {
        canceled_ = true;
        EndMoveLoop();
        return ui::POST_DISPATCH_NONE;
      }
      break;
    default:
      break;
  }
  return ui::POST_DISPATCH_PERFORM_DEFAULT;
}

bool X11WholeScreenMoveLoop::RunMoveLoop(
    bool can_grab_pointer,
    scoped_refptr<ui::X11Cursor> old_cursor,
    scoped_refptr<ui::X11Cursor> new_cursor,
    base::OnceClosure started_callback) {
  DCHECK(!in_move_loop_);  // Can only handle one nested loop at a time.

  // Query the mouse cursor prior to the move loop starting so that it can be
  // restored when the move loop finishes.
  initial_cursor_ = old_cursor;

  auto* connection = x11::Connection::Get();
  CreateDragInputWindow(connection);

  // Only grab mouse capture of |grab_input_window_| if |can_grab_pointer| is
  // true aka the source that initiated the move loop doesn't have explicit
  // grab.
  // - The caller may intend to transfer capture to a different X11Window
  //   when the move loop ends and not release capture.
  // - Releasing capture and X window destruction are both asynchronous. We drop
  //   events targeted at |grab_input_window_| in the time between the move
  //   loop ends and |grab_input_window_| loses capture.
  grabbed_pointer_ = false;
  if (can_grab_pointer) {
    grabbed_pointer_ = GrabPointer(new_cursor);
    if (!grabbed_pointer_) {
      x11::Connection::Get()->DestroyWindow({grab_input_window_});
      return false;
    }
  }

  GrabEscKey();

  std::unique_ptr<ui::ScopedEventDispatcher> old_dispatcher =
      std::move(nested_dispatcher_);
  nested_dispatcher_ =
      ui::PlatformEventSource::GetInstance()->OverrideDispatcher(this);

  std::move(started_callback).Run();

  base::WeakPtr<X11WholeScreenMoveLoop> alive(weak_factory_.GetWeakPtr());

  in_move_loop_ = true;
  canceled_ = false;
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();

  if (!alive) {
    return false;
  }

  nested_dispatcher_ = std::move(old_dispatcher);
  return !canceled_;
}

void X11WholeScreenMoveLoop::UpdateCursor(scoped_refptr<ui::X11Cursor> cursor) {
  if (in_move_loop_) {
    ui::ChangeActivePointerGrabCursor(cursor);
  }
}

void X11WholeScreenMoveLoop::EndMoveLoop() {
  if (!in_move_loop_) {
    return;
  }

  // TODO(erg): Is this ungrab the cause of having to click to give input focus
  // on drawn out windows? Not ungrabbing here screws the X server until I kill
  // the chrome process.

  // Ungrab before we let go of the window.
  if (grabbed_pointer_) {
    ui::UngrabPointer();
  } else {
    UpdateCursor(initial_cursor_);
  }

  auto* connection = x11::Connection::Get();
  auto esc_keycode = connection->KeysymToKeycode(XK_Escape);
  for (auto mask : kModifiersMasks) {
    connection->UngrabKey({esc_keycode, grab_input_window_, mask});
  }

  // Restore the previous dispatcher.
  nested_dispatcher_.reset();
  delegate_->OnMoveLoopEnded();
  grab_input_window_events_.Reset();
  connection->DestroyWindow({grab_input_window_});
  grab_input_window_ = x11::Window::None;

  in_move_loop_ = false;
  std::move(quit_closure_).Run();
}

bool X11WholeScreenMoveLoop::GrabPointer(scoped_refptr<X11Cursor> cursor) {
  auto* connection = x11::Connection::Get();

  // Pass "owner_events" as false so that X sends all mouse events to
  // |grab_input_window_|.
  auto ret = ui::GrabPointer(grab_input_window_, false, cursor);
  if (ret != x11::GrabStatus::Success) {
    DLOG(ERROR) << "Grabbing pointer for dragging failed: "
                << GrabStatusToString(ret);
  }
  connection->Flush();
  return ret == x11::GrabStatus::Success;
}

void X11WholeScreenMoveLoop::GrabEscKey() {
  auto* connection = x11::Connection::Get();
  auto esc_keycode = connection->KeysymToKeycode(XK_Escape);
  for (auto mask : kModifiersMasks) {
    connection->GrabKey({false, grab_input_window_, mask, esc_keycode,
                         x11::GrabMode::Async, x11::GrabMode::Async});
  }
}

void X11WholeScreenMoveLoop::CreateDragInputWindow(
    x11::Connection* connection) {
  grab_input_window_ = connection->GenerateId<x11::Window>();
  connection->CreateWindow(x11::CreateWindowRequest{
      .wid = grab_input_window_,
      .parent = connection->default_root(),
      .x = -100,
      .y = -100,
      .width = 10,
      .height = 10,
      .c_class = x11::WindowClass::InputOnly,
      .override_redirect = x11::Bool32(true),
  });
  auto event_mask =
      x11::EventMask::ButtonPress | x11::EventMask::ButtonRelease |
      x11::EventMask::PointerMotion | x11::EventMask::KeyPress |
      x11::EventMask::KeyRelease | x11::EventMask::StructureNotify;
  grab_input_window_events_ =
      connection->ScopedSelectEvent(grab_input_window_, event_mask);
  connection->MapWindow(grab_input_window_);
  connection->RaiseWindow(grab_input_window_);
}

}  // namespace ui
