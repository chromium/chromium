// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/x11_whole_screen_move_loop.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/x/x11_pointer_grab.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/scoped_event_dispatcher.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/events/x/x11_window_event_manager.h"
#include "ui/gfx/x/x11.h"

namespace views {

// XGrabKey requires the modifier mask to explicitly be specified.
const unsigned int kModifiersMasks[] = {
  0,                                // No additional modifier.
  Mod2Mask,                         // Num lock
  LockMask,                         // Caps lock
  Mod5Mask,                         // Scroll lock
  Mod2Mask | LockMask,
  Mod2Mask | Mod5Mask,
  LockMask | Mod5Mask,
  Mod2Mask | LockMask | Mod5Mask
};

X11WholeScreenMoveLoop::X11WholeScreenMoveLoop(X11MoveLoopDelegate* delegate)
    : delegate_(delegate),
      in_move_loop_(false),
      initial_cursor_(ui::CursorType::kNull),
      should_reset_mouse_flags_(false),
      grab_input_window_(x11::None),
      grabbed_pointer_(false),
      canceled_(false) {}

X11WholeScreenMoveLoop::~X11WholeScreenMoveLoop() = default;

void X11WholeScreenMoveLoop::DispatchMouseMovement() {
  if (!last_motion_in_screen_)
    return;
  delegate_->OnMouseMovement(last_motion_in_screen_->location(),
                             last_motion_in_screen_->flags(),
                             last_motion_in_screen_->time_stamp());
  last_motion_in_screen_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostLinux, ui::PlatformEventDispatcher implementation:

bool X11WholeScreenMoveLoop::CanDispatchEvent(const ui::PlatformEvent& event) {
  return in_move_loop_;
}

uint32_t X11WholeScreenMoveLoop::DispatchEvent(const ui::PlatformEvent& event) {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());

  // This method processes all events while the move loop is active.
  if (!in_move_loop_)
    return ui::POST_DISPATCH_PERFORM_DEFAULT;

  XEvent* xev = event;
  ui::EventType type = ui::EventTypeFromNative(xev);
  switch (type) {
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_DRAGGED: {
      bool dispatch_mouse_event = !last_motion_in_screen_.get();
      last_motion_in_screen_.reset(
          ui::EventFromNative(xev).release()->AsMouseEvent());
      last_motion_in_screen_->set_location(
          ui::EventSystemLocationFromNative(xev));
      if (dispatch_mouse_event) {
        // Post a task to dispatch mouse movement event when control returns to
        // the message loop. This allows smoother dragging since the events are
        // dispatched without waiting for the drag widget updates.
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::BindOnce(&X11WholeScreenMoveLoop::DispatchMouseMovement,
                           weak_factory_.GetWeakPtr()));
      }
      return ui::POST_DISPATCH_NONE;
    }
    case ui::ET_MOUSE_RELEASED: {
      int button = (xev->type == ButtonRelease)
          ? xev->xbutton.button
          : ui::EventButtonFromNative(xev);
      if (button == Button1) {
        // Assume that drags are being done with the left mouse button. Only
        // break the drag if the left mouse button was released.
        DispatchMouseMovement();
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
    case ui::ET_KEY_PRESSED:
      if (ui::KeyboardCodeFromXKeyEvent(xev) == ui::VKEY_ESCAPE) {
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

bool X11WholeScreenMoveLoop::RunMoveLoop(aura::Window* source,
                                         gfx::NativeCursor cursor) {
  DCHECK(!in_move_loop_);  // Can only handle one nested loop at a time.

  // Query the mouse cursor prior to the move loop starting so that it can be
  // restored when the move loop finishes.
  initial_cursor_ = source->GetHost()->last_cursor();

  CreateDragInputWindow(gfx::GetXDisplay());

  // Only grab mouse capture of |grab_input_window_| if |source| does not have
  // capture.
  // - The caller may intend to transfer capture to a different aura::Window
  //   when the move loop ends and not release capture.
  // - Releasing capture and X window destruction are both asynchronous. We drop
  //   events targeted at |grab_input_window_| in the time between the move
  //   loop ends and |grab_input_window_| loses capture.
  grabbed_pointer_ = false;
  if (!source->HasCapture()) {
    aura::client::CaptureClient* capture_client =
        aura::client::GetCaptureClient(source->GetRootWindow());
    CHECK(capture_client->GetGlobalCaptureWindow() == nullptr);
    grabbed_pointer_ = GrabPointer(cursor);
    if (!grabbed_pointer_) {
      XDestroyWindow(gfx::GetXDisplay(), grab_input_window_);
      return false;
    }
  }

  GrabEscKey();

  std::unique_ptr<ui::ScopedEventDispatcher> old_dispatcher =
      std::move(nested_dispatcher_);
  nested_dispatcher_ =
         ui::PlatformEventSource::GetInstance()->OverrideDispatcher(this);

  // We are handling a mouse drag outside of the aura::Window system. We must
  // manually make aura think that the mouse button is pressed so that we don't
  // draw extraneous tooltips.
  aura::Env* env = aura::Env::GetInstance();
  if (!env->IsMouseButtonDown()) {
    env->set_mouse_button_flags(ui::EF_LEFT_MOUSE_BUTTON);
    should_reset_mouse_flags_ = true;
  }

  base::WeakPtr<X11WholeScreenMoveLoop> alive(weak_factory_.GetWeakPtr());

  in_move_loop_ = true;
  canceled_ = false;
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();

  if (!alive)
    return false;

  nested_dispatcher_ = std::move(old_dispatcher);
  return !canceled_;
}

void X11WholeScreenMoveLoop::UpdateCursor(gfx::NativeCursor cursor) {
  if (in_move_loop_)
    ui::ChangeActivePointerGrabCursor(cursor.platform());
}

void X11WholeScreenMoveLoop::EndMoveLoop() {
  if (!in_move_loop_)
    return;

  // Prevent DispatchMouseMovement from dispatching any posted motion event.
  last_motion_in_screen_.reset();

  // We undo our emulated mouse click from RunMoveLoop();
  if (should_reset_mouse_flags_) {
    aura::Env::GetInstance()->set_mouse_button_flags(0);
    should_reset_mouse_flags_ = false;
  }

  // TODO(erg): Is this ungrab the cause of having to click to give input focus
  // on drawn out windows? Not ungrabbing here screws the X server until I kill
  // the chrome process.

  // Ungrab before we let go of the window.
  if (grabbed_pointer_)
    ui::UngrabPointer();
  else
    UpdateCursor(initial_cursor_);

  XDisplay* display = gfx::GetXDisplay();
  unsigned int esc_keycode = XKeysymToKeycode(display, XK_Escape);
  for (auto mask : kModifiersMasks)
    XUngrabKey(display, esc_keycode, mask, grab_input_window_);

  // Restore the previous dispatcher.
  nested_dispatcher_.reset();
  delegate_->OnMoveLoopEnded();
  grab_input_window_events_.reset();
  XDestroyWindow(display, grab_input_window_);
  grab_input_window_ = x11::None;

  in_move_loop_ = false;
  std::move(quit_closure_).Run();
}

bool X11WholeScreenMoveLoop::GrabPointer(gfx::NativeCursor cursor) {
  XDisplay* display = gfx::GetXDisplay();

  // Pass "owner_events" as false so that X sends all mouse events to
  // |grab_input_window_|.
  int ret = ui::GrabPointer(grab_input_window_, false, cursor.platform());
  if (ret != GrabSuccess) {
    DLOG(ERROR) << "Grabbing pointer for dragging failed: "
                << ui::GetX11ErrorString(display, ret);
  }
  XFlush(display);
  return ret == GrabSuccess;
}

void X11WholeScreenMoveLoop::GrabEscKey() {
  XDisplay* display = gfx::GetXDisplay();
  unsigned int esc_keycode = XKeysymToKeycode(display, XK_Escape);
  for (auto mask : kModifiersMasks) {
    XGrabKey(display, esc_keycode, mask, grab_input_window_, x11::False,
             GrabModeAsync, GrabModeAsync);
  }
}

void X11WholeScreenMoveLoop::CreateDragInputWindow(XDisplay* display) {
  unsigned long attribute_mask = CWEventMask | CWOverrideRedirect;
  XSetWindowAttributes swa;
  memset(&swa, 0, sizeof(swa));
  swa.override_redirect = x11::True;
  grab_input_window_ = XCreateWindow(display, DefaultRootWindow(display), -100,
                                     -100, 10, 10, 0, CopyFromParent, InputOnly,
                                     CopyFromParent, attribute_mask, &swa);
  uint32_t event_mask = ButtonPressMask | ButtonReleaseMask |
                        PointerMotionMask | KeyPressMask | KeyReleaseMask |
                        StructureNotifyMask;
  grab_input_window_events_ = std::make_unique<ui::XScopedEventSelector>(
      grab_input_window_, event_mask);

  XMapRaised(display, grab_input_window_);
}

}  // namespace views
